/*
 * echo_canceller.c
 *
 *  Created on: 2018. 8. 29.
 *      Author: SEONGPIL
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h> // clock() 함수를 사용하기 위한 헤더
#include "sys.h"
// #include "ssl_core.h"
#include "echo_canceller.h"
#include "debug_file.h"

#define DEBUG_AEC

#define FRAME_SIZE 1600 // 프레임 크기
#define FILTER_LENGTH 600
#define FILTER_LENGTH2 600
#define LEAKY_LENGTH 1500
#define STEP_SIZE 1.0f // 스텝 사이즈 (부동소수점)
#define STEP_SIZE2 0.07f // 스텝 사이즈 (부동소수점)
#define MIN_POWER -45.0f // dBFS 기준 최소 파워
#define SAMPLE_RATE 16000 // 샘플링 주파수
#define LONG_LONG_MIN (-9223372036854775807LL - 1) // 최소 long long 값 정의
#define MAX_DELAY 200 // 최대 딜레이 범위 (±2000 샘플)
#define DELAY_BUFFER_SIZE (1600 + MAX_DELAY * 2) // 추가 버퍼 크기

// aecInst_t Sys_aecInst;

// Global variables <- need to be included in structure

// need malloc
// int32_t w_real_Q33[FILTER_LENGTH] = {0}; 
// int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max] = {0}; 
// float weightsback[FILTER_LENGTH] = {0.0f};
// float weights_second[FILTER_LENGTH] = {0.0f};
// float weightsfore[FILTER_LENGTH] = {0.0f};
// int32_t uBuf[FILTER_LENGTH+FRAME_SIZE-1] = {0}; // 32비트 정수 버퍼
// int32_t uBuf2[FILTER_LENGTH+FRAME_SIZE-1] = {0}; // 32비트 정수 버퍼
// float leaky[FILTER_LENGTH] = {0.0f};
// float trans_window1024[FRAME_SIZE*2] = {0.0f}; 
// float trans_window1600[FRAME_SIZE*2] = {0.0f}; 
// int16_t ref_Dly_Buffer[DELAY_BUFFER_SIZE] = {0}; // 레퍼런스 신호 버퍼
// int16_t mic_Dly_Buffer[DELAY_BUFFER_SIZE] = {0}; // 마이크 신호 버퍼
// long long crossCorr[MAX_DELAY * 2 + 1] = {0}; // 누적 크로스 코릴레이션 결과
// float prod[FILTER_LENGTH] = {0.0f};
// need malloc

// float prevAttenuation = 0.0f;
// float prevAttenuation_second = 0.0f;
// float refScale = 0.5; // 스케일링 팩터 (32비트 정수)
// float micScale = 4.5; // 스케일링 팩터 (32비트 정수)

// float  Davg1=0.0f;  /* 1st recursive average of the residual power difference */
// float  Davg2=0.0f;  /* 2nd recursive average of the residual power difference */
// float   Dvar1=0.0f;  /* Estimated variance of 1st estimator */
// float   Dvar2=0.0f;  /* Estimated variance of 2nd estimator */
// float sum_adapt=0.0f;
// int adapted = 0;
// float adapt_rate = 0;
// int update_fore = 0;
// 크로스 코릴레이션을 사용하여 글로벌 딜레이 계산 (정수 연산)

// int frameCount = 0; // 처리한 프레임 수
// int estimatedDelay = 0; // 추정된 글로벌 딜레이    
// int delayupdated = 0;
// int delayinit = 0;
// int corr_hold = 0;

// int32_t mem_r;
// int32_t mem_d;
// int32_t mem_r_d;
// int32_t mem_d_d;

// float gate_aec = 1.0;
// Global variables <- need to be included in structure

void generate_pink_noise(float *noiseBuffer, int length, float noiseLevel);
void add_comfort_noise(int16_t *signal, float *noise, int length, float noiseGainDbfs);
void calc_prod(float * prod, float * weight, int32_t filt_len);

aecInst_t* sysAECCreate(int channum)
{
    aecInst_t * aecInst_p = malloc(sizeof(aecInst_t));
    AEC_Init(aecInst_p);
    aecInst_p->chan_num = channum;

    return aecInst_p;
}

void AEC_DeInit(aecInst_t *aecInst)
{
    aecInst_t *inst = aecInst;

    if(inst) {
        if ((inst->d_buf_g) != NULL) // by skj
        {
            for (int i = 0; i < MicN + RefN; i++)
            {
                free(inst->d_buf_g[i]);
            }
        }

        if (inst->weightsback != NULL)
            free(inst->weightsback);

        if (inst->weights_second != NULL)
            free(inst->weights_second);

        if (inst->weightsfore != NULL)
            free(inst->weightsfore);

        if (inst->uBuf != NULL)
            free(inst->uBuf);

        if (inst->uBuf2 != NULL)
            free(inst->uBuf2);

        if (inst->leaky != NULL)
            free(inst->leaky);

        if (inst->trans_window256 != NULL)
            free(inst->trans_window256);

        if (inst->trans_window512 != NULL)
            free(inst->trans_window512);

        if (inst->trans_window1024 != NULL)
            free(inst->trans_window1024);

        if (inst->trans_window1600 != NULL)
            free(inst->trans_window1600);

        if (inst->ref_Dly_Buffer != NULL)
            free(inst->ref_Dly_Buffer);

        if (inst->mic_Dly_Buffer != NULL)
            free(inst->mic_Dly_Buffer);

        if (inst->crossCorr != NULL)
            free(inst->crossCorr);

        if (inst->prod != NULL)
            free(inst->prod);

        free(inst);
        inst = NULL;
        
    }

 
#ifdef DEBUG_SSL_VAD
    debug_file_close();
#endif
}

void AEC_Init(aecInst_t *aecInst)
{

    // int k, m, muQ15;
    // float mu;
    aecInst_t *inst = (aecInst_t *)aecInst;

    ///////////////////////////////////////////////// malloc
    for (int i = 0; i < MicN + RefN; i++)
    {
        inst->d_buf_g[i] = (int16_t *)malloc((Ref_delay_max + framesize_max) * sizeof(int16_t));
        if (inst->d_buf_g[i] == NULL)
        {
            perror("AEC_Init malloc fail: d_buf_g");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(inst->d_buf_g[j]);
            }
            return;
        }
        memset(inst->d_buf_g[i], 0, sizeof(short) * (Ref_delay_max + framesize_max));
    }

    inst->weightsback = (float *)malloc(FILTER_LENGTH * sizeof(float)); // by skj
    if (inst->weightsback == NULL)
    {
        perror("AEC_Init malloc fail: weightsback");
        return;
    }

    inst->weights_second = (float *)malloc(FILTER_LENGTH * sizeof(float)); // by skj
    if (inst->weights_second == NULL)
    {
        perror("AEC_Init malloc fail: weights_second");
        return;
    }

    inst->weightsfore = (float *)malloc(FILTER_LENGTH * sizeof(float)); // by skj
    if (inst->weightsfore == NULL)
    {
        perror("AEC_Init malloc fail: weightsfore");
        return;
    }

    inst->uBuf = (int32_t *)malloc((FILTER_LENGTH + FRAME_SIZE - 1) * sizeof(int32_t)); // by skj
    if (inst->uBuf == NULL)
    {
        perror("AEC_Init malloc fail: uBuf");
        return;
    }

    inst->uBuf2 = (int32_t *)malloc((FILTER_LENGTH + FRAME_SIZE - 1) * sizeof(int32_t)); // by skj
    if (inst->uBuf2 == NULL)
    {
        perror("AEC_Init malloc fail: uBuf2");
        return;
    }

    inst->leaky = (float *)malloc(FILTER_LENGTH * sizeof(float)); // by skj
    if (inst->leaky == NULL)
    {
        perror("AEC_Init malloc fail: leaky");
        return;
    }

    inst->trans_window256 = (float *)malloc(FRAME_SIZE * 2 * sizeof(float)); // by skj
    if (inst->trans_window256 == NULL)
    {
        perror("AEC_Init malloc fail: trans_window512");
        return;
    }

    inst->trans_window512 = (float *)malloc(FRAME_SIZE * 2 * sizeof(float)); // by skj
    if (inst->trans_window512 == NULL)
    {
        perror("AEC_Init malloc fail: trans_window512");
        return;
    }

    inst->trans_window1024 = (float *)malloc(FRAME_SIZE * 2 * sizeof(float)); // by skj
    if (inst->trans_window1024 == NULL)
    {
        perror("AEC_Init malloc fail: trans_window1024");
        return;
    }

    inst->trans_window1600 = (float *)malloc(FRAME_SIZE * 2 * sizeof(float)); // by skj
    if (inst->trans_window1600 == NULL)
    {
        perror("AEC_Init malloc fail: trans_window1600");
        return;
    }

    inst->ref_Dly_Buffer = (int16_t *)malloc(DELAY_BUFFER_SIZE * sizeof(int16_t)); // by skj
    if (inst->ref_Dly_Buffer == NULL)
    {
        perror("AEC_Init malloc fail: ref_Dly_Buffer");
        return;
    }

    inst->mic_Dly_Buffer = (int16_t *)malloc(DELAY_BUFFER_SIZE * sizeof(int16_t)); // by skj
    if (inst->mic_Dly_Buffer == NULL)
    {
        perror("AEC_Init malloc fail: mic_Dly_Buffer");
        return;
    }

    inst->crossCorr = (long long *)malloc((MAX_DELAY * 2 + 1) * sizeof(long long)); // by skj
    if (inst->crossCorr == NULL)
    {
        perror("AEC_Init malloc fail: crossCorr");
        return;
    }

    inst->prod = (float *)malloc(FILTER_LENGTH * sizeof(float)); // by skj
    if (inst->prod == NULL)
    {
        perror("AEC_Init malloc fail: prod");
        return;
    }    
    ///////////////////////////////////////////////// malloc

    for (int i = 0; i < 256 * 2; i++)
        inst->trans_window256[i] = .5 - .5 * cos(2 * M_PI * i / (256 * 2));

    for (int i = 0; i < 512 * 2; i++)
        inst->trans_window512[i] = .5 - .5 * cos(2 * M_PI * i / (512 * 2));

    for (int i = 0; i < 1024 * 2; i++)
        inst->trans_window1024[i] = .5 - .5 * cos(2 * M_PI * i / (1024 * 2));

    for (int i = 0; i < 1600 * 2; i++)
        inst->trans_window1600[i] = .5 - .5 * cos(2 * M_PI * i / (1600 * 2));

    for (int i = 0; i < FILTER_LENGTH; i++)
    {
        inst->weightsback[i] = 0.0f;
        inst->weights_second[i] = 0.0f;
        inst->weightsfore[i] = 0.0f;
        inst->leaky[i] = 1.0;
        inst->prod[i] = 0.0;
    }

    // // 감소 비율 계산
    // float decrement = (1.0f - 0.99999f) / (FILTER_LENGTH - LEAKY_LENGTH - 1);

    // leaky 계수를 1.0f에서 점진적으로 감소시키기
    // for (int i = LEAKY_LENGTH; i < FILTER_LENGTH; i++) {
    //     leaky[i] = 1.0f - decrement * (i - LEAKY_LENGTH);
    // }  

    for (int i = 0; i < (FILTER_LENGTH + FRAME_SIZE - 1); i++)
    {
        inst->uBuf[i] = 0;  // 32비트 정수 버퍼
        inst->uBuf2[i] = 0; // 32비트 정수 버퍼
    }

    inst->prevAttenuation = 0.0f;
    inst->prevAttenuation_second = 0.0f;
    inst->refScale = 0.5; // 스케일링 팩터 (32비트 정수)
    // inst->micScale = 4.5; // 스케일링 팩터 (32비트 정수)

    inst->Davg1 = 0.0f; /* 1st recursive average of the residual power difference */
    inst->Davg2 = 0.0f; /* 2nd recursive average of the residual power difference */
    inst->Dvar1 = 0.0f; /* Estimated variance of 1st estimator */
    inst->Dvar2 = 0.0f; /* Estimated variance of 2nd estimator */
    inst->sum_adapt = 0.0f;
    inst->adapted = 0;
    inst->adapt_rate = 0;
    inst->update_fore = 0;

    for (int i = 0; i < (DELAY_BUFFER_SIZE); i++)
    {
        inst->ref_Dly_Buffer[i] = 0; // 레퍼런스 신호 버퍼
        inst->mic_Dly_Buffer[i] = 0; // 마이크 신호 버퍼
    }

    for (int i = 0; i < (MAX_DELAY * 2 + 1); i++)
    {
        inst->crossCorr[i] = 0;
    }

    inst->frameCount = 0;     // 처리한 프레임 수
    inst->estimatedDelay = 0; // 추정된 글로벌 딜레이
    inst->delayupdated = 0;
    inst->delayinit = 0;
    inst->corr_hold = 0;

    inst->mem_r = 0;
    inst->mem_d = 0;
    inst->mem_r_d = 0;
    inst->mem_d_d = 0;

    inst->gate_aec = 1.0;
}

void AEC_delay_init(aecInst_t *aecInst, short *d_Buf, int delay)
{

    aecInst_t *inst = (aecInst_t *)aecInst;

    short *delay_Buf = (short *)d_Buf;

    memset(&delay_Buf[0], 0, sizeof(short) * (Ref_delay_max + framesize_max));
}

