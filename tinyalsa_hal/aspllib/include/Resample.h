#pragma once
#include <stdio.h>


#define LP_PASS_TAPS  9
#define LP_PASS_TAPS3  15
#define LP_PASS_TAPS6  31

#define IN_BUFF_SIZE_RC  768*2
#define In_BUFF_SIZE  682


typedef struct UpdamplerContext
{
//    short* Int16InputBuffer;
//    short* Int16OutputBuffer;
    unsigned short  FilterOrder;
    unsigned short  CurrentBufferSz;
    unsigned short  MaxBufferSz;
    double FilterWindow[LP_PASS_TAPS3];
    double FilterCoef[LP_PASS_TAPS3];
}UpdamplerContext;


int CreateResampler(UpdamplerContext* pContext, unsigned short MaxBufferSz, unsigned short FilterTaps, double* pFIlter);
void DoUpsample(UpdamplerContext* pContext, short* pIn, short* pOut,unsigned short Sz,unsigned int Scale,double MultVal);
void DoDnsample(UpdamplerContext* pContext, short* pIn, short* pOut, unsigned short Sz,unsigned int Scale, double MultVal);
void DestroyResampler(UpdamplerContext* pContext);


extern double pLowPassFilter[LP_PASS_TAPS];
extern double pLowPassFilter3[LP_PASS_TAPS3];
extern double pLowPassFilter6[LP_PASS_TAPS6];




