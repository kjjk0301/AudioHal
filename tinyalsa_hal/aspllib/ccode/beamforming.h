/*
 * beamforming.h
 *
 *  Created on: 2018. 9. 2.
 *      Author: Seong Pil Moon
 */

#ifndef BEAMFORMING_H_
#define BEAMFORMING_H_

typedef struct bfInst_s {
    short       blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block
    int         bits;
	
    int			MN; // number of microphones 
    int			J;  //  number of beam channels : BeamN

	int32_t 		*fft_bfin_mat[MicN];
	int32_t 		*fft_bfout_mat[BeamN];	
	int32_t 		*fft_bfout_rear_mat[BeamN];		

    int         taps;
    int         bufsize;
    int         delay;
    int         mu_tapQ15;

    double      mu;

//	    short*      p_w_real[PolyM];
//	    short*      p_w_imag[PolyM];
    int*      p_w_real[BeamN][PolyM];
    int*      p_w_imag[BeamN][PolyM];	

    int32_t*      p_uBuf_real[BeamN][PolyM];
    int32_t*      p_uBuf_imag[BeamN][PolyM];

    int*        p_xref_pow[BeamN][PolyM];
    int*        p_err_pow[BeamN][PolyM];
	
} bfInst_t;

bfInst_t* sysBFCreate();
void BF_Init(bfInst_t *bfInst, void *fft_xin_mat, void *fft_xout_mat, void *fft_xout_rear_mat);
void BF_process(bfInst_t *bfInst);
void BF_process_rear(bfInst_t *bfInst);
void BF_delay(bfInst_t *bfInst, int32_t *inBuf, int32_t *outBuf, int32_t *d_Buf, int delay);
void BF_delay_init(bfInst_t *bfInst, int32_t *d_Buf, int delay);
void BF_adaptive_Proc(bfInst_t *bfInst, int32_t *inBufRef, int32_t *inBufMic, int32_t *outBuf, int band, int Beam);
void BF_adaptive_Init(bfInst_t *bfInst);

extern bfInst_t Sys_bfInst;
extern int32_t 	bf_d_buf_g[BeamN][PolyM][(adaptive_BF_delay+PolyL+5)*2];

#endif /* NOISE_SUPRESSION_H_ */