int AEC_delay_with_framelen(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay, int framelen)
{
    int i;
    unsigned char *tempPtr1, *tempPtr2;

    aecInst_t *inst = (aecInst_t *)aecInst;

    short *xin = (short *)inBuf;
    short *xout = (short *)outBuf;
    short *delay_Buf = (short *)d_Buf;

    int blocksize = framelen;

    if ((framelen + delay) > (Ref_delay_max + framesize_max))
    {
        printf("total delay buffer length too liong, not support\r\n");
        return -1;
    }

    tempPtr1 = (unsigned char *)&delay_Buf[delay];
    tempPtr2 = (unsigned char *)&xin[0];
    for (i = 0; i < blocksize; i++)
    {
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1 = (unsigned char *)&xout[0];
    tempPtr2 = (unsigned char *)&delay_Buf[0];
    for (i = 0; i < blocksize; i++)
    {
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1 = (unsigned char *)&delay_Buf[0];
    tempPtr2 = (unsigned char *)&delay_Buf[blocksize];
    for (i = 0; i < delay; i++)
    {
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    return 0;
}

// RMS 계산 함수 (32비트 정수)
float calculate_rms_int32(int16_t *signal, int length)
{
    long long sum = 0;
    for (int i = 0; i < length; i++)
    {
        sum += (int32_t)signal[i] * (int32_t)signal[i];
    }
    return sqrtf((float)sum / (float)length);
}

// 신호를 부드럽게 노멀라이즈하는 함수 (32비트 정수)
void smooth_normalize_signal_rms_int32(int16_t *signal, float rmsVal, int length, float *currentScale, float targetRMSdB)
{
    
    float targetRMS = (pow(10, targetRMSdB / 20.0) * 32768.0f); // 1842.68f; //3276.8f; //(pow(10, targetRMSdB / 20.0) * (1 << 15)); // -20dB에 해당하는 RMS 값

    if (rmsVal > 0)
    {
        float newScale = targetRMS / rmsVal;
        *currentScale = (0.95f * (*currentScale) + 0.05f * newScale);
        int32_t scaleQ15 = (*currentScale) * 32768.0f;

        // printf("rmsVal=%f, targetRMS=%f, newScale=%f, currentScale=%f \r\n", rmsVal, targetRMS,  newScale, *currentScale);

        for (int i = 0; i < length; i++)
        {
            signal[i] = (int16_t)(((int32_t)signal[i] * (int32_t)(scaleQ15)) >> 15);
        }
    }
}

// #define ENABLE_PROFILING2 // 연산 시간 측정 활성화

#ifdef ENABLE_PROFILING2
#define NUM_STEPS 8 // 총 연산 단계 수
#define PRINT_INTERVAL 1.0 // 평균을 출력할 간격 (초)
    static double totalTime[NUM_STEPS] = {0.0};
    static int count = 0;
    clock_t start, end;
    clock_t total_start, total_end;
    double stepTime;
    double totalRunTime = 0.0;
#endif


// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller_2ch_two_path(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {
    
    aecInst_t *inst = (aecInst_t *)aecInst;

    const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[2][FILTER_LENGTH];
    float weights_slow[2][FILTER_LENGTH];
    float error_fore[2][FRAME_SIZE];
    float error_back[2][FRAME_SIZE];
    int32_t y_fore[2][FRAME_SIZE];
    int32_t y_back[2][FRAME_SIZE];

    float * trans_window = &inst->trans_window1024[0]; 

    if (length == 1024) {
        trans_window = &inst->trans_window1024[0];
    } else if (length == 1600) {
        trans_window = &inst->trans_window1600[0];
    } else if (length == 512) {
        trans_window = &inst->trans_window512[0];
    } else {

        for (int i = 0; i < length * 2; i++) {
            inst->trans_window1600[i] = .5 - .5 * cos(2 * M_PI * i / (length * 2));        
        }
            
        trans_window = &inst->trans_window1600[0];
    }
    // for (int i=0;i<length*2;i++)
    //     trans_window[i] = .5-.5*cos(2*M_PI*i/(length*2));    


    for (int n=0; n<FILTER_LENGTH-1; n++){
        x[n]=x[n+length];
    }

    for (int n=0; n<length; n++){
        x[n+FILTER_LENGTH-1]=refSignal[n];
    }

    // Foreground filtering
    for (int j = 0; j < 2 ; j++){
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weightsQ15[j][i] = (int32_t)(weights_fore[i+j*FILTER_LENGTH] * 32768.0f);
        }   

        for (int n = 0; n < length; n++) {

            int curidx=FILTER_LENGTH-1+n;
        
            int32_t y = 16384;
            int m = curidx;
            for (int i = 0; i < FILTER_LENGTH; i++) {
                y += (weightsQ15[j][i] * x[m]);
                m--;
            }

            y = y>>15;

            y_fore[j][n] = y;

            // 잔차 신호 계산
            float e = (float)micSignal[n+j*length] -  (float)y;
            error_fore[j][n] = e;
        }
    }

    // Calc foreground filter residual power
    float Sff = 0.0f;
    for (int j = 0; j < 2 ; j++){
        for (int n = 0; n < length; n++) {
            Sff += error_fore[j][n] * error_fore[j][n];
        }
    }

    float normFactor = 500.0f;
    for (int n=0; n<FILTER_LENGTH; n++){
        normFactor += (float)((int32_t)x[n+length-1] * (int32_t)x[n+length-1]);
    }
    
    // Calc foreground filter residual power
    float Sxx = 0.0f;
    for (int n = 0; n < length; n++) {
        Sxx += (float)((int32_t)refSignal[n] * (int32_t)refSignal[n]);
    }

    // Calc foreground filter residual power
    float Sdd = 0.0f;
    for (int j = 0; j < 2 ; j++){
        for (int n = 0; n < length; n++) {
            Sdd += (float)((int32_t)micSignal[n+j*length] * (int32_t)micSignal[n+j*length]);
        }
    }
    float micPower = sqrtf(Sdd / (float)length / 2.0f);

    float step_size1000 = STEP_SIZE * 10000;

    float mu;
    // 스텝 크기 조정
    if (updateon==1) {
        if (*prevAttenuation > 25.0f || refPower < -50.0f){
            mu = 0.1f * step_size1000 / normFactor;
        } else if (inst->adapted){
            mu = 0.5f * step_size1000 / normFactor;
        }
        // mu = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? 0.1f * step_size1000 / normFactor : step_size1000 / normFactor;
        // n_mu_Q28 = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? n_mu_Q28>>3 : n_mu_Q28;
    } else {
        mu = 0.05f * step_size1000 / normFactor;
        // n_mu_Q28 = n_mu_Q28>>3;        
    }
    // printf("normFactor=%f mu=%f\r\n", normFactor, mu);

    float Attenuationtmp = *prevAttenuation;

    int itermax = 1;
    // if (inst->adapted==0 && Attenuationtmp <= 20.0f){
    //     // if (refPower > -30.0f ){
    //         itermax = 5;
    //     //     printf("itermax = 5\r\n");
    //     // } else if (refPower > -40.0f ){
    //     //     itermax = 4;
    //     //     printf("itermax = 4\r\n");
    //     // } else {
    //     //     itermax = 3;
    //     //     printf("itermax = 3\r\n");
    //     // }
    // }

    for (int j = 0; j < 2 ; j++){
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weights_slow[j][i] = weights[i+j*FILTER_LENGTH];
        }
    }

    float See = 0.0f;
    for (int m=0; m<itermax; m++){
        for (int j = 0; j < 2 ; j++){
            for (int n = 0; n < length; n++) {

                int curidx=FILTER_LENGTH-1+n;

                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[j][i] = (int32_t)(weights[i+j*FILTER_LENGTH] * 32768.0f);
                }            

                int32_t y = 16384;
                    int m = curidx;
                for (int i = 0; i < FILTER_LENGTH; i++) {
                        y += (weightsQ15[j][i] * x[m]);
                        m--;
                }

                y = y>>15;

                y_back[j][n] = y;

                // 잔차 신호 계산
                float e = (float)(micSignal[n+j*length] -  y);
                error_back[j][n] = e;
                // errorSignal[n]=(int16_t)e;

                if (updateon==1) {
                    float e_mu_temp = mu * e;
                    m = curidx;
                    for (int i = 0; i < FILTER_LENGTH; i++) {
                        weights[i+j*FILTER_LENGTH] = weights[i+j*FILTER_LENGTH] + e_mu_temp * (float)x[m];
                        m--;
                    }
                }
            }

            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights_slow[j][i] = weights_slow[j][i] * 0.99 + weights[i+j*FILTER_LENGTH] * 0.01;
            } 
        }                   

            // mu *= 0.7;s

        // Calc foreground filter residual power
        See = 0.0f;
        for (int j = 0; j < 2 ; j++){
            for (int n = 0; n < length; n++) {
                See += error_back[j][n] * error_back[j][n];
            }
        }

        float errorPowertemp = sqrtf(See / (float)length / 2.0f);
        Attenuationtmp = 20.0f * log10f(micPower / (errorPowertemp + 1));  

        if (Attenuationtmp>20.0f) break;

    }    

    int32_t dbf[2][FRAME_SIZE];
    // Calc power of Difference in response
    for (int j = 0; j < 2 ; j++){
        for (int n = 0; n < length; n++) {
            dbf[j][n]=y_fore[j][n]-y_back[j][n];
        }
    }

    float Dbf = 0.0f;
    for (int j = 0; j < 2 ; j++){
        for (int n = 0; n < length; n++) {
            Dbf += (float)dbf[j][n] * (float)dbf[j][n];
        }
    }

    inst->Davg1 = .6*inst->Davg1 + .4*(Sff-See);
    inst->Davg2 = .85*inst->Davg2 + .15*(Sff-See);
    inst->Dvar1 = .36*inst->Dvar1 + .16*Sff*Dbf;
    inst->Dvar2 = .7225*inst->Dvar2 + .0225*Sff*Dbf;

    inst->update_fore = 0;
    if ((Sff-See)*fabsf(Sff-See) > Sff*Dbf) inst->update_fore = 1;
    if ((inst->Davg1)*fabsf(inst->Davg1) > 0.5*inst->Dvar1) inst->update_fore = 2;
    if ((inst->Davg2)*fabsf(inst->Davg2) > 0.25*inst->Dvar2) inst->update_fore = 3;

    for (int j = 0; j < 2 ; j++){
        for (int n = 0; n < length; n++) {
            errorSignal[n+j*length]=(int16_t)error_fore[j][n];
        }
    }

    
    if (inst->update_fore) {

        for (int j = 0; j < 2 ; j++){
            for (int i = 0; i < FILTER_LENGTH; i++) {
                // weights_fore[i] = weights_fore[i]*0.75 + weights[i]* 0.25; // 필터 계수를 0으로 초기화
                // weights_fore[i] = weights[i];
                weights_fore[i+j*FILTER_LENGTH] = weights_slow[j][i];
            }
            
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weightsQ15[j][i] = (int32_t)(weights[i+j*FILTER_LENGTH] * 32768.0f);
            }            
        }

        inst->Davg1 = 0.0;
        inst->Davg2 = 0.0;
        inst->Dvar1 = 0.0;
        inst->Dvar2 = 0.0;         

        for (int j = 0; j < 2 ; j++){
            for (int n = 0; n < length; n++) {
                errorSignal[n+j*length]=(int16_t)((float)micSignal[n+j*length] - ((float)y_back[j][n]*trans_window[n] + (float)y_fore[j][n]*trans_window[n+length]));
                // errorSignal[n]=(int16_t)error_back[n];
            }
        // printf("update fore\n");
        }

        } else {
        int reset_back = 0;
        if (-1.0*(Sff-See)*fabsf(Sff-See) > 4.0*Sff*Dbf) reset_back = 1;
        if (-1.0*(inst->Davg1)*fabsf(inst->Davg1) > 4.0*inst->Dvar1) reset_back = 1;
        if (-1.0*(inst->Davg2)*fabsf(inst->Davg2) > 4.0*inst->Dvar2) reset_back = 1;


        if (reset_back) {
            for (int j = 0; j < 2 ; j++){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weights[i+j*FILTER_LENGTH] = weights_fore[i+j*FILTER_LENGTH];
                }
            }

            See = Sff;
            inst->Davg1 = 0.0;
            inst->Davg2 = 0.0;
            inst->Dvar1 = 0.0;
            inst->Dvar2 = 0.0;

            for (int j = 0; j < 2 ; j++){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[j][i] = (int32_t)(weights[i+j*FILTER_LENGTH] * 32768.0f);
                }                
            }

            // printf("reset back\n");
        }  else {
            for (int j = 0; j < 2 ; j++){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[j][i] = (int32_t)(weights_fore[i+j*FILTER_LENGTH] * 32768.0f);
                }            
            }
            // printf("just go\n");
        }
    }

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int j = 0; j < 2 ; j++){
        for (int i = 0; i < FILTER_LENGTH; i++) {
            if (fabs(weights[i+j*FILTER_LENGTH]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
                resetWeights = 1; // 참
                break;
            }
        }
    }
    if (resetWeights) {
        for (int j = 0; j < 2 ; j++){
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i+j*FILTER_LENGTH] = 0.0f; // 필터 계수를 0으로 초기화
            }
        }
        inst->adapted = 0;
        inst->sum_adapt = 0;
        printf("Weights reset to prevent divergence.\n");
    }

    // 감쇄량 계산

    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length * 2);
    // prevAttenuation_ratio = 0.7 * prevAttenuation_ratio + 0.3 * (micPower / (errorPower + 1));
    // *prevAttenuation = 20.0f * log10f(prevAttenuation_ratio);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));
    // printf("micPower= %.2f, errorPower=%.2f, Attenuation = %.2f updateon=%d\n",20.0f * log10f(micPower)-90.3f, 20.0f * log10f(errorPower)-90.3f, *prevAttenuation, updateon);

    if (updateon==1) {
        float tmp = Sxx;
        if (tmp>See){
            tmp = See;
        }
        inst->adapt_rate = 0.25* tmp / (See+1);
        inst->sum_adapt += inst->adapt_rate;
    }

    if (!inst->adapted && inst->sum_adapt > 7 && *prevAttenuation > 20.0f)
    {    
        inst->adapted = 1;
    }
