
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "audio_utils.h"
#include "audioEQ_biquad.h"
#include "weight_filt_coef.h"

int DC_rejection(int * offset_Q15_p, int xin, int beta)
{
    int roundbit=0x01<<(beta-1);
    int temp=(*offset_Q15_p+roundbit)>>beta;

    *offset_Q15_p-=temp;
    int xinQ15=xin<<15;
    temp=(xinQ15+roundbit)>>beta;

    *offset_Q15_p+=temp;
    temp=xinQ15-*offset_Q15_p;
    temp=(temp+16384)>>15;
    return temp;
}

typedef void            *Ptr;

BIQUAD_T filter_sample[2][3];
BIQUAD_T filter[2][3];
BIQUAD_T *p_filter[2][3];

float output_gain[2];

static Ptr scratch[2][3] = {NULL};

void audioEQ_allocate_bufs(uint32_t bufsize) {
    int count, count2;

    /* A weighting filter (fs=48kHz) */
    filter_sample[0][0].gain = filt_A_gain1;
    filter_sample[0][0].a0 = filt_A_b1[0];
    filter_sample[0][0].a1 = filt_A_b1[1];
    filter_sample[0][0].a2 = filt_A_b1[2];
    filter_sample[0][0].b1 = filt_A_a1[1];
    filter_sample[0][0].b2 = filt_A_a1[2];

    filter_sample[0][1].gain = filt_A_gain2;
    filter_sample[0][1].a0 = filt_A_b2[0];
    filter_sample[0][1].a1 = filt_A_b2[1];
    filter_sample[0][1].a2 = filt_A_b2[2];
    filter_sample[0][1].b1 = filt_A_a2[1];
    filter_sample[0][1].b2 = filt_A_a2[2];

    filter_sample[0][2].gain = filt_A_gain3;
    filter_sample[0][2].a0 = filt_A_b3[0];
    filter_sample[0][2].a1 = filt_A_b3[1];
    filter_sample[0][2].a2 = filt_A_b3[2];
    filter_sample[0][2].b1 = filt_A_a3[1];
    filter_sample[0][2].b2 = filt_A_a3[2];

    output_gain[0] = filt_A_output_gain;

    /* A weighting filter (fs=48kHz) */
    filter_sample[1][0].gain = filt_A_gain1;
    filter_sample[1][0].a0 = filt_A_b1[0];
    filter_sample[1][0].a1 = filt_A_b1[1];
    filter_sample[1][0].a2 = filt_A_b1[2];
    filter_sample[1][0].b1 = filt_A_a1[1];
    filter_sample[1][0].b2 = filt_A_a1[2];

    filter_sample[1][1].gain = filt_A_gain2;
    filter_sample[1][1].a0 = filt_A_b2[0];
    filter_sample[1][1].a1 = filt_A_b2[1];
    filter_sample[1][1].a2 = filt_A_b2[2];
    filter_sample[1][1].b1 = filt_A_a2[1];
    filter_sample[1][1].b2 = filt_A_a2[2];

    filter_sample[1][2].gain = filt_A_gain3;
    filter_sample[1][2].a0 = filt_A_b3[0];
    filter_sample[1][2].a1 = filt_A_b3[1];
    filter_sample[1][2].a2 = filt_A_b3[2];
    filter_sample[1][2].b1 = filt_A_a3[1];
    filter_sample[1][2].b2 = filt_A_a3[2];

    output_gain[1] = filt_A_output_gain;

    for(count2 = 0; count2 < 2; count2 ++)
    {
        for (count=0; count<3; count++){
            p_filter[count2][count]=&filter[count2][count];
            Biquad_initParams(p_filter[count2][count], filter_sample[count2][count]);
        }
    }

    /* Allocate buffers */
    for(count2 = 0; count2 < 2; count2 ++)
    {
        for(count = 0; count < 3; count ++)
        {
           scratch[count2][count] = malloc(bufsize);
           if(0 == scratch[count2][count])
           {
               printf("buffer malloc failed.\n");
           }
           else
               memset((uint8_t *)scratch[count2][count], 0xFF, bufsize);
        }
    }
}

void audioEQ_free_bufs(){
    int count, count2;
    for(count2 = 0; count2 < 2; count2 ++)
    {
        for(count = 0; count < 3; count ++)
        {
            if (scratch[count2][count] != NULL) {
                free(scratch[count2][count]);
                scratch[count2][count] = NULL;
            }
        }
    }
}

void audioEQ_process_samples_float(void *tx_buf, void *rx_buf, uint32_t filt_len, uint32_t filt_num, uint32_t chan)
{
    int i;
    float *output=tx_buf;

    if (filt_num == 3){
        Biquad_applyFilter(p_filter[chan][0], (float *)rx_buf,          (float *)scratch[chan][0], filt_len);
        Biquad_applyFilter(p_filter[chan][1], (float *)scratch[chan][0],(float *)scratch[chan][1], filt_len);
        Biquad_applyFilter(p_filter[chan][2], (float *)scratch[chan][1],(float *)tx_buf,           filt_len);
    } else if (filt_num == 2){
        Biquad_applyFilter(p_filter[chan][0], (float *)rx_buf,          (float *)scratch[chan][0], filt_len);
        Biquad_applyFilter(p_filter[chan][1], (float *)scratch[chan][0],(float *)tx_buf,           filt_len);
    }

    for (i = 0; i<filt_len; i++){
        output[i] *= output_gain[chan];
    }
}

#ifdef CALC_SPL
void squarer_float(void *tx_buf, void *rx_buf, uint32_t filt_len)
{
    int i;
    float *input=rx_buf;
    float *output=tx_buf;
    for (i = 0; i<filt_len; i++){
        output[i] = input[i]*input[i];
    }
}

// float LAF;
// float LAF_dB;
// LAF_dB = calc_timeweight(&LAF, (void *)&scratch_A[0], Proc_buf_frme_length, beta_fast, p0_dB);

float calc_timeweight(float *L, void *rx_buf, uint32_t input_len, float beta, float P0_dB)
{
    int i;
    float LdB;
    float *input=rx_buf;

    for (i = 0; i<input_len; i++){
        *L = *L*beta + input[i]*(1-beta);
    }

    LdB = calc_SPL_dB(*L, P0_dB);

    return LdB;
}


// LAeq_dB = calc_Leq(&LAeq, &Leq_calc_buf[0][0], (void *)&scratch_A[0], LEQTIMESAMPLES, Proc_buf_frme_length, p0_dB);
// float Leq_calc_buf[4][LEQTIMESAMPLES+Proc_buf_frme_length];
// float LAeq;
// float LAeq_dB;

float calc_Leq(float *Leq, void *calc_buf, void *rx_buf, uint32_t buf_len, uint32_t input_len, float P0_dB)
{
    int i, j;
    float temp, LeqdB;
    float *input=rx_buf;
    float *buf=calc_buf;

    for (i = 0, j=input_len; i<(buf_len-input_len); i++, j++){
        buf[i]=buf[j];
    }
    memcpy(&buf[buf_len-input_len], &input[0], sizeof(float)*input_len);

    temp=0;
    for (i = 0; i<buf_len; i++){
        temp+=buf[i];
    }

    *Leq = temp/((float)buf_len);
    LeqdB = calc_SPL_dB(*Leq, P0_dB);

    return LeqdB;
}

float calc_SPL_dB(float L, float P0_dB){
    float LdB = 10*log10(L) + P0_dB;
    return LdB;
}

#endif