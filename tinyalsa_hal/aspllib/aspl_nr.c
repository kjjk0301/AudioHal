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
#include "dB_scale.h"
#include "audio_utils.h"
#include "debug_file.h"
#include "level_mon.h"
#include "aspl_nr.h"

// Define version information
#define LIBRARY_VERSION "0.5.1"
#define RELEASE_DATE "2024-12-18"
#define RELEASE_STATUS "MarkT buffersize : 512 -> multi size"

Total_Inst_t Sys_Total_Inst;

int16_t* g_codec_buf=NULL;
int16_t* g_codec_4mic_buf[MULTI_INPUT_CHANNELS+REF_CHANNELS]={NULL, NULL, NULL};
int16_t* g_data_vad=NULL;
char * g_WorkBuf=NULL;

static int total_idx = 0;

int offset_Q15_L=0, offset_Q15_R=0,  offset_Q15_ref=0;
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
    // int CodecBufSize = CODEC_BUF_LENGTH * NUM_BYTE_PER_SAMPLE;
    int CodecBufSize = CODEC_BUF_LENGTH_MAX * NUM_BYTE_PER_SAMPLE;    


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
    void * aecInst_p=(void *)sysAECCreate();
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
    Sys_Total_Inst.g_gainQ15 = (int)(20.0 * 32767.0);

    Sys_Total_Inst.tuning_delay = 1130;
    
    void * Total_Inst_p = &Sys_Total_Inst;

    int WorkBufSize = NUM_FRAMES * (WORK_CHANNELS_2MIC) * NUM_BYTE_PER_SAMPLE;
    // int CodecBufSize = CODEC_BUF_LENGTH * NUM_BYTE_PER_SAMPLE;
    int CodecBufSize = CODEC_BUF_LENGTH_MAX * NUM_BYTE_PER_SAMPLE;        

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

    strncpy(g_nr_config.tuning_file_path, config_p->tuning_file_path, sizeof(g_nr_config.tuning_file_path));
    g_nr_config.tuning_file_path[sizeof(g_nr_config.tuning_file_path) - 1] = '\0';

    if (access(g_nr_config.tuning_file_path, F_OK) != -1) {
        // The file exists
    } else {

        printf("The param file %s does not exist.\n", g_nr_config.tuning_file_path);

        return aspl_RET_FAIL;
    }

    printf("ASPL default param read from %s \r\n", g_nr_config.tuning_file_path);
    aspl_NR_total_param_set_from_file(g_nr_config.tuning_file_path);   

    aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)config_p);     

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


int aspl_NR_param_set(int param_num, int value){

    int dummy[64];
    aspl_nr_params_t tmp;

    aspl_NR_expert_param_read(&tmp);

    memcpy(dummy, &tmp, sizeof(aspl_nr_params_t));

    dummy[param_num]=value;

    memcpy(&tmp, dummy, sizeof(aspl_nr_params_t));

    aspl_NR_expert_param_write(&tmp);

    return aspl_RET_SUCCESS;
}

int aspl_NR_param_get(int param_num){

    int dummy[64];

    aspl_nr_params_t tmp;

    aspl_NR_expert_param_read(&tmp);

    memcpy(dummy, &tmp, sizeof(aspl_nr_params_t));

    return dummy[param_num];
}

