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

#include "./ccode/sys.h"
#include "./ccode/polyphase.h"
#include "./ccode/beamforming.h"
#include "./ccode/noise_supression.h"
#include "./ccode/echo_canceller.h"
#include "./ccode/autogaincontrol.h"
#include "./ccode/ssl_vad.h"
#include "./ccode/ssl_core.h"
#include "./ccode/time_constant.h"
#include "./ccode/dB_scale.h"
#include "./ccode/audio_utils.h"
#include "./ccode/debug_file.h"
#include "./ccode/level_mon.h"
#include "./include/aspl_nr.h"

// Define version information
#define LIBRARY_VERSION "0.3.0"
#define RELEASE_DATE "2024-12-02"
#define RELEASE_STATUS "MarkT 2mic VAD + BF + NS + AGC release"

Total_Inst_t Sys_Total_Inst;

int16_t* g_codec_buf=NULL;
int16_t* g_codec_4mic_buf[MULTI_INPUT_CHANNELS+REF_CHANNELS]={NULL, NULL, NULL};
int16_t* g_data_vad=NULL;
char * g_WorkBuf=NULL;

static int total_idx = 0;

int offset_Q15_L=0, offset_Q15_R=0;
int offset_Q15_multi[6]={0, 0, 0, 0, 0, 0};
int sample_left = 0;

double DoA_mean=0.0;
int g_Beamno=0;
double g_DoA=-1.0;
int no_DoA_cnt=0;

short	  vad_pre[8];
short	  vad_min[8];
short	  vad_max[8];

int count_pre=0;
int count_post=0;

float fade_gs = -30.0;
float fade_gc = 0.0;
float r_a = 0.7;

int32_t fft_in_mat[MicN][PolyM*PolyL*2];
int32_t fft_ref_mat[RefN][PolyM*PolyL*2];
int32_t fft_out_mat[BeamN][PolyM*PolyL*2];
int32_t fft_ns_buf_mat[PolyM*(PolyL+Ma_size_max-1)];

int32_t fft_bf_mat[BeamN][PolyM*PolyL*2];
int32_t fft_bf_rear_mat[BeamN][PolyM*PolyL*2];

aspl_NR_CONFIG g_nr_config;

void aspl_print_ver(){
    printf("ASPL NR Library Version: %s, Release Date: %s, Status: %s\n", LIBRARY_VERSION, RELEASE_DATE, RELEASE_STATUS);
}

// API to return version information
const char* aspl_getVersionInfo() {
    static char versionInfo[100];
    snprintf(versionInfo, sizeof(versionInfo), "Version: %s, Release Date: %s, Status: %s", LIBRARY_VERSION, RELEASE_DATE, RELEASE_STATUS);
    return versionInfo;
}

