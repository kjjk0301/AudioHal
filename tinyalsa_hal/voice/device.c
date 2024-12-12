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
 * @date    2023-04-12
 */

#include "alsa_mixer.h"
#include "alsa_route.h"
#include "audio_hw.h"

#include "voice.h"

#define LOG_NDEBUG 0
#define LOG_TAG "voice-device"

/* NOTE: Enable 16ms period to apply rockchip audio preprocess. */
static struct pcm_config pcm_config_voice_call = {
    .channels = 2,
    .rate = 8000,
    .period_size = 128,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

/* NOTE: Enable 16ms period to apply rockchip audio preprocess. */
static struct pcm_config pcm_config_voice_call_with_ref = {
    .channels = 4,
    .rate = 8000,
    .period_size = 128,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

static int voice_create_devcie(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    int num = 0;

    adev->voice.device_num = 0;
    node = nodes->xmlChildrenNode;
    while (node) {
        /* FIXME: There is always a "text" node before the real node  */
        if (xmlStrcmp(node->name, (const xmlChar *)"device")) {
            node = node->next;
            continue;
        }

        num++;
        node = node->next;
    }

    if (!num)
        return -EINVAL;

    adev->voice.devices = (struct voice_device *)malloc((sizeof(struct voice_device) * num));
    if (!adev->voice.devices)
        return -ENOMEM;

    adev->voice.device_num = num;
    memset(adev->voice.devices, 0, (sizeof(struct voice_device) * num));

    return 0;
}

static int voice_parse_type(struct voice_device *device, xmlChar *type)
{
    if (!xmlStrcmp(type, (const xmlChar *)"VOICE_DAILINK_FE_BE")) {
        device->type = VOICE_DAILINK_FE_BE;
    } else if (!xmlStrcmp(type, (const xmlChar *)"VOICE_DAILINK_BE_BE")) {
        device->type = VOICE_DAILINK_BE_BE;
    } else if (!xmlStrcmp(type, (const xmlChar *)"VOICE_DAILINK_HOSTLESS_FE")) {
        device->type = VOICE_DAILINK_HOSTLESS_FE;
    } else {
        ALOGE("invalid dailink type");
        return -EINVAL;
    }

    return 0;
}

static int voice_parse_quirks(struct voice_device *device, xmlChar *quirks)
{
    char *token;

    token = strtok((char *)quirks, " ");
    while (token) {
        if (!strcmp(token, "VOICE_DEVICE_ROUTE_STATIC_UPDATE"))
            device->quirks |= VOICE_DEVICE_ROUTE_STATIC_UPDATE;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_MONO_LEFT"))
            device->quirks |= VOICE_DEVICE_CHANNEL_MONO_LEFT;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_MONO_RIGHT"))
            device->quirks |= VOICE_DEVICE_CHANNEL_MONO_RIGHT;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_HAS_REFERENCE"))
            device->quirks |= VOICE_DEVICE_CHANNEL_HAS_REFERENCE;
        else {
            ALOGE("unknown quirks %s", token);
            return -EINVAL;
        }

        token = strtok(NULL, " ");
    }

    return 0;
}

static int voice_parse_device(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    xmlChar *name = NULL;
    xmlChar *snd_card = NULL;
    xmlChar *snd_pcm = NULL;
    xmlChar *type = NULL;
    xmlChar *backend = NULL;
    xmlChar *quirks = NULL;
    struct voice_device *device;
    int i = 0;
    int ret;

    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"device")) {
            node = node->next;
            continue;
        }

        /*
         * NOTE: A backend is necessary to hostless FE, and is optional to
         * other dailink types.
         */
        name = xmlGetProp(node, BAD_CAST "name");
        snd_card = xmlGetProp(node, BAD_CAST "snd_card");
        snd_pcm = xmlGetProp(node, BAD_CAST "snd_pcm");
        type = xmlGetProp(node, BAD_CAST "type");
        backend = xmlGetProp(node, BAD_CAST "backend");
        quirks = xmlGetProp(node, BAD_CAST "quirks");
        if (!name || !snd_card || !snd_pcm || !type) {
            ALOGE("invalid device");
            ret = -EINVAL;
            goto cleanup;
        }

