/*
 * echo_canceller.h
 *
 *  Created on: 2018. 8. 29.
 *      Author: SEONGPIL
 */

#ifndef ECHO_CANCELLER_H_
#define ECHO_CANCELLER_H_

#include "sys.h"

typedef struct aecInst_s
{

    // need malloc
    /*
    int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max];
    float weightsback[FILTER_LENGTH] = {0.0f};
    float weights_second[FILTER_LENGTH] = {0.0f};
    float weightsfore[FILTER_LENGTH] = {0.0f};

    int32_t uBuf[FILTER_LENGTH+FRAME_SIZE-1] = {0}; // 32비트 정수 버퍼
    int32_t uBuf2[FILTER_LENGTH+FRAME_SIZE-1] = {0}; // 32비트 정수 버퍼

    float leaky[FILTER_LENGTH];

    float trans_window1024[FRAME_SIZE*2];
    float trans_window1600[FRAME_SIZE*2];

    int16_t ref_Dly_Buffer[DELAY_BUFFER_SIZE] = {0}; // 레퍼런스 신호 버퍼
    int16_t mic_Dly_Buffer[DELAY_BUFFER_SIZE] = {0}; // 마이크 신호 버퍼

    long long crossCorr[MAX_DELAY * 2 + 1] = {0}; // 누적 크로스 코릴레이션 결과
    */
    // need malloc

    int16_t *d_buf_g[MicN + RefN];
    float *weightsback;
    float *weights_second;
    float *weightsfore;
    int32_t *uBuf;  // 32비트 정수 버퍼
    int32_t *uBuf2; // 32비트 정수 버퍼
    float *leaky;
    float *trans_window512;
    float *trans_window1024;
    float *trans_window1600;
    int16_t *ref_Dly_Buffer; // 레퍼런스 신호 버퍼
    int16_t *mic_Dly_Buffer; // 마이크 신호 버퍼
    long long *crossCorr;    // 누적 크로스 코릴레이션 결과
    float *prod;

    float prevAttenuation;
    float prevAttenuation_second;
    float refScale; // 스케일링 팩터 (32비트 정수)
    // float micScale; // 스케일링 팩터 (32비트 정수)

    float Davg1; /* 1st recursive average of the residual power difference */
    float Davg2; /* 2nd recursive average of the residual power difference */
    float Dvar1; /* Estimated variance of 1st estimator */
    float Dvar2; /* Estimated variance of 2nd estimator */
    float sum_adapt;
    int adapted;
    float adapt_rate;
    int update_fore;
    // 크로스 코릴레이션을 사용하여 글로벌 딜레이 계산 (정수 연산)

    int frameCount;     // 처리한 프레임 수
    int estimatedDelay; // 추정된 글로벌 딜레이
    int delayupdated;
    int delayinit;
    int corr_hold;

    int32_t mem_r;
    int32_t mem_d;
    int32_t mem_d2;
    int32_t mem_r_d;
    int32_t mem_d_d;
    int32_t mem_d_d2;

    float gate_aec;

} aecInst_t;

aecInst_t *sysAECCreate();
void AEC_Init(aecInst_t *aecInst);
int AEC_delay_with_framelen(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay, int framelen);
void AEC_delay_init(aecInst_t *aecInst, short *d_Buf, int delay);

// 함수 선언
float calculate_rms_int32(int16_t *signal, int length);
void smooth_normalize_signal_rms_int32(int16_t *signal, float rmsVal, int length, float *currentScale, float targetRMSdB);
void nlms_echo_canceller(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);
void nlms_echo_canceller_two_path(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);
void nlms_echo_canceller_two_path_strong(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);

void calculate_global_delay(int16_t *refBuffer, int16_t *micBuffer, int framelen, long long *crossCorr, int *frameCount, int *estimatedDelay, int *delayinit, int *delayupdated);
void AEC_single_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, float MicscaledB);
void AEC_2ch_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB);
int AEC_single_Proc_filter_save(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB, float** filter, int* filter_len, int* globaldelay);
void AEC_single_filter_load(aecInst_t *aecInst, float* filter, int filter_len, int global_delay);
int pre_emphasis(int16_t *signal, int32_t pre_emph_q15, int32_t *mem, size_t frame_size);
int de_emphasis(int16_t *signal, int32_t de_emph_q15, int32_t *mem, size_t frame_size);

// extern aecInst_t Sys_aecInst;
// extern int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max];

#endif /* ECHO_CANCELLER_H_ */
