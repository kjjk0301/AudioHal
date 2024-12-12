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
 * @file    device.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-04-12
 */

#ifndef VOICE_DEVICE_H
#define VOICE_DEVICE_H

#include <libxml/parser.h>
#include <libxml/tree.h>

/*
 *  | Front End PCMs    |  SoC DSP  | Back End DAIs | Audio devices |
 *                      *************
 *  PCM0 <============> *           * <====DAI0=====> Codec Headset
 *                      *           *
 *  PCM1 <------------> *           * <----DAI1-----> Codec Speakers
 *                      *   DSP     *
 *  PCM2 <------------> *           * <----DAI2-----> MODEM
 *                      *           *
 *  PCM3 <------------> *           * <----DAI3-----> BT
 *                      *           *
 *                      *           * <----DAI4-----> DMIC
 *                      *************
 */
#define VOICE_DAILINK_FE_BE         0

/*
 *  | Front End PCMs    |  SoC DSP  | Back End DAIs | Audio devices |
 *                      *************
 *  PCM0 <------------> *           * <----DAI0-----> Codec Headset
 *                      *           *
 *  PCM1 <------------> *           * <----DAI1-----> Codec Speakers
 *                      *   DSP     *
 *  PCM2 <------------> *           * <====DAI2=====> MODEM
 *                      *           *
 *  PCM3 <------------> *           * <====DAI3=====> BT
 *                      *           *
 *                      *           * <----DAI4-----> DMIC
 *                      *************
 *
 * In this case the link is enabled or disabled by the state of the DAPM graph.
 * This usually means there is a mixer control that can be used to connect or
 * disconnect the path between both DAIs.
 */
#define VOICE_DAILINK_BE_BE         1

/*
 *  | Front End PCMs    |  SoC DSP  | Back End DAIs | Audio devices |
 *                      *************
 *  PCM0 <------------> *           * <----DAI0-----> Codec Headset
 *                      *           *
 *  PCM1 <------------> *           * <----DAI1-----> Codec Speakers
 *          (dummy)     *   DSP     *
 *  PCM2 <============> *           * <====DAI2=====> MODEM
 *                      *           *
 *  PCM3 <------------> *           * <----DAI3-----> BT
 *                      *           *
 *                      *           * <----DAI4-----> DMIC
 *                      *************
 *
 * This FE has a virtual connection to the BE DAI links on the DAPM graph.
 * Control is then carried out by the FE as regular PCM operations.
 */
#define VOICE_DAILINK_HOSTLESS_FE   2

/*
 * NOTE: These quirks are possibly used in the future, we should add other
 * quirks here if we found.
 */

/* Device can't apply route if the PCM is opened */
#define VOICE_DEVICE_ROUTE_STATIC_UPDATE    (0x01 << 0)

/* Recording or playback is mono in the left channel */
#define VOICE_DEVICE_CHANNEL_MONO_LEFT      (0x01 << 1)

/* Recording or playback is mono in the right channel */
#define VOICE_DEVICE_CHANNEL_MONO_RIGHT     (0x01 << 2)

/* Recording has reference channel */
#define VOICE_DEVICE_CHANNEL_HAS_REFERENCE  (0x01 << 3)

struct audio_device;

/**
 * struct voice_device - device configuration structure.
 * @name: device name.
 * @snd_id: ID string of the device.
 * @snd_card: ID of the device.
 * @snd_pcm: PCM ID of the device.
 * @type: DAI link type.
 * @quirks: a bitmap of hardware quirks that require some special action.
 * @config: PCM configuration.
 * @backend: the BE (backend) of the device which is connected to the frontend (FE).
 */
struct voice_device {
    char *name;
    int snd_card;
    int snd_pcm;
    int type;
    int quirks;
    struct pcm_config *config;
    const struct voice_device *backend;
};

void voice_dump_device(struct audio_device *adev);
int voice_load_device(struct audio_device *adev, xmlNodePtr nodes);

#endif
