/*
 * autogaincontrol.h
 *
 *  Created on: 2018. 9. 11.
 *      Author: Seong Pil Moon
 */

#ifndef AUTOGAINCONTROL_H_
#define AUTOGAINCONTROL_H_

typedef struct agcInst_s {
    short       blocksize;
    int         N; // polyphase filter length
    int         M; // filterbank channel number
    int         R; // decimation factor
    int         L; // blocksize/R : channel samples within a block
    int         bits;

    short beta_s_r_num;
    short beta_s_f_num;
    short beta_f_r_num;
    short beta_f_f_num;
    short beta_p_r_num;
    short beta_p_f_num;

    short beta_s_r;
    short beta_s_f;
    short beta_f_r;
    short beta_f_f;
    short beta_p_r;
    short beta_p_f;

	short round_bit_s_r;
	short round_bit_s_f;
    short round_bit_f_r;
	short round_bit_f_f;
    short round_bit_p_r;
	short round_bit_p_f;

	// float ginc;
	// float gdec;
	// float ginc_gate;
	// float gdec_gate;

	// float gain_red;

	float max_gain_dB;
	float min_gain_dB;
	float max_gain;
	float min_gain;

	// int Kn;
	float Kp;
	float Kp_dB;
	float target_SPL;
	// float gate_dB;

	short att_num;
	short rel_num;
	float tau_att;
	float tau_rel;
	float r_att; 
	float r_rel;

	short hold_cnt;

	short min_vcnt;

    short beta_f_r_band;
    short beta_f_f_band;
    short beta_p_r_band;
    short beta_p_f_band;

    short round_bit_f_r_band;
	short round_bit_f_f_band;
    short round_bit_p_r_band; 
	short round_bit_p_f_band; 

	float ginc_band;
	float gdec_band;

	float max_gain_band;
	float min_gain_band;

	float Kp_band;

	float globalMakeupGain_dB;
	float threshold_dBFS;	
	
} agcInst_t;

agcInst_t * sysAGCCreate();
void AGC_Init(agcInst_t *agcInst);
void AGC_DeInit();
void AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat);
void en_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat);
void de_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat);
// void AGC_total_no_vad(agcInst_t *agcInst, void *in, void *out, short vad, short vad_long);
void AGC_total_w_ref(agcInst_t *agcInst, void *in, void *ref, void *out, short vad, short vad_long);
void AGC_input_5ch(agcInst_t *agcInst, int leng, short *pvad, short *pvad_long, void *in1, void *in2, void *in3, void *in4, void *in5, void *out, void *out2, void *out3, void *out4, void *out5);
void AGC_input_5ch_2(agcInst_t *agcInst, int leng, void *in1, void *in2, void *in3, void *in4, void *in5, void *out, void *out2, void *out3, void *out4, void *out5, float globalMakeupGain_dB, float threshold_dBFS);






extern agcInst_t Sys_agcInst;
#endif /* AUTOGAINCONTROL_H_ */