#ifdef DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
       
        FloatPacket packet;
        // packet.val1 = (float)((Sff-See)*fabsf(Sff-See));
        // packet.val2 = (float)(Sff*Dbf);
        // packet.val3 = (float)((Davg1)*fabsf(Davg1));
        // packet.val4 = (float)(0.5*Dvar1);
        // packet.val5 = (float)((Davg2)*fabsf(Davg2));
        // packet.val6 = (float)(0.25*Dvar2);

        packet.val1 = (float)calculate_rms_int32((int16_t *)refSignal, length);
        packet.val2 = (float)micPower;
        packet.val3 = (float)micPower;
        packet.val4 = (float)errorPower;
        packet.val5 = (float)inst->sum_adapt;
        packet.val6 = (float)inst->adapt_rate;      

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &weightsQ15[0][0], FILTER_LENGTH);
        debug_matlab_ssl_float_send(10, (*prevAttenuation));
        // debug_matlab_ssl_float_send(10, (Dbf));
    }

	if ((g_aec_debug_on==1)&&(inst->chan_num==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 

}

// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller_two_path(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {

    aecInst_t *inst = (aecInst_t *)aecInst;

    const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[FILTER_LENGTH];
    int32_t error_fore[FRAME_SIZE];
    int32_t error_back[FRAME_SIZE];
    int32_t y_fore[FRAME_SIZE];
    int32_t y_back[FRAME_SIZE];
    float * trans_window = &inst->trans_window1024[0];

    float weights_slow[FILTER_LENGTH];
    float e_mu_temp = 0;
#ifdef fixed_point_aec    
    int32_t n_x_Q15[FILTER_LENGTH];
    int32_t n_e_mu_Q23 = 0;
#endif


    if (length == 1024) {
        trans_window = &inst->trans_window1024[0];
    } else if (length == 1600) {
        trans_window = &inst->trans_window1600[0];
    } else if (length == 512) {
        trans_window = &inst->trans_window512[0];
    } else if (length == 256) {
        trans_window = &inst->trans_window256[0];        
    } else {
        for (int i = 0; i < length * 2; i++) {
            inst->trans_window1600[i] = .5 - .5 * cos(2 * M_PI * i / (length * 2));        
        }           
        trans_window = &inst->trans_window1600[0];
    }

#ifdef ENABLE_PROFILING2
        total_start = clock();
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n=0; n<FILTER_LENGTH-1; n++){
        x[n]=x[n+length];
    }

    for (int n=0; n<length; n++){
        x[n+FILTER_LENGTH-1]=refSignal[n];
    }

    // Foreground filtering
    for (int i = 0; i < FILTER_LENGTH; i++) {
        weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
    }    


#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[0] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n = 0; n < length; n++) {

        int curidx=FILTER_LENGTH-1+n;
       
        int32_t y = 16384;
        int m = curidx;
        for (int i = 0; i < FILTER_LENGTH; i++) {
            y += (weightsQ15[i] * x[m--]);
            // m--;
        }

        y = y>>15;

        y_fore[n] = y;

        // 잔차 신호 계산
        int32_t e = (micSignal[n] -  y);
        error_fore[n] = e;
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[1] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    // Calc foreground filter residual power
    float Sff = 0.0f;
    for (int n = 0; n < length; n++) {
        Sff += (float)(error_fore[n] * error_fore[n]);
    }

    // Calc foreground filter residual power
    float Sxx = 0.0f;
    for (int n = 0; n < length; n++) {
        Sxx += (float)((int32_t)refSignal[n] * (int32_t)refSignal[n]);
    }

    // Calc foreground filter residual power
    float Sdd = 0.0f;
    for (int n = 0; n < length; n++) {
        Sdd += (float)((int32_t)micSignal[n] * (int32_t)micSignal[n]);
    }
    float micPower = sqrtf(Sdd / (float)length);

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[2] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      

    float normFactor = 500.0f;
    for (int n=0; n<FILTER_LENGTH; n++){
        normFactor += (float)(x[n+length-1] * x[n+length-1]);
    }

#ifdef fixed_point_aec
    int32_t half_normFactor = 3000;
    for (int n=0; n<FILTER_LENGTH; n++){
        half_normFactor += abs(x[n+length-1]);
    }

    int32_t Q28MAX = 268435456;
    float half_norm_inv_Q28f = (float)(Q28MAX) / (float)half_normFactor;
    float mu_Q15f = STEP_SIZE * Q15_MAXf;
    int32_t n_mu_Q28 = (int32_t)(half_norm_inv_Q28f * mu_Q15f)>>15;
#endif

    float step_size1000 = STEP_SIZE * 10000;

    float mu;
    // 스텝 크기 조정
    if (updateon==1) {
        if (*prevAttenuation > 15.0f || refPower < -50.0f){
            mu = 0.1f * step_size1000 / normFactor;
        } else if (inst->adapted){
            mu = 0.7f * step_size1000 / normFactor;
        }
        // mu = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? 0.1f * step_size1000 / normFactor : step_size1000 / normFactor;
        // n_mu_Q28 = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? n_mu_Q28>>3 : n_mu_Q28;
    } else {
        mu = 0.05f * step_size1000 / normFactor;
        // n_mu_Q28 = n_mu_Q28>>3;        
    }
    // printf("normFactor=%f mu=%f\r\n", normFactor, mu);

    float Attenuationtmp = *prevAttenuation;

    int itermax = 1;
    // if (inst->adapted==0 && Attenuationtmp <= 20.0f){
    //     if (refPower > -40.0f ){
    //         itermax = 5;
    //     //     printf("itermax = 5\r\n");
    //     // } else if (refPower > -40.0f ){
    //     //     itermax = 4;
    //     //     printf("itermax = 4\r\n");
    //     // } else {
    //     //     itermax = 3;
    //     //     printf("itermax = 3\r\n");
    //     }
    // }

    for (int i = 0; i < FILTER_LENGTH; i++) {
        weights_slow[i] = weights[i];
    }
    int filt_iter = FILTER_LENGTH;

    float See = 0.0f;
    for (int nn=0; nn<itermax; nn++){
        if (nn>1) filt_iter = 300;
        for (int n = 0; n < length; n++) {
            int curidx=FILTER_LENGTH-1+n;
            for (int i = 0; i < filt_iter; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }

            int32_t y = 16384;
            int m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                y += (weightsQ15[i] * x[m--]);
                // y += ((w_real_Q33[i]+128)>>8 * x[m--]);
            }

#ifdef fixed_point_aec
            // filtering
            int32_t temp_real=16384; // (1<<14)
            m=curidx;
            for (int k=0; k<FILTER_LENGTH; k++){
                temp_real=temp_real + (((w_real_Q33[k]+128)>>8) * (int32_t)(x[m]));
                m--;
            }      

            int32_t y =  temp_real>>15;      
#endif
            y = y>>15;

            y_back[n] = y;

            // 잔차 신호 계산
            int32_t e = micSignal[n] -  y;
            error_back[n] = e;

#ifdef fixed_point_aec            
            n_e_mu_Q23 = (n_mu_Q28 * e + 32)>>5;            

            m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                n_x_Q15[i] = (int32_t)((float)x[m]*half_norm_inv_Q28f)>>13;
                m--;
            }
#endif            
            if (updateon==1) {
                e_mu_temp = mu * e;
#ifdef fixed_point_aec
                double temp_d = (double)e_mu_temp * (8.589934592000000e+09);
                int32_t n_e_mu_Q33 = (int32_t)(temp_d);
#endif
                m = curidx;
                if (inst->adapted){
                    for (int i = 0; i < filt_iter; i++) {
                            weights[i] = weights[i] + (e_mu_temp * ((float)x[m--]*0.0001)) * inst->prod[i];  
                    }
                } else {
                    for (int i = 0; i < filt_iter; i++) {
                            weights[i] = weights[i] + e_mu_temp * ((float)x[m--]*0.0001);
    #ifdef fixed_point_aec
                        int32_t temp_real = n_e_mu_Q33*(int)(x[m]) + 512; //Q23
                        w_real_Q33[i] = w_real_Q33[i] + (temp_real>>10);
    #endif                    
                    }                    
                }
            } 
        }

        for (int i = 0; i < filt_iter; i++) {
            weights_slow[i] = weights_slow[i] * 0.99 + weights[i] * 0.01;
        }        

        // mu *= 0.7;s

        // Calc foreground filter residual power
        See = 0.0f;
        for (int n = 0; n < length; n++) {
            See += (float)(error_back[n] * error_back[n]);
        }

        float errorPowertemp = sqrtf(See / (float)length);
        Attenuationtmp = 20.0f * log10f(micPower / (errorPowertemp + 1));  

        if (Attenuationtmp>20.0f) break;
    }    

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[3] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      


    int32_t dbf[FRAME_SIZE];
    // Calc power of Difference in response
    for (int n = 0; n < length; n++) {
        dbf[n]=y_fore[n]-y_back[n];
    }

    float Dbf = 0.0f;
    for (int n = 0; n < length; n++) {
        Dbf += (float)dbf[n] * (float)dbf[n];
    }

    inst->Davg1 = .6*inst->Davg1 + .4*(Sff-See);
    inst->Davg2 = .85*inst->Davg2 + .15*(Sff-See);
    inst->Dvar1 = .36*inst->Dvar1 + .16*Sff*Dbf;
    inst->Dvar2 = .7225*inst->Dvar2 + .0225*Sff*Dbf;

    inst->update_fore = 0;
    if ((Sff-See)*fabsf(Sff-See) > Sff*Dbf) inst->update_fore = 1;
    if ((inst->Davg1)*fabsf(inst->Davg1) > 0.5*inst->Dvar1) inst->update_fore = 2;
    if ((inst->Davg2)*fabsf(inst->Davg2) > 0.25*inst->Dvar2) inst->update_fore = 3;

    for (int n = 0; n < length; n++) {
        errorSignal[n]=(int16_t)error_fore[n];
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[4] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   
    
    if (inst->update_fore) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            // weights_fore[i] = weights_fore[i]*0.75 + weights[i]* 0.25; // 필터 계수를 0으로 초기화
            // weights_fore[i] = weights[i];
            weights_fore[i] = weights_slow[i];
#ifdef fixed_point_aec            
            weights_fore[i] = ((float)w_real_Q33[i] * 1.1921e-07);
#endif        
        }

        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }
        }

        inst->Davg1 = 0.0;
        inst->Davg2 = 0.0;
        inst->Dvar1 = 0.0;
        inst->Dvar2 = 0.0;        

        for (int n = 0; n < length; n++) {
            errorSignal[n]=(int16_t)((float)micSignal[n] - ((float)y_back[n]*trans_window[n] + (float)y_fore[n]*trans_window[n+length]));
            // errorSignal[n]=(int16_t)error_back[n];
        }
        // printf("update fore = %d\n", inst->update_fore);

    } else {
        int reset_back = 0;
        if (-1.0*(Sff-See)*fabsf(Sff-See) > 4.0*Sff*Dbf) reset_back = 1;
        if (-1.0*(inst->Davg1)*fabsf(inst->Davg1) > 4.0*inst->Dvar1) reset_back = 1;
        if (-1.0*(inst->Davg2)*fabsf(inst->Davg2) > 4.0*inst->Dvar2) reset_back = 1;

        if (reset_back) {
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i] = weights_fore[i]; // 필터 계수를 0으로 초기화
            }

            See = Sff;
            inst->Davg1 = 0.0;
            inst->Davg2 = 0.0;
            inst->Dvar1 = 0.0;
            inst->Dvar2 = 0.0;

	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
                }                
            }

            // printf("reset back\n");
        }  else {
	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){            
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
                }                
            }
            // printf("just go\n");
        }      
    }  

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[5] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int i = 0; i < FILTER_LENGTH; i++) {
        if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
            printf("inst= %d, Weights[%d] is %f\n", inst->chan_num, i, fabs(weights[i]));
            resetWeights = 1; // 참
            break;
        }
    }
    if (resetWeights) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            // weights[i] = 0.0f; // 필터 계수를 0으로 초기화
            weights[i] = weights_fore[i]; // 필터 계수를 0으로 초기화
        }
        inst->adapted = 0;
        inst->sum_adapt = 0;
        printf("Weights reset to prevent divergence.\n");
    }

    // 감쇄량 계산

    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));

    if (updateon==1) {
        float tmp = Sxx;
        if (tmp>See){
            tmp = See;
        }
        inst->adapt_rate = 0.25* tmp / (See+1);
        inst->sum_adapt += inst->adapt_rate;
    }

    if (!inst->adapted && inst->sum_adapt > 7 && *prevAttenuation > 20.0f)
    {    
        inst->adapted = 1;
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[6] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        total_end = clock();
        totalRunTime += ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
    // 평균 시간 출력
    count++;
    if (totalRunTime >= PRINT_INTERVAL) {
        printf("Average execution time for each step (1-second intervals):\n");
        for (int i = 0; i < NUM_STEPS; i++) {
            double averageTime = totalTime[i] / count;
            double percentage = (totalTime[i] / totalRunTime) * 100.0;
            printf("Step %d: Average Time = %.2f milli seconds, Percentage = %f%%\n", i + 1, averageTime* 1000, percentage);
        }
        // 총 실행 시간 및 각 단계별 누적 시간 초기화
        totalRunTime = 0.0;
        for (int i = 0; i < NUM_STEPS; i++) {
            totalTime[i] = 0.0;
        }
        count = 0; // 카운트 초기화
    }
#endif


#ifdef DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
       
        FloatPacket packet;
        // packet.val1 = (float)((Sff-See)*fabsf(Sff-See));
        // packet.val2 = (float)(Sff*Dbf);
        // packet.val3 = (float)((inst->Davg1)*fabsf(inst->Davg1));
        // packet.val4 = (float)(0.5*inst->Dvar1);
        // packet.val5 = (float)((inst->Davg2)*fabsf(inst->Davg2));
        // packet.val6 = (float)(0.25*inst->Dvar2);

        packet.val1 = (float)calculate_rms_int32((int16_t *)refSignal, length);
        packet.val2 = (float)micPower;
        packet.val3 = (float)micPower;
        packet.val4 = (float)errorPower;
        packet.val5 = (float)inst->sum_adapt;
        packet.val6 = (float)inst->adapt_rate;        

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &weightsQ15[0], FILTER_LENGTH);
        // debug_matlab_int_array_send(9, &w_real_Q33[0], FILTER_LENGTH);

        debug_matlab_ssl_float_send(10, (*prevAttenuation));
        // debug_matlab_ssl_float_send(10, e_mu_temp*8.5899e+09);
        // debug_matlab_ssl_float_send(10, (float)n_e_mu_Q23);

        // debug_matlab_ssl_float_send(10, (Dbf));
    }

	if ((g_aec_debug_on==1)&&(inst->chan_num==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 

}

// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller_two_path_soft(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {

    aecInst_t *inst = (aecInst_t *)aecInst;

    const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[FILTER_LENGTH];
    int32_t error_fore[FRAME_SIZE];
    int32_t error_back[FRAME_SIZE];
    int32_t y_fore[FRAME_SIZE];
    int32_t y_back[FRAME_SIZE];
    float *trans_window = &inst->trans_window1024[0];

    float weights_slow[FILTER_LENGTH];
    float e_mu_temp = 0;    

#ifdef fixed_point_aec
    int32_t n_x_Q15[FILTER_LENGTH];
    int32_t n_e_mu_Q23 = 0;
#endif


    if (length == 1024) {
        trans_window = &inst->trans_window1024[0];
    } else if (length == 1600) {
        trans_window = &inst->trans_window1600[0];
    } else if (length == 512) {
        trans_window = &inst->trans_window512[0];
    } else {

        for (int i = 0; i < length * 2; i++) {
            inst->trans_window1600[i] = .5 - .5 * cos(2 * M_PI * i / (length * 2));        
        }
            
        trans_window = &inst->trans_window1600[0];
    }
#ifdef ENABLE_PROFILING2
        total_start = clock();
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n=0; n<FILTER_LENGTH-1; n++){
        x[n]=x[n+length];
    }

    for (int n=0; n<length; n++){
        x[n+FILTER_LENGTH-1]=refSignal[n];
    }

    // Foreground filtering
    for (int i = 0; i < FILTER_LENGTH; i++) {
        weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
    }    


