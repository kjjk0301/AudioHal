#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <time.h>

#include "sys.h"
#include "polyphase.h"
#include "beamforming.h"
#include "noise_supression.h"
#include "echo_canceller.h"
#include "autogaincontrol.h"
#include "ssl_vad.h"
#include "ssl_core.h"
#include "time_constant.h"
#include "audio_utils.h"
#include "debug_file.h"
#include "aspl_nr.h"

// Define version information
#define LIBRARY_VERSION "0.5.6"
#define RELEASE_DATE "2025-03-25"
#define RELEASE_STATUS "MarkT renewal multi instance"

void aspl_NR_expert_param_read(aspl_nr_params_t* tmp_p, aspl_NR_CONFIG* config);
void aspl_NR_expert_param_write(aspl_nr_params_t* tmp_p, aspl_NR_CONFIG* config);

void aspl_print_ver(){
    printf("ASPL NR Library Version: %s, Release Date: %s, Status: %s\n", LIBRARY_VERSION, RELEASE_DATE, RELEASE_STATUS);
}

// API to return version information
const char* aspl_getVersionInfo() {
    static char versionInfo[100];
    snprintf(versionInfo, sizeof(versionInfo), "Version: %s, Release Date: %s, Status: %s", LIBRARY_VERSION, RELEASE_DATE, RELEASE_STATUS);
    return versionInfo;
}


