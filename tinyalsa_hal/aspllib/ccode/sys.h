#ifndef _SYS_H
#define _SYS_H

#include "../include/aspl_nr.h"

typedef signed char  int8_t;  
typedef signed short int16_t;  
typedef signed int   int32_t;  

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

#define aspl_RET_SUCCESS 0
#define aspl_RET_FAIL    1
#define aspl_RET_FAIL_MALLOC    2

#define SAMPLING_FREQ 		16000
#define SAMPLING_FREQ_REF	48000

#define MULTI_INPUT_CHANNELS 		2
#define NUM_BYTE_PER_SAMPLE	2
#define FILESAVE_CHANNELS 	2
#define NUM_FRAMES			512
#define CODEC_FRM_LEGNTH	1600
#define CODEC_BUF_LENGTH	3200


#define SAMPLING_FREQ       16000
#define IN_CHANNELS         6
#define IN_CHANNELS_2MIC    2
#define REF_CHANNELS        1
#define WORK_CHANNELS       (IN_CHANNELS+REF_CHANNELS)
#define WORK_CHANNELS_2MIC       (IN_CHANNELS_2MIC+REF_CHANNELS)
#define OUTPUT_CHANNELS     2
#define NUM_FRAMES          512
#define NUM_BYTE_PER_SAMPLE 2

#define framesize_max   1600
#define polyblocksize   NUM_FRAMES // blocksize
#define PolyN           512        // polyphase filter length N
#define PolyM           256        // filterbank channel number M
#define PolyR           64         // decimation factor R
#define PolyL           polyblocksize/PolyR // channel samples within a block L=blocksize/R
#define Q15             15         // bits number for Q format
#define Q15_val         32768      // Q15 max value
#define Q15_MAX         32767      // Q15 max value
#define Q15_3dB_val     23170      // Q15 max -3dB format
#define Q15_dB          90.3090
#define Q30_dB          180.6180
#define Q31_dB          186.6386
#define FS_SPL          110        // 0 FSdB -> 110 SPL dB

#define Q31_val         2.147483648000000e+09      // Q15 max value
#define Q31_MAX         2.147483647000000e+09      // Q15 max value

#define Q15_MAXf 32767.0f  // Q15의 최댓값
#define Q15_MINf -32768.0f // Q15의 최솟값
#define FLOAT_TO_Q15(x) ((int16_t)((x)*Q15_MAXf + 0.5f)) // float를 Q15으로 변환
#define Q15_TO_FLOAT(x) ((float)(x) / Q15_MAXf)           // Q15을 float로 변환
#define Q15_TO_DOUBLE(x) ((double)(x) / (double)Q15_MAX)           // Q15을 float로 변환

#define POLY_SCALE      4

#define MicN      		MULTI_INPUT_CHANNELS 	// number of microphones 
#define RefN      		2
#define BeamN      		9         		// number of beam channels
#define PolyinputN      MicN+REF_CHANNELS //INPUT_CHANNELS          // number of polyphase input signals
#define PolyOutputN      MicN //OUTPUT_CHANNELS

#define Echo_taps 20 //12
#define Echo_delay 40 //6
// #define Ref_delay 400
#define Ref_delay 4096
#define Ref_delay_max 4096

#define adaptive_BF_taps 16
#define adaptive_BF_delay 6

// #define taps_LP 50
// #define delay_LP 25

#define Echo_single_taps 1024
#define Echo_multi_taps 600

#define PRINT_DURATION 0.2

#define Ma_size_max 8

// #define ITD_min -43
// #define ITD_max 43
// #define ITD_size 87

// #define ITD_min -54
// #define ITD_max 54
// #define ITD_size 109

// #define ITD_min0 -44
// #define ITD_max0 44
// #define ITD_size0 89

// #define ITD_min2 -73
// #define ITD_max2 73
// #define ITD_size2 147

// #define ITD_min1 -63
// #define ITD_max1 63
// #define ITD_size1 127

// #define ITD_min3 -31
// #define ITD_max3 31
// #define ITD_size3 63

#define mode_gun 0
#define mode_normal 1
#define mode_noavg 2

#define ANALYSIS_DURATION 3.0

extern int32_t fft_in_mat[MicN][PolyM*PolyL*2];
extern int32_t fft_ref_mat[RefN][PolyM*PolyL*2];
extern int32_t fft_out_mat[BeamN][PolyM*PolyL*2];
extern int32_t fft_ns_buf_mat[PolyM*(PolyL+Ma_size_max-1)];
extern int32_t fft_bf_mat[BeamN][PolyM*PolyL*2];
extern int32_t fft_bf_rear_mat[BeamN][PolyM*PolyL*2];

typedef struct Total_Inst_s {

    int DC_rej_enable;
    int NS_enable;
    int AGC_enable;
    int AGC_band_enable;

    int bf_enable;

    int DC_beta;
    int poly_scale;
    int g_gainQ15;

    int tuning_delay;

    void * bfInst_p;
    void * polyInst_p;
    void * NSinst_p;
    void * agcInst_p;
    void * vadInst_p;
    void * aecInst_p;
    void * sslInst_p;
    void * MonInst_p;

    int betaQ15_NR1;     // 36
    int max_att_NR1;     // 37
    int min_att_NR1;     // 38
    int slope_NR1;       // 39
    int high_att_NR1;    // 40
    int target_SPL_NR1;  // 41
    
    int betaQ15_NR2;     // 42
    int max_att_NR2;     // 43
    int min_att_NR2;     // 44
    int slope_NR2;       // 45
    int high_att_NR2;    // 46
    int target_SPL_NR2;  // 47

    int betaQ15_NR3;     // 48
    int max_att_NR3;     // 49
    int min_att_NR3;     // 50
    int slope_NR3;       // 51
    int high_att_NR3;    // 52
    int target_SPL_NR3;  // 53


// from Global variables 
    aspl_NR_CONFIG g_nr_config;
    // aspl_nr_params_t g_aspl_nr_params;

    int total_idx; // = 0;

    // int16_t* g_codec_buf=NULL;
    int16_t* g_codec_4mic_buf[MULTI_INPUT_CHANNELS+REF_CHANNELS]; //={NULL, NULL, NULL, NULL, NULL, NULL};
    int16_t* g_data_vad; //=NULL;
    char * g_WorkBuf; //=NULL;

    int32_t * fft_in_mat[MicN]; //[PolyM*PolyL*2];
    int32_t * fft_ref_mat[RefN]; //[PolyM*PolyL*2];
    int32_t * fft_out_mat[BeamN]; //[PolyM*PolyL*2];
    int32_t * fft_ns_buf_mat; //[PolyM*(PolyL+Ma_size_max-1)];
    int32_t * fft_bf_mat[BeamN]; //[PolyM*PolyL*2];
    int32_t * fft_bf_rear_mat[BeamN]; //[PolyM*PolyL*2];

    short	  vad_pre[8];
    short	  vad_min[8];
    short	  vad_max[8];

    int count_pre; //=0;
    int count_post; //=0;

    float fade_gs; // = -30.0;
    float fade_gc; // = 0.0;
    float r_a; // = 0.7;

    int offset_Q15_L; //=0,
    int offset_Q15_R; //=0;
    int offset_Q15_multi[6]; //={0, 0, 0, 0, 0, 0};
    int sample_left; // = 0;

    double DoA_mean; //=0.0;
    int g_Beamno; //=0;
    double g_DoA; //=-1.0;
    int no_DoA_cnt; //=0;

// from Global variables   

} Total_Inst_t;


#endif


