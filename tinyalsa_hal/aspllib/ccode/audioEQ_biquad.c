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

/** \file     audioEQ_biquad.c
 *
 *  \brief    Biquad filterer implementation
 *
 *  This file contains the implementation of biquad filters used 
 *  for audio equalization
 *
 *  References:
 *  http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *  http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/ 
 */
 
#include <math.h>
#include "audioEQ_biquad.h"

void Biquad_initParams(BIQUAD_T *filter, BIQUAD_T filter_sample) {
    filter->z1 = filter->z2 = 0.0;

    filter->gain = filter_sample.gain;
    filter->a0 = filter_sample.a0;
    filter->a1 = filter_sample.a1;
    filter->a2 = filter_sample.a2;
    filter->b1 = filter_sample.b1;
    filter->b2 = filter_sample.b2;
}

void Biquad_applyFilter(BIQUAD_T *filter, float *input, float *output, int BufferSize){
    int i;
    for (i = 0; i<BufferSize; i++){
        output[i] = Biquad_process(filter, input[i]);
    }
}

float Biquad_process(BIQUAD_T *filter, float in) {
    double in_gain = in * filter->gain;
    double out = in_gain + filter->z1;
    filter->z1 = in_gain * filter->a1 + filter->z2 - filter->b1 * out;
    filter->z2 = in_gain * filter->a2 - filter->b2 * out;
    return out;
}
