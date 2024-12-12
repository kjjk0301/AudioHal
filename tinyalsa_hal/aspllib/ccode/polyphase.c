/*
 * polyphase.c
 *
 *  Created on: 2018. 8. 24.
 *      Author: sbroad01
 */
#include <string.h>
#include <stdio.h>
//#include "sys.h"
#include "polyphase.h"
#include "polyphase_win.h"
#include <stdint.h>
#include <math.h>

#include "pffft.h"
#define MAX_OF(x,y) ((x)>(y)?(x):(y))

// #include "fftw3.h"

#include "aspl_nr.h"

#pragma DATA_ALIGN(fft_xin, 8)
int32_t fft_xin[PolyM*2];
#pragma DATA_ALIGN(fft_xout, 8)
int32_t fft_xout[PolyM*2];

#pragma DATA_ALIGN(inbuf1, 8);
short inbuf1 [PolyinputN][polyblocksize+PolyN-PolyR];
#pragma DATA_ALIGN(outbuf1, 8);
short outbuf1 [PolyOutputN][polyblocksize+PolyN-PolyR];

// fftwf_complex *TempFFTIN, *TempFFT;
// fftwf_plan p, pinv;

PFFFT_Setup *p_pffft = NULL;
float *pffftin = NULL;
float *pffftout = NULL;
float *pffftwork = NULL;

polyInst_t Sys_polyInst;

void free_pffts(PFFFT_Setup **s, float **X, float **Y, float **Z);

polyInst_t* sysPolyCreate()
{
    int ret;
    // printf("sysPolyCreate in\n");
    // printf("p_pffft = %p\n", p_pffft);
    // printf("pffftin = %p\n", pffftin);
    // printf("pffftout = %p\n", pffftout);
    // printf("pffftwork = %p\n", pffftwork);

    ret = Poly_Init(&Sys_polyInst);

    // printf("sysPolyCreate out\n");
    // printf("p_pffft = %p\n", p_pffft);
    // printf("pffftin = %p\n", pffftin);
    // printf("pffftout = %p\n", pffftout);
    // printf("pffftwork = %p\n", pffftwork);

    return &Sys_polyInst;
}

void Poly_DeInit(){

    // printf("Poly_DeInit in\n");
    // printf("p_pffft = %p\n", p_pffft);
    // printf("pffftin = %p\n", pffftin);
    // printf("pffftout = %p\n", pffftout);
    // printf("pffftwork = %p\n", pffftwork);

   free_pffts(&p_pffft, &pffftin, &pffftout, &pffftwork);

    // printf("Poly_DeInit out\n");
    // printf("p_pffft = %p\n", p_pffft);
    // printf("pffftin = %p\n", pffftin);
    // printf("pffftout = %p\n", pffftout);
    // printf("pffftwork = %p\n", pffftwork);
}

void free_pffts(PFFFT_Setup **s, float **X, float **Y, float **Z){
    if (*s!=NULL) {
        pffft_destroy_setup(*s);
        *s=NULL;
    }
    if (*X!=NULL) {
        pffft_aligned_free(*X);
        *X=NULL;
    }
    if (*Y!=NULL) {
        pffft_aligned_free(*Y);
        *Y=NULL;
    }
    if (*Z!=NULL) {
        pffft_aligned_free(*Z);
        *Z=NULL;
    }    
}

#ifndef PFFFT_SIMD_DISABLE
void validate_pffft_simd(); // a small function inside pffft.c that will detect compiler bugs with respect to simd instruction 
#endif

int Poly_Init(void *polyInst){
    int n;
    polyInst_t *inst = (polyInst_t *)polyInst;

    inst->blocksize=polyblocksize;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block
//	    inst->bits=Q15;

    for (n=0 ; n<PolyinputN ; n++){
        inst->p_inBuf[n]=&inbuf1[n][0]; // polyphase analysis buffer
    }
	for (n=0 ; n<PolyOutputN; n++){
    inst->p_outBuf[n]=&outbuf1[n][0]; // polyphase analysis buffer
	}
    inst->p_inwin=win_in; // polyphase analysis filter pointer
    inst->p_outwin=win_out; // polyphase synthesis filter pointer

    for (n=0 ; n<PolyinputN ; n++){
        memset(&inbuf1[n][0], 0, sizeof(short)*(polyblocksize+PolyN-PolyR));
    }
	for (n=0 ; n<PolyOutputN; n++){
	    memset(&outbuf1[n][0], 0, sizeof(short)*(polyblocksize+PolyN-PolyR));
	}

    int cplx = 0;
    int N = PolyM;
    int Nfloat = (cplx ? N*2 : N);
    int Nbytes = Nfloat * sizeof(float);
    pffftin = pffft_aligned_malloc(Nbytes);
    pffftout = pffft_aligned_malloc(Nbytes);
    pffftwork = pffft_aligned_malloc(Nbytes);

    // PFFFT benchmark
    p_pffft = pffft_new_setup(N, cplx ? PFFFT_COMPLEX : PFFFT_REAL);

// #ifndef PFFFT_SIMD_DISABLE
//     validate_pffft_simd();
// #endif

    return 0;
}