int aspl_NR_create(void* data){
    int i;

    aspl_print_ver();

    if (g_WorkBuf != NULL) {
        free(g_WorkBuf);
        g_WorkBuf = NULL;
    }

    if (g_data_vad != NULL) {
        free(g_data_vad);
        g_data_vad = NULL;
    }

    if (g_codec_buf != NULL) {
        free(g_codec_buf);
        g_codec_buf = NULL;
    }        

    for (int k=0; k<(MULTI_INPUT_CHANNELS+REF_CHANNELS); k++){
        if (g_codec_4mic_buf[k] != NULL) {
            free(g_codec_4mic_buf[k]);
            g_codec_4mic_buf[k] = NULL;
        }   
    }

    // debug_matlab_close();

    Poly_DeInit();
    ssl_vad_DeInit();
    AGC_DeInit();
    Mon_DeInit();

    aspl_NR_CONFIG * config_p = (aspl_NR_CONFIG * )data;

    void * polyInst_p = (void *)sysPolyCreate();
    void * NSinst_p=(void *)sysNSCreate();
    void * agcInst_p = (void *)sysAGCCreate();
    void * vadInst_p=(void *)sysSSLVADCreate();
    void * aecInst_p=(void *)sysAECCreate(NUM_FRAMES);
    void * MonInst_p = (void *)sysMonCreate();

    // debug_matlab_open();

    Sys_Total_Inst.polyInst_p = polyInst_p;
    Sys_Total_Inst.agcInst_p = agcInst_p;
    Sys_Total_Inst.NSinst_p = NSinst_p;
    Sys_Total_Inst.vadInst_p = vadInst_p;
    Sys_Total_Inst.aecInst_p = aecInst_p;
    Sys_Total_Inst.MonInst_p = MonInst_p;

    Sys_Total_Inst.DC_rej_enable = 1;
    Sys_Total_Inst.NS_enable = 1;
    Sys_Total_Inst.AGC_enable = 1;
    Sys_Total_Inst.AGC_band_enable = 0;

    Sys_Total_Inst.DC_beta = 5;
    Sys_Total_Inst.poly_scale = 8;
    Sys_Total_Inst.g_gainQ15 = (int)(0.0 * 32767.0);

    Sys_Total_Inst.tuning_delay = 1500;

    void * Total_Inst_p = &Sys_Total_Inst;

    int WorkBufSize = NUM_FRAMES * (WORK_CHANNELS) * NUM_BYTE_PER_SAMPLE;
    int CodecBufSize = CODEC_BUF_LENGTH * NUM_BYTE_PER_SAMPLE;


    for (i=0; i<(MULTI_INPUT_CHANNELS+REF_CHANNELS) ; i++){
        g_codec_4mic_buf[i] = (int16_t *) malloc(CodecBufSize);
        if (NULL == g_codec_4mic_buf[i])
        {
            printf("g_WorkBuf_4mic malloc ....failed\n");
            return aspl_RET_FAIL_MALLOC;
        }
    }

    g_codec_buf = (int16_t *) malloc(CodecBufSize);
    if (NULL == g_codec_buf)
    {
        printf("g_WorkBuf malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    g_WorkBuf = (char *) malloc(WorkBufSize);
    if (NULL == g_WorkBuf)
    {
        printf("g_WorkBuf malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    g_data_vad = (int16_t *)malloc(NUM_FRAMES * 4 * sizeof(int16_t));  
    if (NULL == g_data_vad)
    {
        printf("g_data_vad malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    for (i=0; i<6; i++){
        vad_pre[i]=0;
        vad_min[i]=0;
        vad_max[i]=0;
    }

#ifdef CALC_SPL
    double cali_factor_mic = Q15_TO_DOUBLE(1.0);
    float tau_slow = 0.5;
    beta_slow = exp(-1.0/((float)tau_slow * (float)SAMPLING_FREQ));

    float tau_fast = 0.1;
    beta_fast = exp(-1.0/((float)tau_fast * (float)SAMPLING_FREQ));    
#endif

    total_idx = 0;
    fade_gs = -50.0;

    // strncpy(g_nr_config.tuning_file_path, config_p->tuning_file_path, sizeof(g_nr_config.tuning_file_path));
    // g_nr_config.tuning_file_path[sizeof(g_nr_config.tuning_file_path) - 1] = '\0';

    // if (access(g_nr_config.tuning_file_path, F_OK) != -1) {
    //     // The file exists
    // } else {

    //     printf("The param file %s does not exist.\n", g_nr_config.tuning_file_path);

    //     return aspl_RET_FAIL;
    // }

    // printf("ASPL default param read from %s \r\n", g_nr_config.tuning_file_path);
    // aspl_NR_total_param_set_from_file(g_nr_config.tuning_file_path);   

    // aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)config_p);    

    printf("aspl_NR_create() initialized done.\r\n");

    return aspl_RET_SUCCESS;
}

int aspl_NR_create_2mic(void* data){
    int i;

    aspl_print_ver();

    if (g_WorkBuf != NULL) {
        free(g_WorkBuf);
        g_WorkBuf = NULL;
    }

    if (g_data_vad != NULL) {
        free(g_data_vad);
        g_data_vad = NULL;
    }


    for (i=0; i<(IN_CHANNELS_2MIC+REF_CHANNELS) ; i++){
        if (g_codec_4mic_buf[i] != NULL)
        {
            free(g_codec_4mic_buf[i] );
            g_codec_4mic_buf[i] = NULL;
        }
    }

    if (g_codec_buf != NULL) {
        free(g_codec_buf);
        g_codec_buf = NULL;
    }      

    Poly_DeInit();
    ssl_core_DeInit();
    Mon_DeInit();

    aspl_NR_CONFIG * config_p = (aspl_NR_CONFIG * )data;

    void * polyInst_p = (void *)sysPolyCreate();
    void * bfInst_p = (void *)sysBFCreate();
    void * vadInst_p=(void *)sysSSLVADCreate();    
    void * sslInst_p =  (void *)sysSSLCORECreate();
    void * NSinst_p=(void *)sysNSCreate();
    void * agcInst_p = (void *)sysAGCCreate();
    void * aecInst_p=(void *)sysAEC_multi_Create();
    void * MonInst_p = (void *)sysMonCreate();

    Sys_Total_Inst.polyInst_p = polyInst_p;
    Sys_Total_Inst.bfInst_p = bfInst_p;
    Sys_Total_Inst.vadInst_p = vadInst_p;    
    Sys_Total_Inst.sslInst_p = sslInst_p;
    Sys_Total_Inst.NSinst_p = NSinst_p;
    Sys_Total_Inst.agcInst_p = agcInst_p;
    Sys_Total_Inst.aecInst_p = aecInst_p;
    Sys_Total_Inst.MonInst_p = MonInst_p;    

    Sys_Total_Inst.poly_scale = 12;

    Sys_Total_Inst.DC_rej_enable = 1;
    Sys_Total_Inst.DC_beta = 8;
    Sys_Total_Inst.bf_enable = 1;
    Sys_Total_Inst.NS_enable = 1;
    Sys_Total_Inst.AGC_enable = 0;
    Sys_Total_Inst.g_gainQ15 = (int)(0.0 * 32767.0);

    Sys_Total_Inst.tuning_delay = 1130;
    
    void * Total_Inst_p = &Sys_Total_Inst;

    int WorkBufSize = NUM_FRAMES * (WORK_CHANNELS_2MIC) * NUM_BYTE_PER_SAMPLE;
    int CodecBufSize = CODEC_BUF_LENGTH * NUM_BYTE_PER_SAMPLE;

    for (i=0; i<(IN_CHANNELS_2MIC+REF_CHANNELS) ; i++){
        g_codec_4mic_buf[i] = (int16_t *) malloc(CodecBufSize);
        if (NULL == g_codec_4mic_buf[i])
        {
            printf("g_WorkBuf_4mic malloc ....failed\n");
            return aspl_RET_FAIL_MALLOC;
        }
    }

    g_WorkBuf = (char *) malloc(WorkBufSize);
    if (NULL == g_WorkBuf)
    {
        printf("g_WorkBuf malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    memset(g_WorkBuf, 0, WorkBufSize);

    g_data_vad = (int16_t *)malloc((NUM_FRAMES+NUM_FRAMES/2) * IN_CHANNELS_2MIC * sizeof(int16_t));  
    if (NULL == g_data_vad)
    {
        printf("g_data_vad malloc ....failed\n");
        return aspl_RET_FAIL_MALLOC;
    }

    memset(g_data_vad, 0, (NUM_FRAMES+NUM_FRAMES/2) * IN_CHANNELS_2MIC * sizeof(int16_t));

    for (i=0; i<6; i++){
        vad_pre[i]=0;
        vad_min[i]=0;
        vad_max[i]=0;
    }

    total_idx = 0;
    fade_gs = -50.0;

    // strncpy(g_nr_config.tuning_file_path, config_p->tuning_file_path, sizeof(g_nr_config.tuning_file_path));
    // g_nr_config.tuning_file_path[sizeof(g_nr_config.tuning_file_path) - 1] = '\0';

    // if (access(g_nr_config.tuning_file_path, F_OK) != -1) {
    //     // The file exists
    // } else {

    //     printf("The param file %s does not exist.\n", g_nr_config.tuning_file_path);

    //     return aspl_RET_FAIL;
    // }

    // printf("ASPL default param read from %s \r\n", g_nr_config.tuning_file_path);
    // aspl_NR_total_param_set_from_file(g_nr_config.tuning_file_path);   

    // aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)config_p);     

    DoA_mean = 0.0;
    no_DoA_cnt = -1;    
    g_Beamno = 0;

    printf("aspl_NR_create_2mic() initialized done.\r\n");
    
    return aspl_RET_SUCCESS;

}

int aspl_NR_destroy(void){  

    if (g_WorkBuf != NULL) {
        free(g_WorkBuf);
        g_WorkBuf = NULL;
    }

    if (g_data_vad != NULL) {
        free(g_data_vad);
        g_data_vad = NULL;
    }

    for (int i=0; i<(MULTI_INPUT_CHANNELS+REF_CHANNELS) ; i++){
        if (g_codec_4mic_buf[i] != NULL)
        {
            free(g_codec_4mic_buf[i] );
            g_codec_4mic_buf[i] = NULL;
        }
    }

    if (g_codec_buf != NULL) {
        free(g_codec_buf);
        g_codec_buf = NULL;
    }      

    Poly_DeInit();
    ssl_vad_DeInit();
    ssl_core_DeInit();
    AGC_DeInit();
    NS_DeInit();
    Mon_DeInit();

    // debug_matlab_close();
    return aspl_RET_SUCCESS;
}

int aspl_NR_set(aspl_NR_CMD_E cmd, void* data){
    int k;

    if (cmd==aspl_NR_CMD_SET_NR) {
        aspl_NR_CONFIG * config_p = (aspl_NR_CONFIG * )data;

        nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
        agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;

        g_nr_config.enable =  config_p->enable;
        g_nr_config.sensitivity = config_p->sensitivity;

        if (g_nr_config.enable == 1){
            Sys_Total_Inst.DC_rej_enable = 1;
            Sys_Total_Inst.NS_enable = 1;
            Sys_Total_Inst.AGC_enable = 1;
            Sys_Total_Inst.AGC_band_enable = 0;
        } else if (g_nr_config.enable == 0){
            Sys_Total_Inst.DC_rej_enable = 0;
            Sys_Total_Inst.NS_enable = 0;
            Sys_Total_Inst.AGC_enable = 0;
            Sys_Total_Inst.AGC_band_enable = 0;
        } else {
            printf("g_nr_config.enable is set as wrong value\n");

            return aspl_RET_FAIL;
        }

        if (g_nr_config.sensitivity==3){
            nsInstp->betaQ15 = Sys_Total_Inst.betaQ15_NR3; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Sys_Total_Inst.max_att_NR3; //-25;
            nsInstp->min_att = (float)Sys_Total_Inst.min_att_NR3; //-8;
            nsInstp->slope = (float)(Sys_Total_Inst.slope_NR3)/10.0; //2.0;
            nsInstp->high_att = (float)Sys_Total_Inst.high_att_NR3; //-15; 
            agcInstp->target_SPL = (float)Sys_Total_Inst.target_SPL_NR3;//78;

        } else if (g_nr_config.sensitivity==2){
            nsInstp->betaQ15 = Sys_Total_Inst.betaQ15_NR2; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Sys_Total_Inst.max_att_NR2; //-20;
            nsInstp->min_att = (float)Sys_Total_Inst.min_att_NR2; //-4;
            nsInstp->slope = (float)(Sys_Total_Inst.slope_NR2)/10.0; //1.8;
            nsInstp->high_att = (float)Sys_Total_Inst.high_att_NR2; //-12; 
            agcInstp->target_SPL = (float)Sys_Total_Inst.target_SPL_NR2;//75;

        } else if (g_nr_config.sensitivity==1){
            nsInstp->betaQ15 = Sys_Total_Inst.betaQ15_NR1; //(int32_t)(0.4*32767.0);
            nsInstp->max_att = (float)Sys_Total_Inst.max_att_NR1; //-15;
            nsInstp->min_att = (float)Sys_Total_Inst.min_att_NR1; //-1;
            nsInstp->slope = (float)(Sys_Total_Inst.slope_NR1)/10.0; //1.5;
            nsInstp->high_att = (float)Sys_Total_Inst.high_att_NR1; //-10; 
            agcInstp->target_SPL = (float)Sys_Total_Inst.target_SPL_NR1;//73;
            
        } else if (g_nr_config.sensitivity==0){

            Sys_Total_Inst.NS_enable = 0;

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
        aspl_NR_CONFIG * config_p = (aspl_NR_CONFIG * )data;
        g_nr_config.enable =  config_p->enable;
        g_nr_config.sensitivity = config_p->sensitivity;

        Sys_Total_Inst.DC_rej_enable = 1;
        Sys_Total_Inst.NS_enable = 1;
        Sys_Total_Inst.AGC_enable = 1;
        Sys_Total_Inst.AGC_band_enable = 0;

        nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;

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


int aspl_AEC_process_single(int16_t* data, int16_t* ref, int len, int aec_delay, float micscaledB){

    Total_Inst_t* Total_Instp = (void *)&Sys_Total_Inst;
    aecInst_t* aecInstp = Sys_Total_Inst.aecInst_p;
    int16_t   *mics_in;     /* pointers to microphone inputs */
	int16_t   *refs_in;     /* pointers to microphone inputs */

    mics_in =(int16_t *)data;
    refs_in = (int16_t *)ref;

    ///////////////////////////   DC rejection         ////////////////////////////////////////
    if (Sys_Total_Inst.DC_rej_enable==1) {
        for (int i = 0; i < len; i++) {
            mics_in[i] = (int16_t)DC_rejection(&offset_Q15_L, mics_in[i], Sys_Total_Inst.DC_beta);
            if (ref != NULL ) {
                refs_in[i] = (int16_t)DC_rejection(&offset_Q15_R, refs_in[i], Sys_Total_Inst.DC_beta);
            }
        }
    } else {
        for (int i = 0; i < len; i++) {
            mics_in[i] = mics_in[i];
            if (ref != NULL ) {
                refs_in[i] = refs_in[i];
            }
        }
    }
    ///////////////////////////   DC rejection         ////////////////////////////////////////

    if (ref == NULL){
        AEC_single_Proc_matlab(aecInstp, NULL, &mics_in[0], &mics_in[0], len, aec_delay, 1, 0, micscaledB);
    } else {
        AEC_single_Proc_matlab(aecInstp, &refs_in[0], &mics_in[0], &mics_in[0], len, aec_delay, 1, 0, micscaledB);
    }
    

    return aspl_RET_SUCCESS;
}

int aspl_NR_process_2mic(short* data, int len, int Beam1, int Beam_auto, double * pDoA){    

    int i, k, j, m, n;
    int j_max;
    short   *in_r;
    short   *frame_p;
    short   *outframe_p;
    short   *refframe_p;
    short   *mics_in[IN_CHANNELS_2MIC];     /* pointers to microphone inputs */
	short   *refs_in[REF_CHANNELS];     /* pointers to microphone inputs */

    short   *vad_input[IN_CHANNELS_2MIC];
    short   *inputbuf[IN_CHANNELS_2MIC];    
    short   *work_buf[WORK_CHANNELS_2MIC];     /* pointers to microphone inputs */    

    polyInst_t* polyInstp = Sys_Total_Inst.polyInst_p;
    bfInst_t* bfInstp = Sys_Total_Inst.bfInst_p;
    vadInst_t* vadInstp = Sys_Total_Inst.vadInst_p;
    sslInst_t* sslInstp = Sys_Total_Inst.sslInst_p;
    nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
    aecInst_t* aecInstp = Sys_Total_Inst.aecInst_p;
    agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;

    in_r  = (short *)(g_data_vad);
    for (k = 0; k < IN_CHANNELS_2MIC; k++) {
        vad_input[k] = &in_r[k*(NUM_FRAMES+NUM_FRAMES/2)]; /* find the frame start for each microphone */
    }  

    in_r  = (short *)g_WorkBuf;
    for (k = 0; k < WORK_CHANNELS_2MIC; k++) {
        work_buf[k] = &in_r[k*NUM_FRAMES];   /* find the frame start for each ref channel */
    }

    in_r  = (short *)data;
    for (k = 0; k < IN_CHANNELS_2MIC; k++) {
        mics_in[k] = &in_r[k*len];	/* find the frame start for each microphone */
    }

    if (len==1024){
        j_max = 2;
        for (k = 0; k < IN_CHANNELS_2MIC; k++) {
            inputbuf[k] = (short *) &mics_in[k][0];
        }        
    } else if (len==1600){
        for (k = 0; k < IN_CHANNELS_2MIC; k++) {
            memcpy(&g_codec_4mic_buf[k][CODEC_FRM_LEGNTH], &mics_in[k][0], sizeof(int16_t)*CODEC_FRM_LEGNTH);
            inputbuf[k] = (short *) &g_codec_4mic_buf[k][CODEC_FRM_LEGNTH-sample_left];
        }

        if ((CODEC_FRM_LEGNTH+sample_left)>=2048){
            j_max = 4;
        } else {
            j_max = 3;
        }

        sample_left = (CODEC_FRM_LEGNTH+sample_left) - (NUM_FRAMES*j_max);
    } else if (len==NUM_FRAMES){
        j_max = 1;
        for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
            inputbuf[k] = (short *) &mics_in[k][0];
        }        
    } else {
        printf("frame length not support\r\n");
        return aspl_RET_FAIL;
    } 

    for (n=0; n<j_max; n++){

        if (Sys_Total_Inst.DC_rej_enable==1) {
            for (j=0; j<IN_CHANNELS_2MIC; j++){
                for (i = 0; i < NUM_FRAMES; i++)
                {
                    work_buf[j][i] = (int16_t)DC_rejection(&offset_Q15_multi[j], inputbuf[j][i+n*NUM_FRAMES], Sys_Total_Inst.DC_beta);
                }
            }
        } else {
            for (j=0; j<IN_CHANNELS_2MIC; j++){
                for (i = 0; i < NUM_FRAMES; i++)
                {
                    work_buf[j][i] = (int16_t)inputbuf[j][i+n*NUM_FRAMES];
                }
            }        
        }  

        for (m=0; m<IN_CHANNELS_2MIC; m++){
            ssl_filtering((vadInst_t*) vadInstp, &work_buf[m][0], &BPF_Buf[m][0], &vad_input[m][NUM_FRAMES/2]);

            // frame_p = (void*)&work_buf[m][0];
            // refframe_p = (void*)&vad_input[m][NUM_FRAMES/2];
            // memcpy(refframe_p, frame_p, NUM_FRAMES*sizeof(int16_t));
        }            

        k=0;
        for (m = ssl_blocksize/4 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/4){
            ssl_vad_process((vadInst_t*) vadInstp, &vad_input[0][m], &vad_pre[k], &vad_min[k], &vad_max[k], 0);
        }

        // AGC_input_5ch_2(agcInstp, NUM_FRAMES, &vad_input[0][NUM_FRAMES/2], &vad_input[1][NUM_FRAMES/2], &vad_input[2][NUM_FRAMES/2], &vad_input[3][NUM_FRAMES/2], &vad_input[4][NUM_FRAMES/2], &vad_input[0][NUM_FRAMES/2], &vad_input[1][NUM_FRAMES/2], &vad_input[2][NUM_FRAMES/2], &vad_input[3][NUM_FRAMES/2], &vad_input[4][NUM_FRAMES/2], agcInstp->globalMakeupGain_dB, agcInstp->threshold_dBFS);

        // k=0;
        // for (m = ssl_blocksize/4 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/4){
        //     ssl_core_process_gun((sslInst_t*) sslInstp, &vad_input[3][m], &vad_input[1][m], &vad_input[0][m],  &vad_input[2][m], &DoA_mean, mode_normal);
        //     // ssl_core_process_linear((sslInst_t*) sslInstp, &vad_input[3][m], &vad_input[1][m], &vad_input[4][m], &DoA_mean);
        //     k++;

        //     if ((DoA_mean != -1)){
        //         g_DoA = DoA_mean;
        //         g_Beamno = ((int)(g_DoA + 22.5) / (int)45) % 8;
        //         no_DoA_cnt = 0;
        //         printf("\n*************************************************************************\r\n");
        //         printf("DoA = %.2f degree, Beam no = %d \r\n\n", g_DoA, g_Beamno);     
        //         printf("*************************************************************************\r\n\n");		   
        //     } else {
        //         no_DoA_cnt++;
        //         if (no_DoA_cnt>(60*3*4)){
        //             g_DoA = -1;
        //             no_DoA_cnt=(60*3*4)+1;
        //         }
        //     }
        // }

  

        // for (m=0; m<IN_CHANNELS_2MIC; m++){
        //     frame_p = (void*)&vad_input[m][NUM_FRAMES];
        //     refframe_p = (void*)&vad_input[m][0];
        //     memcpy(refframe_p, frame_p, (NUM_FRAMES/2)*sizeof(int16_t));
        // }           

        // if (Beam_auto==1){
        //     if (g_DoA != -1){
        //         Beam1 = g_Beamno;
        //     }

        // }

        // // printf("DoA = %.2f degree, Beam no = %d no_DoA_cnt=%d \r\n", g_DoA, g_Beamno, no_DoA_cnt );

        // *pDoA = g_DoA;
        // if ((*pDoA) >= 0.0){
        //     printf("\n*************************************************************************\r\n");
        //     printf("final DoA = %.2f degree, Beam no = %d \r\n\n", *pDoA, g_Beamno);     
        //     printf("*************************************************************************\r\n\n");		
        // }
              
        
        // AGC_band(agcInstp, &fft_ref_mat[0][0], &fft_ref_mat[0][0]);

        // frame_p = (void*)refs_in[1];
        // poly_analysis(polyInstp, frame_p, &fft_ref_mat[0][0], (MicN), (int)(1)<<18);

        int temp_scale = ((int)(1)<<(Sys_Total_Inst.poly_scale));

        for (m = 0; m<IN_CHANNELS_2MIC ; m++){	
            frame_p = (void*)work_buf[m];
            poly_analysis(polyInstp, frame_p, &fft_in_mat[m][0], m, temp_scale);
        }    

        if (Sys_Total_Inst.bf_enable==1) {

            BF_process(bfInstp);
            BF_process_rear(bfInstp);
            
            for (j=0 ; j<BeamN; j++){
                m=PolyL*2;			
                for (k=1 ; k<PolyM/2+1 ; k++){
                    BF_delay(bfInstp, &fft_bf_mat[j][m], &fft_bf_mat[j][m], &bf_d_buf_g[j][k][0], adaptive_BF_delay+1);
                    m=m+PolyL*2;
                }
            }
            
            for (j=0 ; j<BeamN; j++){
                m=PolyL*2;
                for (k=1 ; k<27 ; k++){
                    BF_adaptive_Proc(bfInstp, &fft_bf_rear_mat[j][m], &fft_bf_mat[j][m], &fft_bf_mat[j][m], k, j);
                    m=m+PolyL*2;
                }
            }	
        }    


    //     m=PolyL*2;
    //     for (k=1 ; k<PolyM/2 ; k++){
    //         sb_AEC_Proc(aecInstp, &fft_ref_mat[0][m], &fft_bf_mat[Beam1][m], &fft_out_mat[1][m], k);
    // //		        sb_AEC_Proc(inst_p, &fft_ref_mat[0][m], &fft_in_mat[0][m], &fft_out_mat[m], k);
    //         m=m+PolyL*2;
    //     }


        if (Sys_Total_Inst.NS_enable==1){
            // NS_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], count_pre);
            if (total_idx < 50) {
                if (Sys_Total_Inst.bf_enable==1) {
                    // NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], -1);
                    NS_oct_process(nsInstp, &fft_bf_mat[Beam1][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], -1);
                    // printf("init period\n");
                } else {
                    NS_oct_process(nsInstp, &fft_in_mat[Beam1/2][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], -1);
                }
            } else {
                if (Sys_Total_Inst.bf_enable==1) {
                    // NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], -1);
                    NS_oct_process(nsInstp, &fft_bf_mat[Beam1][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], vad_max[0]+vad_max[1]);
                    // printf("init period\n");
                } else {
                    NS_oct_process(nsInstp, &fft_in_mat[Beam1/2][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], vad_max[0]+vad_max[1]);
                }            
                // NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], vad_max[0]+vad_max[1]);
                
            }
        } 


        temp_scale = ((int)(1)<<(15-Sys_Total_Inst.poly_scale));

        for (m = 0; m<1 ; m++){	
            outframe_p =  (void*)&inputbuf[m][n*NUM_FRAMES];
            if (Sys_Total_Inst.bf_enable==1) {
                if (Sys_Total_Inst.NS_enable==1){
                    // poly_synthesis(polyInstp, &fft_bf_mat[m*2][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_out_mat[m][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[m*2][0], outframe_p, m, temp_scale);
                } else {
                    poly_synthesis(polyInstp, &fft_bf_mat[Beam1][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[Beam1][0], outframe_p, m, temp_scale);
                }
            } else {
                if (Sys_Total_Inst.NS_enable==1){
                    // poly_synthesis(polyInstp, &fft_bf_mat[m*2][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_out_mat[m][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[m*2][0], outframe_p, m, temp_scale);
                } else {
                    // poly_synthesis(polyInstp, &fft_in_mat[Beam1/2][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_in_mat[Beam1/2][0], outframe_p, m, temp_scale);
                }            
                
            }
        }   

        int temp;
        for (j = 0; j<IN_CHANNELS_2MIC ; j++){
                for (i = 0; i < NUM_FRAMES; i++)
                {
                        temp = 16384 + ((int32_t)inputbuf[j][i+n*NUM_FRAMES] * (int32_t)(Sys_Total_Inst.g_gainQ15));
                        inputbuf[j][i+n*NUM_FRAMES]= temp>>15;
                }        
        }         
    }

    if (len==1600){
        for (k = 0; k < IN_CHANNELS_2MIC; k++) {
            for (i=0; i<CODEC_FRM_LEGNTH; i++) {
                mics_in[k][i] = g_codec_4mic_buf[k][i];
            }
        }

        for (k = 0; k < IN_CHANNELS_2MIC; k++) {
            for (i=0; i<CODEC_FRM_LEGNTH; i++) {
                g_codec_4mic_buf[k][i] = g_codec_4mic_buf[k][CODEC_FRM_LEGNTH+i];
            }
        }
    }

    return aspl_RET_SUCCESS;    
}

int aspl_NR_process_enables(short* data, int len, int NS_enable, int AGC_enable, int globalgain_dB){

    Total_Inst_t* Total_Instp = (void *)&Sys_Total_Inst;
    polyInst_t* polyInstp = Sys_Total_Inst.polyInst_p;
    nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
    agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;
    vadInst_t* vadInstp = Sys_Total_Inst.vadInst_p;
    aecInst_t* aecInstp = Sys_Total_Inst.aecInst_p;
    MonInst_t* MonInstp = Sys_Total_Inst.MonInst_p;

    void    *inst_p;

    int rc;
    int i, k, j;
    int j_max;
    unsigned char *tempTxPtr, *tempRxPtr, *tempWkPtr;
    unsigned char *tempOutPtr, *tempMicPtr, *tempMicPtr2;

    short   *in_r;
    short   *frame_p;
    short   *refframe_p;
    short   *outframe_p;
    short   *work_buf[WORK_CHANNELS];     /* pointers to microphone inputs */
    short   *mics_in[IN_CHANNELS];     /* pointers to microphone inputs */
	short   *refs_in[REF_CHANNELS];     /* pointers to microphone inputs */
    short   *vad_input[4];
    short   *inputbuf;
    short   *refbuf;
   
    in_r  = (short *)(g_data_vad);
    for (k = 0; k < 4; k++) {
        vad_input[k] = &in_r[k*NUM_FRAMES]; /* find the frame start for each microphone */
    }  

    in_r  = (short *)g_WorkBuf;
    for (k = 0; k < WORK_CHANNELS; k++) {
        work_buf[k] = &in_r[k*NUM_FRAMES];   /* find the frame start for each ref channel */
    }

    in_r  = (short *)data;
    for (k = 0; k < 1; k++) {
        mics_in[k] = &in_r[k*len];	/* find the frame start for each microphone */
    }    


    if (len==1024){
        j_max = 2;
        inputbuf = (short *) &mics_in[0][0];
        

    } else if (len==1600){

        memcpy(&g_codec_4mic_buf[0][CODEC_FRM_LEGNTH],  &mics_in[0][0], sizeof(int16_t)*CODEC_FRM_LEGNTH);
        inputbuf = (short *) &g_codec_4mic_buf[0][CODEC_FRM_LEGNTH-sample_left];

        if ((CODEC_FRM_LEGNTH+sample_left)>=2048){
            j_max = 4;
        } else {
            j_max = 3;
        }

        sample_left = (CODEC_FRM_LEGNTH+sample_left) - (NUM_FRAMES*j_max);
    } else if (len==NUM_FRAMES){
        j_max = 1;
        inputbuf = (short *) &mics_in[0][0];

    } else {
        printf("frame length not support\r\n");
        return aspl_RET_FAIL;
    } 


    for (j=0; j<j_max; j++){

        ///////////////////////////   fade in         ////////////////////////////////////////
        if (fade_gs < -1.0) {
            fade_gs = r_a * fade_gs + (1.0 - r_a) * fade_gc;
			float fadein = powf (10, fade_gs*0.05);
            // printf("fade_gs= %f, fadein=%f \n ", fade_gs, fadein);
            for (i = 0; i < NUM_FRAMES; i++) {
                inputbuf[i+j*NUM_FRAMES] = (short)((float)inputbuf[i+j*NUM_FRAMES]*fadein);
            }
        }
        ///////////////////////////   fade in end        ////////////////////////////////////////

        ///////////////////////////   DC rejection         ////////////////////////////////////////
        // if (Sys_Total_Inst.DC_rej_enable==1) {
        //     for (i = 0; i < NUM_FRAMES; i++) {
        //         work_buf[0][i] = (int16_t)DC_rejection(&offset_Q15_L, inputbuf[i+j*NUM_FRAMES], Sys_Total_Inst.DC_beta);
        //         if (ref != NULL ) {
        //             work_buf[1][i] = (int16_t)DC_rejection(&offset_Q15_R, refbuf[i+j*NUM_FRAMES], Sys_Total_Inst.DC_beta);
        //         }
        //     }
        // } else {
            for (i = 0; i < NUM_FRAMES; i++) {
                work_buf[0][i] = inputbuf[i+j*NUM_FRAMES];
            }
        // }
        ///////////////////////////   DC rejection end        ////////////////////////////////////////        

        /////////////////////////   AEC                     ////////////////////////////////////////   
        // if (ref != NULL ) {
        //     for (i = 0; i < NUM_FRAMES; i++) {
        //         work_buf[1][i] = work_buf[1][i]>>2; //refbuf[i+j*NUM_FRAMES]>>4;               
        //     }
        //     AEC_delay(aecInstp, work_buf[1], work_buf[2], &d_buf_g[0][0], Sys_Total_Inst.tuning_delay); 
        //     // AEC_delay(aecInstp, work_buf[1], work_buf[2], &d_buf_g[1][0], 50); 
        //     // AEC_single_Proc_double(aecInstp, &work_buf[2][0], &work_buf[0][0], &work_buf[3][0]);
        //     AEC_single_Proc(aecInstp, &work_buf[2][0], &work_buf[0][0], &work_buf[3][0]);
        // }
        /////////////////////////   AEC end                    ////////////////////////////////////////   


        ///////////////////////////   VAD filtering & process    ////////////////////////////////////////
        ssl_filtering((vadInst_t*) vadInstp, &work_buf[0][0], &BPF_Buf[0][0], &vad_input[0][0]);
        // sb_AEC_LP_Proc(aecInstp, &vad_input[0][0], &vad_input[1][0]);
        k=0;
        for (i = 0 ; i<NUM_FRAMES ; i=i+ssl_blocksize){
            ssl_vad_process((vadInst_t*) vadInstp, &vad_input[0][i], &vad_pre[k], &vad_min[k], &vad_max[k], 0);
            k++;
        }
        // AEC_delay(aecInstp, work_buf[3], work_buf[3], &d_buf_g[1][0], Sys_Total_Inst.tuning_delay);  
        ///////////////////////////   VAD filtering & process end   ////////////////////////////////////////


        if (NS_enable==1){
            ///////////////////////////   poly_analysis         ////////////////////////////////////////
            int temp_scale = ((int)(1)<<(Sys_Total_Inst.poly_scale));
            poly_analysis(polyInstp, (void*)work_buf[0], &fft_in_mat[0][0], 0, temp_scale);
           
            if (total_idx < 50) {
                NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], -1);
            } else {
                NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], vad_max[0]+vad_max[1]);
            }
            ///////////////////////////   NS_process end           ////////////////////////////////////////

            ///////////////////////////   poly_synthesis         ////////////////////////////////////////
            temp_scale = ((int)(1)<<(15-Sys_Total_Inst.poly_scale));
            poly_synthesis(polyInstp, &fft_out_mat[0], &inputbuf[j*NUM_FRAMES], 0, temp_scale);
        } else {
            memcpy(&inputbuf[j*NUM_FRAMES], work_buf[0], sizeof(short)*NUM_FRAMES);
        }

        outframe_p = &inputbuf[j*NUM_FRAMES];
        if (AGC_enable==1){
            AGC_total_w_ref(agcInstp, outframe_p, refframe_p, outframe_p,   (vad_min[0]+(vad_min[1]<<1)), vad_max[0]+vad_max[1]);
        }

        double temp_gain = pow(10, (double)(globalgain_dB)/20.0);
        int g_gainQ15 = (int)(temp_gain * 32767.0);

        int temp;
        outframe_p = &inputbuf[j*NUM_FRAMES];
        for (i = 0; i < NUM_FRAMES; i++) {
            temp = 16384 + ((int32_t)outframe_p[i] * (int32_t)(g_gainQ15));
            outframe_p[i]= temp>>15;
        }            

        total_idx++;
        if (total_idx >= 50){
            total_idx=50;
        }
    }

    if (len==1600){
        for (i=0; i<CODEC_FRM_LEGNTH; i++) {
            mics_in[0][i] = g_codec_4mic_buf[0][i];
        }

        for (i=0; i<CODEC_FRM_LEGNTH; i++) {
            g_codec_4mic_buf[0][i] = g_codec_4mic_buf[0][CODEC_FRM_LEGNTH+i];
        }
    }

    return aspl_RET_SUCCESS;
}
