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


short * AsplSrcBuf[2];
short * AsplRefBuf[2];
short * AsplSrcBuf_48k[2];
short * AsplRefBuf_48k[2];

short TempResample[768*4];
short workbuf[768*4];

aspl_NR_CONFIG aspl_nr_config;

#if 1
#include <time.h>

#define MAX_FILES 5
#define SAVE_INTERVAL 10 // seconds

int file_count = 1;
time_t start_time, current_time;
char filename[256];
FILE *file;

#endif 

	
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

	//aspl_NR_create_2mic
    aspl_nr_config.enable = 1;
    aspl_nr_config.sensitivity = 2;

	char* file_path = "/data/misc/aspl_nr_params_default.bin";
	printf("ASPL Resampler INIT %s",file_path);

	
	strncpy(aspl_nr_config.tuning_file_path, file_path, sizeof(aspl_nr_config.tuning_file_path));
 	aspl_nr_config.tuning_file_path[sizeof(aspl_nr_config.tuning_file_path) - 1] = '\0';  // Ensure null termination	 
	ret = aspl_NR_create_2mic((void *)&aspl_nr_config);
 
	aspl_nr_config.aec_Mic_N = 2;
	ret = aspl_AEC_create(&aspl_nr_config);

	aspl_nr_config.AEC_filter_updated = 0;

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

#define PCM_CAPTURE_CHANNELS 2
#define PCM_REFERENCE_CHANNELS 2


