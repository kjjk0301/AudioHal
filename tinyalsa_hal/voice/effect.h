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

#ifndef VOICE_EFFECT_H
#define VOICE_EFFECT_H

#include "rkaudio_preprocess.h"

/**
 * struct voice_effect - voice effect structure.
 * @channels: total channels of input buffer to process.
 * @rk_param: the parameters of rockchip audio preprocess.
 * @rk_preprocess: the context of rockchip audio prerocess.
 * @mono_buffer: the mono buffer used by rockchip audio preprocess.
 * @stereo_buffer: the stereo buffer.
 */
struct voice_effect {
    unsigned int channels;
    RKAUDIOParam rk_param;
    void *rk_preprocess;
    int16_t *mono_buffer;
    int16_t *stereo_buffer;
};


struct voice_effect_aspl {
    unsigned int channels;
//    RKAUDIOParam rk_param;
//    void *rk_preprocess;
    int16_t *mono_buffer;
    int16_t *stereo_buffer;
};




int voice_effect_init(struct voice_effect *effect, unsigned int rate,
                      unsigned int channels, unsigned int frames, unsigned int bits);
int voice_effect_process(struct voice_effect *effect, void *src, size_t frames);
int voice_effect_processASPL(struct voice_effect *effect, void *src, size_t frames);

void voice_effect_release(struct voice_effect *effect);

#endif