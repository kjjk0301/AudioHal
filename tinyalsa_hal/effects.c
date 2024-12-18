/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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

#include <stdlib.h>
#include "effects.h"
#include "aspl_nr.h"
#include "Resample.h"


UpdamplerContext DownSampleHandle1;
UpdamplerContext DownSampleHandle2;
UpdamplerContext DownSampleHandle3;
UpdamplerContext DownSampleHandle4;


UpdamplerContext UpSampleHandle;


int audio_effect_count(struct listnode *effects)
{
    struct listnode *node;
    int count = 0;

    list_for_each(node, effects) {
        count++;
    }

    return count;
}

void audio_effect_add(struct listnode *effects, effect_handle_t effect)
{
    effect_descriptor_t desc;
    struct listnode *node;
    struct effect_list *entry;

    if ((*effect)->get_descriptor(effect, &desc))
        return;

    /*
     * The effect implements a process function should be processed in the
     * audio flinger, otherwise the audio flinger and the stream HAL will
     * process the same effect.
     */
    if ((desc.flags & EFFECT_FLAG_NO_PROCESS_MASK) != EFFECT_FLAG_NO_PROCESS)
        return;

    if ((desc.flags & EFFECT_FLAG_INSERT_MASK) == EFFECT_FLAG_INSERT_EXCLUSIVE) {
        list_for_each(node, effects) {
            entry = node_to_item(node, struct effect_list, list);
            if (!memcmp(&entry->desc.uuid, &desc.uuid, sizeof(desc.uuid)))
                return;
        }
    }

    entry = (struct effect_list *)calloc(1, sizeof(*entry));
    if (entry) {
        entry->handle = effect;
        memcpy(&entry->desc, &desc, sizeof(desc));
        list_add_tail(effects, &entry->list);
    }
}

void audio_effect_remove(struct listnode *effects, effect_handle_t effect)
{
    struct listnode *node;
    struct effect_list *entry;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if (entry->handle == effect) {
            list_remove(&entry->list);
            free(entry);
            break;
        }
    }
}

#if 0
void audio_effect_process(struct listnode *effects, void *in, void *out, size_t frames)
{
    struct listnode *node;
    struct effect_list *entry;
    audio_buffer_t input;
    audio_buffer_t output;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if ((*entry->handle)->process) {
            input.frameCount = frames;
            input.raw = in;
            output.frameCount = frames;
            output.raw = out;

            (*entry->handle)->process(entry->handle, &input, &output);
        }
    }
}
#endif


short AsplSrcBuf[2][512];
short AsplRefBuf[2][512];
short TempResample[768*2];


int audio_effect_init_ASPL(unsigned int rate,unsigned int channels, unsigned int frames, unsigned int bits)
{
    int ref_channels = 2;
    int src_channels = channels - 2;
	int rc;
    int ret;

	printf("ASPL Resampler INIT");

	// ASPL Algorithm Init
	//aspl_NR_create(NULL);
	rc=CreateResampler(&DownSampleHandle1,768*4,LP_PASS_TAPS3,pLowPassFilter3);
	printf("ASPL Resampler INIT %d",rc);
	if (rc != 1) {
		  printf(" Error Creating ASPL Resampler!!");
		  ret = -EINVAL;
		  goto err;
	}
	
	rc=CreateResampler(&DownSampleHandle2,768*4,LP_PASS_TAPS3,pLowPassFilter3);
	if (rc != 1) {
		  printf(" Error Creating ASPL Resampler!!");
		  ret = -EINVAL;
		  goto err;
	}
	rc=CreateResampler(&DownSampleHandle3,768*4,LP_PASS_TAPS3,pLowPassFilter3);
	if (rc != 1) {
		  printf(" Error Creating ASPL Resampler!!");
		  ret = -EINVAL;
		  goto err;
	}
	rc=CreateResampler(&DownSampleHandle4,768*4,LP_PASS_TAPS3,pLowPassFilter3);
	if (rc != 1) {
		  printf(" Error Creating ASPL Resampler!!");
		  ret = -EINVAL;
		  goto err;
	}
	rc=CreateResampler(&UpSampleHandle,768*4,LP_PASS_TAPS3,pLowPassFilter3);
		if (rc != 1) {
			  printf(" Error Creating ASPL Resampler!!");
			  ret = -EINVAL;
			  goto err;
		}


	aspl_NR_create_2mic(NULL);



//	effect->channels = channels;
	
		return 0;
	
	DestroyResampler(&DownSampleHandle1);
	DestroyResampler(&DownSampleHandle2);
	DestroyResampler(&DownSampleHandle3);
	DestroyResampler(&DownSampleHandle4);
	DestroyResampler(&UpSampleHandle);
	aspl_NR_destroy();
err:
    return ret;
}