void aspl_NR_expert_param_read(aspl_nr_params_t* tmp_p) {

    aspl_nr_params_t tmp;
    int k;

    nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
    agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;
    vadInst_t* vadInstp = Sys_Total_Inst.vadInst_p;

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

    tmp.NS_enable = (int) Sys_Total_Inst.NS_enable;
    tmp.AGC_enable = (int) Sys_Total_Inst.AGC_enable;

#ifdef PRINT_EXPERT_PARAM    
    printf("NS enabled = %d \r\n", Sys_Total_Inst.NS_enable);
    printf("AGC enabled =%d \r\n", Sys_Total_Inst.AGC_enable);
#endif

    tmp.EQ_Low = (int)nsInstp->EQ_Low;
    tmp.EQ_Hi = (int)nsInstp->EQ_Hi;

#ifdef PRINT_EXPERT_PARAM
    printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
    printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
    printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
#endif

    tmp.betaQ15_NR1 = Sys_Total_Inst.betaQ15_NR1;//(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR1;
    tmp.max_att_NR1 = Sys_Total_Inst.max_att_NR1;//(int32_t)-15; //Sys_Total_Inst.max_att_NR1;
    tmp.min_att_NR1 = Sys_Total_Inst.min_att_NR1;  //(int32_t)-1; //Sys_Total_Inst.min_att_NR1; 
    tmp.slope_NR1 = Sys_Total_Inst.slope_NR1;  //(int32_t)15; //Sys_Total_Inst.slope_NR1; 
    tmp.high_att_NR1 = Sys_Total_Inst.high_att_NR1; //(int32_t)-10; //Sys_Total_Inst.high_att_NR1;
    tmp.target_SPL_NR1 = Sys_Total_Inst.target_SPL_NR1; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR1;

    tmp.betaQ15_NR2 = Sys_Total_Inst.betaQ15_NR2; //(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR2;
    tmp.max_att_NR2 = Sys_Total_Inst.max_att_NR2; //(int32_t)-20; //Sys_Total_Inst.max_att_NR2;
    tmp.min_att_NR2 = Sys_Total_Inst.min_att_NR2;  //(int32_t)-4; //Sys_Total_Inst.min_att_NR2; 
    tmp.slope_NR2 = Sys_Total_Inst.slope_NR2; //(int32_t)18; //Sys_Total_Inst.slope_NR2; 
    tmp.high_att_NR2 = Sys_Total_Inst.high_att_NR2; //(int32_t)-12; //Sys_Total_Inst.high_att_NR2;
    tmp.target_SPL_NR2 = Sys_Total_Inst.target_SPL_NR2; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR2;

    tmp.betaQ15_NR3 = Sys_Total_Inst.betaQ15_NR3; //(int32_t)(0.4*32767.0); // Sys_Total_Inst.betaQ15_NR3;
    tmp.max_att_NR3 =  Sys_Total_Inst.max_att_NR3; //(int32_t)-25; //Sys_Total_Inst.max_att_NR3;
    tmp.min_att_NR3 = Sys_Total_Inst.min_att_NR3; //(int32_t)-8; //Sys_Total_Inst.min_att_NR3; 
    tmp.slope_NR3 = Sys_Total_Inst.slope_NR3;  //(int32_t)20; //Sys_Total_Inst.slope_NR3; 
    tmp.high_att_NR3 = Sys_Total_Inst.high_att_NR3; //(int32_t)-15; //Sys_Total_Inst.high_att_NR3;
    tmp.target_SPL_NR3 = Sys_Total_Inst.target_SPL_NR3;  //(int32_t)73; //Sys_Total_Inst.target_SPL_NR3; 

    tmp.DC_reject_beta = Sys_Total_Inst.DC_beta;
    tmp.ns_beta_low_ratio = (int32_t)(nsInstp->beta_low_ratio*10.0);
    tmp.ns_beta_high_ratio =(int32_t)(nsInstp->beta_high_ratio*10.0);

    tmp.vad_tau_s_r = (int32_t)(vadInstp->tau_s_r*Q31_MAX);
    tmp.vad_tau_s_f = (int32_t)(vadInstp->tau_s_f*Q31_MAX);
    tmp.global_gain = (int32_t)(20*log10((double)Sys_Total_Inst.g_gainQ15/32767.0));

    memcpy(tmp_p, &tmp, sizeof(aspl_nr_params_t));

}


