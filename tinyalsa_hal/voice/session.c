/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file    session.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#include "alsa_mixer.h"
#include "alsa_route.h"
#include "audio_hw.h"
#include "utils/channel.h"
#include "voice.h"

#define LOG_NDEBUG 0
#define LOG_TAG "voice-session"

static size_t voice_correct_frames_size(unsigned int rate_in, unsigned int rate_out, int frames)
{
    size_t size;

    size = (frames * rate_out) / rate_in;
    size = ((size + 15) / 16) * 16;

    return size;
}

static int voice_prepare_dump(struct voice_session *session, int index)
{
    struct voice_dump *dump;
    int i;

    for (i = 0; i < MAX_STREAM_DIR; ++i) {
        dump = &session->dumps[i];

        memset(dump->path, 0, sizeof(dump->path));
        sprintf(dump->path, "/data/misc/audioserver/session%d_%s.pcm", index,
                i == VOICE_STREAM_SINK ? "in" : "out");
        dump->session_index = index;
    }

    return 0;
}

static int voice_prepare_buffer(struct voice_session *session)
{
    const struct voice_stream *stream;
    size_t frames;
    size_t expect;

    stream = session->stream;

    frames = stream->source->config->period_size;
    session->in_buffer_size = pcm_frames_to_bytes(session->pcm_rx, frames);

    session->in_buffer = (int16_t *)malloc(session->in_buffer_size);
    if (!session->in_buffer) {
        ALOGE("%s: failed to allocate in buffer", __FUNCTION__);
        return -ENOMEM;
    }

    frames = stream->sink->config->period_size;
    expect = voice_correct_frames_size(stream->source->config->rate,
                                       stream->sink->config->rate,
                                       stream->source->config->period_size);

    if (frames != expect)
        frames = expect;

    session->out_buffer_size = pcm_frames_to_bytes(session->pcm_tx, frames);
    session->out_buffer = (int16_t *)malloc(session->out_buffer_size);
    if (!session->out_buffer) {
        ALOGE("%s: failed to allocate out buffer", __FUNCTION__);
        return -ENOMEM;
    }

    return 0;
}

static int voice_prepare_processor(struct voice_session *session)
{
    const struct voice_stream *stream = session->stream;
    int ret = 0;
    
    if ((stream->quirks & VOICE_STREAM_OUTGOING) &&
        (stream->source->quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)) {
        ret = voice_effect_init(&session->effect, stream->source->config->rate,
                                stream->source->config->channels,
                                stream->source->config->period_size,
                                pcm_format_to_bits(stream->source->config->format));
        if (ret)
            ALOGE("%s: failed to initialize effect", __FUNCTION__);
    }

    return ret;
}

static void voice_dump_data(struct voice_session *session, const void *buffer, size_t bytes,
                            bool is_sink)
{
    struct voice_dump *dump;

    if (is_sink)
        dump = &session->dumps[VOICE_STREAM_SINK];
    else
        dump = &session->dumps[VOICE_STREAM_SOURCE];

    if(dump->file == NULL && NULL != buffer && 0 != bytes) {
        char value[PROPERTY_VALUE_MAX];
        property_get("vendor.audio.voice.dump", value, "0");
        dump->size = atoi(value);

        if (!dump->size)
            return;

        ALOGD("%s: property vendor.audio.voice.dump = %d MB", __func__, dump->size);
        dump->offset = 0;
        dump->size = dump->size * 1024 * 1024;
        dump->file = fopen(dump->path, "wb+");
        if (!dump->file) {
            ALOGE("failed to open %s, errno = %s", dump->path, strerror(errno));
            return;
        }
        ALOGD("%s: create session%d_%s.pcm successful", __func__, dump->session_index, is_sink?"in":"out");
    }

    if(dump->file) {
        if(NULL == buffer || 0 == bytes) {
            fclose(dump->file);
            dump->file = NULL;
            ALOGD("session%d_%s.pcm file closed!",
                  is_sink?VOICE_STREAM_SINK:VOICE_STREAM_SOURCE, is_sink?"in":"out");
        }
        else {
            fwrite(buffer, bytes, 1, dump->file);
            dump->offset += bytes;
            fflush(dump->file);

            if (dump->offset >= dump->size) {
                fclose(dump->file);
                ALOGD("session session%d_%s.pcm closed!", dump->session_index, is_sink?"in":"out");

                dump->file = NULL;
                dump->offset = 0;
                dump->size = 0;
                dump->session_index = -1;
                property_set("vendor.audio.voice.dump", "0");

                ALOGD("%s: clean property vendor.audio.voice.dump", __func__);
            }
        }
    }
}

