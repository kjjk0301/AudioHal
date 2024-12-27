/*
 * ssl_core.h
 *
 *  Created on: 2019. 7. 3.
 *      Author: Seong Pil Moon
 */

#ifndef SSL_CORE_H_
#define SSL_CORE_H_

#include "pffft.h"

typedef struct sslInst_s {
    short       blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block

    int fbin_min;
    int fbin_max;

    int gun_trig_thresold;
    int gun_trig_thresold2;

    double CCVth;

    int ccv_stored_min;
    int ccv_last_num;

    double gate_dB;

} sslInst_t;


sslInst_t* sysSSLCORECreate();
sslInst_t* sysSSLCORECreate_wall();
void ssl_core_Init(sslInst_t *sslInst);
void ssl_core_Init_wall(sslInst_t *sslInst);
void ssl_core_DeInit();
void ssl_core_process(sslInst_t *sslInst, short *in_L, short *in_R, short *in_F, short *in_B, short ssl_vad, short ssl_vad_long, double *DoA_mean_p);
void ssl_core_process_gun(sslInst_t *sslInst, short *in_L, short *in_R, short *in_F, short *in_B, double *DoA_mean_p, int mode );
void ssl_core_process_linear(sslInst_t *sslInst, short *in_L, short *in_R, short *in_C, double *DoA_mean_p);

extern sslInst_t Sys_sslInst;

// 결과 값을 담을 구조체 정의
typedef struct {
    float dBFS;
    float ratio;
} dBFSResult;



void calculate_peak_temp_total(double *peaklevel, 
                                double *peaktemp, double *peaktotal,
                                double r, int ccv_stored, int peakmax_ccv_idx);

void calculate_ccv_mean(int avg_mode, 
						double *pp_ccv, double *ccv_mean, 
						double *peaktemp, double peaktotal, 
						double *ccv_thre_mean, double *ccv_thre_max, int *ccv_max_idx, 
						int ccv_stored, int peakmax_ccv_idx, int itd_size);

void calculate_ccv(PFFFT_Setup *ssl_ccv_p, double * ccv,
                    float* FFT1, float* FFT2, float* CCVF,
                    int colsize, int fbin_min, int fbin_max, int fsup_ssl_blocksize,
                    double gate_dB);

void calculate_CCVF(float* FFT1, float* FFT2, float* CCVF,
					int fbin_min, int fbin_max, int fsup_ssl_blocksize,
					double gate_dB);

float calculate_dBFS_peak(int16_t samples[], size_t num_samples);
dBFSResult calculate_dBFS_peak_and_ratio(int16_t samples[], size_t num_samples);
dBFSResult calculate_dBFS_peak_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers);
float calculate_dBFS_rms(int16_t samples[], size_t num_samples);
dBFSResult calculate_dBFS_rms_and_ratio(int16_t samples[], size_t num_samples);
dBFSResult calculate_dBFS_rms_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers);
int countZeroCrossings(int16_t *buffer, int size);
void find_outlier_and_average(double arr[], int size, double *average);
int find_outlier(double arr[], int size);
double find_two_outliers(double arr[], int size, int* outlier_idx1, int* outlier_idx2);
double update_noise_floor(double new_rms, double new_peak, double *p_noise_floor, double threshold_dB) ;
double update_peak_max(double noisefloor_dB, double new_rms, double new_peak, double *p_peak_max, double threshold_dB);
double calculate_dbfs(double rms);
double calculate_dbfsfromPower(double power);
double calculate_ratio(double dbfs);
double phase_difference(double phase1, double phase2);
double phase_average(double phases[], int n);


#endif /* SSL_CORE_H_ */