int aspl_NR_create(aspl_NR_CONFIG* config){
    int i;

    aspl_print_ver();

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_create :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    }

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;

    if (Total_Inst_p != NULL){     

        for (int k=0; k<(MULTI_INPUT_CHANNELS+REF_CHANNELS); k++){
            if (Total_Inst_p->g_codec_4mic_buf[k] != NULL) {
                free(Total_Inst_p->g_codec_4mic_buf[k]);
                Total_Inst_p->g_codec_4mic_buf[k] = NULL;
            }   
        }

        if (Total_Inst_p->g_WorkBuf != NULL) {
            free(Total_Inst_p->g_WorkBuf);
            Total_Inst_p->g_WorkBuf = NULL;
        }

        if (Total_Inst_p->g_data_vad != NULL) {
            free(Total_Inst_p->g_data_vad);
            Total_Inst_p->g_data_vad = NULL;
        }

        Poly_DeInit(&Total_Inst_p->polyInst_p);
        ssl_vad_DeInit(&Total_Inst_p->vadInst_p);
        NS_DeInit(&Total_Inst_p->NSinst_p);
        AGC_DeInit(&Total_Inst_p->agcInst_p);

        for (int i = 0; i < MicN; i++)
        {
            if (Total_Inst_p->fft_in_mat[i] != NULL)
            {                
                // 이미 할당된 메모리 해제
                free(Total_Inst_p->fft_in_mat[i]);
                Total_Inst_p->fft_in_mat[i] = NULL;
            }
        }

        for (int i = 0; i < BeamN; i++)
        {
            if (Total_Inst_p->fft_out_mat[i] != NULL)
            {
                free(Total_Inst_p->fft_out_mat[i]);
                Total_Inst_p->fft_out_mat[i] = NULL;
            }
        }        

        if (Total_Inst_p->fft_ns_buf_mat != NULL)
        {
            free(Total_Inst_p->fft_ns_buf_mat);
            Total_Inst_p->fft_ns_buf_mat = NULL;
        }                   

        free(Total_Inst_p);
        Total_Inst_p = NULL;
    }

    Total_Inst_p = malloc(sizeof(Total_Inst_t));
    config_p->Total_Inst_p = (void *)Total_Inst_p;

    void * polyInst_p = (void *)sysPolyCreate();
    void * vadInst_p=(void *)sysSSLVADCreate();
    void * NSinst_p=(void *)sysNSCreate();
    void * agcInst_p = (void *)sysAGCCreate();

    Total_Inst_p->polyInst_p = polyInst_p;
    Total_Inst_p->vadInst_p = vadInst_p;
    Total_Inst_p->NSinst_p = NSinst_p;
    Total_Inst_p->agcInst_p = agcInst_p;

    Total_Inst_p->DC_rej_enable = 1;
    Total_Inst_p->NS_enable = 1;
    Total_Inst_p->AGC_enable = 1;
    Total_Inst_p->AGC_band_enable = 0;

    Total_Inst_p->DC_beta = 5;
    Total_Inst_p->poly_scale = 8;
    Total_Inst_p->g_gainQ15 = (int)(0.0 * 32767.0);

    Total_Inst_p->tuning_delay = 1500;

    int WorkBufSize = NUM_FRAMES * (WORK_CHANNELS) * NUM_BYTE_PER_SAMPLE;
        int CodecBufSize = CODEC_BUF_LENGTH_MAX * NUM_BYTE_PER_SAMPLE;

    for (i=0; i<(MULTI_INPUT_CHANNELS+REF_CHANNELS) ; i++){
        Total_Inst_p->g_codec_4mic_buf[i] = (int16_t *) malloc(CodecBufSize);
        if (NULL == Total_Inst_p->g_codec_4mic_buf[i])
        {
            printf("aspl_NR_create :: g_WorkBuf_4mic malloc ....failed\n");
            return aspl_RET_FAIL_MALLOC;
        }
    }

    Total_Inst_p->g_WorkBuf = (char *) malloc(WorkBufSize);
    if (NULL == Total_Inst_p->g_WorkBuf)
    {
        printf("aspl_NR_create :: g_WorkBuf malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    Total_Inst_p->g_data_vad = (int16_t *)malloc(NUM_FRAMES * MAX_VAD_CHANNELS * sizeof(int16_t));  
    if (NULL == Total_Inst_p->g_data_vad)
    {
        printf("aspl_NR_create :: g_data_vad malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    for (int i = 0; i < MicN; i++)
    {
        Total_Inst_p->fft_in_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
        if (Total_Inst_p->fft_in_mat[i] == NULL)
        {
            perror("aspl_NR_create :: fft_in_mat  malloc fail");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(Total_Inst_p->fft_in_mat[j]);
                Total_Inst_p->fft_in_mat[j] = NULL;
            }
            return aspl_RET_FAIL_MALLOC;
        }
    }    

    for (int i = 0; i < BeamN; i++)
    {
        Total_Inst_p->fft_out_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
        if (Total_Inst_p->fft_out_mat[i] == NULL)
        {
            perror("aspl_NR_create :: fft_out_mat  malloc fail");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(Total_Inst_p->fft_out_mat[j]);
                Total_Inst_p->fft_out_mat[j] = NULL;
            }
            for (int j = 0; j < MicN; j++)
            {
                free(Total_Inst_p->fft_in_mat[j]);
                Total_Inst_p->fft_in_mat[j] = NULL;
            }            
            return aspl_RET_FAIL_MALLOC;
        }
    }

    Total_Inst_p->fft_ns_buf_mat = (int32_t *)malloc((PolyM*(PolyL+Ma_size_max-1)) * sizeof(int32_t));
    if (Total_Inst_p->fft_ns_buf_mat == NULL)
    {
        perror("Poly_init malloc fail: fft_xout");
                    // 이미 할당된 메모리 해제
        for (int j = 0; j < BeamN; j++)
        {
            free(Total_Inst_p->fft_out_mat[j]);
            Total_Inst_p->fft_out_mat[j] = NULL;
        }
        for (int j = 0; j < MicN; j++)
        {
            free(Total_Inst_p->fft_in_mat[j]);
            Total_Inst_p->fft_in_mat[j] = NULL;
        }   
        return aspl_RET_FAIL_MALLOC;
    }

    memset(&Total_Inst_p->fft_ns_buf_mat[0], 0, sizeof(int32_t)*(PolyM*(PolyL+Ma_size_max-1))); 	// need to be moved to asplnr.c

    for (i=0; i<6; i++){
        Total_Inst_p->vad_pre[i]=0;
        Total_Inst_p->vad_min[i]=0;
        Total_Inst_p->vad_max[i]=0;
    }

    Total_Inst_p->total_idx = 0;
    Total_Inst_p->fade_gs = -50.0;
    Total_Inst_p->fade_gc = 0.0;
    Total_Inst_p->r_a = 0.7;

    Total_Inst_p->offset_Q15_L = 0;
    Total_Inst_p->offset_Q15_R = 0;
    for (int i=0; i<6; i++) Total_Inst_p->offset_Q15_multi[i] =0; 

    Total_Inst_p->sample_left = 0;

    strncpy(Total_Inst_p->g_nr_config.tuning_file_path, config_p->tuning_file_path, sizeof(Total_Inst_p->g_nr_config.tuning_file_path));
    Total_Inst_p->g_nr_config.tuning_file_path[sizeof(Total_Inst_p->g_nr_config.tuning_file_path) - 1] = '\0';

    if (access(Total_Inst_p->g_nr_config.tuning_file_path, F_OK) != -1) {
        // The file exists
    } else {

        printf("The param file %s does not exist.\n", Total_Inst_p->g_nr_config.tuning_file_path);

        return aspl_RET_FAIL;
    }

    printf("ASPL default param read from %s \r\n", Total_Inst_p->g_nr_config.tuning_file_path);
    aspl_NR_total_param_set_from_file(Total_Inst_p->g_nr_config.tuning_file_path, (aspl_NR_CONFIG *)config_p);   

    aspl_NR_set(aspl_NR_CMD_SET_NR, (aspl_NR_CONFIG *)config_p);       

    printf("aspl_NR_create() initialized done.\r\n");

    return aspl_RET_SUCCESS;
}


int aspl_NR_create_2mic(aspl_NR_CONFIG* config){
    int i;

    aspl_print_ver();

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_create :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    }

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;

    if (Total_Inst_p != NULL){     

        for (int k=0; k<(MULTI_INPUT_CHANNELS+REF_CHANNELS); k++){
            if (Total_Inst_p->g_codec_4mic_buf[k] != NULL) {
                free(Total_Inst_p->g_codec_4mic_buf[k]);
                Total_Inst_p->g_codec_4mic_buf[k] = NULL;
            }   
        }

        if (Total_Inst_p->g_WorkBuf != NULL) {
            free(Total_Inst_p->g_WorkBuf);
            Total_Inst_p->g_WorkBuf = NULL;
        }

        if (Total_Inst_p->g_data_vad != NULL) {
            free(Total_Inst_p->g_data_vad);
            Total_Inst_p->g_data_vad = NULL;
        }

        Poly_DeInit(&Total_Inst_p->polyInst_p);
        ssl_vad_DeInit(&Total_Inst_p->vadInst_p);
        NS_DeInit(&Total_Inst_p->NSinst_p);
        AGC_DeInit(&Total_Inst_p->agcInst_p);
        ssl_core_DeInit();

        for (int i = 0; i < MicN; i++)
        {
            if (Total_Inst_p->fft_in_mat[i] != NULL)
            {                
                // 이미 할당된 메모리 해제
                free(Total_Inst_p->fft_in_mat[i]);
                Total_Inst_p->fft_in_mat[i] = NULL;
            }
        }

        for (int i = 0; i < BeamN; i++)
        {
            if (Total_Inst_p->fft_out_mat[i] != NULL)
            {
                free(Total_Inst_p->fft_out_mat[i]);
                Total_Inst_p->fft_out_mat[i] = NULL;
            }
        }        

        if (Total_Inst_p->fft_ns_buf_mat != NULL)
        {
            free(Total_Inst_p->fft_ns_buf_mat);
            Total_Inst_p->fft_ns_buf_mat = NULL;
        }                   

        free(Total_Inst_p);
        Total_Inst_p = NULL;
    }

    Total_Inst_p = malloc(sizeof(Total_Inst_t));
    config_p->Total_Inst_p = (void *)Total_Inst_p;

    void * polyInst_p = (void *)sysPolyCreate();
    void * vadInst_p=(void *)sysSSLVADCreate();
    void * NSinst_p=(void *)sysNSCreate();
    void * agcInst_p = (void *)sysAGCCreate();

    Total_Inst_p->polyInst_p = polyInst_p;
    Total_Inst_p->vadInst_p = vadInst_p;
    Total_Inst_p->NSinst_p = NSinst_p;
    Total_Inst_p->agcInst_p = agcInst_p;

    Total_Inst_p->DC_rej_enable = 1;
    Total_Inst_p->NS_enable = 1;
    Total_Inst_p->AGC_enable = 1;
    Total_Inst_p->AGC_band_enable = 0;
    Total_Inst_p->bf_enable = 1;

    Total_Inst_p->DC_beta = 5;
    Total_Inst_p->poly_scale = 8;
    Total_Inst_p->g_gainQ15 = (int)(0.0 * 32767.0);

    Total_Inst_p->tuning_delay = 1500;

    int WorkBufSize = NUM_FRAMES * (WORK_CHANNELS) * NUM_BYTE_PER_SAMPLE;
    int CodecBufSize = CODEC_BUF_LENGTH_MAX * NUM_BYTE_PER_SAMPLE;

    for (i=0; i<(MULTI_INPUT_CHANNELS+REF_CHANNELS) ; i++){
        Total_Inst_p->g_codec_4mic_buf[i] = (int16_t *) malloc(CodecBufSize);
        if (NULL == Total_Inst_p->g_codec_4mic_buf[i])
        {
            printf("aspl_NR_create :: g_WorkBuf_4mic malloc ....failed\n");
            return aspl_RET_FAIL_MALLOC;
        }
    }

    Total_Inst_p->g_WorkBuf = (char *) malloc(WorkBufSize);
    if (NULL == Total_Inst_p->g_WorkBuf)
    {
        printf("aspl_NR_create :: g_WorkBuf malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    Total_Inst_p->g_data_vad = (int16_t *)malloc(NUM_FRAMES * MAX_VAD_CHANNELS * sizeof(int16_t));  
    if (NULL == Total_Inst_p->g_data_vad)
    {
        printf("aspl_NR_create :: g_data_vad malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    for (int i = 0; i < MicN; i++)
    {
        Total_Inst_p->fft_in_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
        if (Total_Inst_p->fft_in_mat[i] == NULL)
        {
            perror("aspl_NR_create :: fft_in_mat  malloc fail");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(Total_Inst_p->fft_in_mat[j]);
                Total_Inst_p->fft_in_mat[j] = NULL;
            }
            return aspl_RET_FAIL_MALLOC;
        }
    }    

    for (int i = 0; i < BeamN; i++)
    {
        Total_Inst_p->fft_out_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
        if (Total_Inst_p->fft_out_mat[i] == NULL)
        {
            perror("aspl_NR_create :: fft_out_mat  malloc fail");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(Total_Inst_p->fft_out_mat[j]);
                Total_Inst_p->fft_out_mat[j] = NULL;
            }
            for (int j = 0; j < MicN; j++)
            {
                free(Total_Inst_p->fft_in_mat[j]);
                Total_Inst_p->fft_in_mat[j] = NULL;
            }            
            return aspl_RET_FAIL_MALLOC;
        }
    }

    for (int i = 0; i < BeamN; i++)
    {
        Total_Inst_p->fft_bf_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
        Total_Inst_p->fft_bf_rear_mat[i] = (int32_t *)malloc((PolyM*PolyL*2) * sizeof(int32_t));
    }    

    Total_Inst_p->fft_ns_buf_mat = (int32_t *)malloc((PolyM*(PolyL+Ma_size_max-1)) * sizeof(int32_t));
    if (Total_Inst_p->fft_ns_buf_mat == NULL)
    {
        perror("Poly_init malloc fail: fft_xout");
                    // 이미 할당된 메모리 해제
        for (int j = 0; j < BeamN; j++)
        {
            free(Total_Inst_p->fft_out_mat[j]);
            Total_Inst_p->fft_out_mat[j] = NULL;
        }
        for (int j = 0; j < MicN; j++)
        {
            free(Total_Inst_p->fft_in_mat[j]);
            Total_Inst_p->fft_in_mat[j] = NULL;
        }   
        return aspl_RET_FAIL_MALLOC;
    }

    memset(&Total_Inst_p->fft_ns_buf_mat[0], 0, sizeof(int32_t)*(PolyM*(PolyL+Ma_size_max-1))); 	// need to be moved to asplnr.c

    void * bfInst_p = (void *)sysBFCreate(Total_Inst_p);
    void * sslInst_p =  (void *)sysSSLCORECreate();
    
    Total_Inst_p->bfInst_p = bfInst_p;
    Total_Inst_p->sslInst_p = sslInst_p;

    for (i=0; i<6; i++){
        Total_Inst_p->vad_pre[i]=0;
        Total_Inst_p->vad_min[i]=0;
        Total_Inst_p->vad_max[i]=0;
    }

    Total_Inst_p->total_idx = 0;
    Total_Inst_p->fade_gs = -50.0;
    Total_Inst_p->fade_gc = 0.0;
    Total_Inst_p->r_a = 0.7;

    Total_Inst_p->offset_Q15_L = 0;
    Total_Inst_p->offset_Q15_R = 0;
    for (int i=0; i<6; i++) Total_Inst_p->offset_Q15_multi[i] =0; 

    Total_Inst_p->sample_left = 0;

    strncpy(Total_Inst_p->g_nr_config.tuning_file_path, config_p->tuning_file_path, sizeof(Total_Inst_p->g_nr_config.tuning_file_path));
    Total_Inst_p->g_nr_config.tuning_file_path[sizeof(Total_Inst_p->g_nr_config.tuning_file_path) - 1] = '\0';

    if (access(Total_Inst_p->g_nr_config.tuning_file_path, F_OK) != -1) {
        // The file exists
    } else {
        printf("The param file %s does not exist.\n", Total_Inst_p->g_nr_config.tuning_file_path);
        return aspl_RET_FAIL;
    }

    printf("ASPL default param read from %s \r\n", Total_Inst_p->g_nr_config.tuning_file_path);
    aspl_NR_total_param_set_from_file(Total_Inst_p->g_nr_config.tuning_file_path, (aspl_NR_CONFIG *)config_p);   

    aspl_NR_set(aspl_NR_CMD_SET_NR, (aspl_NR_CONFIG *)config_p);
    
    Total_Inst_p->DoA_mean = 0.0;
    Total_Inst_p->no_DoA_cnt = -1;
    Total_Inst_p->g_Beamno = 0;

    printf("aspl_NR_create_2mic() initialized done.\r\n");

    return aspl_RET_SUCCESS;
}


int aspl_NR_destroy(aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;

    if (config_p != NULL){
        Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;

        config_p->Total_Inst_p = NULL;        

        if (Total_Inst_p != NULL){
            for (int k=0; k<(MULTI_INPUT_CHANNELS+REF_CHANNELS); k++){
                if (Total_Inst_p->g_codec_4mic_buf[k] != NULL) {
                    free(Total_Inst_p->g_codec_4mic_buf[k]);
                    Total_Inst_p->g_codec_4mic_buf[k] = NULL;
                }   
            }

            if (Total_Inst_p->g_WorkBuf != NULL) {
                free(Total_Inst_p->g_WorkBuf);
                Total_Inst_p->g_WorkBuf = NULL;
            }

            if (Total_Inst_p->g_data_vad != NULL) {
                free(Total_Inst_p->g_data_vad);
                Total_Inst_p->g_data_vad = NULL;
            }

            Poly_DeInit(Total_Inst_p->polyInst_p);
            ssl_vad_DeInit(Total_Inst_p->vadInst_p);
            NS_DeInit(Total_Inst_p->NSinst_p);
            AGC_DeInit(Total_Inst_p->agcInst_p);
            ssl_core_DeInit();

            for (int i = 0; i < MicN; i++)
            {
                if (Total_Inst_p->fft_in_mat[i] != NULL)
                {                
                    // 이미 할당된 메모리 해제
                    free(Total_Inst_p->fft_in_mat[i]);
                    Total_Inst_p->fft_in_mat[i] = NULL;
                }
            }
    
            for (int i = 0; i < BeamN; i++)
            {
                if (Total_Inst_p->fft_out_mat[i] != NULL)
                {
                    free(Total_Inst_p->fft_out_mat[i]);
                    Total_Inst_p->fft_out_mat[i] = NULL;
                }
            }        
    
            if (Total_Inst_p->fft_ns_buf_mat != NULL)
            {
                free(Total_Inst_p->fft_ns_buf_mat);
                Total_Inst_p->fft_ns_buf_mat = NULL;
            }    

            free(Total_Inst_p);
            Total_Inst_p = NULL;        
        } else if (Total_Inst_p == NULL) {
        printf("aspl_NR_destroy :: (Total_Inst_t *)Total_Inst_p has not been created");
        return aspl_RET_FAIL;
    }

    } else if (config_p == NULL) {
        printf("aspl_NR_destroy :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }

    // debug_matlab_close();

    return aspl_RET_SUCCESS;
}


int aspl_NR_param_set(int param_num, int value, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_param_set :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }

    int dummy[64];
    aspl_nr_params_t tmp;

    aspl_NR_expert_param_read(&tmp, (aspl_NR_CONFIG *) config_p);

    memcpy(dummy, &tmp, sizeof(aspl_nr_params_t));

    dummy[param_num]=value;

    memcpy(&tmp, dummy, sizeof(aspl_nr_params_t));

    aspl_NR_expert_param_write(&tmp, (aspl_NR_CONFIG *) config_p);

    return aspl_RET_SUCCESS;
}

int aspl_NR_param_get(int param_num, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_param_get :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }

    int dummy[64];

    aspl_nr_params_t tmp;

    aspl_NR_expert_param_read(&tmp, (aspl_NR_CONFIG *) config_p);

    memcpy(dummy, &tmp, sizeof(aspl_nr_params_t));

    return dummy[param_num];
}

void aspl_NR_expert_param_read(aspl_nr_params_t* tmp_p, aspl_NR_CONFIG* config) {

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_expert_param_read :: (aspl_NR_CONFIG *)config has not been created");
        return;
    }

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
    if (Total_Inst_p == NULL) {
        printf("aspl_NR_expert_param_read :: (Total_Inst_t *)Total_Inst_p has not been created");
        return;
    }

    aspl_nr_params_t tmp;
    int k;

    nsInst_t* nsInstp = Total_Inst_p->NSinst_p;
    agcInst_t* agcInstp = Total_Inst_p->agcInst_p;
    vadInst_t* vadInstp = Total_Inst_p->vadInst_p;

    tmp.ns_voice_start_bin = (int) nsInstp->voice_start_bin;

    tmp.ns_voice_end_bin=(int) nsInstp->voice_end_bin;

#ifdef PRINT_EXPERT_PARAM
    printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
    printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
#endif
    tmp.ns_Low_solo = (int) nsInstp->Low_solo ;
    tmp.ns_Mid_solo= (int)  nsInstp->Mid_solo ;
    tmp.ns_Hi_solo= (int) nsInstp->Hi_solo ;

#ifdef PRINT_EXPERT_PARAM
    printf("ns_Low_solo = %d\n", nsInstp->Low_solo); 
    printf("ns_Mid_solo = %d\n", nsInstp->Mid_solo); 
    printf("ns_Hi_solo = %d\n", nsInstp->Hi_solo); 
#endif

    tmp.ns_beta_e_num_0 = (int)  nsInstp->beta_e_num[0];
    tmp.ns_beta_e_num_1 = (int)  nsInstp->beta_e_num[1];
    tmp.ns_beta_e_num_2 = (int)  nsInstp->beta_e_num[2];
    tmp.ns_beta_e_num_3 = (int)  nsInstp->beta_e_num[3];
    tmp.ns_beta_e_num_4 = (int)  nsInstp->beta_e_num[4];
    tmp.ns_beta_e_num_5 = (int)  nsInstp->beta_e_num[5];

#ifdef PRINT_EXPERT_PARAM
    printf("High band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
    printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);      
    printf("Mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
    printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);      
    printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
    printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);      
#endif

    tmp.ns_beta_r_num_0 = (int) nsInstp->beta_r_num[0];
    tmp.ns_beta_r_num_1 = (int) nsInstp->beta_r_num[2];
    tmp.ns_beta_r_num_2 = (int) nsInstp->beta_r_num[3];
    tmp.ns_beta_r_num_3 = (int) nsInstp->beta_r_num[5];
    tmp.ns_beta_r_num_4 = (int) nsInstp->beta_r_num[6];
    tmp.ns_beta_r_num_5 = (int) nsInstp->beta_r_num[8];

#ifdef PRINT_EXPERT_PARAM
    printf("High band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);
    printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);      
    printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);      
    printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);      
    printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
    printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