static void voice_apply_volume(struct voice_session *session, int16_t *buffer, size_t size)
{
    int i;
    int32_t tmp;
    int32_t chn_interval = 0;
    float volume = session->volume;

    #if 1
    if((/*from mic*/VOICE_STREAM_OUTGOING == (session->stream->quirks & VOICE_STREAM_OUTGOING))
        && !(/*from es7202*/AUDIO_DEVICE_OUT_ALL_SCO & session->stream->route))
    {
        volume *= 10.0f;
    }
    #endif

    for (i = 0; i < size; ++i) {
        tmp = buffer[i] * /*session->volume*/volume;
        if (tmp > 0x7fff) {
            buffer[i] = 0x7fff;
        } else if (tmp < -0x8000) {
            buffer[i] = -0x8000;
        } else {
            buffer[i] = tmp;
        }
    }
}

static int voice_apply_effects(struct voice_session *session)
{
    struct resampler_itfe *resampler = session->resampler;
    const struct voice_stream *stream = session->stream;
    int16_t *final;
    size_t frames_in;
    size_t frames_out;
    size_t samples;
    size_t copy;
    int ret;

    /* NOTE: If the recording is mono, we need to turn it to stereo */
    if (stream->source->config->channels == 2) {
        samples = session->in_buffer_size / 2;
        if (stream->quirks & VOICE_STREAM_CHANNEL_MONO_RIGHT)
            channel_fixed(session->in_buffer, samples, CHR_VALID);

        if (stream->quirks & VOICE_STREAM_CHANNEL_MONO_LEFT)
            channel_fixed(session->in_buffer, samples, CHL_VALID);
    }

    frames_in = stream->source->config->period_size;
    frames_out = stream->sink->config->period_size;

    final = session->in_buffer;
    /*
     * NOTE: If the stream is outgoing and the recording has reference channel,
     * we should apply AEC with rockchip audio preprocess to remove echoes.
     */
    if ((stream->quirks & VOICE_STREAM_OUTGOING) &&
        (stream->source->quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)) {
        ret = voice_effect_process(&session->effect, session->in_buffer, frames_in);
        if (ret) {
            ALOGE("%s: failed to process with effect", __FUNCTION__);
            return 0;
        }
        
        final = session->effect.stereo_buffer;
    }

    if (resampler) {
        resampler->resample_from_input(resampler, final, &frames_in, session->out_buffer,
                                       &frames_out);
        copy = pcm_frames_to_bytes(session->pcm_tx, frames_out);
    } else {
        copy = pcm_frames_to_bytes(session->pcm_tx,
                                   frames_in > frames_out ? frames_out : frames_in);
        memcpy(session->out_buffer, final, copy);
    }

    return copy;
}

static void *voice_offload_thread(void *args)
{
    struct voice_session *session = (struct voice_session *)args;
    const struct voice_stream *stream;
    size_t copy;
    int ret;

    session->offload_running = 1;

    while (session->offload_running) {
        stream = session->stream;

        ret = pcm_read(session->pcm_rx, session->in_buffer, session->in_buffer_size);
        if (ret) {
            ALOGE("%s: failed to read %d", __FUNCTION__, ret);
            ALOGE("%s: %s", __func__, pcm_get_error(session->pcm_rx));
            goto out;
        }

        voice_dump_data(session, session->in_buffer, session->in_buffer_size, true);

        #if 0
            if (!session->mute)
                voice_apply_volume(session, session->in_buffer, session->in_buffer_size / 2);
            else
                memset(session->in_buffer, 0, session->in_buffer_size);

            copy = voice_apply_effects(session);
        
        #else
        
            copy = voice_apply_effects(session);

            if (!session->mute)
                voice_apply_volume(session, session->out_buffer, copy / 2);
            else
                memset(session->out_buffer, 0, copy);
        #endif

        if (copy) {
            voice_dump_data(session, session->out_buffer, copy, false);

            ret = pcm_write(session->pcm_tx, session->out_buffer, copy);
            //ALOGD("[%s %d] %s thread %d: end pcm_read pcm_tx..\n", __func__, __LINE__, to_speaker?"Speaker":"Mic", session->offload_thread);
            if (ret) {
                ALOGE("%s: failed to write %d", __FUNCTION__, ret);
                ALOGE("%s: %s", __func__, pcm_get_error(session->pcm_tx));
                goto out;
            }
        }
    }

out:
    voice_dump_data(session, NULL, 0, false);

    ALOGD("%s: quit, stream->quirks = 0x%x\n", __func__, session->stream->quirks);

    return NULL;
}

