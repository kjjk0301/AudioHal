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
#define MAX_VAD_CHANNELS 5
#define MAX_VAD_FILTER_CHANNELS 5
#define avgsize 64      // 128 //64
#define vad_idx_total 6 // NUM_FRAMES/ssl_blocksize
#define MAX_vad_hist 80

typedef struct vadInst_s
{
    short blocksize;
    int N; // polyphase filter length
    int M; // filterbank channel number
    int R; // decimation factor
    int L; // blocksize/R : channel samples within a block

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

    // Global variables <- need to be included in structure
    // int **BPF_Buf; // need malloc, buffers for VAD filter calculation
    int *BPF_Buf[MAX_VAD_FILTER_CHANNELS];
    int e_hist_Buf[MAX_VAD_CHANNELS][avgsize + vad_idx_total - 1];
    double g_x_slow_vad[MAX_VAD_CHANNELS];
    double g_t_chg[MAX_VAD_CHANNELS];
    double g_t_vad[MAX_VAD_CHANNELS];
    int g_n_floor[MAX_VAD_CHANNELS];
    int g_state[MAX_VAD_CHANNELS];
    int vad_hist1[MAX_vad_hist];
    int vad_hist2[MAX_vad_hist];
    // Global variables <- need to be included in structure

} vadInst_t;

vadInst_t *sysSSLVADCreate();
void ssl_vad_Init(void *vadInst);
void ssl_vad_DeInit(void **vadInst);
void ssl_filtering(vadInst_t *vadInst, short *in, int *inBuf, short *out);
void ssl_vad_process(vadInst_t *vadInst, short *in, short *ssl_vad_p, short *min_vad_p, short *max_vad_p, int channel);
int filter_Q15(int *x_p, int *filtQ15_p, short filt_leng);

// extern int BPF_Buf[5][NWz + NUM_FRAMES - 1];
// extern vadInst_t Sys_vadInst;

#endif /* SSL_VAD_H_ */