#endif

    tmp.ns_betaQ15 = (int) nsInstp->betaQ15;
    tmp.ns_max_att  = (int) nsInstp->max_att;
    tmp.ns_min_att  = (int) nsInstp->min_att;
    tmp.ns_slope  = (int) (nsInstp->slope*10.0);
    tmp.ns_high_att  = (int) nsInstp->high_att;
    tmp.ns_Ma_size = (int) nsInstp->Ma_size;

#ifdef PRINT_EXPERT_PARAM
    printf("nsInstp->betaQ15=%d (%02f)\n", nsInstp->betaQ15, (((float)nsInstp->betaQ15)/32768.0));
    printf("nsInstp->max_att=%02f\n", nsInstp->max_att);
    printf("nsInstp->min_att=%02f\n", nsInstp->min_att);
    printf("nsInstp->slope=%02f\n", nsInstp->slope);
    printf("nsInstp->high_att=%02f\n", nsInstp->high_att);
#endif

    tmp.vad_d_SNR  = (int) (vadInstp->d_SNR*2.0);
    tmp.vad_d_SNR_vad  = (int) (vadInstp->d_SNR_vad*2.0);
    tmp.vad_n_floor_min = (int) vadInstp->n_floor_min;

#ifdef PRINT_EXPERT_PARAM
    printf("vadInstp->d_SNR=%02f, vadInstp->d_SNR_ratio=%02.3f\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);               
    printf("vadInstp->d_SNR_vad=%02f, vadInstp->d_SNR_vad_ratio=%02.3f\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);            
    printf("vadInstp->n_floor_min=%d, vadInstp->t_chg_min=%.2f\n", vadInstp->n_floor_min, vadInstp->t_chg_min);            
#endif

    tmp.agc_max_gain_dB  = (int) agcInstp->max_gain_dB;
    tmp.agc_min_gain_dB  = (int) agcInstp->min_gain_dB;
    tmp.agc_target_SPL  = (int) agcInstp->target_SPL;
    tmp.agc_gate_dB  = (int) 0;
    tmp.agc_att_num  = (int) agcInstp->att_num;
    tmp.agc_rel_num = (int) agcInstp->rel_num ;
    tmp.vad_hold_max = (int) vadInstp->vad_hold_max;
    tmp.vad_hold_min = (int) vadInstp->vad_hold_min;

#ifdef PRINT_EXPERT_PARAM
    printf("max_gain_dB = %f, max_gain=%f\r\n", agcInstp->max_gain_dB, agcInstp->max_gain);  
    printf("min_gain_dB = %f, min_gain=%f\r\n", agcInstp->min_gain_dB, agcInstp->min_gain);
    printf("target_SPL = %f, Kp_dB=%f, Kp=%f\r\n", agcInstp->target_SPL, agcInstp->Kp_dB, agcInstp->Kp);
    printf("gate_dB = %f\r\n", agcInstp->gate_dB);   
    printf("AGC attack time = %f [msec]\r\n", tau16k_90_msec[agcInstp->att_num]);
    printf("AGC release time = %f [msec]\r\n", tau16k_90_msec[agcInstp->rel_num]);
#endif

    tmp.NS_enable = (int) Total_Inst_p->NS_enable;
    tmp.AGC_enable = (int) Total_Inst_p->AGC_enable;

#ifdef PRINT_EXPERT_PARAM    
    printf("NS enabled = %d \r\n", Total_Inst_p->NS_enable);
    printf("AGC enabled =%d \r\n", Total_Inst_p->AGC_enable);
#endif

    tmp.EQ_Low = (int)nsInstp->EQ_Low;
    tmp.EQ_Hi = (int)nsInstp->EQ_Hi;

#ifdef PRINT_EXPERT_PARAM
    printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
    printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
    printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
#endif

    tmp.betaQ15_NR1 = Total_Inst_p->betaQ15_NR1;//(int32_t)(0.4*32767.0); //Total_Inst_p->betaQ15_NR1;
    tmp.max_att_NR1 = Total_Inst_p->max_att_NR1;//(int32_t)-15; //Total_Inst_p->max_att_NR1;
    tmp.min_att_NR1 = Total_Inst_p->min_att_NR1;  //(int32_t)-1; //Total_Inst_p->min_att_NR1; 
    tmp.slope_NR1 = Total_Inst_p->slope_NR1;  //(int32_t)15; //Total_Inst_p->slope_NR1; 
    tmp.high_att_NR1 = Total_Inst_p->high_att_NR1; //(int32_t)-10; //Total_Inst_p->high_att_NR1;
    tmp.target_SPL_NR1 = Total_Inst_p->target_SPL_NR1; //(int32_t)73; //Total_Inst_p->target_SPL_NR1;

    tmp.betaQ15_NR2 = Total_Inst_p->betaQ15_NR2; //(int32_t)(0.4*32767.0); //Total_Inst_p->betaQ15_NR2;
    tmp.max_att_NR2 = Total_Inst_p->max_att_NR2; //(int32_t)-20; //Total_Inst_p->max_att_NR2;
    tmp.min_att_NR2 = Total_Inst_p->min_att_NR2;  //(int32_t)-4; //Total_Inst_p->min_att_NR2; 
    tmp.slope_NR2 = Total_Inst_p->slope_NR2; //(int32_t)18; //Total_Inst_p->slope_NR2; 
    tmp.high_att_NR2 = Total_Inst_p->high_att_NR2; //(int32_t)-12; //Total_Inst_p->high_att_NR2;
    tmp.target_SPL_NR2 = Total_Inst_p->target_SPL_NR2; //(int32_t)73; //Total_Inst_p->target_SPL_NR2;

    tmp.betaQ15_NR3 = Total_Inst_p->betaQ15_NR3; //(int32_t)(0.4*32767.0); // Total_Inst_p->betaQ15_NR3;
    tmp.max_att_NR3 =  Total_Inst_p->max_att_NR3; //(int32_t)-25; //Total_Inst_p->max_att_NR3;
    tmp.min_att_NR3 = Total_Inst_p->min_att_NR3; //(int32_t)-8; //Total_Inst_p->min_att_NR3; 
    tmp.slope_NR3 = Total_Inst_p->slope_NR3;  //(int32_t)20; //Total_Inst_p->slope_NR3; 
    tmp.high_att_NR3 = Total_Inst_p->high_att_NR3; //(int32_t)-15; //Total_Inst_p->high_att_NR3;
    tmp.target_SPL_NR3 = Total_Inst_p->target_SPL_NR3;  //(int32_t)73; //Total_Inst_p->target_SPL_NR3; 

    tmp.DC_reject_beta = Total_Inst_p->DC_beta;
    tmp.ns_beta_low_ratio = (int32_t)(nsInstp->beta_low_ratio*10.0);
    tmp.ns_beta_high_ratio =(int32_t)(nsInstp->beta_high_ratio*10.0);

    tmp.vad_tau_s_r = (int32_t)(vadInstp->tau_s_r*Q31_MAX);
    tmp.vad_tau_s_f = (int32_t)(vadInstp->tau_s_f*Q31_MAX);
    tmp.global_gain = (int32_t)(20*log10((double)Total_Inst_p->g_gainQ15/32767.0));

    memcpy(tmp_p, &tmp, sizeof(aspl_nr_params_t));

}


void aspl_NR_expert_param_write(aspl_nr_params_t* tmp_p, aspl_NR_CONFIG* config){

        int k;
        aspl_NR_CONFIG * config_p = config;
        if (config_p == NULL) {
            printf("aspl_NR_expert_param_write :: (aspl_NR_CONFIG *)config has not been created");
            return;
        }

        Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
        if (Total_Inst_p == NULL) {
            printf("aspl_NR_expert_param_write :: (Total_Inst_t *)Total_Inst_p has not been created");
            return;
        } 
        
        nsInst_t* nsInstp = Total_Inst_p->NSinst_p;
        agcInst_t* agcInstp = Total_Inst_p->agcInst_p;
        vadInst_t* vadInstp = Total_Inst_p->vadInst_p;

        nsInstp->voice_start_bin=(short) tmp_p->ns_voice_start_bin;
        if (nsInstp->voice_start_bin>=nsInstp->voice_end_bin) nsInstp->voice_start_bin=nsInstp->voice_end_bin-1;
        if (nsInstp->voice_start_bin<1) nsInstp->voice_start_bin=1;

        nsInstp->voice_end_bin=(short) tmp_p->ns_voice_end_bin;
        if (nsInstp->voice_end_bin>=PolyM/2-2) nsInstp->voice_end_bin=PolyM/2-2;
        if (nsInstp->voice_end_bin<=nsInstp->voice_start_bin) nsInstp->voice_end_bin=nsInstp->voice_start_bin+1;

#ifdef PRINT_EXPERT_PARAM 
        printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
        printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
#endif
        nsInstp->Low_solo = (short) tmp_p->ns_Low_solo;
        nsInstp->Mid_solo = (short) tmp_p->ns_Mid_solo;
        nsInstp->Hi_solo = (short) tmp_p->ns_Hi_solo;

        if (nsInstp->Low_solo==1){
            nsInstp->Mid_solo=0;
            nsInstp->Hi_solo=0;
            nsInstp->Low_bypass = 0;
            nsInstp->Mid_bypass = 1;
            nsInstp->Hi_bypass =1;
        } else if (nsInstp->Low_solo==0){
                if (nsInstp->Mid_solo==1){
                    nsInstp->Low_solo=0;
                    nsInstp->Hi_solo=0;  
                    nsInstp->Low_bypass = 1;
                    nsInstp->Mid_bypass = 0;
                    nsInstp->Hi_bypass =1;
                } else if (nsInstp->Mid_solo==0){
                    if (nsInstp->Hi_solo==1){
                        nsInstp->Low_solo=0;
                        nsInstp->Mid_solo=0;
                        nsInstp->Low_bypass = 1;
                        nsInstp->Mid_bypass = 1;
                        nsInstp->Hi_bypass =0;
                    } else if (nsInstp->Hi_solo==0){
                        nsInstp->Low_solo=0;
                        nsInstp->Mid_solo=0;
                        nsInstp->Hi_solo=0;  
                        nsInstp->Low_bypass = 0;
                        nsInstp->Mid_bypass = 0;
                        nsInstp->Hi_bypass = 0;
                    }
                }
        }

        nsInstp->beta_e_num[0] = (short) tmp_p->ns_beta_e_num_0;
        if (nsInstp->beta_e_num[0]>=23) nsInstp->beta_e_num[0]=22;
        if (nsInstp->beta_e_num[0]<0) nsInstp->beta_e_num[0]=0;
        nsInstp->beta_e[0]=beta[nsInstp->beta_e_num[0]];
        nsInstp->round_bit_e[0]=round_bit[nsInstp->beta_e_num[0]];
        nsInstp->gamma_inv_e[0]=gamma_inv[nsInstp->beta_e_num[0]];
#ifdef PRINT_EXPERT_PARAM        
        printf("High band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
#endif 
        nsInstp->beta_e_num[1] = (short) tmp_p->ns_beta_e_num_1;
        if (nsInstp->beta_e_num[1]>=23) nsInstp->beta_e_num[1]=22;
        if (nsInstp->beta_e_num[1]<0) nsInstp->beta_e_num[1]=0;
        nsInstp->beta_e[1]=beta[nsInstp->beta_e_num[1]];
        nsInstp->round_bit_e[1]=round_bit[nsInstp->beta_e_num[1]];
        nsInstp->gamma_inv_e[1]=gamma_inv[nsInstp->beta_e_num[1]];
#ifdef PRINT_EXPERT_PARAM
        printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);      
#endif
        nsInstp->beta_e_num[2] = (short) tmp_p->ns_beta_e_num_2;
        if (nsInstp->beta_e_num[2]>=23) nsInstp->beta_e_num[2]=22;
        if (nsInstp->beta_e_num[2]<0) nsInstp->beta_e_num[2]=0;
        nsInstp->beta_e[2]=beta[nsInstp->beta_e_num[2]];
        nsInstp->round_bit_e[2]=round_bit[nsInstp->beta_e_num[2]];
        nsInstp->gamma_inv_e[2]=gamma_inv[nsInstp->beta_e_num[2]];
#ifdef PRINT_EXPERT_PARAM
        printf("Mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
#endif
        nsInstp->beta_e_num[3] = (short) tmp_p->ns_beta_e_num_3;
        if (nsInstp->beta_e_num[3]>=23) nsInstp->beta_e_num[3]=22;
        if (nsInstp->beta_e_num[3]<0) nsInstp->beta_e_num[3]=0;
        nsInstp->beta_e[3]=beta[nsInstp->beta_e_num[3]];
        nsInstp->round_bit_e[3]=round_bit[nsInstp->beta_e_num[3]];
        nsInstp->gamma_inv_e[3]=gamma_inv[nsInstp->beta_e_num[3]];
#ifdef PRINT_EXPERT_PARAM
        printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);      
#endif
        nsInstp->beta_e_num[4] = (short) tmp_p->ns_beta_e_num_4;
        if (nsInstp->beta_e_num[4]>=23) nsInstp->beta_e_num[4]=22;
        if (nsInstp->beta_e_num[4]<0) nsInstp->beta_e_num[4]=0;
        nsInstp->beta_e[4]=beta[nsInstp->beta_e_num[4]];
        nsInstp->round_bit_e[4]=round_bit[nsInstp->beta_e_num[4]];
        nsInstp->gamma_inv_e[4]=gamma_inv[nsInstp->beta_e_num[4]];
#ifdef PRINT_EXPERT_PARAM
        printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
#endif
        nsInstp->beta_e_num[5] = (short) tmp_p->ns_beta_e_num_5;
        if (nsInstp->beta_e_num[5]>=23) nsInstp->beta_e_num[5]=22;
        if (nsInstp->beta_e_num[5]<0) nsInstp->beta_e_num[5]=0;
        nsInstp->beta_e[5]=beta[nsInstp->beta_e_num[5]];
        nsInstp->round_bit_e[5]=round_bit[nsInstp->beta_e_num[5]];
        nsInstp->gamma_inv_e[5]=gamma_inv[nsInstp->beta_e_num[5]];
#ifdef PRINT_EXPERT_PARAM
        printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);      
#endif
        nsInstp->beta_r_num[0] = (short) tmp_p->ns_beta_r_num_0;
        if (nsInstp->beta_r_num[0]>=23) nsInstp->beta_r_num[0]=22;
        if (nsInstp->beta_r_num[0]<0) nsInstp->beta_r_num[0]=0;
        nsInstp->beta_r[0]=beta[nsInstp->beta_r_num[0]];
        nsInstp->round_bit_r[0]=round_bit[nsInstp->beta_r_num[0]];
        nsInstp->gamma_inv_r[0]=gamma_inv[nsInstp->beta_r_num[0]];
#ifdef PRINT_EXPERT_PARAM
        printf("High band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);
#endif
        nsInstp->beta_r_num[2] = (short) tmp_p->ns_beta_r_num_1;
        if (nsInstp->beta_r_num[2]>=23) nsInstp->beta_r_num[2]=22;
        if (nsInstp->beta_r_num[2]<0) nsInstp->beta_r_num[2]=0;
        nsInstp->beta_r[2]=beta[nsInstp->beta_r_num[2]];
        nsInstp->round_bit_r[2]=round_bit[nsInstp->beta_r_num[2]];
        nsInstp->gamma_inv_r[2]=gamma_inv[nsInstp->beta_r_num[2]];
#ifdef PRINT_EXPERT_PARAM
        printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);      
#endif
        nsInstp->beta_r_num[3] = (short) tmp_p->ns_beta_r_num_2;
        if (nsInstp->beta_r_num[3]>=23) nsInstp->beta_r_num[3]=22;
        if (nsInstp->beta_r_num[3]<0) nsInstp->beta_r_num[3]=0;
        nsInstp->beta_r[3]=beta[nsInstp->beta_r_num[3]];
        nsInstp->round_bit_r[3]=round_bit[nsInstp->beta_r_num[3]];
        nsInstp->gamma_inv_r[3]=gamma_inv[nsInstp->beta_r_num[3]];
#ifdef PRINT_EXPERT_PARAM
        printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);      
