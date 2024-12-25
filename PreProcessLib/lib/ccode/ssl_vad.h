/*
 * ssl_vad.h
 *
 *  Created on: 2019. 6. 24.
 *      Author: Seong Pil Moon
 */

#ifndef SSL_VAD_H_
#define SSL_VAD_H_

#define NWz 151
#define ssl_blocksize 256
#define ssl_blocksize512 512
#define vad_hold_max_def 35
#define vad_hold_min_def 1

typedef struct vadInst_s {
    short       blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block

    double tau_s_r;
    double tau_s_f;

    double d_SNR;
    double d_SNR_ratio;
    double d_SNR_vad;
    double d_SNR_vad_ratio;
    double t_chg_min;

    int n_floor_min;
	int n_floor;

    int vad_hold_max;
    int vad_hold_min;

} vadInst_t;

vadInst_t * sysSSLVADCreate();
void ssl_vad_Init(vadInst_t *vadInst);
void ssl_vad_DeInit();
void ssl_filtering(vadInst_t *vadInst, short *in, int *inBuf, short *out);
void ssl_vad_process(vadInst_t *vadInst, short *in, short *ssl_vad_p, short *min_vad_p, short *max_vad_p, int channel);
int filter_Q15(int * x_p, int * filtQ15_p, short filt_leng);


// extern int n_floor;
// extern double x_slow_vad;
extern int BPF_Buf[5][NWz+NUM_FRAMES-1];
extern vadInst_t Sys_vadInst;

extern double g_x_slow_vad[2];
extern int g_n_floor[2];
extern int g_state[2];


#endif /* SSL_VAD_H_ */


