/*
 * polyphase.h
 *
 *  Created on: 2018. 8. 24.
 *      Author: sbroad01
 */

#ifndef POLYPHASE_H_
#define POLYPHASE_H_

#include "sys.h"
#include "pffft.h"

typedef struct polyInst_s
{
    int blocksize;
    int N; // polyphase filter length
    int M; // filterbank channel number
    int R; // decimation factor
    int L; // blocksize/R : channel samples within a block
    int bits;

    void *p_inBuf[PolyinputN];   // polyphase analysis buffer
    void *p_outBuf[PolyOutputN]; // polyphase synthesis buffer
    void *p_inwin;               // polyphase analysis filter pointer
    void *p_outwin;              // polyphase analysis filter pointer

    int32_t *fft_xin;  // malloc
    int32_t *fft_xout; // malloc

    short *inbuf1[PolyinputN];  // need malloc
    short *outbuf1[PolyOutputN]; // need malloc

    PFFFT_Setup *p_pffft;
    float *pffftin;
    float *pffftout;
    float *pffftwork;
} polyInst_t;

polyInst_t *sysPolyCreate();
int Poly_Init(void *polyInst);
void Poly_DeInit(void **polyInst);
void poly_analysis(void *polyInst, void *in, void *fft_out_mat, int inputN, int scale);
void poly_synthesis(void *polyInst, void *fft_in_mat, void *out, int outputN, int scale);

#endif /* POLYPHASE_H_ */
