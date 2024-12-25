/*
 * noise_supression.h
 *
 *  Created on: 2018. 8. 28.
 *      Author: Seong Pil Moon
 */

#ifndef NOISE_SUPRESSION_H_
#define NOISE_SUPRESSION_H_

typedef struct nsInst_s {
    short       blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block
    int         bits;

    short       voice_start_bin;
    short       voice_end_bin;

    short Low_bypass;
    short Mid_bypass;
    short Hi_bypass;

    short Low_solo;
    short Mid_solo;
    short Hi_solo;

    short EQ_Low;
    short EQ_Mid;
    short EQ_Hi;

    int Ma_size;

    // short       beta_e1;
    // short       beta_e2;
    // short       beta_r1;
    // short       beta_r2;
    // short       beta_f;
    // short       beta_freq;
    // short       beta_freq_high;    

    // short       round_bit_e1;
    // short       round_bit_e2;
    // short       round_bit_r1;
    // short       round_bit_r2;
    // short       round_bit_f;
    // short       round_bit_freq;
    // short       round_bit_freq_high;

    short       beta_e_num[6];
    short       beta_e[6];
    short       round_bit_e[6];
    unsigned char gamma_inv_e[6];

    // int Hmin_num[3];
    // int Hmin[PolyM];

    short       beta_r_num[9];
    short       beta_r[9];
    short       round_bit_r[9];
    unsigned char gamma_inv_r[9];    

    int32_t *        See_p;
    int32_t *        Sbb_p;

    int32_t *        Hpre2_Q15_p;
    int32_t *        H_p;
	int32_t *		HminQ15_p;
	int32_t			betaQ15;

    float			beta_low_ratio;
    float			beta_high_ratio;


    float max_att;
    float min_att;
    float high_att;
    float slope;

} nsInst_t;



nsInst_t* sysNSCreate();
void NS_Init(void *nsInst);
void NS_DeInit();
void NS_process(void *nsInst, void *fft_in_mat, void *fft_buf_mat, void *fft_out_mat, int vad);
void NS_oct_process(void *nsInst, void *fft_in_mat, void *fft_buf_mat, void *fft_out_mat, int vad);



extern nsInst_t Sys_nsInst;
#endif /* NOISE_SUPRESSION_H_ */
