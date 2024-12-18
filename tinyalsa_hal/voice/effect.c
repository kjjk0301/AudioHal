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
 * @file    effect.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-11-09
 */

#include <errno.h>
#include <cutils/log.h>
#include <audio_utils/primitives.h>
#include "voice.h"
#include "aspl_nr.h"
#include "Resample.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_NDEBUG 0

#define LOG_TAG "voice-effect"

#define VOICE_PREPROCESS_RATE 8000






int voice_effect_init(struct voice_effect *effect, unsigned int rate,
                      unsigned int channels, unsigned int frames, unsigned int bits)
{
    int ref_channels = NUM_REF_CHANNEL;
    int src_channels = channels - NUM_REF_CHANNEL;
    /* NOTE: Frames for pcm_read is default to the period of a pcm configuration */
    size_t mono_buffer_size = frames * 1 * (bits >> 3);
    size_t stereo_buffer_size = frames * 2 * (bits >> 3);
    int ret;

    if (125 != (rate/(frames>>1))) {
        ALOGE("%s: got rate %u frames %u, expected rate %u frames %u", __FUNCTION__,
              rate, frames, VOICE_PREPROCESS_RATE, VOICE_PREPROCESS_RATE*2/125);
        ret = -EINVAL;
        goto err;
    }

    memset(&effect->rk_param, 0, sizeof(RKAUDIOParam));
    effect->rk_param.model_en = RKAUDIO_EN_AEC | RKAUDIO_EN_BF;
    effect->rk_param.aec_param = rkaudio_aec_param_init();
    effect->rk_param.bf_param = rkaudio_preprocess_param_init();
    effect->rk_param.rx_param = rkaudio_rx_param_init();
    effect->rk_preprocess = rkaudio_preprocess_init(rate, bits, src_channels, ref_channels,
                                                    &effect->rk_param);
    if (!effect->rk_preprocess) {
        ALOGE("%s: failed to init rockchip preprocess", __FUNCTION__);
        ret = -EINVAL;
        goto err;
    }

    effect->mono_buffer = malloc(mono_buffer_size);
    if (!effect->mono_buffer) {
        ALOGE("%s: failed to allocate mono buffer, expected size %zu", __FUNCTION__,
              mono_buffer_size);
        ret = -ENOMEM;
        goto deinit_preprocess;
    }

    effect->stereo_buffer = malloc(stereo_buffer_size);
    if (!effect->stereo_buffer) {
        ALOGE("%s: failed to allocate stereo buffer, expected size %zu", __FUNCTION__,
              stereo_buffer_size);
        ret = -ENOMEM;
        goto free_mono_buffer;
    }

    rkaudio_param_printf(src_channels, ref_channels, &effect->rk_param);

    effect->channels = channels;

    return 0;

free_mono_buffer:
    free(effect->mono_buffer);

deinit_preprocess:
    rkaudio_preprocess_destory(effect->rk_preprocess);
    rkaudio_param_deinit(&effect->rk_param);

err:
    return ret;
}





int voice_effect_process(struct voice_effect *effect, void *src, size_t frames)
{
    int targ_doa;
    int wakeup_status = 0;
    int samples = frames * effect->channels;
    int processed;

    /* NOTE: The preprocess outputs a buffer of 1 channel. */
    /* FIXME: The processed samples indicates 2 channel, actually it's 1 channel :( */
    processed = rkaudio_preprocess_short(effect->rk_preprocess, (short *)src,
                                         effect->mono_buffer, samples, &wakeup_status);

    /* TODO: What is Doa? Should we invoke it here ? */
    targ_doa = rkaudio_Doa_invoke(effect->rk_preprocess);

    upmix_to_stereo_i16_from_mono_i16(effect->stereo_buffer,
                                      (const int16_t *)effect->mono_buffer, frames);

    return 0;
}


#if 0
short AsplSrcBuf[2][512];
short AsplRefBuf[2][512];
short TempResample[768*2];


int voice_effect_processASPL(struct voice_effect *effect, void *src, size_t frames)
{
    int targ_doa;
    int wakeup_status = 0;
    int samples = frames * effect->channels;
    int processed;
	int i;
	double DoAVal;
	short *pData = (short *) src;
	


		// Do Data align
	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+0];
	// DownSample
	DoDnsample(&DownSampleHandle[0],TempResample,&AsplSrcBuf[0][0],768*2,3,1.0);
		
	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+1];
	// DownSample
	DoDnsample(&DownSampleHandle[1],TempResample,&AsplSrcBuf[1][0],768*2,3,1.0);

	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+2];
	// DownSample
	DoDnsample(&DownSampleHandle[2],TempResample,&AsplRefBuf[0][0],768*2,3,1.0);
	
	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+3];
	// DownSample
	DoDnsample(&DownSampleHandle[3],TempResample,&AsplRefBuf[1][0],768*2,3,1.0);
		
	aspl_AEC_process_single(&AsplSrcBuf[0][0],&AsplRefBuf[0][0],512,1050,-10.0);	
	aspl_NR_process_2mic(&AsplSrcBuf[0][0],512,1,1,&DoAVal);
	
	DoUpsample(&UpSampleHandle,&AsplSrcBuf[0][0],effect->mono_buffer,512,3,1.0);


    upmix_to_stereo_i16_from_mono_i16(effect->stereo_buffer,
                                      (const int16_t *)effect->mono_buffer, frames);

    return 0;
}
#endif

void voice_effect_release(struct voice_effect *effect)
{
    ALOGD("%s: begin\n", __func__);
    if (effect->rk_preprocess) {
        rkaudio_preprocess_destory(effect->rk_preprocess);
        effect->rk_preprocess = NULL;
    }

    rkaudio_param_deinit(&effect->rk_param);

    if (effect->mono_buffer) {
        free(effect->mono_buffer);
        effect->mono_buffer = NULL;
    }

    if (effect->stereo_buffer) {
        free(effect->stereo_buffer);
        effect->stereo_buffer = NULL;
    }
    ALOGD("%s: end\n", __func__);
}