void voice_dump_session(struct audio_device *adev)
{
    const struct voice_stream *stream;
    struct voice_session *session;
    /* FIXME: We should extend this buffer length If the path is too long. */
    char buf[128];
    int i;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        stream = session->stream;
        buf[0] = '\0';

        ALOGD("session%d:", i);
        strcat(buf, "  ");
        if (stream->sink && stream->source) {
            if (stream->source->type == VOICE_DAILINK_FE_BE) {
                strcat(buf, stream->source->name);
            } else if (stream->source->type == VOICE_DAILINK_HOSTLESS_FE) {
                strcat(buf, stream->source->name);
                strcat(buf, " -> ");
                strcat(buf, stream->source->backend->name);
            }

            if ((stream->quirks & VOICE_STREAM_OUTGOING) &&
                (stream->source->quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)) {
                strcat(buf, " + ref");
            }

            strcat(buf, " -> SoC -> ");

            if (stream->sink->type == VOICE_DAILINK_FE_BE) {
                strcat(buf, stream->sink->name);
            } else if (stream->sink->type == VOICE_DAILINK_HOSTLESS_FE) {
                strcat(buf, stream->sink->backend->name);
                strcat(buf, " -> ");
                strcat(buf, stream->sink->name);
            }
        } else {
            if (stream->sink) {
                if (stream->sink->type == VOICE_DAILINK_FE_BE) {
                    strcat(buf, "SoC -> ");
                    strcat(buf, stream->sink->name);
                } else if (stream->sink->type == VOICE_DAILINK_HOSTLESS_FE) {
                    strcat(buf, stream->sink->name);
                    strcat(buf, " -> ");
                    strcat(buf, stream->sink->backend->name);
                }
            }

            if (stream->source) {
                if (stream->source->type == VOICE_DAILINK_FE_BE) {
                    strcat(buf, stream->source->name);
                    strcat(buf, " -> SoC");
                } else if (stream->source->type == VOICE_DAILINK_HOSTLESS_FE) {
                    strcat(buf, stream->source->backend->name);
                    strcat(buf, " -> ");
                    strcat(buf, stream->source->name);
                }
            }
        }

        ALOGD("%s", buf);
    }
}

int voice_create_session(struct audio_device *adev)
{
    struct voice_session *session;
    const struct voice_stream *stream;
    int i;
    int ret;

    for (i = 0; i < adev->voice.stream_num; ++i) {
        stream = &adev->voice.streams[i];
        if (0 == (stream->route & adev->voice.route))
            continue;

        if (adev->voice.session_num >= MAX_VOICE_SESSIONS) {
            ALOGE("%s: session num exceed, expect %d", __FUNCTION__, MAX_VOICE_SESSIONS);
            return -EPERM;
        }

        session = &adev->voice.sessions[adev->voice.session_num];
        adev->voice.session_num++;
        session->stream = stream;

        /* NOTE: Default to unmute */
        session->mute = 0;

        session->volume = 1.0f;

        session->offload_running = 0;
        session->offload_thread = -1;
        
        memset(&session->effect, 0, sizeof(session->effect));
    }

    return 0;
}

int voice_prepare_session(struct audio_device *adev)
{
    const struct voice_stream *stream;
    struct voice_session *session;
    int i;
    int ret;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        stream = session->stream;

        /* NOTE: Open streams for sink and source */
        ret = voice_open_stream(session);
        if (ret)
            return ret;

        /* NOTE: Create resampler */
        if ((stream->sink && stream->source) &&
            (stream->sink->config->rate != stream->source->config->rate) &&
            (!session->resampler)) {
            ALOGD("%s: create resampler", __FUNCTION__);
            ret = create_resampler(stream->source->config->rate,
                                   stream->sink->config->rate,
                                   stream->sink->config->channels,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   NULL,
                                   &session->resampler);
            if (ret < 0) {
                ALOGE("%s: failed to create resampler", __FUNCTION__);
                return ret;
            }
        }

        /* NOTE: Allocate buffer and prepare dump configurations */
        if (stream->sink && stream->source) {
            ret = voice_prepare_buffer(session);
            if (ret)
                return ret;

            ret = voice_prepare_processor(session);
            if (ret)
                return ret;

            ret = voice_prepare_dump(session, i);
            if (ret)
                return ret;
        }
    }

    return 0;
}

