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

#include "alsa_proxy.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "alsa_proxy"

static void alsa_proxy_get(struct alsa_proxy *proxy)
{
    proxy->ref_count++;
}

static void alsa_proxy_put(struct alsa_proxy *proxy)
{
    if (proxy->ref_count != 0)
        proxy->ref_count--;
}

int alsa_proxy_create(struct alsa_proxy **proxy)
{
    struct alsa_proxy *tmp = calloc(1, sizeof(*tmp));

    if (!tmp) {
        return -ENOMEM;
    }

    *proxy = tmp;
    return 0;
}

int alsa_proxy_open(struct alsa_proxy *proxy, int *master_id, int card, int device, int direction,
                    struct pcm_config *config, audio_devices_t route)
{
    struct alsa_proxy_master *master;
    unsigned int flags = (direction == PCM_IN) ? (PCM_IN) : (PCM_OUT | PCM_MONOTONIC);
    int ret;

    pthread_mutex_lock(&proxy->lock);

    alsa_proxy_get(proxy);

    if (!proxy->ar) {
        ret = route_open(&proxy->ar, card);
        if (ret) {
            alsa_proxy_put(proxy);
            goto err;
        }
    }

    ret = route_apply(proxy->ar, route);
    if (ret) {
        alsa_proxy_put(proxy);
        goto err_close_route;
    }

    if (!proxy->pcm) {
        proxy->pcm = pcm_open(card, device, flags, config);
        if (!proxy->pcm) {
            ALOGE("%s: failed to open pcm", __func__);
            ret = -EINVAL;
            alsa_proxy_put(proxy);
            goto err_close_route;
        }

        if (!pcm_is_ready(proxy->pcm)) {
            ALOGE("%s: pcm is not ready, %s", __func__, pcm_get_error(proxy->pcm));
            ret = -EAGAIN;
            alsa_proxy_put(proxy);
            goto err_close_pcm;
        }
    }

    master = calloc(1, sizeof(*master));
    if (!master) {
        ret = -ENOMEM;
        goto err_close_pcm;
    }

    *master_id = proxy->next_master_id++;

    pthread_mutex_unlock(&proxy->lock);

    return 0;

err_close_pcm:
    if (!proxy->ref_count) {
        pcm_close(proxy->pcm);
        proxy->pcm = NULL;
    }

err_close_route:
    if (!proxy->ref_count) {
        route_close(proxy->ar);
        proxy->ar = NULL;
    }

err:
    pthread_mutex_unlock(&proxy->lock);

    return ret;
}

int alsa_proxx_read(struct alsa_proxy *proxy, int master_id, void *buffer, size_t size)
{

}

int alsa_proxx_write(struct alsa_proxy *proxy, int master_id, void *buffer, size_t size)
{

}

void alsa_proxy_close(struct alsa_proxy *proxy, int master_id)
{

}