#endif
        nsInstp->beta_r_num[5] = (short) tmp_p->ns_beta_r_num_3;
        if (nsInstp->beta_r_num[5]>=23) nsInstp->beta_r_num[5]=22;
        if (nsInstp->beta_r_num[5]<0) nsInstp->beta_r_num[5]=0;
        nsInstp->beta_r[5]=beta[nsInstp->beta_r_num[5]];
        nsInstp->round_bit_r[5]=round_bit[nsInstp->beta_r_num[5]];
        nsInstp->gamma_inv_r[5]=gamma_inv[nsInstp->beta_r_num[5]];
#ifdef PRINT_EXPERT_PARAM
        printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);      
#endif
        nsInstp->beta_r_num[6] = (short) tmp_p->ns_beta_r_num_4;
        if (nsInstp->beta_r_num[6]>=23) nsInstp->beta_r_num[6]=22;
        if (nsInstp->beta_r_num[6]<0) nsInstp->beta_r_num[6]=0;
        nsInstp->beta_r[6]=beta[nsInstp->beta_r_num[6]];
        nsInstp->round_bit_r[6]=round_bit[nsInstp->beta_r_num[6]];
        nsInstp->gamma_inv_r[6]=gamma_inv[nsInstp->beta_r_num[6]];
#ifdef PRINT_EXPERT_PARAM
        printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
#endif
        nsInstp->beta_r_num[8] = (short) tmp_p->ns_beta_r_num_5;
        if (nsInstp->beta_r_num[8]>=23) nsInstp->beta_r_num[8]=22;
        if (nsInstp->beta_r_num[8]<0) nsInstp->beta_r_num[8]=0;
        nsInstp->beta_r[8]=beta[nsInstp->beta_r_num[8]];
        nsInstp->round_bit_r[8]=round_bit[nsInstp->beta_r_num[8]];
        nsInstp->gamma_inv_r[8]=gamma_inv[nsInstp->beta_r_num[8]];
#ifdef PRINT_EXPERT_PARAM
        printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
