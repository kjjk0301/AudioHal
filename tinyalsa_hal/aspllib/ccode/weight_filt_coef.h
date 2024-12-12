/*
 * weight_filt_coef.h
 *
 *  Created on: 2021. 2. 11.
 *      Author: mczz0
 */

#ifndef WEIGHT_FILT_COEF_H_
#define WEIGHT_FILT_COEF_H_

/* A weighting filter (fs=48kHz) */
float filt_A_a1[3] = {1, -0.22455845805977917,  0.0126066252715464};
float filt_A_b1[3] = {0.19701204180294182, 0.39402408360588365, 0.19701204180294182  };
float filt_A_gain1 = 1.0;

float filt_A_a2[3]={1, -1.8938704947230705,  0.89515976909466144};
float filt_A_b2[3]={1, -2, 1};
float filt_A_gain2 = 1.0;

float filt_A_a3[3]={ 1,-1.9946144559930217,  0.99462170701408448 };
float filt_A_b3[3]={1, -2, 1};
float filt_A_gain3 = 1.0;

float filt_A_output_gain = 1.1892765038893924/2;


/* C weighting filter (fs=48kHz) */
float filt_C_a1[3] = {1, -0.22455845805977917,   0.0126066252715464};
float filt_C_b1[3] = {0.19701204180294182, 0.39402408360588365, 0.19701204180294182};
float filt_C_gain1 = 1.0;

float filt_C_a2[3]={1, -1.9946144559930217,   0.99462170701408448};
float filt_C_b2[3]={1, -2, 1};
float filt_C_gain2 = 1.0;

float filt_C_a3[3]={1, 0, 0};
float filt_C_b3[3]={1, 0, 0};
float filt_C_gain3 = 1.0;

float filt_C_output_gain = 1.0044417499328639/2;

#endif /* WEIGHT_FILT_COEF_H_ */