void aspl_NR_expert_param_write(aspl_nr_params_t* tmp_p){

        int k;
        
        nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
        agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;
        vadInst_t* vadInstp = Sys_Total_Inst.vadInst_p;

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


    Sys_Total_Inst.NS_enable = (int) tmp_p->NS_enable;
    Sys_Total_Inst.AGC_enable = (int) tmp_p->AGC_enable ;
#ifdef PRINT_EXPERT_PARAM
    printf("NS enabled = %d \r\n", Sys_Total_Inst.NS_enable);
    printf("AGC enabled =%d \r\n", Sys_Total_Inst.AGC_enable);
#endif
    nsInstp->EQ_Low  = (short) tmp_p->EQ_Low;
    nsInstp->EQ_Hi = (short) tmp_p->EQ_Hi;

#ifdef PRINT_EXPERT_PARAM
    printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
    printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
    printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
#endif

    Sys_Total_Inst.betaQ15_NR1 = tmp_p->betaQ15_NR1;
    Sys_Total_Inst.max_att_NR1 = tmp_p->max_att_NR1;
    Sys_Total_Inst.min_att_NR1 = tmp_p->min_att_NR1;
    Sys_Total_Inst.slope_NR1 = tmp_p->slope_NR1;
    Sys_Total_Inst.high_att_NR1 = tmp_p->high_att_NR1;
    Sys_Total_Inst.target_SPL_NR1 = tmp_p->target_SPL_NR1;

    Sys_Total_Inst.betaQ15_NR2 = tmp_p->betaQ15_NR2;
    Sys_Total_Inst.max_att_NR2 = tmp_p->max_att_NR2;
    Sys_Total_Inst.min_att_NR2 = tmp_p->min_att_NR2;
    Sys_Total_Inst.slope_NR2 = tmp_p->slope_NR2;
    Sys_Total_Inst.high_att_NR2 = tmp_p->high_att_NR2;
    Sys_Total_Inst.target_SPL_NR2 = tmp_p->target_SPL_NR2;

    Sys_Total_Inst.betaQ15_NR3 = tmp_p->betaQ15_NR3;
    Sys_Total_Inst.max_att_NR3 = tmp_p->max_att_NR3;
    Sys_Total_Inst.min_att_NR3 = tmp_p->min_att_NR3;
    Sys_Total_Inst.slope_NR3 = tmp_p->slope_NR3;
    Sys_Total_Inst.high_att_NR3 = tmp_p->high_att_NR3;
    Sys_Total_Inst.target_SPL_NR3 = tmp_p->target_SPL_NR3;   

    Sys_Total_Inst.DC_beta = tmp_p->DC_reject_beta; //;

    nsInstp->beta_low_ratio = (float)(tmp_p->ns_beta_low_ratio)/10.0;
    nsInstp->beta_high_ratio = (float)(tmp_p->ns_beta_high_ratio)/10.0;

    vadInstp->tau_s_r = (double)(tmp_p->vad_tau_s_r)/Q31_MAX; // 0.05; //
    vadInstp->tau_s_f = (double)(tmp_p->vad_tau_s_f)/Q31_MAX; //0.1; //

    double temp_gain = pow(10, (double)(tmp_p->global_gain)/20.0);
    Sys_Total_Inst.g_gainQ15 = (int)(temp_gain * 32767.0);

}