#endif
        nsInstp->betaQ15 = (int) tmp_p->ns_betaQ15;
        if (nsInstp->betaQ15>=32767) nsInstp->betaQ15=32767;
        if (nsInstp->betaQ15<0) nsInstp->betaQ15=0;

        nsInstp->max_att = (float) tmp_p->ns_max_att;
        if (nsInstp->max_att>0) nsInstp->max_att=0;
        nsInstp->min_att = (float) tmp_p->ns_min_att;
        if (nsInstp->min_att>0) nsInstp->min_att=0;
        nsInstp->slope = (float) tmp_p->ns_slope/10.0;
        nsInstp->high_att = (float) tmp_p->ns_high_att;
        if (nsInstp->high_att>0) nsInstp->high_att=0;

#ifdef PRINT_EXPERT_PARAM
        printf("nsInstp->max_att=%02f\n", nsInstp->max_att);
        printf("nsInstp->min_att=%02f\n", nsInstp->min_att);
        printf("nsInstp->slope=%02f\n", nsInstp->slope);
        printf("nsInstp->high_att=%02f\n", nsInstp->high_att);

        printf("nsInstp->HminQ15_p=\n");
#endif


        float rtemp = 1.0 / (float) (nsInstp->slope);

        for (k=0; k<nsInstp->voice_start_bin; k++) {
            float temp= nsInstp->min_att + (nsInstp->max_att/((float)nsInstp->voice_start_bin*(float)nsInstp->voice_start_bin)) * (float)((k-nsInstp->voice_start_bin)*(k-nsInstp->voice_start_bin));
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM            
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif
        }
            


        for (k=nsInstp->voice_start_bin ; k<PolyM/2+1; k++){
            float temp= nsInstp->min_att + nsInstp->high_att + (-1 * nsInstp->high_att)*powf(rtemp,((float)(k-nsInstp->voice_start_bin)/(float)(nsInstp->voice_end_bin-nsInstp->voice_start_bin)));
            
            // if (k>nsInstp->voice_end_bin){
            //     temp= temp + (float)(k-nsInstp->voice_end_bin)/(float)(PolyM/2-nsInstp->voice_end_bin)*3.0;
            // }
            
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif
        }

//         for (k=0 ; k<=nsInstp->voice_end_bin; k++){
//             (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)k+nsInstp->slope)+nsInstp->min_att)/20))*(float)Q15_val);
// #ifdef PRINT_EXPERT_PARAM
//                 if (k%8==0) printf("\n");
//                 printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
// #endif
//         }

//         for (k=nsInstp->voice_end_bin+1 ; k<PolyM/2+1; k++){
//             (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)k+nsInstp->slope)+nsInstp->min_att+((nsInstp->high_att-nsInstp->min_att)*(nsInstp->slope))/((float)(PolyM/2+1- k) *0.5+nsInstp->slope*0.5))/20))*(float)Q15_val);
// #ifdef PRINT_EXPERT_PARAM
//                 if (k%8==0) printf("\n");
//                 printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
// #endif
//         }

        nsInstp->Ma_size = tmp_p->ns_Ma_size;

        vadInstp->d_SNR = ((double) tmp_p->vad_d_SNR)/2.0;
        if (vadInstp->d_SNR<=0) vadInstp->d_SNR=0;
        vadInstp->d_SNR_ratio = pow(10, vadInstp->d_SNR/10.0)-1.0;
#ifdef PRINT_EXPERT_PARAM
        printf("vadInstp->d_SNR=%02f, vadInstp->d_SNR_ratio=%02.3f\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);            
#endif
        vadInstp->d_SNR_vad = ((double) tmp_p->vad_d_SNR_vad)/2.0;
        if (vadInstp->d_SNR_vad<=0) vadInstp->d_SNR_vad=0;
        vadInstp->d_SNR_vad_ratio = pow(10, vadInstp->d_SNR_vad/10.0)-1.0;

#ifdef PRINT_EXPERT_PARAM
        printf("vadInstp->d_SNR_vad=%02f, vadInstp->d_SNR_vad_ratio=%02.3f\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);            
#endif
        vadInstp->n_floor_min = (int) tmp_p->vad_n_floor_min;
        vadInstp->t_chg_min=pow(10,(((double)vadInstp->n_floor_min)/10));
#ifdef PRINT_EXPERT_PARAM
        printf("vadInstp->n_floor_min=%d, vadInstp->t_chg_min=%.2f\n", vadInstp->n_floor_min, vadInstp->t_chg_min);            
#endif
        agcInstp->max_gain_dB = (float) tmp_p->agc_max_gain_dB;
        agcInstp->max_gain= powf (10, agcInstp->max_gain_dB*0.05);
#ifdef PRINT_EXPERT_PARAM
        printf("max_gain_dB = %f, max_gain=%f\r\n", agcInstp->max_gain_dB, agcInstp->max_gain);
#endif
        agcInstp->min_gain_dB = (float) tmp_p->agc_min_gain_dB;
        agcInstp->min_gain= powf (10, agcInstp->min_gain_dB*0.05);
#ifdef PRINT_EXPERT_PARAM
        printf("min_gain_dB = %f, min_gain=%f\r\n", agcInstp->min_gain_dB, agcInstp->min_gain);
#endif
        agcInstp->target_SPL = (float) tmp_p->agc_target_SPL;
        agcInstp->Kp_dB = agcInstp->target_SPL - FS_SPL + Q15_dB *2;
        agcInstp->Kp=powf(10.0,(agcInstp->Kp_dB*0.05)); 
#ifdef PRINT_EXPERT_PARAM
        printf("target_SPL = %f, Kp_dB=%f, Kp=%f\r\n", agcInstp->target_SPL, agcInstp->Kp_dB, agcInstp->Kp);
#endif
        // agcInstp->gate_dB = (float) tmp_p->agc_gate_dB;
#ifdef PRINT_EXPERT_PARAM
        printf("gate_dB = %f\r\n", agcInstp->gate_dB);   
#endif
        agcInstp->att_num = (short) tmp_p->agc_att_num;
        if (agcInstp->att_num>=14) agcInstp->att_num=14;
        if (agcInstp->att_num<=0) agcInstp->att_num=0;
        agcInstp->tau_att=tau16k_90_msec[agcInstp->att_num];	
        agcInstp->r_att=gamma16k[agcInstp->att_num];
#ifdef PRINT_EXPERT_PARAM
        printf("AGC attack time = %f [msec]\r\n", tau16k_90_msec[agcInstp->att_num]);
#endif
        agcInstp->rel_num = (short) tmp_p->agc_rel_num;
        if (agcInstp->rel_num>=14) agcInstp->rel_num=14;
        if (agcInstp->rel_num<=0) agcInstp->rel_num=0;
        agcInstp->tau_rel=tau16k_90_msec[agcInstp->rel_num];
        agcInstp->r_rel=gamma16k[agcInstp->rel_num];      
#ifdef PRINT_EXPERT_PARAM
        printf("AGC release time = %f [msec]\r\n", tau16k_90_msec[agcInstp->rel_num]);
#endif
        vadInstp->vad_hold_max = (short) tmp_p->vad_hold_max;
        vadInstp->vad_hold_min = (short) tmp_p->vad_hold_min;


    Total_Inst_p->NS_enable = (int) tmp_p->NS_enable;
    Total_Inst_p->AGC_enable = (int) tmp_p->AGC_enable ;
#ifdef PRINT_EXPERT_PARAM
    printf("NS enabled = %d \r\n", Total_Inst_p->NS_enable);
    printf("AGC enabled =%d \r\n", Total_Inst_p->AGC_enable);
#endif
    nsInstp->EQ_Low  = (short) tmp_p->EQ_Low;
    nsInstp->EQ_Hi = (short) tmp_p->EQ_Hi;

#ifdef PRINT_EXPERT_PARAM
    printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
    printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
    printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
#endif

    Total_Inst_p->betaQ15_NR1 = tmp_p->betaQ15_NR1;
    Total_Inst_p->max_att_NR1 = tmp_p->max_att_NR1;
    Total_Inst_p->min_att_NR1 = tmp_p->min_att_NR1;
    Total_Inst_p->slope_NR1 = tmp_p->slope_NR1;
    Total_Inst_p->high_att_NR1 = tmp_p->high_att_NR1;
    Total_Inst_p->target_SPL_NR1 = tmp_p->target_SPL_NR1;

    Total_Inst_p->betaQ15_NR2 = tmp_p->betaQ15_NR2;
    Total_Inst_p->max_att_NR2 = tmp_p->max_att_NR2;
    Total_Inst_p->min_att_NR2 = tmp_p->min_att_NR2;
    Total_Inst_p->slope_NR2 = tmp_p->slope_NR2;
    Total_Inst_p->high_att_NR2 = tmp_p->high_att_NR2;
    Total_Inst_p->target_SPL_NR2 = tmp_p->target_SPL_NR2;

    Total_Inst_p->betaQ15_NR3 = tmp_p->betaQ15_NR3;
    Total_Inst_p->max_att_NR3 = tmp_p->max_att_NR3;
    Total_Inst_p->min_att_NR3 = tmp_p->min_att_NR3;
    Total_Inst_p->slope_NR3 = tmp_p->slope_NR3;
    Total_Inst_p->high_att_NR3 = tmp_p->high_att_NR3;
    Total_Inst_p->target_SPL_NR3 = tmp_p->target_SPL_NR3;   

    Total_Inst_p->DC_beta = tmp_p->DC_reject_beta; //;

    nsInstp->beta_low_ratio = (float)(tmp_p->ns_beta_low_ratio)/10.0;
    nsInstp->beta_high_ratio = (float)(tmp_p->ns_beta_high_ratio)/10.0;

    vadInstp->tau_s_r = (double)(tmp_p->vad_tau_s_r)/Q31_MAX; // 0.05; //
    vadInstp->tau_s_f = (double)(tmp_p->vad_tau_s_f)/Q31_MAX; //0.1; //

    double temp_gain = pow(10, (double)(tmp_p->global_gain)/20.0);
    Total_Inst_p->g_gainQ15 = (int)(temp_gain * 32767.0);

}

int aspl_NR_total_param_write_to_file(const char* file_path, aspl_NR_CONFIG* config) {

    aspl_nr_params_t tmp;

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_total_param_write_to_file :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }

    aspl_NR_expert_param_read(&tmp, (aspl_NR_CONFIG *) config_p);

    FILE* file = fopen(file_path, "wb");
    if (file != NULL) {
        fwrite(&tmp, sizeof(aspl_nr_params_t), 1, file);
        fclose(file);
    } else {
        printf("Failed to open file for writing.\n");
        return aspl_RET_FAIL;
    }

    return aspl_RET_SUCCESS;
}

