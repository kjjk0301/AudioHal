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
 * @file    device.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-11-03
 */

#ifndef ALSA_PROXY_H
#define ALSA_PROXY_H

#include "asoundlib.h"
#include "alsa_route.h"

struct alsa_proxy_master {
    int id;
    struct listnode node;
    struct pcm_config config;
    struct resampler_itfe *resampler;
    int16_t *buffer;
};

struct alsa_proxy {
    int card;
    int device;
    int direction;
    int ref_count;
    int next_master_id;
    struct listnode masters;
    struct pcm_config config;
    struct pcm *pcm;
    struct alsa_route *ar;
    pthread_mutex_t lock;
};

int alsa_proxy_create(struct alsa_proxy **proxy);
int alsa_proxy_open(struct alsa_proxy *proxy, int card, int device, int direction,
                    struct pcm_config *config, audio_devices_t route);
int alsa_proxx_read(struct alsa_proxy *proxy, void *buffer, size_t size);
int alsa_proxx_write(struct alsa_proxy *proxy, void *buffer, size_t size);
void alsa_proxy_close(struct alsa_proxy *proxy);

#endif
