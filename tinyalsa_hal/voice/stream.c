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
 * @file    stream.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "alsa_mixer.h"
#include "alsa_route.h"
#include "audio_hw.h"

#include "voice.h"

#define LOG_NDEBUG 0
#define LOG_TAG "voice-stream"

static int voice_create_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    int num = 0;

    adev->voice.stream_num = 0;
    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"stream")) {
            node = node->next;
            continue;
        }

        num++;
        node = node->next;
    }

    if (!num)
        return -EINVAL;

    adev->voice.streams = (struct voice_stream *)malloc((sizeof(struct voice_stream) * num));
    if (!adev->voice.streams)
        return -ENOMEM;

    adev->voice.stream_num = num;
    memset(adev->voice.streams, 0, (sizeof(struct voice_stream) * num));

    return 0;
}

static int voice_find_device(struct audio_device *adev, xmlChar *name,
                             struct voice_device **device)
{
    struct voice_device *tmp;
    bool found = false;
    int i;

    for (i = 0; i < adev->voice.device_num; ++i) {
        tmp = &adev->voice.devices[i];

        if (!xmlStrcmp(name, (const xmlChar *)tmp->name)) {
            found = true;
            *device = tmp;
            break;
        }
    }

    return found ? 0 : -ENODEV;
}

static int voice_parse_route(struct voice_stream *stream, xmlChar *route)
{
    if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_SPEAKER")) {
        stream->route = AUDIO_DEVICE_OUT_SPEAKER;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_WIRED_HEADSET")) {
        stream->route = AUDIO_DEVICE_OUT_WIRED_HEADSET;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_WIRED_HEADPHONE")) {
        stream->route = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_ALL_SCO")) {
        stream->route = AUDIO_DEVICE_OUT_ALL_SCO;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_BLUETOOTH_SCO")) {
        stream->route = AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET")) {
        stream->route = AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
    } else if (!xmlStrcmp(route, (const xmlChar *)"AUDIO_DEVICE_OUT_EARPIECE")) {
        stream->route = AUDIO_DEVICE_OUT_EARPIECE;
    } else {
        ALOGE("unsupported route %s", route);
        return -EINVAL;
    }

    return 0;
}

static int voice_parse_quirks(struct voice_stream *stream, xmlChar *quirks)
{
    char *token;

    token = strtok((char *)quirks, " ");
    while (token) {
        if (!strcmp(token, "VOICE_STREAM_CHANNEL_MONO_LEFT"))
            stream->quirks |= VOICE_STREAM_CHANNEL_MONO_LEFT;
        else if (!strcmp(token, "VOICE_STREAM_CHANNEL_MONO_RIGHT"))
            stream->quirks |= VOICE_STREAM_CHANNEL_MONO_RIGHT;
        else if (!strcmp(token, "VOICE_STREAM_OUTGOING"))
            stream->quirks |= VOICE_STREAM_OUTGOING;
        else {
            ALOGE("unknown quirks %s", token);
            return -EINVAL;
        }

        token = strtok(NULL, " ");
    }

    return 0;
}

static int voice_parse_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    xmlChar *sink;
    xmlChar *source;
    xmlChar *route;
    xmlChar *quirks;
    struct voice_device *device;
    struct voice_stream *stream;
    int i = 0;
    int ret;

    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"stream")) {
            node = node->next;
            continue;
        }

        sink = xmlGetProp(node, BAD_CAST "sink");
        source = xmlGetProp(node, BAD_CAST "source");
        route = xmlGetProp(node, BAD_CAST "route");
        quirks = xmlGetProp(node, BAD_CAST "quirks");
        if (!sink || !source || !route) {
            if (sink)
                xmlFree(sink);

            if (source)
                xmlFree(source);

            if (route)
                xmlFree(route);

            ret = -EINVAL;
            goto cleanup;
        }

        stream = &adev->voice.streams[i];

        if (xmlStrcmp(sink, (const xmlChar *)"none")) {
            ret = voice_find_device(adev, sink, &device);
            if (ret)
                goto cleanup;

            stream->sink = device;
        }

        if (xmlStrcmp(source, (const xmlChar *)"none")) {
            ret = voice_find_device(adev, source, &device);
            if (ret)
                goto cleanup;

            stream->source = device;
        }

        ret = voice_parse_route(stream, route);
        if (ret)
            goto cleanup;

        if (quirks) {
            ret = voice_parse_quirks(stream, quirks);
            if (ret)
                goto cleanup;
        }

        xmlFree(sink);
        xmlFree(source);
        xmlFree(route);

        if (quirks)
            xmlFree(quirks);

        node = node->next;
        i++;
    }

    return 0;

cleanup:
    if (sink)
        xmlFree(sink);

    if (source)
        xmlFree(source);

    if (route)
        xmlFree(route);

    if (quirks)
        xmlFree(quirks);

    return ret;
}

static int voice_open_pcm(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm **pcm;
    unsigned int flags;

    if (is_sink) {
        flags = PCM_OUT | PCM_MONOTONIC;
        device = session->stream->sink;
    } else {
        flags = PCM_IN;
        device = session->stream->source;
    }

    if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        if (is_sink)
            pcm = &session->pcm_hostless_tx;
        else
            pcm = &session->pcm_hostless_rx;
    } else {
        if (is_sink)
            pcm = &session->pcm_tx;
        else
            pcm = &session->pcm_rx;
    }

    *pcm = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

    if (!(*pcm) || !pcm_is_ready(*pcm)) {
        ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
              device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
        return -ENODEV;
    }

    //ALOGD("%s: sussess to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
    //      device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
    return 0;
}

