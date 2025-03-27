/*
 * polyphase.c
 *
 *  Created on: 2018. 8. 24.
 *      Author: sbroad01
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sys.h"
#include "polyphase.h"
#include "polyphase_win.h"
#include <stdint.h>
#include <math.h>

#include "pffft.h"
#define MAX_OF(x, y) ((x) > (y) ? (x) : (y))

#include "../include/aspl_nr.h"

// Global variables <- need to be included in structure
/*
#pragma DATA_ALIGN(fft_xin, 8)
int32_t fft_xin[PolyM*2]; // malloc
#pragma DATA_ALIGN(fft_xout, 8)
int32_t fft_xout[PolyM*2]; // malloc

#pragma DATA_ALIGN(inbuf1, 8);
short inbuf1 [PolyinputN][polyblocksize+PolyN-PolyR]; // need malloc
#pragma DATA_ALIGN(outbuf1, 8);
short outbuf1 [PolyOutputN][polyblocksize+PolyN-PolyR]; // need malloc

PFFFT_Setup *p_pffft = NULL;
float *pffftin = NULL;
float *pffftout = NULL;
float *pffftwork = NULL;
*/
// Global variables <- need to be included in structure

void free_pffts(PFFFT_Setup **s, float **X, float **Y, float **Z);

polyInst_t *sysPolyCreate()
{
    polyInst_t *polyInst_p = malloc(sizeof(polyInst_t));
    Poly_Init(polyInst_p);
    printf("new inst = %p \r\n", polyInst_p);

    return polyInst_p;
}

void Poly_DeInit(void **polyInst)
{
    polyInst_t *inst = (polyInst_t *)*polyInst;

    if(inst) {
        if (inst->fft_xin != NULL){
            free(inst->fft_xin);
            inst->fft_xin = NULL;
        }
            
        if (inst->fft_xout != NULL){
            free(inst->fft_xout);
            inst->fft_xout = NULL;
        }
            
        for (int i = 0; i < PolyinputN; i++)
        {
            if (inst->inbuf1[i] != NULL){
                free(inst->inbuf1[i]);
                inst->inbuf1[i] = NULL;
            }       
        }

        for (int i = 0; i < PolyOutputN; i++)
        {
            if (inst->outbuf1[i] != NULL){
                free(inst->outbuf1[i]);
                inst->outbuf1[i] = NULL;
            }
        }

        free_pffts(&(inst->p_pffft), &(inst->pffftin), &(inst->pffftout), &(inst->pffftwork));

        free(inst);
        *polyInst = NULL;

    }
}

void free_pffts(PFFFT_Setup **s, float **X, float **Y, float **Z)
{
    if (*s != NULL)
    {
        pffft_destroy_setup(*s);
        *s = NULL;
    }
    if (*X != NULL)
    {
        pffft_aligned_free(*X);
        *X = NULL;
    }
    if (*Y != NULL)
    {
        pffft_aligned_free(*Y);
        *Y = NULL;
    }
    if (*Z != NULL)
    {
        pffft_aligned_free(*Z);
        *Z = NULL;
    }
}

#ifndef PFFFT_SIMD_DISABLE
void validate_pffft_simd(); // a small function inside pffft.c that will detect compiler bugs with respect to simd instruction
#endif