int aspl_NR_total_param_set_from_file(const char* file_path, aspl_NR_CONFIG* config){

    aspl_nr_params_t tmp;

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_total_param_set_from_file :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }    

    FILE* file = fopen(file_path, "rb");
    if (file != NULL) {
        fread(&tmp, sizeof(aspl_nr_params_t), 1, file);
        fclose(file);

        aspl_NR_expert_param_write(&tmp, (aspl_NR_CONFIG *) config_p);

    } else {
        printf("Failed to open file for reading.\n");
        return aspl_RET_FAIL;
    }

    return aspl_RET_SUCCESS;
}

int aspl_NR_set(aspl_NR_CMD_E cmd, aspl_NR_CONFIG* config){
    int k;

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_set :: (aspl_NR_CONFIG *)config has not been created");
        return aspl_RET_FAIL;
    }

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
    if (Total_Inst_p == NULL) {
        printf("aspl_NR_set :: (Total_Inst_t *)Total_Inst_p has not been created");
        return aspl_RET_FAIL;
    }

    if (cmd==aspl_NR_CMD_SET_NR) {

        nsInst_t* nsInstp = Total_Inst_p->NSinst_p;
        agcInst_t* agcInstp = Total_Inst_p->agcInst_p;

        Total_Inst_p->g_nr_config.enable =  config_p->enable;
        Total_Inst_p->g_nr_config.sensitivity = config_p->sensitivity;

        if (Total_Inst_p->g_nr_config.enable == 1){
            Total_Inst_p->DC_rej_enable = 1;
            Total_Inst_p->NS_enable = 1;
            Total_Inst_p->AGC_enable = 1;
            Total_Inst_p->AGC_band_enable = 0;
        } else if (Total_Inst_p->g_nr_config.enable == 0){
            Total_Inst_p->DC_rej_enable = 0;
            Total_Inst_p->NS_enable = 0;
            Total_Inst_p->AGC_enable = 0;
            Total_Inst_p->AGC_band_enable = 0;
        } else {
            printf("g_nr_config.enable is set as wrong value\n");

            return aspl_RET_FAIL;
        }

        if (Total_Inst_p->g_nr_config.sensitivity==3){
            nsInstp->betaQ15 = Total_Inst_p->betaQ15_NR3; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Total_Inst_p->max_att_NR3; //-25;
            nsInstp->min_att = (float)Total_Inst_p->min_att_NR3; //-8;
            nsInstp->slope = (float)(Total_Inst_p->slope_NR3)/10.0; //2.0;
            nsInstp->high_att = (float)Total_Inst_p->high_att_NR3; //-15; 
            agcInstp->target_SPL = (float)Total_Inst_p->target_SPL_NR3;//78;

        } else if (Total_Inst_p->g_nr_config.sensitivity==2){
            nsInstp->betaQ15 = Total_Inst_p->betaQ15_NR2; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Total_Inst_p->max_att_NR2; //-20;
            nsInstp->min_att = (float)Total_Inst_p->min_att_NR2; //-4;
            nsInstp->slope = (float)(Total_Inst_p->slope_NR2)/10.0; //1.8;
            nsInstp->high_att = (float)Total_Inst_p->high_att_NR2; //-12; 
            agcInstp->target_SPL = (float)Total_Inst_p->target_SPL_NR2;//75;

        } else if (Total_Inst_p->g_nr_config.sensitivity==1){
            nsInstp->betaQ15 = Total_Inst_p->betaQ15_NR1; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Total_Inst_p->max_att_NR1; //-15;
            nsInstp->min_att = (float)Total_Inst_p->min_att_NR1; //-1;
            nsInstp->slope = (float)(Total_Inst_p->slope_NR1)/10.0; //1.5;
            nsInstp->high_att = (float)Total_Inst_p->high_att_NR1; //-10; 
            agcInstp->target_SPL = (float)Total_Inst_p->target_SPL_NR1;//73;
            
        } else if (Total_Inst_p->g_nr_config.sensitivity==0){

            Total_Inst_p->NS_enable = 0;

            nsInstp->betaQ15 = (int32_t)(0*32767.0);

            nsInstp->max_att = 0;
            nsInstp->min_att = 0;
            nsInstp->slope = 2;
            nsInstp->high_att = 0; 

        } else {
            printf("g_nr_config.sensitivity is set as wrong value\n");
            return aspl_RET_FAIL;
        }


        float rtemp = 1.0 / (float) (nsInstp->slope);

        for (k=0; k<nsInstp->voice_start_bin; k++) {
            float temp= nsInstp->min_att + (nsInstp->max_att/((float)nsInstp->voice_start_bin*(float)nsInstp->voice_start_bin)) * (float)((k-nsInstp->voice_start_bin)*(k-nsInstp->voice_start_bin));
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif
        }
            

        for (k=nsInstp->voice_start_bin ; k<PolyM/2+1; k++){
            float temp= nsInstp->min_att + nsInstp->high_att + (-1 * nsInstp->high_att)*powf(rtemp,((float)(k-nsInstp->voice_start_bin)/(float)(nsInstp->voice_end_bin-nsInstp->voice_start_bin)));
            
            // if (k>nsInstp->voice_end_bin){
            //     temp= temp + (float)(k-nsInstp->voice_end_bin)/(float)(PolyM/2-nsInstp->voice_end_bin)*3.0;
            // }
            
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif
        }


        agcInstp->Kp_dB = agcInstp->target_SPL - FS_SPL + Q15_dB*2;
        agcInstp->Kp=powf(10.0,(agcInstp->Kp_dB*0.05)); //3.395366161309280e+07; //131072000; //49152000; //3000; //49152000;

        // for (k=0 ; k<=nsInstp->voice_end_bin; k++){
        //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)k+nsInstp->slope)+nsInstp->min_att)/20))*(float)Q15_val);
        //         // if (k%8==0) printf("\n");
        //         // printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
        // }

        // for (k=nsInstp->voice_end_bin+1 ; k<PolyM/2+1; k++){
        //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)(k)+nsInstp->slope)+nsInstp->min_att+((nsInstp->high_att-nsInstp->min_att)*(nsInstp->slope))/((float)(PolyM/2+1- k) *0.5+nsInstp->slope*0.5))/20))*(float)Q15_val);
        //     // if (k%8==0) printf("\n");
        //     //     printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
        // }

    }

    if (cmd == aspl_NR_CMD_ENABLE){
        // aspl_NR_CONFIG * config_p = (aspl_NR_CONFIG * )data;
        Total_Inst_p->g_nr_config.enable =  config_p->enable;
        Total_Inst_p->g_nr_config.sensitivity = config_p->sensitivity;

        Total_Inst_p->DC_rej_enable = 1;
        Total_Inst_p->NS_enable = 1;
        Total_Inst_p->AGC_enable = 1;
        Total_Inst_p->AGC_band_enable = 0;

        nsInst_t* nsInstp = Total_Inst_p->NSinst_p;

        nsInstp->betaQ15 = (int32_t)(0.4*32767.0);

        nsInstp->max_att = -15;
        nsInstp->min_att = -1;
        nsInstp->slope = 1.5;
        nsInstp->high_att = -8; 

        float rtemp = 1.0 / (float) (nsInstp->slope);

        for (k=0; k<nsInstp->voice_start_bin; k++) {
            float temp= nsInstp->min_att + (nsInstp->max_att/((float)nsInstp->voice_start_bin*(float)nsInstp->voice_start_bin)) * (float)((k-nsInstp->voice_start_bin)*(k-nsInstp->voice_start_bin));
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif    
        }
            


        for (k=nsInstp->voice_start_bin ; k<PolyM/2+1; k++){
            float temp= nsInstp->min_att + nsInstp->high_att + (-1 * nsInstp->high_att)*powf(rtemp,((float)(k-nsInstp->voice_start_bin)/(float)(nsInstp->voice_end_bin-nsInstp->voice_start_bin)));
            
            // if (k>nsInstp->voice_end_bin){
            //     temp= temp + (float)(k-nsInstp->voice_end_bin)/(float)(PolyM/2-nsInstp->voice_end_bin)*3.0;
            // }
            
            (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
#ifdef PRINT_EXPERT_PARAM
            if (k%8==0) printf("\n");
                printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
#endif
        }

        // for (k=0 ; k<=nsInstp->voice_end_bin; k++){
        //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)k+nsInstp->slope)+nsInstp->min_att)/20))*(float)Q15_val);
        //         // if (k%8==0) printf("\n");
        //         // printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
        // }

        // for (k=nsInstp->voice_end_bin+1 ; k<PolyM/2+1; k++){
        //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)(k)+nsInstp->slope)+nsInstp->min_att+((nsInstp->high_att-nsInstp->min_att)*(nsInstp->slope))/((float)(PolyM/2+1- k) *0.5+nsInstp->slope*0.5))/20))*(float)Q15_val);
        // //         if (k%8==0) printf("\n");
        // //         printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
        // }        
    }

    return aspl_RET_SUCCESS;
}

int aspl_NR_process(short* data, int len, aspl_NR_CONFIG * config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_process_enables :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    } 

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
    if (Total_Inst_p == NULL) {
        printf("aspl_NR_process_enables :: (Total_Inst_t *)Total_Inst_p has not been created");
        return aspl_RET_FAIL;
    }    

    int globalgain_dB = (int32_t)(20*log10((double)(Total_Inst_p->g_gainQ15)/32767.0));

    int ret = aspl_NR_process_enables(data, len, Total_Inst_p->NS_enable, Total_Inst_p->AGC_enable, globalgain_dB, config);

    return ret;

}