        device = &adev->voice.devices[i];
        device->name = strdup((const char *)name);
        device->snd_card = atoi((const char *)snd_card);
        device->snd_pcm = atoi((const char *)snd_pcm);

        ret = voice_parse_type(device, type);
        if (ret)
            goto cleanup;

        if ((device->type == VOICE_DAILINK_HOSTLESS_FE) && !backend) {
            ALOGE("a backend is necessary to hostless FE");
            ret = -EINVAL;
            goto cleanup;
        }

        if (quirks) {
            ret = voice_parse_quirks(device, quirks);
            if (ret)
                goto cleanup;
        }

        if (device->quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)
            device->config = &pcm_config_voice_call_with_ref;
        else
            device->config = &pcm_config_voice_call;

        node = node->next;
        i++;

        xmlFree(name);
        xmlFree(snd_card);
        xmlFree(snd_pcm);
        xmlFree(type);

        if (backend)
            xmlFree(backend);

        if (quirks)
            xmlFree(quirks);
    }

    return 0;

cleanup:
    if (name)
        xmlFree(name);

    if (snd_card)
        xmlFree(snd_card);

    if (snd_pcm)
        xmlFree(snd_pcm);

    if (type)
        xmlFree(type);

    if (backend)
        xmlFree(backend);

    if (quirks)
        xmlFree(quirks);

    return ret;
}

static int voice_parse_backend(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    xmlChar *name;
    xmlChar *backend;
    struct voice_device *device;
    struct voice_device *found;
    struct voice_device *tmp;
    int i = 0;

    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"device")) {
            node = node->next;
            continue;
        }

        backend = xmlGetProp(node, BAD_CAST "backend");
        if (!backend) {
            node = node->next;
            continue;
        }

        name = xmlGetProp(node, BAD_CAST "name");

        found = NULL;
        tmp = NULL;
        for (i = 0; i < adev->voice.device_num; ++i) {
            device = &adev->voice.devices[i];
            if (!xmlStrcmp(name, (const xmlChar *)device->name))
                found = device;

            if (!xmlStrcmp(backend, (const xmlChar *)device->name))
                tmp = device;
        }

        if (found && tmp)
            found->backend = tmp;

        xmlFree(name);
        xmlFree(backend);
        node = node->next;
    }

    return 0;
}

static void voice_dump_quirks(int quirks)
{
    ALOGD("  quirks =");

    if (quirks & VOICE_DEVICE_ROUTE_STATIC_UPDATE)
        ALOGD("    VOICE_DEVICE_ROUTE_STATIC_UPDATE");

    if (quirks & VOICE_DEVICE_CHANNEL_MONO_LEFT)
        ALOGD("    VOICE_DEVICE_CHANNEL_MONO_LEFT");

    if (quirks & VOICE_DEVICE_CHANNEL_MONO_RIGHT)
        ALOGD("    VOICE_DEVICE_CHANNEL_MONO_RIGHT");

    if (quirks & VOICE_DEVICE_CHANNEL_HAS_REFERENCE)
        ALOGD("    VOICE_DEVICE_CHANNEL_HAS_REFERENCE");
}

void voice_dump_device(struct audio_device *adev)
{
    const struct voice_device *device;
    int i;

    for (i = 0; i < adev->voice.device_num; ++i) {
        device = &adev->voice.devices[i];

        ALOGD("device%d:", i);
        ALOGD("  name = %s", device->name);
        ALOGD("  card = %d", device->snd_card);
        ALOGD("  pcm = %d", device->snd_pcm);
        ALOGD("  type = %d", device->type);

        if (device->backend)
            ALOGD("  backend = %s", device->backend->name);

        if (device->quirks)
            voice_dump_quirks(device->quirks);
    }
}

int voice_load_device(struct audio_device *adev, xmlNodePtr nodes)
{
    int ret;

    ret = voice_create_devcie(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_device(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_backend(adev, nodes);
    if (ret)
        return ret;

    return 0;
}