int Poly_Init(void *polyInst)
{
    int n;
    polyInst_t *inst = (polyInst_t *)polyInst;
    inst->fft_xin = (int32_t *)malloc(PolyM * 2 * sizeof(int32_t)); // by skj
    if (inst->fft_xin == NULL)
    {
        perror("Poly_init malloc fail: fft_xin");
        return -1;
    }

    inst->fft_xout = (int32_t *)malloc(PolyM * 2 * sizeof(int32_t));
    if (inst->fft_xout == NULL)
    {
        perror("Poly_init malloc fail: fft_xout");
        free(inst->fft_xin);
        return -1;
    }

    for (int i = 0; i < PolyinputN; i++)
    {
        inst->inbuf1[i] = (short *)malloc((polyblocksize + PolyN - PolyR) * sizeof(short));
        if (inst->inbuf1[i] == NULL)
        {
            perror("Poly_init malloc fail: inbuf1 col");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(inst->inbuf1[j]);
            }
            free(inst->fft_xin);
            free(inst->fft_xout);
            return -1;
        }
    }

    for (int i = 0; i < PolyOutputN; i++)
    {
        inst->outbuf1[i] = (short *)malloc((polyblocksize + PolyN - PolyR) * sizeof(short));
        if (inst->outbuf1[i] == NULL)
        {
            perror("Poly_init malloc fail: outbuf1 col");
            // 이미 할당된 메모리 해제
            for (int j = 0; j < i; j++)
            {
                free(inst->outbuf1[j]);
            }
            for (int j = 0; j < PolyinputN; j++)
            {
                free(inst->inbuf1[j]);
            }
            free(inst->fft_xin);
            free(inst->fft_xout);
            return -1;
        }
    }

    inst->blocksize = polyblocksize;
    inst->N = PolyN; // polyphase filter length
    inst->M = PolyM; // filterbank channel number
    inst->R = PolyR; // decimation factor
    inst->L = PolyL; // blocksize/R : channel samples within a block
                     //	    inst->bits=Q15;

    for (n = 0; n < PolyinputN; n++)
    {
        inst->p_inBuf[n] = &(inst->inbuf1[n][0]); // polyphase analysis buffer,
    }
    for (n = 0; n < PolyOutputN; n++)
    {
        inst->p_outBuf[n] = &(inst->outbuf1[n][0]); // polyphase analysis buffer
    }
    inst->p_inwin = win_in;   // polyphase analysis filter pointer
    inst->p_outwin = win_out; // polyphase synthesis filter pointer

    for (n = 0; n < PolyinputN; n++)
    {
        memset(&(inst->inbuf1[n][0]), 0, sizeof(short) * (polyblocksize + PolyN - PolyR));
    }
    for (n = 0; n < PolyOutputN; n++)
    {
        memset(&(inst->outbuf1[n][0]), 0, sizeof(short) * (polyblocksize + PolyN - PolyR));
    }

    int cplx = 0;
    int N = PolyM;
    int Nfloat = (cplx ? N * 2 : N);
    int Nbytes = Nfloat * sizeof(float);
    inst->pffftin = pffft_aligned_malloc(Nbytes);
    inst->pffftout = pffft_aligned_malloc(Nbytes);
    inst->pffftwork = pffft_aligned_malloc(Nbytes);

    // PFFFT benchmark
    inst->p_pffft = pffft_new_setup(N, cplx ? PFFFT_COMPLEX : PFFFT_REAL);

    // #ifndef PFFFT_SIMD_DISABLE
    //     validate_pffft_simd();
    // #endif

    return 0;
}