#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[0] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n = 0; n < length; n++) {

        int curidx=FILTER_LENGTH-1+n;
       
        int32_t y = 16384;
        int m = curidx;
        for (int i = 0; i < FILTER_LENGTH/2; i++) {
            y += (weightsQ15[i] * x[m--]);
            // m--;
        }

        y = y>>15;

        y_fore[n] = y;

        // 잔차 신호 계산
        int32_t e = (micSignal[n] -  y);
        error_fore[n] = e;
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[1] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    // Calc foreground filter residual power
    float Sff = 0.0f;
    for (int n = 0; n < length; n++) {
        Sff += (float)(error_fore[n] * error_fore[n]);
    }

    // Calc foreground filter residual power
    float Sxx = 0.0f;
    for (int n = 0; n < length; n++) {
        Sxx += (float)((int32_t)refSignal[n] * (int32_t)refSignal[n]);
    }

    // Calc foreground filter residual power
    float Sdd = 0.0f;
    for (int n = 0; n < length; n++) {
        Sdd += (float)((int32_t)micSignal[n] * (int32_t)micSignal[n]);
    }
    float micPower = sqrtf(Sdd / (float)length);

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[2] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      

    float normFactor = 500.0f;
    for (int n=0; n<FILTER_LENGTH; n++){
        normFactor += (float)(x[n+length-1] * x[n+length-1]);
    }

#ifdef fixed_point_aec
    int32_t half_normFactor = 3000;
    for (int n=0; n<FILTER_LENGTH; n++){
        half_normFactor += abs(x[n+length-1]);
    }

    int32_t Q28MAX = 268435456;
    float half_norm_inv_Q28f = (float)(Q28MAX) / (float)half_normFactor;
    float mu_Q15f = STEP_SIZE * Q15_MAXf;
    int32_t n_mu_Q28 = (int32_t)(half_norm_inv_Q28f * mu_Q15f)>>15;
#endif

    float mu;
    // 스텝 크기 조정
    if (updateon==1) {
        mu = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? 0.05f * STEP_SIZE / normFactor : STEP_SIZE / normFactor;
        // n_mu_Q28 = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? n_mu_Q28>>3 : n_mu_Q28;
    } else {
        mu = 0.05f * STEP_SIZE / normFactor;
        // n_mu_Q28 = n_mu_Q28>>3;        
    }
    // printf("normFactor=%f mu=%f\r\n", normFactor, mu);

    float Attenuationtmp = *prevAttenuation;

    int itermax = 1;
    // if (inst->adapted==0 && Attenuationtmp <= 20.0f){
    //     if (refPower > -40.0f ){
    //         itermax = 3;
    //     //     printf("itermax = 5\r\n");
    //     // } else if (refPower > -40.0f ){
    //     //     itermax = 4;
    //     //     printf("itermax = 4\r\n");
    //     // } else {
    //     //     itermax = 3;
    //     //     printf("itermax = 3\r\n");
    //     }
    // }

    for (int i = 0; i < FILTER_LENGTH; i++) {
        weights_slow[i] = weights[i];
    }
    int filt_iter = FILTER_LENGTH;

    float See = 0.0f;
    for (int nn=0; nn<itermax; nn++){
        if (nn>1) filt_iter = 300;
        for (int n = 0; n < length; n++) {

            int curidx=FILTER_LENGTH-1+n;

            for (int i = 0; i < filt_iter; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }

            int32_t y = 16384;
            int m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                y += (weightsQ15[i] * x[m--]);
                // y += ((w_real_Q33[i]+128)>>8 * x[m--]);
            }

#ifdef fixed_point_aec
            // filtering
            int32_t temp_real=16384; // (1<<14)
            m=curidx;
            for (int k=0; k<FILTER_LENGTH; k++){
                temp_real=temp_real + (((w_real_Q33[k]+128)>>8) * (int32_t)(x[m]));
                m--;
            }      

            int32_t y =  temp_real>>15;      
#endif
            y = y>>15;

            y_back[n] = y;

            // 잔차 신호 계산
            int32_t e = micSignal[n] -  y;
            error_back[n] = e;

#ifdef fixed_point_aec            
            n_e_mu_Q23 = (n_mu_Q28 * e + 32)>>5;            

            m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                n_x_Q15[i] = (int32_t)((float)x[m]*half_norm_inv_Q28f)>>13;
                m--;
            }
#endif            

            if (updateon==1) {

                e_mu_temp = mu * e;



                // printf("e_mu_temp = %f \r\n", e_mu_temp);

#ifdef fixed_point_aec
                double temp_d = (double)e_mu_temp * (8.589934592000000e+09);
                int32_t n_e_mu_Q33 = (int32_t)(temp_d);
#endif
                // 필터 가중치 업데이트 (플로팅 연산)



                m = curidx;
                for (int i = 0; i < filt_iter; i++) {
                    // weights[i] = weights[i] * leaky[i] + mu * e * (float)x[m];
                    weights[i] = weights[i] + e_mu_temp * (float)x[m];
                    m--;

                    // w_real_Q33[i] = w_real_Q33[i] + (n_x_Q15[i] * n_e_mu_Q23+16)>>5;

                    // printf("w_real_Q33[i]  = %d n_x_Q15[%d] = %d\r\n", w_real_Q33[i], i, n_x_Q15[i]);

#ifdef fixed_point_aec
                    int32_t temp_real = n_e_mu_Q33*(int)(x[m]) + 512; //Q23
                    w_real_Q33[i] = w_real_Q33[i] + (temp_real>>10);
#endif                    
                }
            } else {
                // for (int i = 0; i < FILTER_LENGTH; i++) {
                //     weights[i] = weights[i] * leaky[i];
                // }            
            }
        }

        for (int i = 0; i < filt_iter; i++) {
            weights_slow[i] = weights_slow[i] * 0.99 + weights[i] * 0.01;
        }        

        // mu *= 0.7;s

        // Calc foreground filter residual power
        See = 0.0f;
        for (int n = 0; n < length; n++) {
            See += (float)(error_back[n] * error_back[n]);
        }

        float errorPowertemp = sqrtf(See / (float)length);
        Attenuationtmp = 20.0f * log10f(micPower / (errorPowertemp + 1));  

        if (Attenuationtmp>20.0f) break;
    }    

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[3] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      


    int32_t dbf[FRAME_SIZE];
    // Calc power of Difference in response
    for (int n = 0; n < length; n++) {
        dbf[n]=y_fore[n]-y_back[n];
    }

    float Dbf = 0.0f;
    for (int n = 0; n < length; n++) {
        Dbf += (float)dbf[n] * (float)dbf[n];
    }

    inst->Davg1 = .6*inst->Davg1 + .4*(Sff-See);
    inst->Davg2 = .85*inst->Davg2 + .15*(Sff-See);
    inst->Dvar1 = .36*inst->Dvar1 + .16*Sff*Dbf;
    inst->Dvar2 = .7225*inst->Dvar2 + .0225*Sff*Dbf;

    inst->update_fore = 0;
    if ((Sff-See)*fabsf(Sff-See) > Sff*Dbf) inst->update_fore = 1;
    if ((inst->Davg1)*fabsf(inst->Davg1) > 0.5*inst->Dvar1) inst->update_fore = 2;
    if ((inst->Davg2)*fabsf(inst->Davg2) > 0.25*inst->Dvar2) inst->update_fore = 3;

    for (int n = 0; n < length; n++) {
        errorSignal[n]=(int16_t)error_fore[n];
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[4] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   
    
    if (inst->update_fore) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            // weights_fore[i] = weights_fore[i]*0.75 + weights[i]* 0.25; // 필터 계수를 0으로 초기화
            // weights_fore[i] = weights[i];
            weights_fore[i] = weights_slow[i];
#ifdef fixed_point_aec            
            weights_fore[i] = ((float)w_real_Q33[i] * 1.1921e-07);
#endif        
        }

        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }
        }

        inst->Davg1 = 0.0;
        inst->Davg2 = 0.0;
        inst->Dvar1 = 0.0;
        inst->Dvar2 = 0.0;        

        for (int n = 0; n < length; n++) {
            errorSignal[n]=(int16_t)((float)micSignal[n] - ((float)y_back[n]*trans_window[n] + (float)y_fore[n]*trans_window[n+length]));
            // errorSignal[n]=(int16_t)error_back[n];
        }
        // printf("update fore = %d\n", inst->update_fore);

    } else {
        int reset_back = 0;
        if (-1.0*(Sff-See)*fabsf(Sff-See) > 4.0*Sff*Dbf) reset_back = 1;
        if (-1.0*(inst->Davg1)*fabsf(inst->Davg1) > 4.0*inst->Dvar1) reset_back = 1;
        if (-1.0*(inst->Davg2)*fabsf(inst->Davg2) > 4.0*inst->Dvar2) reset_back = 1;

        if (reset_back) {
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i] = weights_fore[i]; // 필터 계수를 0으로 초기화
            }

            See = Sff;
            inst->Davg1 = 0.0;
            inst->Davg2 = 0.0;
            inst->Dvar1 = 0.0;
            inst->Dvar2 = 0.0;

	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
                }                
            }

            // printf("reset back\n");
        }  else {
	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){            
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
                }                
            }
            // printf("just go\n");
        }      
    }  

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[5] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int i = 0; i < FILTER_LENGTH; i++) {
        if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
            printf("Weights[%d] is %f\n", i, fabs(weights[i]));
            resetWeights = 1; // 참
            break;
        }
    }
    if (resetWeights) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weights[i] = 0.0f; // 필터 계수를 0으로 초기화
        }
        inst->adapted = 0;
        inst->sum_adapt = 0;
        printf("Weights reset to prevent divergence.\n");
    }

    // 감쇄량 계산

    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));

    if (updateon==1) {
        float tmp = Sxx;
        if (tmp>See){
            tmp = See;
        }
        inst->adapt_rate = 0.25* tmp / (See+1);
        inst->sum_adapt += inst->adapt_rate;
    }

    if (!inst->adapted && inst->sum_adapt > 7 && *prevAttenuation > 20.0f)
    {    
        inst->adapted = 1;
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[6] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        total_end = clock();
        totalRunTime += ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
    // 평균 시간 출력
    count++;
    if (totalRunTime >= PRINT_INTERVAL) {
        printf("Average execution time for each step (1-second intervals):\n");
        for (int i = 0; i < NUM_STEPS; i++) {
            double averageTime = totalTime[i] / count;
            double percentage = (totalTime[i] / totalRunTime) * 100.0;
            printf("Step %d: Average Time = %.2f milli seconds, Percentage = %f%%\n", i + 1, averageTime* 1000, percentage);
        }
        // 총 실행 시간 및 각 단계별 누적 시간 초기화
        totalRunTime = 0.0;
        for (int i = 0; i < NUM_STEPS; i++) {
            totalTime[i] = 0.0;
        }
        count = 0; // 카운트 초기화
    }
#endif


#ifdef DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
       
        FloatPacket packet;
        // packet.val1 = (float)((Sff-See)*fabsf(Sff-See));
        // packet.val2 = (float)(Sff*Dbf);
        // packet.val3 = (float)((inst->Davg1)*fabsf(inst->Davg1));
        // packet.val4 = (float)(0.5*inst->Dvar1);
        // packet.val5 = (float)((inst->Davg2)*fabsf(inst->Davg2));
        // packet.val6 = (float)(0.25*inst->Dvar2);

        packet.val1 = (float)calculate_rms_int32((int16_t *)refSignal, length);
        packet.val2 = (float)micPower;
        packet.val3 = (float)micPower;
        packet.val4 = (float)errorPower;
        packet.val5 = (float)inst->sum_adapt;
        packet.val6 = (float)inst->adapt_rate;        

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &weightsQ15[0], FILTER_LENGTH);
        // debug_matlab_int_array_send(9, &w_real_Q33[0], FILTER_LENGTH);

        debug_matlab_ssl_float_send(10, (*prevAttenuation));
        // debug_matlab_ssl_float_send(10, e_mu_temp*8.5899e+09);
        // debug_matlab_ssl_float_send(10, (float)n_e_mu_Q23);

        // debug_matlab_ssl_float_send(10, (Dbf));
    }

	if ((g_aec_debug_on==1)&&(inst->chan_num==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 

}

// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller_two_path_strong(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {

    aecInst_t *inst = (aecInst_t *)aecInst;

    const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[FILTER_LENGTH];
    int32_t error_fore[FRAME_SIZE];
    int32_t error_back[FRAME_SIZE];
    int32_t y_fore[FRAME_SIZE];
    int32_t y_back[FRAME_SIZE];
    float *trans_window = &inst->trans_window1024[0];

    float weights_slow[FILTER_LENGTH];
    int32_t n_x_Q15[FILTER_LENGTH];

    float e_mu_temp = 0;
    int32_t n_e_mu_Q23 = 0;


    if (length == 1024) {
        trans_window = &inst->trans_window1024[0];
    } else if (length == 1600) {
        trans_window = &inst->trans_window1600[0];
    } else if (length == 512) {
        trans_window = &inst->trans_window512[0];
    } else if (length == 256) {
        trans_window = &inst->trans_window256[0];             
    } else {

        for (int i = 0; i < length * 2; i++) {
            inst->trans_window1600[i] = .5 - .5 * cos(2 * M_PI * i / (length * 2));        
        }
            
        trans_window = &inst->trans_window1600[0];
    }

#ifdef ENABLE_PROFILING2
        total_start = clock();
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n=0; n<FILTER_LENGTH-1; n++){
        x[n]=x[n+length];
    }

    for (int n=0; n<length; n++){
        x[n+FILTER_LENGTH-1]=refSignal[n];
    }

    // Foreground filtering
    for (int i = 0; i < FILTER_LENGTH; i++) {
        weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
    }    