int voice_start_session(struct audio_device *adev)
{
    struct voice_session *session;
    int i;
    int ret;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];

        if (session->stream->quirks & VOICE_STREAM_OUTGOING)
            session->volume = 1.0f;
        else
            session->volume = adev->voice.volume;
        ALOGD("%s: sessions[%d] volume = %f\n", __func__, i, session->volume);

        if (session->pcm_hostless_rx) {
            ret = pcm_start(session->pcm_hostless_rx);
            if (ret != 0) {
                ALOGE("%s: rx %s", __FUNCTION__, pcm_get_error(session->pcm_hostless_rx));
                goto err;
            }
        }

        if (session->pcm_hostless_tx) {
            char buf[8];

            /* FIXME: Write dummy buffer here to trigger playback. */
            ret = pcm_write(session->pcm_hostless_tx, buf, 8);
            if (ret != 0) {
                ALOGE("%s: tx %s", __FUNCTION__, pcm_get_error(session->pcm_hostless_tx));
                goto err;
            }
        }

        if (session->pcm_rx && session->pcm_tx) {
            pcm_start(session->pcm_rx);
            pcm_start(session->pcm_tx);
            ret = pthread_create(&session->offload_thread, NULL, &voice_offload_thread, session);
            if (ret) {
                ALOGD("%s failed to create session offload thread", __FUNCTION__);
                session->offload_thread = -1;
                goto stop_session;
            }
        }
    }

    return 0;

stop_session:
    voice_stop_session(adev);

err:
    return ret;
}

void voice_stop_session(struct audio_device *adev)
{
    struct voice_session *session;
    int i;

    ALOGD("[%s %d] enter..\n", __func__, __LINE__);
    
    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];

        if (session->offload_running) {
            session->offload_running = 0;

            #if 1
            if(session->pcm_rx)
            {
                pcm_stop(session->pcm_rx);
            }
            if(session->pcm_tx)
            {
                pcm_stop(session->pcm_tx);
            }
            #endif

            ALOGD("%s: start pthread_join(session->offload_thread)\n", __func__);
            pthread_join(session->offload_thread, NULL);
            session->offload_thread = -1;
            ALOGD("%s: end pthread_join(session->offload_thread)\n", __func__);
        }
    }
}

void voice_release_session(struct audio_device *adev)
{
    struct voice_session *session;
    int i;

    for (i = 0; i < adev->voice.session_num; ++i) {
        session = &adev->voice.sessions[i];
        
        ALOGD("%s: begin session %d\n", __func__, i);

        if (session->pcm_rx) {
            pcm_close(session->pcm_rx);
            session->pcm_rx = NULL;
        }

        if (session->pcm_tx) {
            pcm_close(session->pcm_tx);
            session->pcm_tx = NULL;
        }

        if (session->pcm_hostless_rx) {
            pcm_close(session->pcm_hostless_rx);
            session->pcm_hostless_rx = NULL;
        }

        if (session->pcm_hostless_tx) {
            pcm_close(session->pcm_hostless_tx);
            session->pcm_hostless_tx = NULL;
        }

        if (session->in_buffer) {
            free(session->in_buffer);
            session->in_buffer = NULL;
            session->in_buffer_size = 0;
        }

        if (session->out_buffer) {
            free(session->out_buffer);
            session->out_buffer = NULL;
            session->out_buffer_size = 0;
        }

        if (session->resampler) {
            release_resampler(session->resampler);
            session->resampler = NULL;
        }

        if (session->dumps[VOICE_STREAM_SINK].file) {
            fclose(session->dumps[VOICE_STREAM_SINK].file);
            session->dumps[VOICE_STREAM_SINK].file = NULL;
            session->dumps[VOICE_STREAM_SINK].offset = 0;
            session->dumps[VOICE_STREAM_SINK].size = 0;
        }

        if (session->dumps[VOICE_STREAM_SOURCE].file) {
            fclose(session->dumps[VOICE_STREAM_SOURCE].file);
            session->dumps[VOICE_STREAM_SOURCE].file = NULL;
            session->dumps[VOICE_STREAM_SOURCE].offset = 0;
            session->dumps[VOICE_STREAM_SOURCE].size = 0;
        }

        voice_effect_release(&session->effect);

        session->stream = NULL;
    }

    adev->voice.session_num = 0;
}