static void voice_dump_pre_pcm(int card, int device, unsigned int flags, struct pcm_config *config)
{
    ALOGI("%s: card = %d, device = %d, flags = %d, rate = %d, frames = %d, count = %d, format = %d",
          __FUNCTION__, card, device, flags, config->rate, config->period_size,
          config->period_count, config->format);
}

static int voice_open_pcm_full(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm **pcm;
    struct pcm **pcm_hostless;
    unsigned int flags;

    if (is_sink) {
        flags = PCM_OUT | PCM_MONOTONIC;
        pcm = &session->pcm_tx;
        pcm_hostless = &session->pcm_hostless_rx;
        device = session->stream->sink;
    } else {
        flags = PCM_IN;
        pcm = &session->pcm_rx;
        pcm_hostless = &session->pcm_hostless_tx;
        device = session->stream->source;
    }

    //ALOGD("%s: start to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
    //      device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');

    if (device->type == VOICE_DAILINK_FE_BE) {
        voice_dump_pre_pcm(device->snd_card, device->snd_pcm, flags, device->config);
        *pcm = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

        if (!(*pcm) || !pcm_is_ready(*pcm)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
            return -ENODEV;
        }
        
        //ALOGD("%s: sussess to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
        //      device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');

    } else if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        voice_dump_pre_pcm(device->backend->snd_card, device->backend->snd_pcm, flags,
                           device->backend->config);
        *pcm = pcm_open(device->backend->snd_card, device->backend->snd_pcm, flags,
                        device->backend->config);

        if (!(*pcm) || !pcm_is_ready(*pcm)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');
            return -ENODEV;
        }

        /* NOTE: The direction is referened to the real backend, not SoC */
        flags = is_sink ? (PCM_IN) : (PCM_OUT | PCM_MONOTONIC);
        voice_dump_pre_pcm(device->snd_card, device->snd_pcm, flags, device->config);
        *pcm_hostless = pcm_open(device->snd_card, device->snd_pcm, flags, device->config);

        if (!(*pcm_hostless) || !pcm_is_ready(*pcm_hostless)) {
            ALOGE("%s: failed to open /dev/snd/pcmC%dD%d%c", __FUNCTION__,
                  device->snd_card, device->snd_pcm, is_sink ? 'c' : 'p');
            return -ENODEV;
        }
    }

    return 0;
}

static void voice_stop_pcm(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm *pcm;

    if (is_sink)
        device = session->stream->sink;
    else
        device = session->stream->source;

    if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        if (is_sink)
            pcm = session->pcm_hostless_tx;
        else
            pcm = session->pcm_hostless_rx;
    } else {
        if (is_sink)
            pcm = session->pcm_tx;
        else
            pcm = session->pcm_rx;
    }

    pcm_stop(pcm);
}

static void voice_stop_pcm_full(struct voice_session *session, bool is_sink)
{
    const struct voice_device *device;
    struct pcm *pcm;
    struct pcm *pcm_hostless;

    if (is_sink) {
        pcm = session->pcm_tx;
        pcm_hostless = session->pcm_hostless_rx;
        device = session->stream->sink;
    } else {
        pcm = session->pcm_rx;
        pcm_hostless = session->pcm_hostless_tx;
        device = session->stream->source;
    }

    //ALOGD("%s: stop opened /dev/snd/pcmC%dD%d%c", __FUNCTION__,
    //      device->snd_card, device->snd_pcm, is_sink ? 'p' : 'c');

    if (device->type == VOICE_DAILINK_FE_BE) {
        pcm_stop(pcm);
    } else if (device->type == VOICE_DAILINK_HOSTLESS_FE) {
        pcm_stop(pcm);
        pcm_stop(pcm_hostless);
    }
}

static void voice_dump_quirks(int quirks)
{
    ALOGD("  quirks =");

    if (quirks & VOICE_STREAM_CHANNEL_MONO_LEFT)
        ALOGD("    VOICE_STREAM_CHANNEL_MONO_LEFT");

    if (quirks & VOICE_STREAM_CHANNEL_MONO_RIGHT)
        ALOGD("    VOICE_STREAM_CHANNEL_MONO_RIGHT");
}

void voice_dump_stream(struct audio_device *adev)
{
    const struct voice_stream *stream;
    int i;

    for (i = 0; i < adev->voice.stream_num; ++i) {
        stream = &adev->voice.streams[i];

        ALOGD("stream%d:", i);

        if (stream->sink)
            ALOGD("  sink = %s", stream->sink->name);

        if (stream->source)
            ALOGD("  source = %s", stream->source->name);

        ALOGD("  route = 0x%x", stream->route);

        if (stream->quirks)
            voice_dump_quirks(stream->quirks);
    }
}

int voice_load_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    int ret;

    ret = voice_create_stream(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_stream(adev, nodes);
    if (ret)
        return ret;

    return 0;
}

int voice_open_stream(struct voice_session *session)
{
    const struct voice_stream *stream = session->stream;
    int ret;

    if (stream->sink && stream->source) {
        ret = voice_open_pcm_full(session, true);
        if (ret)
            return ret;

        ret = voice_open_pcm_full(session, false);
        if (ret)
            return ret;
    } else {
        if (stream->sink) {
            ret = voice_open_pcm(session, true);
            if (ret)
                return ret;
        }

        if (stream->source) {
            ret = voice_open_pcm(session, false);
            if (ret)
                return ret;
        }
    }

    return 0;
}

void voice_stop_stream(struct voice_session *session)
{
    const struct voice_stream *stream = session->stream;

    if (stream->sink && stream->source) {
        voice_stop_pcm_full(session, true);

        voice_stop_pcm_full(session, false);
    } else {
        if (stream->sink)
            voice_stop_pcm(session, true);

        if (stream->source)
            voice_stop_pcm(session, false);
    }
}