void audio_effect_process_ASPL(void *in, void *out, size_t frames)
{
    struct listnode *node;
    struct effect_list *entry;
	int i;
	short *	in_r_16k;
	short *	in_r_48k;	
	short * pData;
	short * pData_out;
	unsigned char *tempTxPtr, *tempRxPtr, *tempWkPtr;

	
	double DoAVal=0.0;

	pData = (short *) in;
	pData_out = (short *) out;	

	short * AsplSrcBuf_48k[2];
	short * AsplRefBuf_48k[2];

	in_r_16k  = (short *)workbuf;
	in_r_48k  = (short *)TempResample;

	for (int k = 0; k < PCM_CAPTURE_CHANNELS; k++) {
		AsplSrcBuf_48k[k] = &in_r_48k[k*(frames)];	/* find the frame start for each microphone */
		AsplSrcBuf[k] = &in_r_16k[k*(frames/3)];	/* find the frame start for each microphone */
	}

	for (int k = 0; k < PCM_REFERENCE_CHANNELS; k++) {
		AsplRefBuf_48k[k] = &in_r_48k[(PCM_CAPTURE_CHANNELS+k)*(frames)];	/* find the frame start for each microphone */
		AsplRefBuf[k] = &in_r_16k[(PCM_CAPTURE_CHANNELS+k)*(frames/3)];	 /* find the frame start for each ref channel */
	}

	for (int j=0; j<(PCM_CAPTURE_CHANNELS+PCM_REFERENCE_CHANNELS); j++) {
		// set the RX start pointer
		tempRxPtr = (unsigned char *)pData + j*sizeof(short);
		// set the WK start pointer
		tempWkPtr = (unsigned char *)TempResample + j*(frames)*sizeof(short);
	   
		for (i=0; i<frames; i++){
			memcpy(tempWkPtr, tempRxPtr, sizeof(short));
			tempWkPtr += sizeof(short);
			tempRxPtr += sizeof(short)*(PCM_CAPTURE_CHANNELS+PCM_REFERENCE_CHANNELS);
		}
	}  

//		printf("ASPL effext Process %d",frames);
	
	// Mic 1-2CH DownSample
	DoDnsample(&DownSampleHandle1,&AsplSrcBuf_48k[0][0], &AsplSrcBuf[0][0], frames, 3, 1.0);		
	DoDnsample(&DownSampleHandle2,&AsplSrcBuf_48k[1][0], &AsplSrcBuf[1][0], frames, 3, 1.0);

	// Ref DownSample
	DoDnsample(&DownSampleHandle3,&AsplRefBuf_48k[0][0], &AsplRefBuf[0][0], frames, 3, 0.3);
	DoDnsample(&DownSampleHandle4,&AsplRefBuf_48k[1][0], &AsplRefBuf[1][0], frames, 3, 0.3);

	for (int k=0 ; k<frames/3; k++){
		AsplRefBuf[0][k] = AsplRefBuf[0][k] + AsplRefBuf[1][k];
	}
			
//		aspl_AEC_process_single(&AsplSrcBuf[0][0],&AsplRefBuf[0][0], 256, -34 ,-5.0,  &aspl_nr_config);	
//		int res = aspl_RET_FAIL;
//		if (aspl_nr_config.AEC_filter_updated == 0) {
//			res = aspl_AEC_process_filtersave(&AsplSrcBuf[0][0], &AsplRefBuf[0][0], frames/3, -40, 1, -5.0, &aspl_nr_config); 
//			if (res == aspl_RET_SUCCESS) {
//	//				FILE *aspl_cfg_fd = NULL;
//	//	
//	//				if (NULL == aspl_cfg_fd) {
//	//					aspl_cfg_fd = fopen("/mnt/setting/markTaec.cfg", "wb");
//	//				}
//	//			
//	//				if (NULL != aspl_cfg_fd) {
//	//					for (int i=0; i<aspl_nr_config.aec_Mic_N; i++) {
//	//						fwrite(&aspl_nr_config.AEC_filter_len[i], sizeof(int), 1, aspl_cfg_fd);
//	//						fwrite(&aspl_nr_config.AEC_globaldelay[i], sizeof(int), 1, aspl_cfg_fd);
//	//						fwrite(aspl_nr_config.AEC_filter_save[i], sizeof(float), aspl_nr_config.AEC_filter_len[i], aspl_cfg_fd);
//	//	
//	//					}
//	//				}
//	//	
//	//				if (NULL != aspl_cfg_fd) {
//	//					fclose(aspl_cfg_fd);
//	//					aspl_cfg_fd = NULL;
//	//				}
//				aspl_nr_config.AEC_filter_updated = 1;
//	//				fourthArg = 0;
//				printf("aspl AEC filter save done.\r\n");
//			}
//	//			aspl_NR_process_2mic(&AsplSrcBuf[0][0],frames/3,1,1,&DoAVal);
//		}else if (aspl_nr_config.AEC_filter_updated == 1) {
//	//		
//	//			if (aspl_nr_config.AEC_filter_loaded == 0){
//	//				FILE *aspl_cfg_read_fd = NULL;
//	//		
//	//				if (NULL == aspl_cfg_read_fd) {
//	//					aspl_cfg_read_fd = fopen("/mnt/setting/markTaec.cfg", "rb");
//	//					if (aspl_cfg_read_fd == NULL)
//	//					{
//	//						printf("aspl AEC filter file open error\r\n");
//	//						// file open error, you can not read the file.
//	//					}
//	//				}
//	//		
//	//				if (NULL != aspl_cfg_read_fd) {
//	//					for (int i=0; i<aspl_nr_config.aec_Mic_N; i++) {
//	//						fread(&aspl_nr_config.AEC_filter_len[i], sizeof(int), 1, aspl_cfg_read_fd);
//	//						fread(&aspl_nr_config.AEC_globaldelay[i], sizeof(int), 1, aspl_cfg_read_fd);
//	//						fread(aspl_nr_config.AEC_filter_save[i], sizeof(float), aspl_nr_config.AEC_filter_len[i], aspl_cfg_read_fd);
//	//					}
//	//					aspl_AEC_filterload(&aspl_nr_config);
//	//					aspl_nr_config.AEC_filter_loaded = 1;
//	//					printf("aspl AEC filter load done.\r\n");
//	//				}
//	//		
//	//		
//	//				if (NULL != aspl_cfg_read_fd) {
//	//					fclose(aspl_cfg_read_fd);
//	//					aspl_cfg_read_fd = NULL;
//	//				}				 
//	//		
//	//			}
//		
//	//			aspl_AEC_process_2ch(data, ref, frame_size, aspl_nr_config.AEC_globaldelay[0], 0.0, &aspl_nr_config);
//	//			aspl_NR_process_2mic(data, frame_size, Beam_no, 1, &DoA);
//			
//	//			aspl_AEC_process_2ch(&AsplSrcBuf[0][0], &AsplRefBuf[0][0], frames/3, aspl_nr_config.AEC_globaldelay[0], -5.0,&aspl_nr_config);
//			aspl_NR_process_2mic(&AsplSrcBuf[0][0], &AsplRefBuf[0][0], frames/3, 1, 1, &DoAVal,  &aspl_nr_config);
//		
//		
//		}

	aspl_AEC_process_2ch(&AsplSrcBuf[0][0], &AsplRefBuf[0][0], 256, 0, -5.0, &aspl_nr_config);
	aspl_NR_process_2mic(&AsplSrcBuf[0][0], &AsplRefBuf[0][0], 256, 1, 1, &DoAVal,  &aspl_nr_config);
	



//		aspl_NR_process_2mic(&AsplSrcBuf[0][0],256,1,1,&DoAVal);

	DoUpsample(&UpSampleHandle, &AsplSrcBuf[0][0], TempResample, frames/3, 3, 1.0);

	for(i=0;i<frames;i++) {
		pData_out[2*i+0] = TempResample[i];
		pData_out[2*i+1] = TempResample[i];
	}


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