void poly_analysis(void *polyInst, void *in, void *fft_out_mat, int inputN, int scale)
{
    int i, k, n, m;
    polyInst_t *inst = (polyInst_t *)polyInst;

    int32_t frame_xin[PolyN];

    short *xin = (short *)in;
    short *win = (short *)inst->p_inwin;
    short *buf = (short *)inst->p_inBuf[inputN];
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
    float deno = 2.0 / (float)((PolyM * scale));
    // float deno=1.0/(float)((scale));

    memcpy(&buf[PolyN - PolyR], xin, sizeof(short) * polyblocksize);

    for (k = 0; k < PolyL; k++)
    {
        for (n = k * PolyR, m = 0; n < (PolyN + k * PolyR); n++)
        {
            // frame_xin[m]=(short)(((int)buf[n]*(int)win[m]+16384)>>Q15);
            frame_xin[m] = ((int)buf[n] * (int)win[m]);
            frame_xin[m] = (frame_xin[m] + 1) >> 1;
            m++;
        }

        memset(inst->fft_xin, 0, sizeof(int32_t) * (PolyM * 2));

        for (n = 0, m = 0; n < (PolyM * 2); n += 2)
        {
            inst->fft_xin[n] = frame_xin[m];
            m++;
        }
        for (n = 0, m = (PolyN >> 1); n < (PolyM * 2); n += 2)
        {
            inst->fft_xin[n] = inst->fft_xin[n] + frame_xin[m];
            m++;
        }

        for (n = 0, i = 0; n < PolyM * 2; n += 2)
        {
            inst->pffftin[i] = ((float)(inst->fft_xin[n])) * deno;
            i++;
        }

        pffft_transform_ordered(inst->p_pffft, inst->pffftin, inst->pffftout, inst->pffftwork, PFFFT_FORWARD);

        fft_xout_mat[k * 2] = (int32_t)(inst->pffftout[0]);
        fft_xout_mat[k * 2 + 1] = 0;

        for (n = 2, m = (k + PolyL) * 2; n < PolyM; n += 2)
        {
            fft_xout_mat[m] = (int32_t)(inst->pffftout[n]);
            fft_xout_mat[m + 1] = (int32_t)(inst->pffftout[n + 1]);
            m += PolyL * 2;
        }
        fft_xout_mat[m] = (int32_t)(inst->pffftout[1]);
        fft_xout_mat[m + 1] = 0;
    }
    memcpy(&buf[0], &buf[polyblocksize], sizeof(short) * (PolyN - PolyR));
}

void poly_synthesis(void *polyInst, void *fft_in_mat, void *out, int outputN, int scale)
{
    int i, k, n, m;
    polyInst_t *inst = (polyInst_t *)polyInst;

    short frame_xout[PolyN];

    short *xout = (short *)out;
    short *win = (short *)inst->p_outwin;
    short *buf = (short *)inst->p_outBuf[outputN];
    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    float deno = 1.0 / (float)(scale);
    // float deno=1.0/(float)(PolyM*scale);

    for (k = 0; k < PolyL; k++)
    {
        memset(inst->fft_xin, 0, sizeof(int32_t) * (PolyM * 2));

        inst->pffftout[0] = fft_xin_mat[k * 2];

        for (n = 2, m = (k + PolyL) * 2; n < (PolyM); n += 2)
        {
            inst->pffftout[n] = fft_xin_mat[m];
            inst->pffftout[n + 1] = fft_xin_mat[m + 1];
            m += PolyL * 2;
        }

        inst->pffftout[1] = fft_xin_mat[m];

        pffft_transform_ordered(inst->p_pffft, inst->pffftout, inst->pffftin, inst->pffftwork, PFFFT_BACKWARD);

        for (n = 0, m = 0; n < PolyM; n++)
        {
            inst->fft_xout[m] = ((int32_t)(inst->pffftin[n] * deno));
            m += 2;
        }

        for (n = 0, m = 0; n < PolyM; n++)
        {
            frame_xout[n] = (short)((inst->fft_xout[m] * (int)win[n] + 16384) >> Q15);
            m += 2;
        }

        for (n = PolyM, m = 0; n < PolyN; n++)
        {
            frame_xout[n] = (short)((inst->fft_xout[m] * (int)win[n] + 16384) >> Q15);
            m += 2;
        }

        for (n = k * PolyR, m = 0; n < (PolyN - PolyR + k * PolyR); n++)
        {
            buf[n] = buf[n] + frame_xout[m];
            m++;
        }

        for (n = k * PolyR + PolyN - PolyR, m = PolyN - PolyR; n < k * PolyR + PolyN; n++)
        {
            buf[n] = frame_xout[m];
            m++;
        }
    }
    memcpy(xout, buf, sizeof(short) * polyblocksize);
    memcpy(&buf[0], &buf[polyblocksize], sizeof(short) * (PolyN - PolyR));
    memset(&buf[PolyN - PolyR], 0, sizeof(short) * (polyblocksize));
}