int aspl_NR_total_param_write_to_file(const char* file_path) {

    aspl_nr_params_t tmp;

    aspl_NR_expert_param_read(&tmp);

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

int aspl_NR_total_param_set_from_file(const char* file_path){

    aspl_nr_params_t tmp;

    FILE* file = fopen(file_path, "rb");
    if (file != NULL) {
        fread(&tmp, sizeof(aspl_nr_params_t), 1, file);
        fclose(file);

        aspl_NR_expert_param_write(&tmp);

    } else {
        printf("Failed to open file for reading.\n");
        return aspl_RET_FAIL;
    }

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


int aspl_AEC_create(aspl_NR_CONFIG* config){
    int i;

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_create :: (aspl_NR_CONFIG *)config is NULL\r\n");
        return aspl_RET_FAIL;
    }

    for (i=0; i<config_p->aec_Mic_N  ; i++){
        void * aecInst_p=(void *)sysAECCreate();    
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

    config_p->AEC_globaldelay[0] = -68;
    config_p->AEC_filter_len[0] = 1000;

    config_p->AEC_globaldelay[1] = -68;
    config_p->AEC_filter_len[1] = 1000;

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

    // AEC_2ch_Proc(aecInstp, &refs_in[0], &mics_in[0][0], &mics_in[0][0], len, aec_delay, 1, micscaledB);
    // AEC_single_Proc(aecInstp[1], &refs_in[0], &mics_in[1][0], &mics_in[1][0], len, config_p->AEC_globaldelay[0], micscaledB);
    AEC_single_Proc(aecInstp[0], &refs_in[0], &mics_in[0][0], &mics_in[0][0], len, config_p->AEC_globaldelay[0], micscaledB);
    AEC_single_Proc(aecInstp[1], &refs_in[0], &mics_in[1][0], &mics_in[1][0], len, config_p->AEC_globaldelay[1], micscaledB);
    
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

int aspl_AEC_process_filtersave(int16_t* data, int16_t* ref, int len, int aec_delay, int delayauto, float micscaledB, aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_process_filtersave :: (aspl_NR_CONFIG *)config is NULL \r\n");
        return aspl_RET_FAIL;
    } 


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
    float * filter[2] = {NULL, NULL};
    int filter_len[2];
    int globaldelay[2];
    int res1 = 0;
    int res2 = 0;
    // res2=AEC_single_Proc_filter_save(aecInstp[1], &refs_in[0], &mics_in[1][0], &mics_in[1][0], len, aec_delay, delayauto, micscaledB, &filter[1], &filter_len[1], &globaldelay[1]);
    res1=AEC_single_Proc_filter_save(aecInstp[0], &refs_in[0], &mics_in[0][0], &mics_in[0][0], len, aec_delay, delayauto, micscaledB, &filter[0], &filter_len[0], &globaldelay[0]);
    res2=AEC_single_Proc_filter_save(aecInstp[1], &refs_in[0], &mics_in[1][0], &mics_in[1][0], len, aec_delay, delayauto, micscaledB, &filter[1], &filter_len[1], &globaldelay[1]);
    if (res1 == 1 && res2 == 1 ) {
        memcpy(config_p->AEC_filter_save[0], filter[0], sizeof(float) *(filter_len[0]));
        config_p->AEC_filter_len[0] = filter_len[0];
        config_p->AEC_globaldelay[0] = globaldelay[0]; // for 1024 buffer

        memcpy(config_p->AEC_filter_save[1], filter[1], sizeof(float) *(filter_len[1]));
        config_p->AEC_filter_len[1] = filter_len[1];
        config_p->AEC_globaldelay[1] = globaldelay[1]; // for 1024 buffer

        config_p->AEC_filter_updated = 1;

        printf("aspl_AEC_process_filtersave :: filter[0] = %p filter_len[0] = %d globaldelay[0]= %d \r\n", filter[0], filter_len[0], globaldelay[0]);
        printf("aspl_AEC_process_filtersave :: filter[1] = %p filter_len[1] = %d globaldelay[1]= %d \r\n", filter[1], filter_len[1], globaldelay[1]);

        return aspl_RET_SUCCESS;
    }

    return aspl_RET_FAIL;
}

int aspl_AEC_filterload(aspl_NR_CONFIG* config){

    aspl_NR_CONFIG * config_p = config;
    if (config_p == NULL) {
        printf("aspl_AEC_process_single_filt :: (aspl_NR_CONFIG *)config is NULL");
        return aspl_RET_FAIL;
    } 

    for (int i=0; i<config_p->aec_Mic_N  ; i++){
        AEC_single_filter_load(config_p->aecInst_p[i], config_p->AEC_filter_save[i], config_p->AEC_filter_len[i], config_p->AEC_globaldelay[i]);
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

    // if (len==1024){
    //     j_max = 2;
    //     for (k = 0; k < IN_CHANNELS_2MIC; k++) {
    //         inputbuf[k] = (short *) &mics_in[k][0];
    //     }        
    // } else if (len==1600){
    //     for (k = 0; k < IN_CHANNELS_2MIC; k++) {
    //         memcpy(&g_codec_4mic_buf[k][CODEC_FRM_LEGNTH], &mics_in[k][0], sizeof(int16_t)*CODEC_FRM_LEGNTH);
    //         inputbuf[k] = (short *) &g_codec_4mic_buf[k][CODEC_FRM_LEGNTH-sample_left];
    //     }

    //     if ((CODEC_FRM_LEGNTH+sample_left)>=2048){
    //         j_max = 4;
    //     } else {
    //         j_max = 3;
    //     }

    //     sample_left = (CODEC_FRM_LEGNTH+sample_left) - (NUM_FRAMES*j_max);
    // } else if (len==NUM_FRAMES){
        // j_max = 1;
        // for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
        //     inputbuf[k] = (short *) &mics_in[k][0];
        // }        
    // } else {
    //     printf("frame length not support\r\n");
    //     return aspl_RET_FAIL;
    // } 


    if (len<=CODEC_FRM_LEGNTH_MAX){

        for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
            memcpy(&g_codec_4mic_buf[k][CODEC_BUF_FRONT_LEGNTH],  &mics_in[k][0], sizeof(int16_t)*len);
            inputbuf[k] = (short *) &g_codec_4mic_buf[k][CODEC_BUF_FRONT_LEGNTH-sample_left];
        }

        j_max = (len+sample_left) / NUM_FRAMES;

        sample_left = (len+sample_left) - (NUM_FRAMES*j_max);

        // printf("len = %d, j_max = %d, sample_left = %d \r\n", len, j_max, Total_Inst_p->sample_left);

    } else {
        printf("frame length not support\r\n");
        return aspl_RET_FAIL;
    } 
    

    for (n=0; n<j_max; n++){

        AGC_input_2ch(agcInstp, NUM_FRAMES, &inputbuf[0][n*NUM_FRAMES], &inputbuf[1][n*NUM_FRAMES], &inputbuf[0][n*NUM_FRAMES], &inputbuf[1][n*NUM_FRAMES], agcInstp->globalMakeupGain_dB, agcInstp->threshold_dBFS);            

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
        short vad_sum=0;
        for (m = ssl_blocksize/2 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/2){
            ssl_vad_process((vadInst_t*) vadInstp, &vad_input[0][m], &vad_pre[k], &vad_min[k], &vad_max[k], 0);
            vad_sum += vad_min[k];
            k++;
        }

        // AGC_input_2ch(agcInstp, NUM_FRAMES, &vad_input[0][NUM_FRAMES/2], &vad_input[1][NUM_FRAMES/2], &vad_input[0][NUM_FRAMES/2], &vad_input[1][NUM_FRAMES/2], agcInstp->globalMakeupGain_dB, agcInstp->threshold_dBFS);



        k=0;
        for (m = ssl_blocksize/2 ; m<(NUM_FRAMES-ssl_blocksize) ; m=m+ssl_blocksize/2){
            ssl_core_process_2ch((sslInst_t*) sslInstp, &vad_input[0][m], &vad_input[1][m], &DoA_mean, mode_normal, vad_sum);
            k++;

            if ((DoA_mean != -1)){
                g_DoA = DoA_mean;
                if (g_DoA < -75.0) g_Beamno = 0;
                else if (g_DoA < -55.0) g_Beamno = 1;
                else if (g_DoA < -38.0) g_Beamno = 2;
                else if (g_DoA < -15.0) g_Beamno = 3;
                else if (g_DoA < 15.0) g_Beamno = 4;
                else if (g_DoA < 38.0) g_Beamno = 5;
                else if (g_DoA < 55.0) g_Beamno = 6;
                else if (g_DoA < 75.0) g_Beamno = 7;
                else g_Beamno = 8;

                no_DoA_cnt = 0;
                printf("\n*************************************************************************\r\n");
                printf("DoA = %.2f degree, Beam no = %d \r\n\n", g_DoA, g_Beamno);     
                printf("*************************************************************************\r\n\n");		   
            } else {
                no_DoA_cnt++;
                if (no_DoA_cnt>(60*3*2)){
                    g_DoA = -1;
                    no_DoA_cnt=(60*3*2)+1;
                }
            }
        }

        for (m=0; m<IN_CHANNELS_2MIC; m++){
            frame_p = (void*)&vad_input[m][NUM_FRAMES];
            refframe_p = (void*)&vad_input[m][0];
            memcpy(refframe_p, frame_p, (NUM_FRAMES/2)*sizeof(int16_t));
        }           

        if (Beam_auto==1){
            if (g_DoA != -1){
                Beam1 = g_Beamno;
            }

        }

        // printf("DoA = %.2f degree, Beam no = %d no_DoA_cnt=%d \r\n", g_DoA, g_Beamno, no_DoA_cnt );

        *pDoA = g_DoA;
        if ((*pDoA) >= 0.0){
            // printf("\n*************************************************************************\r\n");
            // printf("final DoA = %.2f degree, Beam no = %d \r\n\n", *pDoA, g_Beamno);     
            // printf("*************************************************************************\r\n\n");		
        }
              
        
        // AGC_band(agcInstp, &fft_ref_mat[0][0], &fft_ref_mat[0][0]);

        // frame_p = (void*)refs_in[0];
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
                    NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], -1);
                }
            } else {
                if (Sys_Total_Inst.bf_enable==1) {
                    // NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0], -1);
                    NS_oct_process(nsInstp, &fft_bf_mat[Beam1][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], vad_max[0]+vad_max[1]);
                    // printf("init period\n");
                } else {
                    NS_oct_process(nsInstp, &fft_in_mat[0][0], &fft_ns_buf_mat[0], &fft_out_mat[0][0], vad_max[0]+vad_max[1]);
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
                    // poly_synthesis(polyInstp, &fft_in_mat[m][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[m*2][0], outframe_p, m, temp_scale);
                } else {
                    // poly_synthesis(polyInstp, &fft_bf_mat[Beam1][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_in_mat[m][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[Beam1][0], outframe_p, m, temp_scale);
                }
            } else {
                if (Sys_Total_Inst.NS_enable==1){
                    // poly_synthesis(polyInstp, &fft_bf_mat[m*2][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_out_mat[m][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_in_mat[m][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_bf_rear_mat[m*2][0], outframe_p, m, temp_scale);
                } else {
                    // poly_synthesis(polyInstp, &fft_in_mat[Beam1/2][0], outframe_p, m, temp_scale);
                    // poly_synthesis(polyInstp, &fft_in_mat[Beam1/2][0], outframe_p, m, temp_scale);
                    poly_synthesis(polyInstp, &fft_in_mat[m][0], outframe_p, m, temp_scale);
                }            
                
            }
        }   

        int temp;
        for (j = 0; j<1 ; j++){
                for (i = 0; i < NUM_FRAMES; i++)
                {
                        temp = 16384 + ((int32_t)inputbuf[j][i+n*NUM_FRAMES] * (int32_t)(Sys_Total_Inst.g_gainQ15));
                        inputbuf[j][i+n*NUM_FRAMES]= temp>>15;
                }        
        }         
    }

    if (len<=CODEC_FRM_LEGNTH_MAX){
        for (k = 0; k < MULTI_INPUT_CHANNELS; k++) {
            for (i=0; i<len; i++) {
                mics_in[k][i] = g_codec_4mic_buf[k][i];
            }

            for (i=0; i<CODEC_BUF_FRONT_LEGNTH; i++) {
                g_codec_4mic_buf[k][i] = g_codec_4mic_buf[k][len+i];
            }
        }
    }           

    // if (len==1600){
    //     for (k = 0; k < IN_CHANNELS_2MIC; k++) {
    //         for (i=0; i<CODEC_FRM_LEGNTH; i++) {
    //             mics_in[k][i] = g_codec_4mic_buf[k][i];
    //         }
    //     }

    //     for (k = 0; k < IN_CHANNELS_2MIC; k++) {
    //         for (i=0; i<CODEC_FRM_LEGNTH; i++) {
    //             g_codec_4mic_buf[k][i] = g_codec_4mic_buf[k][CODEC_FRM_LEGNTH+i];
    //         }
    //     }
    // }

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


    // if (len==1024){
    //     j_max = 2;
    //     inputbuf = (short *) &mics_in[0][0];
        

    // } else if (len==1600){

    //     memcpy(&g_codec_4mic_buf[0][CODEC_FRM_LEGNTH],  &mics_in[0][0], sizeof(int16_t)*CODEC_FRM_LEGNTH);
    //     inputbuf = (short *) &g_codec_4mic_buf[0][CODEC_FRM_LEGNTH-sample_left];

    //     if ((CODEC_FRM_LEGNTH+sample_left)>=2048){
    //         j_max = 4;
    //     } else {
    //         j_max = 3;
    //     }

    //     sample_left = (CODEC_FRM_LEGNTH+sample_left) - (NUM_FRAMES*j_max);
    // } else if (len==NUM_FRAMES){
    //     j_max = 1;
    //     inputbuf = (short *) &mics_in[0][0];

    if (len<=CODEC_FRM_LEGNTH_MAX){

        memcpy(&g_codec_4mic_buf[0][CODEC_BUF_FRONT_LEGNTH],  &mics_in[0][0], sizeof(int16_t)*len);
        inputbuf = (short *) &g_codec_4mic_buf[0][CODEC_BUF_FRONT_LEGNTH-sample_left];

        j_max = (len+sample_left) / NUM_FRAMES;

        sample_left = (len+sample_left) - (NUM_FRAMES*j_max);

        // printf("len = %d, j_max = %d, sample_left = %d \r\n", len, j_max, Total_Inst_p->sample_left);

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

    // if (len==1600){
    //     for (i=0; i<CODEC_FRM_LEGNTH; i++) {
    //         mics_in[0][i] = g_codec_4mic_buf[0][i];
    //     }

    //     for (i=0; i<CODEC_FRM_LEGNTH; i++) {
    //         g_codec_4mic_buf[0][i] = g_codec_4mic_buf[0][CODEC_FRM_LEGNTH+i];
    //     }
    // }

    if (len<=CODEC_FRM_LEGNTH_MAX){
        for (i=0; i<len; i++) {
            mics_in[0][i] = g_codec_4mic_buf[0][i];
        }

        for (i=0; i<CODEC_BUF_FRONT_LEGNTH; i++) {
            g_codec_4mic_buf[0][i] = g_codec_4mic_buf[0][len+i];
        }
    }       

    return aspl_RET_SUCCESS;
}