#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[0] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    for (int n = 0; n < length; n++) {

        int curidx=FILTER_LENGTH-1+n;
       
        int32_t y = 16384;
        int m = curidx;
        for (int i = 0; i < FILTER_LENGTH; i++) {
            y += (weightsQ15[i] * x[m--]);
            // m--;
        }

        y = y>>15;

        y_fore[n] = y;

        // 잔차 신호 계산
        int32_t e = (micSignal[n] -  y);
        error_fore[n] = e;
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[1] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif          

    // Calc foreground filter residual power
    float Sff = 0.0f;
    for (int n = 0; n < length; n++) {
        Sff += (float)(error_fore[n] * error_fore[n]);
    }

    // Calc foreground filter residual power
    float Sxx = 0.0f;
    for (int n = 0; n < length; n++) {
        Sxx += (float)((int32_t)refSignal[n] * (int32_t)refSignal[n]);
    }

    // Calc foreground filter residual power
    float Sdd = 0.0f;
    for (int n = 0; n < length; n++) {
        Sdd += (float)((int32_t)micSignal[n] * (int32_t)micSignal[n]);
    }
    float micPower = sqrtf(Sdd / (float)length);

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[2] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      

    float normFactor = 500.0f;
    for (int n=0; n<FILTER_LENGTH; n++){
        normFactor += (float)(x[n+length-1] * x[n+length-1]);
    }

#ifdef fixed_point_aec
    int32_t half_normFactor = 3000;
    for (int n=0; n<FILTER_LENGTH; n++){
        half_normFactor += abs(x[n+length-1]);
    }

    int32_t Q28MAX = 268435456;
    float half_norm_inv_Q28f = (float)(Q28MAX) / (float)half_normFactor;
    float mu_Q15f = STEP_SIZE * Q15_MAXf;
    int32_t n_mu_Q28 = (int32_t)(half_norm_inv_Q28f * mu_Q15f)>>15;
#endif

    float step_size1000 = STEP_SIZE * 10000;

    float mu;
    // 스텝 크기 조정
    if (updateon==1) {
        mu = (*prevAttenuation > 15.0f || refPower < -50.0f || inst->adapted) ? 0.1f * step_size1000 / normFactor : step_size1000 / normFactor;
        // n_mu_Q28 = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? n_mu_Q28>>3 : n_mu_Q28;
    } else {
        mu = 0.1f * step_size1000 / normFactor;
        // n_mu_Q28 = n_mu_Q28>>3;        
    }
    // printf("normFactor=%f mu=%f\r\n", normFactor, mu);

    // float mu;
    // // 스텝 크기 조정
    // if (updateon==1) {
    //     mu = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? 0.05f * STEP_SIZE / normFactor : STEP_SIZE / normFactor;
    //     // n_mu_Q28 = (*prevAttenuation > 25.0f || refPower < -50.0f || inst->adapted) ? n_mu_Q28>>3 : n_mu_Q28;
    // } else {
    //     mu = 0.05f * STEP_SIZE / normFactor;
    //     // n_mu_Q28 = n_mu_Q28>>3;        
    // }
    // // printf("normFactor=%f mu=%f\r\n", normFactor, mu);

    float Attenuationtmp = *prevAttenuation;

    int itermax = 1;
    if (inst->adapted==0 && Attenuationtmp <= 15.0f){
        if (refPower > -40.0f ){
            itermax = 5;
        //     printf("itermax = 5\r\n");
        // } else if (refPower > -40.0f ){
        //     itermax = 4;
        //     printf("itermax = 4\r\n");
        // } else {
        //     itermax = 3;
        //     printf("itermax = 3\r\n");
        }
    }

    for (int i = 0; i < FILTER_LENGTH; i++) {
        weights_slow[i] = weights[i];
    }
    int filt_iter = FILTER_LENGTH;

    float See = 0.0f;
    for (int nn=0; nn<itermax; nn++){
        if (nn>1) filt_iter = 300;
        for (int n = 0; n < length; n++) {

            int curidx=FILTER_LENGTH-1+n;

            for (int i = 0; i < filt_iter; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }

            int32_t y = 16384;
            int m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                y += (weightsQ15[i] * x[m--]);
                // y += ((w_real_Q33[i]+128)>>8 * x[m--]);
            }

#ifdef fixed_point_aec
            // filtering
            int32_t temp_real=16384; // (1<<14)
            m=curidx;
            for (int k=0; k<FILTER_LENGTH; k++){
                temp_real=temp_real + (((w_real_Q33[k]+128)>>8) * (int32_t)(x[m]));
                m--;
            }      

            int32_t y =  temp_real>>15;      
#endif
            y = y>>15;

            y_back[n] = y;

            // 잔차 신호 계산
            int32_t e = micSignal[n] -  y;
            error_back[n] = e;

#ifdef fixed_point_aec            
            n_e_mu_Q23 = (n_mu_Q28 * e + 32)>>5;            

            m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                n_x_Q15[i] = (int32_t)((float)x[m]*half_norm_inv_Q28f)>>13;
                m--;
            }
#endif            

            if (updateon==1) {

                e_mu_temp = mu * e;
#ifdef fixed_point_aec
                double temp_d = (double)e_mu_temp * (8.589934592000000e+09);
                int32_t n_e_mu_Q33 = (int32_t)(temp_d);
#endif
                m = curidx;
                for (int i = 0; i < filt_iter; i++) {
                    if (inst->adapted){
                        weights[i] = weights[i] + (e_mu_temp * ((float)x[m]*0.0001)) * inst->prod[i];
                    } else {
                        weights[i] = weights[i] + e_mu_temp * ((float)x[m]*0.0001);
                    }
                    m--;
#ifdef fixed_point_aec
                    int32_t temp_real = n_e_mu_Q33*(int)(x[m]) + 512; //Q23
                    w_real_Q33[i] = w_real_Q33[i] + (temp_real>>10);
#endif                    
                }
            } 
        }

        for (int i = 0; i < filt_iter; i++) {
            weights_slow[i] = weights_slow[i] * 0.99 + weights[i] * 0.01;
        }        

        // mu *= 0.7;s

        // Calc foreground filter residual power
        See = 0.0f;
        for (int n = 0; n < length; n++) {
            See += (float)(error_back[n] * error_back[n]);
        }

        float errorPowertemp = sqrtf(See / (float)length);
        Attenuationtmp = 20.0f * log10f(micPower / (errorPowertemp + 1));  

        if (Attenuationtmp>20.0f) break;
    }    

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[3] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif      


    int32_t dbf[FRAME_SIZE];
    // Calc power of Difference in response
    for (int n = 0; n < length; n++) {
        dbf[n]=y_fore[n]-y_back[n];
    }

    float Dbf = 0.0f;
    for (int n = 0; n < length; n++) {
        Dbf += (float)dbf[n] * (float)dbf[n];
    }

    inst->Davg1 = .6*inst->Davg1 + .4*(Sff-See);
    inst->Davg2 = .85*inst->Davg2 + .15*(Sff-See);
    inst->Dvar1 = .36*inst->Dvar1 + .16*Sff*Dbf;
    inst->Dvar2 = .7225*inst->Dvar2 + .0225*Sff*Dbf;

    inst->update_fore = 0;
    if ((Sff-See)*fabsf(Sff-See) > Sff*Dbf) inst->update_fore = 1;
    if ((inst->Davg1)*fabsf(inst->Davg1) > 0.5*inst->Dvar1) inst->update_fore = 2;
    if ((inst->Davg2)*fabsf(inst->Davg2) > 0.25*inst->Dvar2) inst->update_fore = 3;

    for (int n = 0; n < length; n++) {
        errorSignal[n]=(int16_t)error_fore[n];
    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[4] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   
    
    if (inst->update_fore) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            // weights_fore[i] = weights_fore[i]*0.75 + weights[i]* 0.25; // 필터 계수를 0으로 초기화
            // weights_fore[i] = weights[i];
            weights_fore[i] = weights_slow[i];
#ifdef fixed_point_aec            
            weights_fore[i] = ((float)w_real_Q33[i] * 1.1921e-07);
#endif        
        }

        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }
        }

        inst->Davg1 = 0.0;
        inst->Davg2 = 0.0;
        inst->Dvar1 = 0.0;
        inst->Dvar2 = 0.0;        

        for (int n = 0; n < length; n++) {
            errorSignal[n]=(int16_t)((float)micSignal[n] - ((float)y_back[n]*trans_window[n] + (float)y_fore[n]*trans_window[n+length]));
            // errorSignal[n]=(int16_t)error_back[n];
        }
        // printf("update fore = %d\n", inst->update_fore);

    } else {
        int reset_back = 0;
        if (-1.0*(Sff-See)*fabsf(Sff-See) > 4.0*Sff*Dbf) reset_back = 1;
        if (-1.0*(inst->Davg1)*fabsf(inst->Davg1) > 4.0*inst->Dvar1) reset_back = 1;
        if (-1.0*(inst->Davg2)*fabsf(inst->Davg2) > 4.0*inst->Dvar2) reset_back = 1;

        if (reset_back) {
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i] = weights_fore[i]; // 필터 계수를 0으로 초기화
            }

            See = Sff;
            inst->Davg1 = 0.0;
            inst->Davg2 = 0.0;
            inst->Dvar1 = 0.0;
            inst->Dvar2 = 0.0;

	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
                }                
            }

            // printf("reset back\n");
        }  else {
	        if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){            
                for (int i = 0; i < FILTER_LENGTH; i++) {
                    weightsQ15[i] = (int32_t)(weights_fore[i] * 32768.0f);
                }                
            }
            // printf("just go\n");
        }      
    }  

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[5] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        start = clock();
#endif   

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int i = 0; i < FILTER_LENGTH; i++) {
        if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
            printf("inst= %d, Weights[%d] is %f\n", inst->chan_num, i, fabs(weights[i]));
            resetWeights = 1; // 참
            break;
        }
    }
    if (resetWeights) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weights[i] = weights_fore[i]; // 필터 계수를 0으로 초기화
        }
        inst->adapted = 0;
        inst->sum_adapt = 0;
        printf("Weights reset to prevent divergence.\n");
    }

    // 감쇄량 계산

    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));

    if (updateon==1) {
        float tmp = Sxx;
        if (tmp>See){
            tmp = See;
        }
        inst->adapt_rate = 0.25* tmp / (See+1);
        inst->sum_adapt += inst->adapt_rate;
    }

    if (!inst->adapted && inst->sum_adapt > 15 && *prevAttenuation > 15.0f)
    {    
        inst->adapted = 1;

        calc_prod(inst->prod, weights_fore, FILTER_LENGTH);

    }

