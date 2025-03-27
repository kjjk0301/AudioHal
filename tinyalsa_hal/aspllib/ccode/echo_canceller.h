/*
 * echo_canceller.h
 *
 *  Created on: 2018. 8. 29.
 *      Author: SEONGPIL
 */

 #ifndef ECHO_CANCELLER_H_
 #define ECHO_CANCELLER_H_
 
 #include "sys.h"
 
 #define SHR16(a,shift) ((a) >> (shift))
#define SHL16(a,shift) ((a) << (shift))
#define SHR32(a,shift) ((a) >> (shift))
#define SHL32(a,shift) ((a) << (shift))
#define SHR(a,shift) ((a) >> (shift))
#define SHL(a,shift) ((int32_t)(a) << (shift))

#define ADD16(a,b) ((int16_t)((int16_t)(a)+(int16_t)(b)))
#define SUB16(a,b) ((int16_t)(a)-(int16_t)(b))
#define ADD32(a,b) ((int32_t)(a)+(int32_t)(b))
#define SUB32(a,b) ((int32_t)(a)-(int32_t)(b))

#define MULT16_16(a,b)     (((int32_t)(int16_t)(a))*((int32_t)(int16_t)(b)))

#define MAC16_16(c,a,b) (ADD32((c),MULT16_16((a),(b))))

#define MULT16_32_P15(a,b) ADD32(MULT16_32_32(a,SHR((b),15)), PSHR(MULT16_16((a),((b)&0x00007fff)),15))
#define MULT16_32_Q15(a,b) ADD32(MULT16_32_32(a,SHR((b),15)), SHR(MULT16_16((a),((b)&0x00007fff)),15))
#define MAC16_32_Q15(c,a,b) ADD32(c,MULT16_32_Q15(a,b))

#define MAC16_16_Q11(c,a,b)     (ADD32((c),SHR(MULT16_16((a),(b)),11)))
#define MAC16_16_Q13(c,a,b)     (ADD32((c),SHR(MULT16_16((a),(b)),13)))
#define MAC16_16_P13(c,a,b)     (ADD32((c),SHR(ADD32(4096,MULT16_16((a),(b))),13)))

#define MULT16_16_Q11_32(a,b) (SHR(MULT16_16((a),(b)),11))
#define MULT16_16_Q13(a,b) (SHR(MULT16_16((a),(b)),13))
#define MULT16_16_Q14(a,b) (SHR(MULT16_16((a),(b)),14))
#define MULT16_16_Q15(a,b) (SHR(MULT16_16((a),(b)),15))

#define MULT16_16_P13(a,b) (SHR(ADD32(4096,MULT16_16((a),(b))),13))
#define MULT16_16_P14(a,b) (SHR(ADD32(8192,MULT16_16((a),(b))),14))
#define MULT16_16_P15(a,b) (SHR(ADD32(16384,MULT16_16((a),(b))),15))

#define MUL_16_32_R15(a,bh,bl) ADD32(MULT16_16((a),(bh)), SHR(MULT16_16((a),(bl)),15))

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
     float *trans_window256;
     float *trans_window512;
     float *trans_window1024;
     float *trans_window1600;
     int16_t *ref_Dly_Buffer; // 레퍼런스 신호 버퍼
     int16_t *mic_Dly_Buffer; // 마이크 신호 버퍼
     long long *crossCorr;    // 누적 크로스 코릴레이션 결과
     float *prod;

     int32_t *weightsQ18;
     int32_t *prodQ15;     
 
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
 
     int chan_num;
 
 } aecInst_t;
 
 aecInst_t* sysAECCreate(int channum);
 void AEC_Init(aecInst_t *aecInst);
 int AEC_delay_with_framelen(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay, int framelen);
 void AEC_delay_init(aecInst_t *aecInst, short *d_Buf, int delay);
 
 // 함수 선언
 float calculate_rms_int32(int16_t *signal, int length);
 void smooth_normalize_signal_rms_int32(int16_t *signal, float rmsVal, int length, float *currentScale, float targetRMSdB);
 void nlms_echo_canceller(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, int32_t *weightsQ18, int16_t *errorSignal, int32_t *x, float refPowerdB, float *prevAttenuation, int length, int updateon);
 void nlms_echo_canceller_two_path(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);
 void nlms_echo_canceller_two_path_strong(aecInst_t *aecInst, int16_t *refSignal, int16_t *micSignal, float *weights, float *weights_fore, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);
 
 void calculate_global_delay(int16_t *refBuffer, int16_t *micBuffer, int framelen, long long *crossCorr, int *frameCount, int *estimatedDelay, int *delayinit, int *delayupdated);
 void AEC_single_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, float MicscaledB);
 void AEC_2ch_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB);
 int AEC_single_Proc_filter_save(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, float MicscaledB, float** filter, int* filter_len, int* globaldelay);
 int AEC_single_filter_load(aecInst_t *aecInst, float* filter, int filter_len, int global_delay);
 int pre_emphasis(int16_t *signal, int32_t pre_emph_q15, int32_t *mem, size_t frame_size);
 int de_emphasis(int16_t *signal, int32_t de_emph_q15, int32_t *mem, size_t frame_size);
 
 // extern aecInst_t Sys_aecInst;
 // extern int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max];
 
 #endif /* ECHO_CANCELLER_H_ */
 