/*
 * audioEQ_biquad.c
 *
 * This file contains the Biquad implementation used for audio equalization
 *
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
/** \file     audioEQ_biquad.h
 *
 *  \brief    Biquad filter implementation function prototypes
 *
 *  This file contains the function prototypes and macros for the
 *  biquad filter implementation used for audio equalization.
 *
 *  References:
 *  http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *  http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/ 
 */
#ifndef Biquad_h
#define Biquad_h

#ifdef __cplusplus
extern "C" {
#endif

enum {
    bq_type_lowpass = 0,
    bq_type_highpass,
    bq_type_bandpass,
    bq_type_notch,
    bq_type_peak,
    bq_type_lowshelf,
    bq_type_highshelf
};

typedef struct Biquad{    
	int type;
    double gain;
    double a0;
    double a1;
    double a2;
    double b1;
    double b2;
    double Fc;
    double Q;
    double peakGain;
    double z1;
    double z2;
    double K;
} BIQUAD_T;

void Biquad_initParams(BIQUAD_T *filter, BIQUAD_T filter_sample);
void Biquad_applyFilter(BIQUAD_T *filter, float *input, float *output, int BufferSize);
float Biquad_process(BIQUAD_T *filter, float in);

#ifdef __cplusplus
}
#endif

#endif // Biquad_h