#ifdef ENABLE_PROFILING2
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[6] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
        total_end = clock();
        totalRunTime += ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING2
    // 평균 시간 출력
    count++;
    if (totalRunTime >= PRINT_INTERVAL) {
        printf("Average execution time for each step (1-second intervals):\n");
        for (int i = 0; i < NUM_STEPS; i++) {
            double averageTime = totalTime[i] / count;
            double percentage = (totalTime[i] / totalRunTime) * 100.0;
            printf("Step %d: Average Time = %.2f milli seconds, Percentage = %f%%\n", i + 1, averageTime* 1000, percentage);
        }
        // 총 실행 시간 및 각 단계별 누적 시간 초기화
        totalRunTime = 0.0;
        for (int i = 0; i < NUM_STEPS; i++) {
            totalTime[i] = 0.0;
        }
        count = 0; // 카운트 초기화
    }
#endif


#ifdef DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
       
        FloatPacket packet;
        // packet.val1 = (float)((Sff-See)*fabsf(Sff-See));
        // packet.val2 = (float)(Sff*Dbf);
        // packet.val3 = (float)((inst->Davg1)*fabsf(inst->Davg1));
        // packet.val4 = (float)(0.5*inst->Dvar1);
        // packet.val5 = (float)((inst->Davg2)*fabsf(inst->Davg2));
        // packet.val6 = (float)(0.25*inst->Dvar2);

        packet.val1 = (float)calculate_rms_int32((int16_t *)refSignal, length);
        packet.val2 = (float)micPower;
        packet.val3 = (float)micPower;
        packet.val4 = (float)errorPower;
        packet.val5 = (float)inst->sum_adapt;
        packet.val6 = (float)inst->adapt_rate;        

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &weightsQ15[0], FILTER_LENGTH);
        // debug_matlab_int_array_send(9, &w_real_Q33[0], FILTER_LENGTH);

        debug_matlab_ssl_float_send(10, (*prevAttenuation));
        // debug_matlab_ssl_float_send(10, e_mu_temp*8.5899e+09);
        // debug_matlab_ssl_float_send(10, (float)n_e_mu_Q23);

        // debug_matlab_ssl_float_send(10, (Dbf));
    }

	if ((g_aec_debug_on==1)&&(inst->chan_num==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 

}

// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {


    aecInst_t *inst = (aecInst_t *)aecInst;

    const float WEIGHT_THRESHOLD = 5.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[FILTER_LENGTH];

    for (int n=0; n<FILTER_LENGTH-1; n++){
        x[n]=x[n+length];
    }

    for (int n=0; n<length; n++){
        x[n+FILTER_LENGTH-1]=refSignal[n];
    }

    float normFactor = 500.0f;
    for (int n=0; n<FILTER_LENGTH; n++){
        normFactor += (float)((int32_t)x[n+length-1] * (int32_t)x[n+length-1]);
    }

    float mu;
    // 스텝 크기 조정
    if (updateon==1) {
        mu = (*prevAttenuation > 25.0f || refPower < -40.0f) ? 0.5f * STEP_SIZE2 / normFactor : STEP_SIZE2 / normFactor;
    } else {
        mu = 0.05f * STEP_SIZE2 / normFactor;
    }

    int itermax = 1;
    if (*prevAttenuation <= 25.0f){
        if (refPower > -40.0f ){
            itermax = 5;
        }
    }

    int filt_iter = 150;

    for (int nn=0; nn<itermax; nn++){
        if (nn==(itermax-1)) filt_iter = FILTER_LENGTH;

        for (int n = 0; n < length; n++) {

            int curidx=FILTER_LENGTH-1+n;

            for (int i = 0; i < filt_iter; i++) {
                weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
            }

            int32_t y = 16384;
            int m = curidx;
            for (int i = 0; i < filt_iter; i++) {
                y += (weightsQ15[i] * x[m]);
                m--;
            }

            y = y>>15;

            // 잔차 신호 계산
            float e = (float)micSignal[n] -  (float)y;
            errorSignal[n] = (int16_t)e;

            if (updateon==1) {
                float e_mu_temp = mu * e;

                // 필터 가중치 업데이트 (플로팅 연산)
                m = curidx;
                for (int i = 0; i < filt_iter; i++) {
                    // weights[i] = weights[i] * leaky[i] + mu * e * (float)x[m];
                    weights[i] = weights[i] + e_mu_temp * (float)x[m];

                    // if (inst->adapted){
                    //     weights[i] = weights[i] + (e_mu_temp * ((float)x[m])) * inst->prod[i];
                    // } else {
                    //     weights[i] = weights[i] + e_mu_temp * ((float)x[m]);
                    // }

                    m--;
                }
            } else {
                // for (int i = 0; i < FILTER_LENGTH; i++) {
                //     weights[i] = weights[i] * leaky[i];
                // }            
            }
        }
    }

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int i = 0; i < FILTER_LENGTH; i++) {
        if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
            resetWeights = 1; // 참
            break;
        }
    }
    if (resetWeights) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weights[i] = 0.0f; // 필터 계수를 0으로 초기화
        }
        printf("Weights reset to prevent divergence on second stage.\n");
    }

    // 감쇄량 계산
    float micPower = calculate_rms_int32(micSignal, length);
    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));
    // printf("micPower= %.2f, errorPower=%.2f, Attenuation = %.2f updateon=%d\n",20.0f * log10f(micPower)-90.3f, 20.0f * log10f(errorPower)-90.3f, *prevAttenuation, updateon);

//	    if (!inst->adapted && *prevAttenuation > 15.0f)
//	    {    
//	        inst->adapted = 1;
//	
//	        calc_prod(inst->prod, weights, FILTER_LENGTH);
//	    }


#ifdef DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(inst->chan_num==0)){
       
        FloatPacket packet;
        // packet.val1 = (float)((Sff-See)*fabsf(Sff-See));
        // packet.val2 = (float)(Sff*Dbf);
        // packet.val3 = (float)((inst->Davg1)*fabsf(inst->Davg1));
        // packet.val4 = (float)(0.5*inst->Dvar1);
        // packet.val5 = (float)((inst->Davg2)*fabsf(inst->Davg2));
        // packet.val6 = (float)(0.25*inst->Dvar2);

        packet.val1 = (float)calculate_rms_int32((int16_t *)refSignal, length);
        packet.val2 = (float)micPower;
        packet.val3 = (float)micPower;
        packet.val4 = (float)errorPower;
        packet.val5 = (float)inst->sum_adapt;
        packet.val6 = (float)inst->adapt_rate;        

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &weightsQ15[0], FILTER_LENGTH);
        // debug_matlab_int_array_send(9, &w_real_Q33[0], FILTER_LENGTH);

        debug_matlab_ssl_float_send(10, (*prevAttenuation));
        // debug_matlab_ssl_float_send(10, e_mu_temp*8.5899e+09);
        // debug_matlab_ssl_float_send(10, (float)n_e_mu_Q23);

        // debug_matlab_ssl_float_send(10, (Dbf));
    }

	if ((g_aec_debug_on==1)&&(inst->chan_num==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 

}

// #define ENABLE_PROFILING // 연산 시간 측정 활성화
#ifdef ENABLE_PROFILING
#define NUM_STEPS 8 // 총 연산 단계 수
#define PRINT_INTERVAL 1.0 // 평균을 출력할 간격 (초)
    static double totalTime[NUM_STEPS] = {0.0};
    static int count = 0;
    clock_t start, end;
    clock_t total_start, total_end;
    double stepTime;
    double totalRunTime = 0.0;
#endif


void AEC_2ch_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB) {
    aecInst_t *inst = (aecInst_t *)aecInst;
    float refPower, refRMS;
    float micPower, micRMS;

    int16_t errorSignal[FRAME_SIZE];
    float noiseBuffer[FRAME_SIZE];


    int updateon=1;

    float micscale = powf(10.0f, MicscaledB/20.0);

    int32_t pre_emph_q15 = (int32_t)(0.7f * 32768.0f);

    
    float noisedB = -40.0f; // 노이즈 강도 조정    

    if (inBufRef != NULL) {

        if ((delay_auto == 1)&&(inst->delayinit==0)) {
            // 버퍼에 현재 프레임을 추가 (순환 버퍼처럼 사용)
            memmove(&inst->ref_Dly_Buffer[0], &inst->ref_Dly_Buffer[framelen], (DELAY_BUFFER_SIZE - framelen) * sizeof(int16_t));
            memmove(&inst->mic_Dly_Buffer[0], &inst->mic_Dly_Buffer[framelen], (DELAY_BUFFER_SIZE - framelen) * sizeof(int16_t));
            memcpy(&inst->ref_Dly_Buffer[DELAY_BUFFER_SIZE - framelen], inBufRef, framelen * sizeof(int16_t));
            memcpy(&inst->mic_Dly_Buffer[DELAY_BUFFER_SIZE - framelen], inBufMic, framelen * sizeof(int16_t));

            refRMS = calculate_rms_int32(inBufRef, framelen);
            refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환

            if (refPower > MIN_POWER) {
                inst->corr_hold = 2;
            }

            inst->corr_hold--;
            if (inst->corr_hold <= 0) inst->corr_hold = 0;

            if (inst->corr_hold > 0) {
                calculate_global_delay(inst->ref_Dly_Buffer, inst->mic_Dly_Buffer, framelen, inst->crossCorr, &inst->frameCount, &inst->estimatedDelay, &inst->delayinit, &inst->delayupdated);
            }

            if (inst->delayupdated == 1) {
                AEC_delay_init(inst, &inst->d_buf_g[0][0], Ref_delay);
                inst->delayupdated = 0;
                inst->adapted = 0;
                inst->sum_adapt = 0;
            }
        }

        if (inst->delayinit == 0) {
            inst->estimatedDelay = delay;
        }

        pre_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r, framelen);
        pre_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d, framelen);
        pre_emphasis(&inBufMic[0+framelen], pre_emph_q15, &inst->mem_d2, framelen);

        // 글로벌 딜레이 적용
        AEC_delay_with_framelen(inst, &inBufRef[0], &inBufRef[0], &inst->d_buf_g[0][0], inst->estimatedDelay, framelen);        

        // 참조 신호의 RMS 크기 계산
        refRMS = calculate_rms_int32(inBufRef, framelen);
        refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환
        // micRMS = calculate_rms_int32(inBufMic, framelen);
        // micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환
        // printf("refPower = %.2fdB, micPower=%.2f \r\n",refPower, micPower);     

        // 참조 신호와 마이크 신호를 부드럽게 노멀라이즈
        if (refPower > MIN_POWER) {
            smooth_normalize_signal_rms_int32(inBufRef, refRMS, framelen, &inst->refScale, -25.0);

            // micScaleQ15 = (int32_t)(micScale * refScale * 32768.0f);
            int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

            for (int j = 0; j < 2 ; j++){
                for (int i = 0; i < framelen; i++) {
                    inBufMic[i+j*framelen] = (int16_t)(((int32_t)(inBufMic[i+j*framelen]) * micScaleQ15) >> 15);
                }
            }

            updateon = 1;

        } else {
                int32_t refScaleQ15 = (int32_t)(inst->refScale * 32768.0f);
                // micScaleQ15 = (int32_t)(micScale * inst->refScale * 32768.0f);
                int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

            for (int i = 0; i < framelen; i++) {
                inBufRef[i] = (int16_t)(((int32_t)(inBufRef[i]) * refScaleQ15) >> 15);
            }

            for (int j = 0; j < 2 ; j++){
                for (int i = 0; i < framelen; i++) {
                    inBufMic[i+j*framelen] = (int16_t)(((int32_t)(inBufMic[i+j*framelen]) * micScaleQ15) >> 15);
                }                
            }

            updateon = 0;
        }
        // printf("refScale = %f, micScale=%f \r\n",refScale, micScale);
    }

    if (inBufRef != NULL) {

        // printf("refPower= %.2f, ",refPower);
        // NLMS 에코 캔슬러 실행
        // 
        nlms_echo_canceller_2ch_two_path(inst, inBufRef, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);

        // nlms_echo_canceller(inBufRef, errorSignal, weights_second, leaky, errorSignal_second, x2, refPower, &prevAttenuation_second, framelen, updateon);


        float errRMS = calculate_rms_int32(errorSignal, framelen);
        float errPower = 20.0f * log10f(errRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        micRMS = calculate_rms_int32(inBufMic, framelen*2);
        micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        float gate_dB = 0.0f;
        if (refPower > MIN_POWER && (errPower < (micPower-15.0f)|| (inst->adapted == 0))) {
            gate_dB = -10.0f;
            // printf("ref only ");
        }

        // printf("micPower: %.2f errPower: %.2f prevAttenuation = %.2f\n", micPower, errPower, prevAttenuation);

        int32_t inverseMicScale = (int32_t)(32768.0f / (inst->refScale));
        for (int i = 0; i < framelen; i++) {
            // inBufRef[i] = inBufMic[i];
            inBufRef[i] = (int16_t)((inverseMicScale * (int32_t)inBufMic[i])>>15); // 잔차 신호를 역으로 스케일링
        }

        // 역 스케일링: 잔차 신호를 원래 크기로 보정
        inverseMicScale = (int32_t)(32768.0f / (micscale * inst->refScale));
        
        float gate_level = powf(10.0f, (gate_dB/20.0f));
        inst->gate_aec = inst->gate_aec * 0.5 + gate_level * 0.5;
        inverseMicScale = (int32_t)((32768.0f / (micscale * inst->refScale)) * inst->gate_aec);
        // inverseMicScale = (int32_t)((32768.0f / (micscale * refScale)));

        for (int j = 0; j < 2 ; j++){
            for (int n = 0; n < framelen; n++) {
                errorSignal[n+j*framelen] = (int16_t)((inverseMicScale * (int32_t)errorSignal[n+j*framelen])>>15); // 잔차 신호를 역으로 스케일링
            }
        }

        // 핑크 노이즈 생성
        generate_pink_noise(noiseBuffer, framelen, 0.1);

        // 컴포트 노이즈 추가
        add_comfort_noise(&errorSignal[0], noiseBuffer, framelen, noisedB);        
        add_comfort_noise(&errorSignal[framelen], noiseBuffer, framelen, noisedB);       

        for (int i = 0; i < framelen * 2; i++) {
            inBufMic[i] = errorSignal[i];
        }
    }

    de_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r_d, framelen);
    de_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d_d, framelen);
    de_emphasis(&inBufMic[framelen], pre_emph_q15, &inst->mem_d_d2, framelen);
    
}


