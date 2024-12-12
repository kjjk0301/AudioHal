/*
 * echo_canceller.h
 *
 *  Created on: 2018. 8. 29.
 *      Author: SEONGPIL
 */

#ifndef ECHO_CANCELLER_H_
#define ECHO_CANCELLER_H_

#include "sys.h"

typedef struct aecInst_s {
    int         blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block
    int         bits;
    int         taps;
    int         bufsize;
    int16_t     delay;
    int         mu_tapQ15;
    int         mu_tapQ15_LP;
    int         mu_optQ15;
    double      mu_opt_d_slow;
    double      mu_opt_d_slow_multi[MicN];

    int         taps_single;
    int         taps_multi;
    int         bufsize_single;
    int         bufsize_multi;
    int         delay_single;
    int         delay_multi;

    int*      p_w_real[PolyM];
    int*      p_w_imag[PolyM];

    int*      p_w_LP;

    int32_t*      p_uBuf_real[PolyM];
    int32_t*      p_uBuf_imag[PolyM];

    short*      p_uBuf_LP;

    int*        p_xref_pow[PolyM];
    int*        p_err_pow[PolyM];
    int*        p_xref_LP_pow;

    float tau_p_r;
    float tau_p_f;

    float tau_e_r;
    float tau_e_f;

    double mu_tap_d;
    double mu_opt_d;

    double reg1_d;
    double reg2_d;


} aecInst_t;

aecInst_t* sysAECCreate();
aecInst_t* sysAEC_multi_Create();
void AEC_Init(aecInst_t *aecInst, int32_t frame_len);
void AEC_multi_Init(aecInst_t *aecInst, int32_t frame_len);
void AEC_Proc(aecInst_t *aecInst, int32_t *inBufRef, int32_t *inBufMic, int32_t *outBuf, int band);
void AEC_delay(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay);
int AEC_delay_with_framelen(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay, int framelen);
void AEC_delay_init(aecInst_t *aecInst, short *d_Buf, int delay);
// void AEC_LP_Proc(aecInst_t *aecInst, short *inBufRef, short *outBuf);
void AEC_single_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf);
void AEC_single_Proc_with_len(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen);
void AEC_single_Proc_double(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf);
void AEC_multi_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan);
void AEC_multi_Proc_with_len(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan, int framelen);
void AEC_multi_Proc_double(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan);

// 함수 선언

float calculate_rms_int32(int16_t *signal, int length);
void smooth_normalize_signal_rms_int32(int16_t *signal, float rmsVal, int length, float *currentScale, float targetRMSdB);
void apply_global_delay_int16(int16_t *signal, int16_t *delayBuffer, int delay, int length);
void nlms_echo_canceller(int16_t *refSignal, int16_t *micSignal, float *weights, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon);
void bandpass_filter_float(float *signal, int length, double *b, double *a, int order, double *xState, double *yState);
void calculate_global_delay(int16_t *refBuffer, int16_t *micBuffer, int framelen, long long *crossCorr, int *frameCount, int *estimatedDelay, int *delayinit, int *delayupdated);
void AEC_single_Proc_matlab(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, int filton, float MicscaledB);
int pre_emphasis(int16_t *signal, int32_t pre_emph_q15, int32_t *mem, size_t frame_size);
int de_emphasis(int16_t *signal, int32_t de_emph_q15, int32_t *mem, size_t frame_size);


extern aecInst_t Sys_aecInst;
//	extern short d_buf_g[MicN][Echo_delay+polyblocksize];
//	extern short d_buf_g[MicN][Ref_delay-Echo_delay-PolyR+polyblocksize];
extern int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max];
// extern int16_t * d_buf_g[MicN+RefN];


#endif /* AEC_CORE_H_ */