int aspl_NR_process_enables(short* data, int len, int NS_enable, int AGC_enable, int globalgain_dB, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_process_enables :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    } 

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
    if (Total_Inst_p == NULL) {
        printf("aspl_NR_process_enables :: (Total_Inst_t *)Total_Inst_p has not been created");
        return aspl_RET_FAIL;
    }    

    polyInst_t* polyInstp = Total_Inst_p->polyInst_p;
    nsInst_t* nsInstp = Total_Inst_p->NSinst_p;
    agcInst_t* agcInstp = Total_Inst_p->agcInst_p;
    vadInst_t* vadInstp = Total_Inst_p->vadInst_p;

    int i, k, j;
    int j_max;

    short   *in_r;
    short   *frame_p;
    short   *refframe_p;
    short   *outframe_p;
    short   *work_buf[WORK_CHANNELS];     /* pointers to microphone inputs */
    short   *mics_in[IN_CHANNELS];     /* pointers to microphone inputs */
    short   *vad_input[MAX_VAD_CHANNELS];
    short   *inputbuf;
    short   *refbuf;
   
    in_r  = (short *)(Total_Inst_p->g_data_vad);
    for (k = 0; k < MAX_VAD_CHANNELS; k++) {
        vad_input[k] = &in_r[k*NUM_FRAMES]; /* find the frame start for each microphone */
    }  

    in_r  = (short *)(Total_Inst_p->g_WorkBuf);
    for (k = 0; k < WORK_CHANNELS; k++) {
        work_buf[k] = &in_r[k*NUM_FRAMES];   /* find the frame start for each ref channel */
    }

    in_r  = (short *)data;
    for (k = 0; k < 1; k++) {
        mics_in[k] = &in_r[k*len];	/* find the frame start for each microphone */
    }    

    if (len<=CODEC_FRM_LEGNTH_MAX){

        memcpy(&Total_Inst_p->g_codec_4mic_buf[0][CODEC_BUF_FRONT_LEGNTH],  &mics_in[0][0], sizeof(int16_t)*len);
        inputbuf = (short *) &Total_Inst_p->g_codec_4mic_buf[0][CODEC_BUF_FRONT_LEGNTH-Total_Inst_p->sample_left];

        j_max = (len+Total_Inst_p->sample_left) / NUM_FRAMES;

        Total_Inst_p->sample_left = (len+Total_Inst_p->sample_left) - (NUM_FRAMES*j_max);
    } else {
        printf("frame length not support\r\n");
        return aspl_RET_FAIL;
    }     


    for (j=0; j<j_max; j++){

        ///////////////////////////   fade in         ////////////////////////////////////////
        if (Total_Inst_p->fade_gs < -1.0) {
            Total_Inst_p->fade_gs = Total_Inst_p->r_a * Total_Inst_p->fade_gs + (1.0 - Total_Inst_p->r_a) * Total_Inst_p->fade_gc;
			float fadein = powf (10, Total_Inst_p->fade_gs*0.05);
            for (i = 0; i < NUM_FRAMES; i++) {
                inputbuf[i+j*NUM_FRAMES] = (short)((float)inputbuf[i+j*NUM_FRAMES]*fadein);
            }
        }

        ///////////////////////////   DC rejection         ////////////////////////////////////////
        if (Total_Inst_p->DC_rej_enable==1) {
            for (i = 0; i < NUM_FRAMES; i++) {
                work_buf[0][i] = (int16_t)DC_rejection(&Total_Inst_p->offset_Q15_L, inputbuf[i+j*NUM_FRAMES], Total_Inst_p->DC_beta);
            }
        } else {
            for (i = 0; i < NUM_FRAMES; i++) {
                work_buf[0][i] = inputbuf[i+j*NUM_FRAMES];
            }
        } 

        ///////////////////////////   VAD filtering & process    ////////////////////////////////////////
        ssl_filtering((vadInst_t*) vadInstp, &work_buf[0][0], vadInstp->BPF_Buf[0], &vad_input[0][0]);
        k=0;
        for (i = 0 ; i<NUM_FRAMES ; i=i+ssl_blocksize){
            ssl_vad_process((vadInst_t*) vadInstp, &vad_input[0][i], &Total_Inst_p->vad_pre[k], &Total_Inst_p->vad_min[k], &Total_Inst_p->vad_max[k], 0);
            k++;
        }

        if (NS_enable==1){
            ///////////////////////////   poly_analysis         ////////////////////////////////////////
            int temp_scale = ((int)(1)<<(Total_Inst_p->poly_scale));
            poly_analysis(polyInstp, (void*)work_buf[0], &Total_Inst_p->fft_in_mat[0][0], 0, temp_scale);

            if (Total_Inst_p->total_idx < 50) { // init period
                NS_oct_process(nsInstp, &Total_Inst_p->fft_in_mat[0][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], -1);
            } else {
                NS_oct_process(nsInstp, &Total_Inst_p->fft_in_mat[0][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], Total_Inst_p->vad_max[0]+Total_Inst_p->vad_max[1]);
            }

            ///////////////////////////   poly_synthesis         ////////////////////////////////////////
            temp_scale = ((int)(1)<<(15-Total_Inst_p->poly_scale));
            poly_synthesis(polyInstp, &Total_Inst_p->fft_out_mat[0][0], &inputbuf[j*NUM_FRAMES], 0, temp_scale);
        } else {
            memcpy(&inputbuf[j*NUM_FRAMES], work_buf[0], sizeof(short)*NUM_FRAMES);
        }

        outframe_p = &inputbuf[j*NUM_FRAMES];
        refframe_p = (void*)work_buf[0];
        if (AGC_enable==1){
            AGC_total_w_ref(agcInstp, outframe_p, refframe_p, outframe_p,   (Total_Inst_p->vad_min[0]+(Total_Inst_p->vad_min[1]<<1)), Total_Inst_p->vad_max[0]+Total_Inst_p->vad_max[1]);
        }

        double temp_gain = pow(10, (double)(globalgain_dB)/20.0);
        int g_gainQ15 = (int)(temp_gain * 32767.0);

        int temp;
        outframe_p = &inputbuf[j*NUM_FRAMES];
        for (i = 0; i < NUM_FRAMES; i++) {
            temp = 16384 + ((int32_t)outframe_p[i] * (int32_t)(g_gainQ15));
            outframe_p[i]= temp>>15;
        }            

        Total_Inst_p->total_idx++;
        if (Total_Inst_p->total_idx >= 50){
            Total_Inst_p->total_idx=50;
        }
    }

    if (len<=CODEC_FRM_LEGNTH_MAX){
        for (i=0; i<len; i++) {
            mics_in[0][i] = Total_Inst_p->g_codec_4mic_buf[0][i];
        }

        for (i=0; i<CODEC_BUF_FRONT_LEGNTH; i++) {
            Total_Inst_p->g_codec_4mic_buf[0][i] = Total_Inst_p->g_codec_4mic_buf[0][len+i];
        }
    }   

    return aspl_RET_SUCCESS;
}