void audio_effect_release_ASPL()
{
	
	DestroyResampler(&DownSampleHandle1);
	DestroyResampler(&DownSampleHandle2);
	DestroyResampler(&DownSampleHandle3);
	DestroyResampler(&DownSampleHandle4);
	DestroyResampler(&UpSampleHandle);
	aspl_NR_destroy();

}
void audio_effect_process_ASPL(void *in, void *out, size_t frames)
{
    struct listnode *node;
    struct effect_list *entry;
	int i;
	short * pData;
	double DoAVal=0.0;

	pData = (short *) in;

	printf("ASPL effext Process %d",frames);
	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+0];

	// DownSample
	DoDnsample(&DownSampleHandle1,TempResample,&(AsplSrcBuf[0][0]),frames,3,1.0);
		
	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+1];
	// DownSample
	DoDnsample(&DownSampleHandle2,TempResample,&AsplSrcBuf[1][0],frames,3,1.0);


	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+2];
	// DownSample
	DoDnsample(&DownSampleHandle3,TempResample,&AsplRefBuf[0][0],768*2,3,1.0);

	for(i=0;i<frames;i++)
		TempResample[i]=pData[4*i+3];
	// DownSample
	DoDnsample(&DownSampleHandle4,TempResample,&AsplRefBuf[1][0],768*2,3,1.0);
		
	aspl_AEC_process_single(&AsplSrcBuf[0][0],&AsplRefBuf[0][0],512,1050,-10.0);	
	aspl_NR_process_2mic(&AsplSrcBuf[0][0],512,1,1,&DoAVal);

	


//	pData = (short *) out;
//	DoUpsample(&UpSampleHandle,&(AsplSrcBuf[0][0]),pData,512,3,1.0);
	



	
}


void audio_effect_process(struct listnode *effects, void *in, void *out, size_t frames)
{
    struct listnode *node;
    struct effect_list *entry;
	audio_buffer_t input;
    audio_buffer_t output;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if ((*entry->handle)->process) {
            input.frameCount = frames;
            input.raw = in;
            output.frameCount = frames;
            output.raw = out;

            (*entry->handle)->process(entry->handle, &input, &output);
        }
    }
}


void audio_effect_process_reverse(struct listnode *effects, void *in, void *out, size_t frames)
{
    struct listnode *node;
    struct effect_list *entry;
    audio_buffer_t input;
    audio_buffer_t output;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if ((*entry->handle)->process_reverse) {
            input.frameCount = frames;
            input.raw = in;
            output.frameCount = frames;
            output.raw = out;

            (*entry->handle)->process_reverse(entry->handle, &input, &output);
        }
    }
}

void audio_effect_set_config(struct listnode *effects, effect_config_t *config)
{
    struct listnode *node;
    struct effect_list *entry;
    uint32_t cmd_code;
    uint32_t cmd_size;
    void *cmd_data;
    uint32_t reply_size;
    int reply_data;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if ((*entry->handle)->command) {
            cmd_code = EFFECT_CMD_SET_CONFIG;
            cmd_size = sizeof(effect_config_t);
            cmd_data = (void *)config;
            reply_size = sizeof(reply_data);

            (*entry->handle)->command(entry->handle, cmd_code, cmd_size, cmd_data,
                                      &reply_size, &reply_data);
        }
    }
}

void audio_effect_set_config_reverse(struct listnode *effects, effect_config_t *config)
{
    struct listnode *node;
    struct effect_list *entry;
    uint32_t cmd_code;
    uint32_t cmd_size;
    void *cmd_data;
    uint32_t reply_size;
    int reply_data;

    list_for_each(node, effects) {
        entry = node_to_item(node, struct effect_list, list);
        if ((*entry->handle)->command) {
            cmd_code = EFFECT_CMD_SET_CONFIG_REVERSE;
            cmd_size = sizeof(effect_config_t);
            cmd_data = (void *)config;
            reply_size = sizeof(reply_data);

            (*entry->handle)->command(entry->handle, cmd_code, cmd_size, cmd_data,
                                      &reply_size, &reply_data);
        }
    }
}