void AEC_single_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, float MicscaledB) {
    aecInst_t *inst = (aecInst_t *)aecInst;
    float refPower, refRMS;
    float micPower, micRMS;

    int16_t refSignal[FRAME_SIZE];
    int16_t errorSignal[FRAME_SIZE];
    // int16_t errorSignal_second[FRAME_SIZE];
    float noiseBuffer[FRAME_SIZE];

#ifdef ENABLE_PROFILING
        total_start = clock();
#endif

    int updateon=1;
    float micscale = powf(10.0f, MicscaledB/20.0);
    int32_t pre_emph_q15 = (int32_t)(0.6f * 32768.0f);

    if (inBufRef != NULL) {

#ifdef ENABLE_PROFILING
        start = clock();
#endif        

        memcpy(&refSignal[0], &inBufRef[0], sizeof(int16_t)*framelen);

        // pre_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r, framelen);
        // pre_emphasis(&refSignal[0], pre_emph_q15, &inst->mem_r, framelen);
        // pre_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d, framelen);

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[1] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        // 글로벌 딜레이 적용
        // AEC_delay_with_framelen(inst, &inBufRef[0], &inBufRef[0], &inst->d_buf_g[0][0], delay, framelen);        

        if (delay >= 0){
            // AEC_delay_with_framelen(inst, &inBufRef[0], &inBufRef[0], &inst->d_buf_g[0][0], delay, framelen);        
            AEC_delay_with_framelen(inst, &refSignal[0], &refSignal[0], &inst->d_buf_g[0][0], delay, framelen);     
        } else {
            AEC_delay_with_framelen(inst, &inBufMic[0], &inBufMic[0], &inst->d_buf_g[0][0], -1*delay, framelen);        
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[2] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        // 참조 신호의 RMS 크기 계산
        // refRMS = calculate_rms_int32(inBufRef, framelen);
        refRMS = calculate_rms_int32(refSignal, framelen);
        refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환
        // micRMS = calculate_rms_int32(inBufMic, framelen);
        // micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환
        // printf("refPower = %.2fdB, micPower=%.2f \r\n",refPower, micPower);     

        // 참조 신호와 마이크 신호를 부드럽게 노멀라이즈
        if (refPower > MIN_POWER) {
            // smooth_normalize_signal_rms_int32(inBufRef, refRMS, framelen, &inst->refScale, -25.0);
            smooth_normalize_signal_rms_int32(refSignal, refRMS, framelen, &inst->refScale, -25.0);

            // micScaleQ15 = (int32_t)(micScale * inst->refScale * 32768.0f);
            int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

            for (int i = 0; i < framelen; i++) {
                inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
            }

            updateon = 1;

        } else {
            int32_t refScaleQ15 = (int32_t)(inst->refScale * 32768.0f);
            // micScaleQ15 = (int32_t)(micScale * inst->refScale * 32768.0f);
            int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

            for (int i = 0; i < framelen; i++) {
                // inBufRef[i] = (int16_t)(((int32_t)(inBufRef[i]) * refScaleQ15) >> 15);
                refSignal[i] = (int16_t)(((int32_t)(refSignal[i]) * refScaleQ15) >> 15);
                inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
            }

            updateon = 0;
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[3] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        // printf("inst->refScale = %f, micScale=%f \r\n",inst->refScale, micScale);

        // printf("refPower= %.2f, ",refPower);
        // NLMS 에코 캔슬러 실행
        // nlms_echo_canceller_two_path(inst, inBufRef, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
        // nlms_echo_canceller_two_path       (inst, refSignal, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
        nlms_echo_canceller(inst, refSignal, inBufMic, inst->weightsback, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
        // nlms_echo_canceller_two_path_strong(inst, refSignal, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
        // for (int i = 0; i < framelen; i++) {
        //     errorSignal[i] = inBufMic[i];
        // }


#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[4] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif
        // nlms_echo_canceller(inBufRef, errorSignal, weights_second, leaky, errorSignal_second, uBuf2, refPower, &prevAttenuation_second, framelen, updateon);

        float errRMS = calculate_rms_int32(errorSignal, framelen);
        float errPower = 20.0f * log10f(errRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        micRMS = calculate_rms_int32(inBufMic, framelen);
        micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        // printf("micPower: %.2f errPower: %.2f prevAttenuation = %.2f\n", micPower, errPower, prevAttenuation);

        // int32_t inverseMicScale = (int32_t)(32768.0f / (inst->refScale));
        // for (int i = 0; i < framelen; i++) {
        //     // inBufRef[i] = inBufMic[i];
        //     // inBufRef[i] = (int16_t)((inverseMicScale * (int32_t)inBufMic[i])>>15); // 잔차 신호를 역으로 스케일링
        //     refSignal[i] = (int16_t)((inverseMicScale * (int32_t)inBufMic[i])>>15); // 잔차 신호를 역으로 스케일링
        // }
       
        float gate_dB = 0.0f;
        if (refPower > MIN_POWER && (errPower < (micPower-15.0f)|| (inst->adapted == 0))) {
            gate_dB = -10.0f;
        }
        float gate_level = powf(10.0f, (gate_dB/20.0f));
        inst->gate_aec = inst->gate_aec * 0.5 + gate_level * 0.5;
        
        int32_t inverseMicScale = (int32_t)((32768.0f / (micscale * inst->refScale)) * inst->gate_aec);
        // inverseMicScale = (int32_t)((32768.0f / (micscale * inst->refScale)));

        for (int n = 0; n < framelen; n++) {
            errorSignal[n] = (int16_t)((inverseMicScale * (int32_t)errorSignal[n])>>15); // 잔차 신호를 역으로 스케일링
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[5] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif
#ifdef ENABLE_PROFILING
        start = clock();
#endif
        // float noisedB = -30.0f; // 노이즈 강도 조정   

        // // 핑크 노이즈 생성
        // generate_pink_noise(noiseBuffer, framelen, 0.1);

        // // 컴포트 노이즈 추가
        // add_comfort_noise(errorSignal, noiseBuffer, framelen, noisedB);    

        for (int i = 0; i < framelen; i++) {
            inBufMic[i] = errorSignal[i];
        }

        // de_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r_d, framelen);
        // de_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d_d, framelen);
    }



#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[6] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        total_end = clock();
        totalRunTime += ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
    // 평균 시간 출력
    count++;
    if (totalRunTime >= PRINT_INTERVAL) {
        printf("Average execution time for each step (1-second intervals):\n");
        for (int i = 0; i < NUM_STEPS; i++) {
            double averageTime = totalTime[i] / count;
            double percentage = (totalTime[i] / totalRunTime) * 100.0;
            printf("Step %d: Average Time = %.2f milli seconds, Percentage = %f%%\n", i + 1, averageTime* 1000, percentage);
        }
        // 총 실행 시간 및 각 단계별 누적 시간 초기화
        totalRunTime = 0.0;
        for (int i = 0; i < NUM_STEPS; i++) {
            totalTime[i] = 0.0;
        }
        count = 0; // 카운트 초기화
    }
#endif

}

void calc_prod(float * prod, float * weight, int32_t filt_len){
    float max_sum = 0;
    float prod_sum = 0;

    for (int i=0; i<filt_len; i++){
        prod[i] = sqrtf(weight[i] * weight[i]);
        if (prod[i]>max_sum)
        max_sum = prod[i];
    }

    for (int i=0; i<filt_len; i++){
        prod[i] += max_sum * 0.1;
        prod_sum += prod[i];
    }

    float inv = 0.99/prod_sum;

    for (int i=0; i<filt_len; i++){
        prod[i] = inv * prod[i];
    }
}

int AEC_single_Proc_filter_save(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB, float** filter, int* filter_len, int* globaldelay) {
    aecInst_t *inst = (aecInst_t *)aecInst;
    float refPower, refRMS;
    float micPower, micRMS;

    int16_t refSignal[FRAME_SIZE];
    int16_t errorSignal[FRAME_SIZE];
    // int16_t errorSignal_second[FRAME_SIZE];
    float noiseBuffer[FRAME_SIZE];

    int updateon=1;
    float micscale = powf(10.0f, MicscaledB/20.0);
    int32_t pre_emph_q15 = (int32_t)(0.7f * 32768.0f);

#ifdef ENABLE_PROFILING
        total_start = clock();
#endif

    if (inBufRef != NULL) {

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        if ((delay_auto == 1)&&(inst->delayinit==0)) {
            // 버퍼에 현재 프레임을 추가 (순환 버퍼처럼 사용)
            memmove(&inst->ref_Dly_Buffer[0], &inst->ref_Dly_Buffer[framelen], (DELAY_BUFFER_SIZE - framelen) * sizeof(int16_t));
            memmove(&inst->mic_Dly_Buffer[0], &inst->mic_Dly_Buffer[framelen], (DELAY_BUFFER_SIZE - framelen) * sizeof(int16_t));
            memcpy(&inst->ref_Dly_Buffer[DELAY_BUFFER_SIZE - framelen], inBufRef, framelen * sizeof(int16_t));
            memcpy(&inst->mic_Dly_Buffer[DELAY_BUFFER_SIZE - framelen], inBufMic, framelen * sizeof(int16_t));

            refRMS = calculate_rms_int32(inBufRef, framelen);
            refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환

            if (refPower > MIN_POWER) {
                inst->corr_hold = 2;
            }

            inst->corr_hold--;
            if (inst->corr_hold <= 0) inst->corr_hold = 0;

            if (inst->corr_hold > 0) {
                calculate_global_delay(inst->ref_Dly_Buffer, inst->mic_Dly_Buffer, framelen, inst->crossCorr, &inst->frameCount, &inst->estimatedDelay, &inst->delayinit, &inst->delayupdated);
            }

            if (inst->delayupdated == 1) {
                printf("AEC_single_Proc_filter_save :: delayupdated done. config_p->aecInst_p[i] = %p delay = %d\r\n", inst, inst->estimatedDelay);
                AEC_delay_init(inst, &inst->d_buf_g[0][0], Ref_delay);
                inst->delayupdated = 0;
                inst->adapted = 0;
                inst->sum_adapt = 0;
            }
        }

        if (inst->delayinit == 0) {
            inst->estimatedDelay = delay;
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[0] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif        

        if ((delay_auto != 1)||(inst->delayinit==1)) {

            memcpy(&refSignal[0], &inBufRef[0], sizeof(int16_t)*framelen);

            // pre_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r, framelen);
            pre_emphasis(&refSignal[0], pre_emph_q15, &inst->mem_r, framelen);
            pre_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d, framelen);

            // 글로벌 딜레이 적용
            if (inst->estimatedDelay >= 0){
                // AEC_delay_with_framelen(inst, &inBufRef[0], &inBufRef[0], &inst->d_buf_g[0][0], inst->estimatedDelay, framelen);  
                AEC_delay_with_framelen(inst, &refSignal[0], &refSignal[0], &inst->d_buf_g[0][0], inst->estimatedDelay, framelen);          
            } else {
                AEC_delay_with_framelen(inst, &inBufMic[0], &inBufMic[0], &inst->d_buf_g[0][0], -1*inst->estimatedDelay, framelen);        
            }
            

            // 참조 신호의 RMS 크기 계산
            // refRMS = calculate_rms_int32(inBufRef, framelen);
            refRMS = calculate_rms_int32(refSignal, framelen);
            refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환
            // printf("refPower = %.2fdB, micPower=%.2f \r\n",refPower, micPower);     

            // 참조 신호와 마이크 신호를 부드럽게 노멀라이즈
            if (refPower > MIN_POWER) {
                // smooth_normalize_signal_rms_int32(inBufRef, refRMS, framelen, &inst->refScale, -25.0);
                smooth_normalize_signal_rms_int32(refSignal, refRMS, framelen, &inst->refScale, -25.0);

                // micScaleQ15 = (int32_t)(micScale * inst->refScale * 32768.0f);
                int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

                for (int i = 0; i < framelen; i++) {
                    inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
                }

                updateon = 1;

            } else {
                int32_t refScaleQ15 = (int32_t)(inst->refScale * 32768.0f);
                // micScaleQ15 = (int32_t)(micScale * inst->refScale * 32768.0f);
                int32_t micScaleQ15 = (int32_t)(micscale * inst->refScale * 32768.0f);

                for (int i = 0; i < framelen; i++) {
                    // inBufRef[i] = (int16_t)(((int32_t)(inBufRef[i]) * refScaleQ15) >> 15);
                    refSignal[i] = (int16_t)(((int32_t)(refSignal[i]) * refScaleQ15) >> 15);
                    inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
                }

                updateon = 0;
            }
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[1] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        if ((delay_auto != 1)||(inst->delayinit==1)) {
            // NLMS 에코 캔슬러 실행
            // nlms_echo_canceller_two_path_strong(inst, inBufRef, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
            nlms_echo_canceller_two_path_strong(inst, refSignal, inBufMic, inst->weightsback,  inst->weightsfore, inst->leaky, errorSignal, inst->uBuf, refPower, &inst->prevAttenuation, framelen, updateon);
        }
#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[3] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
        start = clock();
#endif

        if ((delay_auto != 1)||(inst->delayinit==1)) {

            float errRMS = calculate_rms_int32(errorSignal, framelen);
            float errPower = 20.0f * log10f(errRMS + 1e-10f) - 90.3090f; // dBFS로 변환

            micRMS = calculate_rms_int32(inBufMic, framelen);
            micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환
            // printf("micPower: %.2f errPower: %.2f prevAttenuation = %.2f\n", micPower, errPower, prevAttenuation);

            // int32_t inverseMicScale = (int32_t)(32768.0f / (inst->refScale));
            // for (int i = 0; i < framelen; i++) {
            //     // inBufRef[i] = inBufMic[i];
            //     inBufRef[i] = (int16_t)((inverseMicScale * (int32_t)inBufMic[i])>>15); // 잔차 신호를 역으로 스케일링
            // }            

            // float gate_dB = 0.0f;
            // if (refPower > MIN_POWER && (errPower < (micPower-15.0f)|| (inst->adapted == 0))) {
            //     gate_dB = -10.0f;
            // }
            // float gate_level = powf(10.0f, (gate_dB/20.0f));
            // inst->gate_aec = inst->gate_aec * 0.5 + gate_level * 0.5;
            
            // inverseMicScale = (int32_t)((32768.0f / (micscale * inst->refScale)) * inst->gate_aec);
            int32_t inverseMicScale = (int32_t)((32768.0f / (micscale * inst->refScale)));

            for (int n = 0; n < framelen; n++) {
                errorSignal[n] = (int16_t)((inverseMicScale * (int32_t)errorSignal[n])>>15); // 잔차 신호를 역으로 스케일링
            }
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[4] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif
#ifdef ENABLE_PROFILING
        start = clock();
#endif

        if ((delay_auto != 1)||(inst->delayinit==1)) {
            // float noisedB = -30.0f; // 노이즈 강도 조정   

            // // 핑크 노이즈 생성
            // generate_pink_noise(noiseBuffer, framelen, 0.1);

            // // 컴포트 노이즈 추가
            // add_comfort_noise(errorSignal, noiseBuffer, framelen, noisedB);    

            for (int i = 0; i < framelen; i++) {
                inBufMic[i] = errorSignal[i];
            }

            // de_emphasis(&inBufRef[0], pre_emph_q15, &inst->mem_r_d, framelen);
            de_emphasis(&inBufMic[0], pre_emph_q15, &inst->mem_d_d, framelen);
        }

#ifdef ENABLE_PROFILING
        end = clock();
        stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
        totalTime[5] += stepTime;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

    }

#ifdef ENABLE_PROFILING
        total_end = clock();
        totalRunTime += ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

        // printf("Time for NLMS echo canceller: %.2f milli seconds\n", stepTime*1000);     
#endif

#ifdef ENABLE_PROFILING
    // 평균 시간 출력
    count++;
    if (totalRunTime >= PRINT_INTERVAL) {
        printf("Average execution time for each step (1-second intervals):\n");
        for (int i = 0; i < NUM_STEPS; i++) {
            double averageTime = totalTime[i] / count;
            double percentage = (totalTime[i] / totalRunTime) * 100.0;
            printf("Step %d: Average Time = %.2f milli seconds, Percentage = %f%%\n", i + 1, averageTime* 1000, percentage);
        }
        // 총 실행 시간 및 각 단계별 누적 시간 초기화
        totalRunTime = 0.0;
        for (int i = 0; i < NUM_STEPS; i++) {
            totalTime[i] = 0.0;
        }
        count = 0; // 카운트 초기화
    }
#endif

    if (inst->adapted == 1 && inst->delayinit == 1){
        *filter = inst->weightsfore;
        *filter_len = (int)FILTER_LENGTH;
        *globaldelay = inst->estimatedDelay;
        inst->adapted = 0;
        inst->sum_adapt = 0;        
        return 1;
    }

    return 0;
    

}

void AEC_single_filter_load(aecInst_t *aecInst, float* filter, int filter_len, int global_delay){
        
        aecInst_t *inst = (aecInst_t *)aecInst;

        float *filter_p = filter;
        for (int i=0; i<filter_len; i++){
            inst->weightsfore[i] = filter_p[i];
            inst->weightsback[i] = filter_p[i];
        }

        inst->adapted = 1;
        inst->delayinit == 1;
        inst->estimatedDelay = global_delay;
}


// 크로스 코릴레이션을 사용하여 글로벌 딜레이를 계산하는 함수 (정수 연산)
void calculate_global_delay(int16_t *refBuffer, int16_t *micBuffer, int framelen, long long *crossCorr, int *frameCount, int *estimatedDelay, int *delayinit, int *delayupdated) {
    // 각 프레임마다 크로스 코릴레이션 계산 (정수 연산)
    for (int delay = -MAX_DELAY; delay <= MAX_DELAY; delay++) {
        long long sum = 0;
        for (int i = 0; i < framelen; i++) {
            int refIndex = i;
            int micIndex = i + delay + MAX_DELAY; // micBuffer에서의 인덱스

            // 범위를 벗어나지 않는 경우에만 계산
            if (micIndex >= 0 && micIndex < DELAY_BUFFER_SIZE) {
                sum += (long long)refBuffer[refIndex] * (long long)micBuffer[micIndex];
            }
        }
        crossCorr[delay + MAX_DELAY] += sum; // 누적 결과에 합산
    }

    (*frameCount)++;

    // 5초 동안 모든 프레임을 처리한 경우 최종 결과 계산
    if (*frameCount >= (SAMPLE_RATE * 3 / framelen)) { // 5초 동안 모든 프레임 처리 완료
        long long maxCorr = LONG_LONG_MIN;
        int delay_result = 0;
        for (int delay = -MAX_DELAY; delay <= MAX_DELAY; delay++) {
            if ((crossCorr[delay + MAX_DELAY]) > maxCorr) {
                maxCorr = (crossCorr[delay + MAX_DELAY]);

                delay_result = delay+MAX_DELAY-50;
            }
        }

        if (*delayinit == 0){
            *estimatedDelay = delay_result;
            *delayinit = 1;
            *delayupdated = 1;
        }

        // if (abs(delay_result-*estimatedDelay)>100) {
        //     *estimatedDelay = delay_result;        
        //     *delayupdated = 1;
        //     *delayinit = 1;
        // }

        printf("calculate_global_delay :: delay_result = %d, Estimated Global Delay: %d\n", delay_result, *estimatedDelay);        
            

        // // 추정된 딜레이 출력
        // printf("Estimated Global Delay: %d\n", *estimatedDelay);

        // 누적 결과 초기화
        for (int i = 0; i < (MAX_DELAY * 2 + 1); i++) {
            crossCorr[i] = 0;
        }
        *frameCount = 0; // 프레임 카운트 초기화
    }
}


int pre_emphasis(int16_t *signal, int32_t pre_emph_q15, int32_t *mem, size_t frame_size) {

    int saturated = 0;

    for (size_t i = 0; i < frame_size; i++) {

        // Pre-emphasis 계산 (Q15 형식)
        int32_t tmp = (int32_t)signal[i] - ((pre_emph_q15 * (*mem))>>15);

         if (tmp > 32767)
         {
            tmp = 32767;
            saturated = 1;
         }
         if (tmp < -32767)
         {
            tmp = -32767;
            saturated = 1;
         }

        // 메모리를 업데이트 (이전 샘플 저장)
        *mem = (int32_t)signal[i];

        // 결과를 다시 int16_t로 변환
        signal[i] = (int16_t)(tmp);
    }

    return saturated;

}

int de_emphasis(int16_t *signal, int32_t de_emph_q15, int32_t *mem, size_t frame_size) {

    int saturated = 0;

    for (size_t i = 0; i < frame_size; i++) {
        // De-emphasis 계산 (Q15 형식)
        int32_t tmp = (int32_t)signal[i] + ((de_emph_q15 * (*mem)) >> 15);

         if (tmp > 32767)
         {
            tmp = 32767;
            saturated = 1;
         }
         if (tmp < -32767)
         {
            tmp = -32767;
            saturated = 1;
         }

        // 메모리를 업데이트 (이전 샘플 저장)
        *mem = (int32_t)signal[i];

        // 결과를 다시 int16_t로 변환
        signal[i] = (int16_t)(tmp);
    }

    return saturated;
}

// dBFS 값을 리니어 게인 값으로 변환
float dbfs_to_line(float dbfs) {
    return powf(10.0f, dbfs / 20.0f);
}

// 핑크 노이즈 생성 함수
void generate_pink_noise(float *noiseBuffer, int length, float noiseLevel) {
    float b[7] = {0.02109238, 0.07113478, 0.68873558, 0.12020611, 0.12509026, 0.13112453, 0.00051465};
    float state[6] = {0};

    float scale = Q15_MAXf / (float)RAND_MAX;

    for (int i = 0; i < length; i++) {
        float white = ((float)rand() * scale) * 2.0f - Q15_MAXf; // -1.0 ~ 1.0 사이의 화이트 노이즈
        float pink = white * b[0] + state[0] * b[1] + state[1] * b[2] + state[2] * b[3] +
                     state[3] * b[4] + state[4] * b[5] + state[5] * b[6];

        // 상태 업데이트
        for (int j = 5; j > 0; j--) {
            state[j] = state[j - 1];
        }
        state[0] = white;

        // 노이즈 크기 조정
        noiseBuffer[i] = pink * noiseLevel;

        // printf("pink= %f white =%f, noiseBuffer[i] = %f noiseLevel = %f\n", pink, white, noiseBuffer[i], noiseLevel);
    }
}

// 컴포트 노이즈 추가 함수
void aec_comfort_noise(int16_t *signal, float *noise, int length, float noiseGainDbfs) {

    float noiseGain = dbfs_to_line(noiseGainDbfs);

    for (int i = 0; i < length; i++) {
        float mixed = (float)signal[i] + noise[i] * noiseGain;
        if (mixed > 32767.0f) {
            mixed = 32767.0f; // 클리핑 방지
        } else if (mixed < -32768.0f) {
            mixed = -32768.0f; // 클리핑 방지
        }
        signal[i] = (int16_t)mixed;

        // printf("noise[i] = %f signal[i] = %d, mixed=%f\n", noise[i], signal[i], mixed);
    }
}