void poly_analysis(void *polyInst, void *in, void *fft_out_mat, int inputN, int scale)
{
    int i, k, n, m;
    polyInst_t *inst = (polyInst_t *)polyInst;

    int32_t       frame_xin[PolyN];

    short *xin = (short *)in;
    short *win = (short *)inst->p_inwin;
    short *buf = (short *)inst->p_inBuf[inputN];
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
	float deno=2.0/(float)((PolyM*scale));
    // float deno=1.0/(float)((scale));
    

    memcpy(&buf[PolyN-PolyR], xin, sizeof(short)*polyblocksize);

    for (k=0 ; k<PolyL ; k++){
        for (n=k*PolyR, m=0 ; n<(PolyN+k*PolyR) ; n++) {
            // frame_xin[m]=(short)(((int)buf[n]*(int)win[m]+16384)>>Q15);
            frame_xin[m]=((int)buf[n]*(int)win[m]);
            frame_xin[m]=(frame_xin[m]+1)>>1;
            m++;
        }

        memset(fft_xin, 0, sizeof(int32_t)*(PolyM*2));

        for (n=0, m=0 ; n<(PolyM*2) ; n+=2) {
            fft_xin[n]=frame_xin[m];
            m++;
        }
        for (n=0, m=(PolyN>>1) ; n<(PolyM*2) ; n+=2) {
            fft_xin[n]=fft_xin[n]+frame_xin[m];
            m++;
        }

// 		for (n = 0, i=0; n<PolyM*2; n+=2) {
// 			TempFFTIN[i][0] = ((float)fft_xin[n])*deno;
// 			i++;
// 		}

//     	fftwf_execute(p);

//         // fft_xout_mat[k*2]=0;
//         // fft_xout_mat[k*2+1]=0;
//         for (n=0, m=(k)*2 ; n<(PolyM/2+1) ; n++){
//             fft_xout_mat[m]=(int32_t)((TempFFT[n][0]));
//             fft_xout_mat[m+1]=(int32_t)((TempFFT[n][1]));
//             m+=PolyL*2;
//         }
//         // fft_xout_mat[m]=0;
//         // fft_xout_mat[m+1]=0;
   
		for (n = 0, i=0; n<PolyM*2; n+=2) {
			pffftin[i] = ((float)fft_xin[n])*deno;
			i++;
		}

        pffft_transform_ordered(p_pffft, pffftin, pffftout, pffftwork, PFFFT_FORWARD);

        fft_xout_mat[k*2]=(int32_t)(pffftout[0]);
        fft_xout_mat[k*2+1]=0;

        for (n=2, m=(k+PolyL)*2; n<PolyM ; n+=2){
            fft_xout_mat[m]=(int32_t)(pffftout[n]);
            fft_xout_mat[m+1]=(int32_t)(pffftout[n+1]);
            m+=PolyL*2;
        }
        fft_xout_mat[m]=(int32_t)(pffftout[1]);
        fft_xout_mat[m+1]=0;           
    }
    memcpy(&buf[0], &buf[polyblocksize], sizeof(short)*(PolyN-PolyR));
}

void poly_synthesis(void *polyInst, void *fft_in_mat, void *out, int outputN, int scale)
{
    int i, k, n, m;
    polyInst_t *inst = (polyInst_t *)polyInst;

    short       frame_xout[PolyN];

    short *xout = (short *)out;
    short *win = (short *)inst->p_outwin;
    short *buf = (short *)inst->p_outBuf[outputN];
    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    float deno=1.0/(float)(scale);
    // float deno=1.0/(float)(PolyM*scale);

    for (k=0 ; k<PolyL ; k++){
        memset(fft_xin, 0, sizeof(int32_t)*(PolyM*2));

        pffftout[0] = fft_xin_mat[k*2];

        for (n=2, m=(k+PolyL)*2 ; n<(PolyM) ; n+=2){
            pffftout[n]=fft_xin_mat[m];
            pffftout[n+1]=fft_xin_mat[m+1];
            m+=PolyL*2;
        }

        pffftout[1] = fft_xin_mat[m];

        pffft_transform_ordered(p_pffft, pffftout, pffftin, pffftwork, PFFFT_BACKWARD);

        for (n=0, m=0 ; n<PolyM ; n++){
            fft_xout[m]=((int32_t)(pffftin[n]*deno));
            m+=2;
        }

        for (n = 0, m = 0 ; n<PolyM ; n++) {
            frame_xout[n]=(short)((fft_xout[m]*(int)win[n]+16384)>>Q15);
            m+=2;
        }

        for (n = PolyM, m = 0 ; n<PolyN ; n++) {
            frame_xout[n]=(short)((fft_xout[m]*(int)win[n]+16384)>>Q15);
            m+=2;
        }

        for (n = k*PolyR, m=0 ; n<(PolyN-PolyR+k*PolyR) ; n++){
            buf[n]=buf[n]+frame_xout[m];
            m++;
        }

        for (n = k*PolyR+PolyN-PolyR, m=PolyN-PolyR ; n < k*PolyR+PolyN ; n++){
            buf[n]=frame_xout[m];
            m++;
        }
    }
    memcpy(xout, buf, sizeof(short)*polyblocksize);
    memcpy(&buf[0], &buf[polyblocksize], sizeof(short)*(PolyN-PolyR));
    memset(&buf[PolyN-PolyR], 0, sizeof(short)*(polyblocksize));
}