int aspl_NR_process_2mic(short* data, int len, int Beam1, int Beam_auto, double * pDoA, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_NR_process_2mic :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    } 

    Total_Inst_t * Total_Inst_p = (Total_Inst_t * )config_p->Total_Inst_p;
    if (Total_Inst_p == NULL) {
        printf("aspl_NR_process_2mic :: (Total_Inst_t *)Total_Inst_p has not been created");
        return aspl_RET_FAIL;
    }    

    polyInst_t* polyInstp = Total_Inst_p->polyInst_p;
    nsInst_t* nsInstp = Total_Inst_p->NSinst_p;
    agcInst_t* agcInstp = Total_Inst_p->agcInst_p;
    vadInst_t* vadInstp = Total_Inst_p->vadInst_p;

    bfInst_t* bfInstp = Total_Inst_p->bfInst_p;
    sslInst_t* sslInstp = Total_Inst_p->sslInst_p;


    int i, k, j, m, n;
    int j_max;

    short   *in_r;
    short   *frame_p;
    short   *refframe_p;
    short   *outframe_p;
    short   *work_buf[WORK_CHANNELS_2MIC];     /* pointers to microphone inputs */
    short   *mics_in[IN_CHANNELS_2MIC];     /* pointers to microphone inputs */
    short   *vad_input[IN_CHANNELS_2MIC];
    short   *inputbuf[IN_CHANNELS_2MIC];    

    in_r  = (short *)(Total_Inst_p->g_data_vad);
    for (k = 0; k < IN_CHANNELS_2MIC; k++) {
        vad_input[k] = &in_r[k*(NUM_FRAMES+NUM_FRAMES/2)]; /* find the frame start for each microphone */
    }  

    in_r  = (short *)(Total_Inst_p->g_WorkBuf);
    for (k = 0; k < WORK_CHANNELS_2MIC; k++) {
        work_buf[k] = &in_r[k*NUM_FRAMES];   /* find the frame start for each ref channel */
    }

    in_r  = (short *)data;
    for (k = 0; k < IN_CHANNELS_2MIC; k++) {
        mics_in[k] = &in_r[k*len];	/* find the frame start for each microphone */
    }    

    if (len<=CODEC_FRM_LEGNTH_MAX){

        for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
            memcpy(&Total_Inst_p->g_codec_4mic_buf[k][CODEC_BUF_FRONT_LEGNTH],  &mics_in[k][0], sizeof(int16_t)*len);
            inputbuf[k] = (short *) &Total_Inst_p->g_codec_4mic_buf[k][CODEC_BUF_FRONT_LEGNTH-Total_Inst_p->sample_left];
        }

        j_max = (len+Total_Inst_p->sample_left) / NUM_FRAMES;

        Total_Inst_p->sample_left = (len+Total_Inst_p->sample_left) - (NUM_FRAMES*j_max);
    } else {
        printf("frame length not support\r\n");
        return aspl_RET_FAIL;
    }     


    for (j=0; j<j_max; j++){

        ///////////////////////////   DC rejection         ////////////////////////////////////////
        if (Total_Inst_p->DC_rej_enable==1) {
            for (k=0; k<IN_CHANNELS_2MIC; k++){
                for (i = 0; i < NUM_FRAMES; i++) {
                    work_buf[k][i] = (int16_t)DC_rejection(&Total_Inst_p->offset_Q15_multi[k], inputbuf[k][i+j*NUM_FRAMES], Total_Inst_p->DC_beta);
                }
            }

        } else {
            for (k=0; k<IN_CHANNELS_2MIC; k++){
                for (i = 0; i < NUM_FRAMES; i++) {
                    work_buf[k][i] = inputbuf[k][i+j*NUM_FRAMES];
                }
            }
        } 

        ///////////////////////////   VAD filtering & process    ////////////////////////////////////////
        for (m=0; m<IN_CHANNELS_2MIC; m++){
            ssl_filtering((vadInst_t*) vadInstp, &work_buf[m][0], vadInstp->BPF_Buf[m], &vad_input[m][NUM_FRAMES/2]);
        }
        
        k=0;
        short vad_sum=0;
        for (m = ssl_blocksize/2 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/2){
            ssl_vad_process((vadInst_t*) vadInstp, &vad_input[0][m], &Total_Inst_p->vad_pre[k], &Total_Inst_p->vad_min[k], &Total_Inst_p->vad_max[k], 0);
            vad_sum += Total_Inst_p->vad_min[k];
            k++;
        }

        k=0;
        for (m = ssl_blocksize/2 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/2){
            ssl_core_process_2ch((sslInst_t*) sslInstp, &vad_input[0][m], &vad_input[1][m], &Total_Inst_p->DoA_mean, mode_normal, vad_sum);
            k++;

            if ((Total_Inst_p->DoA_mean != -1)){
                Total_Inst_p->g_DoA = Total_Inst_p->DoA_mean;
                if (Total_Inst_p->g_DoA < -60.0) Total_Inst_p->g_Beamno = 0;
                else if (Total_Inst_p->g_DoA < -50.0) Total_Inst_p->g_Beamno = 1;
                else if (Total_Inst_p->g_DoA < -35.0) Total_Inst_p->g_Beamno = 2;
                else if (Total_Inst_p->g_DoA < -15.0) Total_Inst_p->g_Beamno = 3;
                else if (Total_Inst_p->g_DoA < 15.0) Total_Inst_p->g_Beamno = 4;
                else if (Total_Inst_p->g_DoA < 35.0) Total_Inst_p->g_Beamno = 5;
                else if (Total_Inst_p->g_DoA < 50.0) Total_Inst_p->g_Beamno = 6;
                else if (Total_Inst_p->g_DoA < 60.0) Total_Inst_p->g_Beamno = 7;
                else Total_Inst_p->g_Beamno = 8;

                Total_Inst_p->no_DoA_cnt = 0;
                printf("\n*************************************************************************\r\n");
                printf("DoA = %.2f degree, Beam no = %d \r\n\n", Total_Inst_p->g_DoA, Total_Inst_p->g_Beamno);     
                printf("*************************************************************************\r\n\n");		   
            } else {
                Total_Inst_p->no_DoA_cnt++;
                if (Total_Inst_p->no_DoA_cnt>(60*3*2)){
                    Total_Inst_p->g_DoA = -1;
                    Total_Inst_p->no_DoA_cnt=(60*3*2)+1;
                }
            }
        }
        
        for (m=0; m<IN_CHANNELS_2MIC; m++){
            frame_p = (void*)&vad_input[m][NUM_FRAMES];
            refframe_p = (void*)&vad_input[m][0];
            memcpy(refframe_p, frame_p, (NUM_FRAMES/2)*sizeof(int16_t));
        }           

        if (Beam_auto==1){
            if (Total_Inst_p->g_DoA != -1){
                Beam1 = Total_Inst_p->g_Beamno;
            }
        }        

        ///////////////////////////   poly_analysis         ////////////////////////////////////////
        int temp_scale = ((int)(1)<<(Total_Inst_p->poly_scale));

        for (m=0; m<IN_CHANNELS_2MIC; m++){
            poly_analysis(polyInstp, (void*)work_buf[m], &Total_Inst_p->fft_in_mat[m][0], m, temp_scale);
        }

        if (Total_Inst_p->bf_enable==1) {

            BF_process(bfInstp);
            BF_process_rear(bfInstp);
            
            for (n=0 ; n<BeamN; n++){
                m=PolyL*2;			
                for (k=1 ; k<PolyM/2+1 ; k++){
                    BF_delay(bfInstp, &Total_Inst_p->fft_bf_mat[n][m], &Total_Inst_p->fft_bf_mat[n][m], &bf_d_buf_g[n][k][0], adaptive_BF_delay+1);
                    m=m+PolyL*2;
                }
            }
            
            for (n=0 ; n<BeamN; n++){
                m=PolyL*2;
                for (k=1 ; k<27 ; k++){
                    BF_adaptive_Proc(bfInstp, &Total_Inst_p->fft_bf_rear_mat[n][m], &Total_Inst_p->fft_bf_mat[n][m], &Total_Inst_p->fft_bf_mat[n][m], k, n);
                    m=m+PolyL*2;
                }
            }	
        }            

        if (Total_Inst_p->NS_enable==1){

            if (Total_Inst_p->total_idx < 50) { // init period
                if (Total_Inst_p->bf_enable==1) {
                    NS_oct_process(nsInstp, &Total_Inst_p->fft_bf_mat[Beam1][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], -1);
                } else {
                    NS_oct_process(nsInstp, &Total_Inst_p->fft_in_mat[0][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], -1);
                }                
            } else {
                if (Total_Inst_p->bf_enable==1) {
                    NS_oct_process(nsInstp, &Total_Inst_p->fft_bf_mat[Beam1][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], Total_Inst_p->vad_max[0]+Total_Inst_p->vad_max[1]);                    
                } else {
                    NS_oct_process(nsInstp, &Total_Inst_p->fft_in_mat[0][0], &Total_Inst_p->fft_ns_buf_mat[0], &Total_Inst_p->fft_out_mat[0][0], Total_Inst_p->vad_max[0]+Total_Inst_p->vad_max[1]);
                }
                
            }
        } 
        
        ///////////////////////////   poly_synthesis         ////////////////////////////////////////
        temp_scale = ((int)(1)<<(15-Total_Inst_p->poly_scale));

        outframe_p =  (void*)&inputbuf[0][j*NUM_FRAMES];
        if (Total_Inst_p->bf_enable==1) {
            if (Total_Inst_p->NS_enable==1){
                poly_synthesis(polyInstp, &Total_Inst_p->fft_out_mat[0][0], outframe_p, 0, temp_scale);
            } else {
                poly_synthesis(polyInstp, &Total_Inst_p->fft_bf_mat[Beam1][0], outframe_p, 0, temp_scale);
            }
        } else {
            if (Total_Inst_p->NS_enable==1){
                poly_synthesis(polyInstp, &Total_Inst_p->fft_out_mat[0][0], outframe_p, 0, temp_scale);
            } else {
                poly_synthesis(polyInstp, &Total_Inst_p->fft_in_mat[0][0], outframe_p, 0, temp_scale);
            }            
            
        }    

        outframe_p = &inputbuf[0][j*NUM_FRAMES];
        refframe_p = (void*)work_buf[0];
        if (Total_Inst_p->AGC_enable==1){
            AGC_total_w_ref(agcInstp, outframe_p, refframe_p, outframe_p,   (Total_Inst_p->vad_min[0]+(Total_Inst_p->vad_min[1]<<1)), Total_Inst_p->vad_max[0]+Total_Inst_p->vad_max[1]);
        }

        int temp;
        outframe_p = &inputbuf[0][j*NUM_FRAMES];
        for (i = 0; i < NUM_FRAMES; i++) {
            temp = 16384 + ((int32_t)outframe_p[i] * (int32_t)(Total_Inst_p->g_gainQ15));
            outframe_p[i]= temp>>15;
        }            

        Total_Inst_p->total_idx++;
        if (Total_Inst_p->total_idx >= 50){
            Total_Inst_p->total_idx=50;
        }
    }

    if (len<=CODEC_FRM_LEGNTH_MAX){
        for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
            for (i=0; i<len; i++) {
                mics_in[k][i] = Total_Inst_p->g_codec_4mic_buf[k][i];
            }

            for (i=0; i<CODEC_BUF_FRONT_LEGNTH; i++) {
                Total_Inst_p->g_codec_4mic_buf[k][i] = Total_Inst_p->g_codec_4mic_buf[k][len+i];
            }            
        }



    }   

    return aspl_RET_SUCCESS;
}

int aspl_AEC_create(aspl_NR_CONFIG* config){
    int i;

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_create :: (aspl_NR_CONFIG *)config is NULL\r\n");
        return aspl_RET_FAIL;
    }

    for (i=0; i<config_p->aec_Mic_N  ; i++){
        void * aecInst_p=(void *)sysAECCreate(i);    
        if (aecInst_p == NULL) {
            printf("aspl_AEC_create :: (aspl_NR_CONFIG *)config is NULL\r\n");
            return aspl_RET_FAIL;
        }    
        config_p->aecInst_p[i] = aecInst_p;
        printf("aspl_AEC_create :: AEC create done. config_p->aecInst_p[%d] = %p\r\n", i, config_p->aecInst_p[i]);
    }

    config_p->offset_Q15_L = 0;
    config_p->offset_Q15_R = 0;
    config_p->offset_Q15_ref = 0;

    config_p->AEC_globaldelay[0] = -10;
    config_p->AEC_filter_len[0] = 600;

    config_p->AEC_globaldelay[1] = -10;
    config_p->AEC_filter_len[1] = 600;

    config_p->AEC_filter_updated = 0;
    config_p->AEC_filter_loaded = 0;

    

    return aspl_RET_SUCCESS;    
}

int aspl_AEC_process_2ch(int16_t* data, int16_t* ref, int len, int aec_delay, float micscaledB, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_process_2ch :: (aspl_NR_CONFIG *)config is NULL\r\n");
        return aspl_RET_FAIL;
    } 

    // Total_Inst_t* Total_Instp = (void *)&Sys_Total_Inst;
    aecInst_t* aecInstp[2];
    aecInstp[0] = config_p->aecInst_p[0];
    aecInstp[1] = config_p->aecInst_p[1];

    int16_t   *mics_in[2];     /* pointers to microphone inputs */
	int16_t   *refs_in;     /* pointers to microphone inputs */
    int16_t   *in_r;

    refs_in = (int16_t *)ref;

    in_r  = (short *)data;
    for (int k = 0; k < IN_CHANNELS_2MIC; k++) {
        mics_in[k] = &in_r[k*len];	/* find the frame start for each microphone */
    }

    ///////////////////////////   DC rejection         ////////////////////////////////////////
    for (int i = 0; i < len; i++) {
        mics_in[0][i] = (int16_t)DC_rejection(&config_p->offset_Q15_L, mics_in[0][i], 5);
        mics_in[1][i] = (int16_t)DC_rejection(&config_p->offset_Q15_R, mics_in[1][i], 5);
        refs_in[i] = (int16_t)DC_rejection(&config_p->offset_Q15_ref, refs_in[i], 5);
    }
    ///////////////////////////   DC rejection         ////////////////////////////////////////

    AEC_single_Proc(aecInstp[0], &refs_in[0], &mics_in[0][0], &mics_in[0][0], len, aec_delay, micscaledB);
    AEC_single_Proc(aecInstp[1], &refs_in[0], &mics_in[1][0], &mics_in[1][0], len, aec_delay, micscaledB);
    
    return aspl_RET_SUCCESS;
}

int aspl_AEC_process_single(int16_t* data, int16_t* ref, int len, int aec_delay, float micscaledB, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_process_single_filt :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    } 

    // Total_Inst_t* Total_Instp = (void *)&Sys_Total_Inst;
    aecInst_t* aecInstp = config_p->aecInst_p[0]; //Sys_Total_Inst.aecInst_p;
    int16_t   *mics_in;     /* pointers to microphone inputs */
	int16_t   *refs_in;     /* pointers to microphone inputs */

    mics_in =(int16_t *)data;
    refs_in = (int16_t *)ref;

    ///////////////////////////   DC rejection         ////////////////////////////////////////
    for (int i = 0; i < len; i++) {
        mics_in[i] = (int16_t)DC_rejection(&config_p->offset_Q15_L, mics_in[i], 5);
        refs_in[i] = (int16_t)DC_rejection(&config_p->offset_Q15_ref, refs_in[i], 5);
    }
    ///////////////////////////   DC rejection         ////////////////////////////////////////
    AEC_single_Proc(aecInstp, &refs_in[0], &mics_in[0], &mics_in[0], len, aec_delay, micscaledB);
    
    return aspl_RET_SUCCESS;
}
