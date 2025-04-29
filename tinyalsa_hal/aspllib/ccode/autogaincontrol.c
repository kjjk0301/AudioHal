/*
 * autogaincontrol.c
 *
 *	Created on: 2018. 9. 11.
 *		Author: Seong Pil Moon
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

	
#include "sys.h"
#include "autogaincontrol.h"
#include "time_constant.h"


#include "debug_file.h"
		
// Global variables <- need to be included in structure
/*
float gain = 1.0;
float inputgain = 20.0;
float gate = 1.0;

int cnt = 0;
int g_cnt = 0;
int x_fast=9000000;
int xref_fast = 9000000;
int x_slow=9000000;
int xref_slow=9000000;
int x_peak=9000000;
int xout_fast=9000000;

float gain_band[PolyM];
float gain_band_hist[PolyL][PolyM];
int x_fast_band[PolyM];
int x_slow_band[PolyM];
int x_peak_band[PolyM];
float gc=0;
int idx=0;
int v_cnt = 0;
*/
// Global variables <- need to be included in structure

agcInst_t * sysAGCCreate()
{
	agcInst_t * agcInst_p = malloc(sizeof(agcInst_t));
	AGC_Init(agcInst_p);
	return agcInst_p;
}

void AGC_DeInit(void **agcInst){

    agcInst_t *inst = (agcInst_t *)*agcInst;

	if(inst)
	{
		for (int i = 0; i < PolyL; i++)
		{
			if (inst->gain_band_hist[i] != NULL) {
				free(inst->gain_band_hist[i]);
				inst->gain_band_hist[i] = NULL;
			}
		}
		free(inst);
		*agcInst = NULL;
	}

	

// #ifdef DEBUG_AGC_MATLAB
// 	debug_matlab_close();
// #endif	
}

void AGC_Init(agcInst_t *agcInst){

	int i;

	//gc=0;

	agcInst_t *inst = (agcInst_t *)agcInst;

///////////////////////////////////////////////////////////////// var init . by skj
	inst->gain = 1.0;
	inst->inputgain = 20.0;
	inst->gate = 1.0;

	inst->cnt = 0;
	inst->g_cnt = 0;
	inst->x_fast=9000000;
	inst->xref_fast = 9000000;
	inst->x_slow=9000000;
	inst->xref_slow=9000000;
	inst->x_peak=9000000;
	inst->xout_fast=9000000;
	inst->gc=0;
	inst->idx=0;
	inst->v_cnt = 0;
	inst->n_cnt = 0;

	for (int i = 0; i < PolyL; i++)
	{
		inst->gain_band_hist[i] = (float *)malloc((PolyM) * sizeof(float));
		if (inst->gain_band_hist[i] == NULL)
		{
			perror("agc init malloc fail: gain_band_hist col");
			// return;
			// 이미 할당된 메모리 해제
			for (int j = 0; j < i; j++)
			{
				free(inst->gain_band_hist[j]);
				inst->gain_band_hist[j]=NULL;
			}
			return;
		}
	}
///////////////////////////////////////////////////////////////// var init
	inst->blocksize=polyblocksize;
	inst->N=PolyN; // polyphase filter length
	inst->M=PolyM; // filterbank channel number
	inst->R=PolyR; // decimation factor
	inst->L=PolyL; // blocksize/R : channel samples within a block

	inst->att_num=9;
	inst->rel_num=12;
	inst->tau_att=tau16k_90_msec[inst->att_num];
	inst->tau_rel=tau16k_90_msec[inst->rel_num];
	inst->r_att=gamma16k[inst->att_num];
	inst->r_rel=gamma16k[inst->rel_num];

    // inst->beta_s_r_num=13;
    // inst->beta_s_f_num=7;
    inst->beta_f_r_num=7;
    inst->beta_f_f_num=9;
    inst->beta_p_r_num=10;
    inst->beta_p_f_num=12;

    inst->beta_f_r=beta16k[inst->beta_f_r_num];
    inst->beta_f_f=beta16k[inst->beta_f_f_num];
    inst->beta_p_r=beta16k[inst->beta_p_r_num];
    inst->beta_p_f=beta16k[inst->beta_p_f_num];

    inst->round_bit_f_r=round_bit16k[inst->beta_f_r_num]; //128; //(short)1<<((inst->beta_f_r)-1);
	inst->round_bit_f_f=round_bit16k[inst->beta_f_f_num]; //1024; //(short)1<<((inst->beta_f_f)-1);
    inst->round_bit_p_r=round_bit16k[inst->beta_p_r_num]; //512; //(short)1<<((inst->beta_p_r)-1);
	inst->round_bit_p_f=round_bit16k[inst->beta_p_f_num]; //4096; //(short)1<<((inst->beta_f_f)-1);

	inst->max_gain_dB=15;
	inst->min_gain_dB=-10;

	inst->max_gain= powf (10, inst->max_gain_dB*0.05); //10; //1.995262314968880;
	inst->min_gain= powf (10, inst->min_gain_dB*0.05); //0.01; //0.501187233627272;

	// float thr_agc_vad = 60; // dB SPL
	inst->target_SPL = 80; // dB SPL
	inst->Kp_dB = inst->target_SPL - FS_SPL + Q15_dB*2;
	inst->Kp=powf(10.0,(inst->Kp_dB*0.05)); //3.395366161309280e+07; //131072000; //49152000; //3000; //49152000;

	inst->hold_cnt=16000; //500;

	inst->min_vcnt = 2000;

	inst->nominal_dB = 0.0;

    inst->beta_f_r_band=9;
    inst->beta_f_f_band=5;
    inst->beta_p_r_band=3;
    inst->beta_p_f_band=7;

    inst->round_bit_f_r_band=2;
	inst->round_bit_f_f_band=16;
    inst->round_bit_p_r_band=4; 
	inst->round_bit_p_f_band=64; 

	float Bdelta=2;
	float eps2_band=powf(10, (Bdelta/20.0/(SAMPLING_FREQ/PolyR)))-1;

	inst->ginc_band=1+eps2_band;
	inst->gdec_band=1.0/(inst->ginc_band);

	inst->max_gain_band=powf(10.0, (6.0/20.0));
	inst->min_gain_band=powf(10.0, (-6.0/20.0)); //0.01;

    float thr_agc_target = 110.0;
	inst->Kp_band = Q15_val*powf(10.0,((thr_agc_target-110)/20))*Q15_val;


	for (i=0; i<PolyM; i++){
		inst->gain_band[i] = 1.0;
		inst->x_fast_band[i] = 0;
		inst->x_slow_band[i] = 0;
		inst->x_peak_band[i] = 0;
	}

	inst->globalMakeupGain_dB = 5.0;
	inst->threshold_dBFS = -8.0;

// #ifdef DEBUG_AGC_MATLAB
// 	debug_matlab_open();
// #endif	
}

void AGC_total_w_ref(agcInst_t *agcInst, void *in, void *ref, void *out, short vad, short vad_long) {
	
	int m;
	float gmod, temp_gain;

	agcInst_t *inst = (agcInst_t *)agcInst;

	float xdB = inst->Kp_dB;
	float xslow_dB, xref_slowdB;

	short *xin = (short *)in;
	short *xref = (short *)ref;
	short *xout = (short *)out;

	short vad_short = vad && 0x0001;

	int abs_x_Q15, abs_xref_Q15, abs_xout_Q15, beta_s, roundbit_s, beta_f, roundbit_f,  beta_p, roundbit_p;
	
	float gs= 20.0*log10f((float)inst->gain);

	float r_a = inst->r_att;
	float r_r = inst->r_rel;
		
    for (m = 0 ; m<polyblocksize ; m++) {

		if (m>=(polyblocksize>>1)) vad_short = (vad && 0x0010)>>1;

		abs_x_Q15=((int)(abs(xin[m])))<<15;

    	if(abs_x_Q15>inst->x_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	inst->x_fast=inst->x_fast-((inst->x_fast+roundbit_f)>>beta_f);
		inst->x_fast=inst->x_fast+((abs_x_Q15+roundbit_f)>>beta_f);

		abs_xref_Q15=((int)(abs(xref[m])))<<15;	

#ifdef DEBUG_AGC_MATLAB
    	if(abs_xref_Q15>inst->xref_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	inst->xref_fast=inst->xref_fast-((inst->xref_fast+roundbit_f)>>beta_f);
		inst->xref_fast=inst->xref_fast+((abs_xref_Q15+roundbit_f)>>beta_f);		
#endif

		if(vad_short>0) {

			inst->v_cnt++;
			if (inst->v_cnt>5001) inst->v_cnt=5000;

			if (inst->v_cnt > (inst->min_vcnt)) inst->n_cnt = 0;

			if(inst->x_fast > inst->x_peak) {			 
				beta_p=inst->beta_p_r;
				roundbit_p=inst->round_bit_p_r;
			}
			else {
				beta_p=inst->beta_p_f;
				roundbit_p=inst->round_bit_p_f;
			} 		   
			inst->x_peak=inst->x_peak - ((inst->x_peak+roundbit_p)>>beta_p);
			inst->x_peak=inst->x_peak + ((inst->x_fast+roundbit_p)>>beta_p); 
		
			xdB = 20.0*log10f((float)inst->x_peak);

			// if (((inst->v_cnt%500)==0)&&(inst->v_cnt>3000)) printf("v_cnt=%d\n", inst->v_cnt);
			if ((xdB>(inst->Kp_dB-40))&&(inst->v_cnt > (inst->min_vcnt))){				
				inst->gc = inst->Kp_dB - xdB; // + gatedB;
			}

			

			inst->gc=MIN(inst->gc, inst->max_gain_dB);
			inst->gc=MAX(inst->gc, inst->min_gain_dB);

			if (gs < inst->gc){
				gs = r_a * gs + (1.0 - r_a) * inst->gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * inst->gc;
			}

			if (inst->v_cnt==5000) inst->nominal_dB = inst->nominal_dB * 0.99 + gs * 0.01;

			inst->gain = powf (10, gs*0.05);
			
		}
		else {

			if(vad_long==0){
				inst->v_cnt=0;
			}

			if (inst->v_cnt==0) {
				inst->n_cnt++;
				if (inst->n_cnt>5001) inst->n_cnt=5000;
			}

			xdB = 20.0*log10f((float)inst->x_peak);


			// if (((inst->v_cnt%500)==0)&&(inst->v_cnt>3000)) printf("v_cnt=%d\n", inst->v_cnt);

			if ((xdB>(inst->Kp_dB-40))&&(inst->v_cnt>5000)){
				inst->gc = inst->Kp_dB - xdB; // + gatedB;
			}

			if (inst->n_cnt>5000) {
				inst->gc = inst->nominal_dB;
			}

			inst->gc=MIN(inst->gc, inst->max_gain_dB);
			inst->gc=MAX(inst->gc, inst->min_gain_dB);

			if (gs < inst->gc){
				gs = r_a * gs + (1.0 - r_a) * inst->gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * inst->gc;
			}

			inst->gain = powf (10, gs*0.05);

		}

 		temp_gain=inst->gain; //*gate;
		xout[m]=(short)((float)xin[m]*temp_gain);

		abs_xout_Q15=((int)(abs(xout[m])))<<15;

    	if(abs_xout_Q15 > inst->xout_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	inst->xout_fast=inst->xout_fast-((inst->xout_fast+roundbit_f)>>beta_f);
		inst->xout_fast=inst->xout_fast+((abs_xout_Q15+roundbit_f)>>beta_f);

#ifdef DEBUG_AGC_MATLAB
	if ((g_agc_debug_on==1)&&(g_agc_debug_snd_idx==0)){    		
			inst->idx--;
			if (inst->idx<=0){
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_fast, 0);
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_peak, 1);
				
				debug_matlab_float(DEBUG_NUM_AGC, inst->Kp_dB, 2);
				// debug_matlab_float(DEBUG_NUM_AGC, inst->gc, 3);
				debug_matlab_float(DEBUG_NUM_AGC, inst->nominal_dB, 3);
				
				debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

				debug_matlab_int(DEBUG_NUM_AGC, inst->v_cnt, 5);
				debug_matlab_int(DEBUG_NUM_AGC, inst->n_cnt, 6);

				// debug_matlab_int(DEBUG_NUM_AGC, inst->x_slow, 7);
				debug_matlab_int(DEBUG_NUM_AGC, inst->xout_fast, 7);
				
				debug_matlab_int(DEBUG_NUM_AGC, inst->xref_fast, 8);
				// debug_matlab_int(DEBUG_NUM_AGC, inst->xref_slow, 9);

				debug_matlab_send(DEBUG_NUM_AGC);
				inst->idx=32;
			}
	}

	if (g_agc_debug_on==1){
		g_agc_debug_snd_idx++;
		if (g_agc_debug_snd_idx>=g_agc_debug_snd_period) {
			g_agc_debug_snd_idx = 0;
		}
	}

#endif

    }

		// inst->idx--;
		// if (inst->idx<=0){
		// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%f, gs=%f gain=%f cnt=%d vad=%d,\n", inst->x_peak, inst->Kp_dB, xdB, inst->gc, gs, inst->gain, inst->cnt, vad);
		// 	inst->idx=20;
		// }
}

void AGC_input_5ch(agcInst_t *agcInst, int leng, short *pvad, short *pvad_long, void *in1, void *in2, void *in3, void *in4, void *in5, void *out, void *out2, void *out3, void *out4, void *out5) {
	
	int m;
	float gmod, temp_gain;

	agcInst_t *inst = (agcInst_t *)agcInst;

	float xdB, xslow_dB, xref_slowdB;
	int abs_x_Q15, abs_xref_Q15, abs_xout_Q15, beta_s, roundbit_s, beta_f, roundbit_f,  beta_p, roundbit_p;

	short *xin = (short *)in1;
	short *xin2 = (short *)in2;
	short *xin3 = (short *)in3;
	short *xin4 = (short *)in4;
	short *xin5 = (short *)in5;
	short *xout = (short *)out;
	short *xout2 = (short *)out2;
	short *xout3 = (short *)out3;
	short *xout4 = (short *)out4;
	short *xout5 = (short *)out5;

	float gs= 20.0*log10f((float)inst->inputgain);
	float r_a = gamma16k[1];
	float r_r = gamma16k[11];	

	short vad_short, vad_long;

	float threshold = (float)Q15_dB + (float)Q15_dB - 26.0f;

	inst->x_fast = 0;
	for (m = 0 ; m<leng ; m++) {
		abs_x_Q15=((int)(abs(xin[m])))<<15;
		if (inst->x_fast<=abs_x_Q15) {
			inst->x_fast = abs_x_Q15;
		}	
	}

    for (m = 0 ; m<leng ; m++) {

		if (m<(polyblocksize)) {
			vad_short = pvad[0];
			vad_long = pvad_long[0];
		} else {
			vad_short = pvad[4];
			vad_long = pvad_long[4];
		}

		// abs_x_Q15=((int)(abs(xin[m])))<<15;

    	// if(abs_x_Q15>inst->x_fast){            
        // 	beta_f=beta16k[1];
        // 	roundbit_f=round_bit16k[1]; //128; //(short)1<<((inst->beta_f_r)-1);	
    	// }
    	// else {
        // 	beta_f=beta16k[8];
        // 	roundbit_f=round_bit16k[8]; //1024; //(short)1<<((inst->beta_f_f)-1);
    	// }
    	// inst->x_fast=inst->x_fast-((inst->x_fast+roundbit_f)>>beta_f);
		// inst->x_fast=inst->x_fast+((abs_x_Q15+roundbit_f)>>beta_f);

		// if(vad_short>0) {

		// 	inst->v_cnt++;

			if(inst->x_fast > inst->x_peak) {		
				if(vad_short>0) {
					inst->x_peak=inst->x_fast;
				} else {
					beta_p=beta16k[1];
					roundbit_p=round_bit16k[1]; //512; //(short)1<<((inst->beta_p_r)-1);
					inst->x_peak=inst->x_peak - ((inst->x_peak+roundbit_p)>>beta_p);
					inst->x_peak=inst->x_peak + ((inst->x_fast+roundbit_p)>>beta_p); 
				}	 
			}
			else {
				beta_p=beta16k[12];
				roundbit_p=round_bit16k[12]; //4096; //(short)1<<((inst->beta_f_f)-1);		
				inst->x_peak=inst->x_peak - ((inst->x_peak+roundbit_p)>>beta_p);
				inst->x_peak=inst->x_peak + ((inst->x_fast+roundbit_p)>>beta_p); 				
			} 		   
		
			xdB = 20.0*log10f((float)inst->x_peak);

			// if (((inst->v_cnt%500)==0)&&(inst->v_cnt>3000)) printf("v_cnt=%d\n", inst->v_cnt);
			if ((xdB>(threshold))){				
				inst->gc = threshold - xdB + 20; // + gatedB;
			} else {
				inst->gc = 20;
			}

			inst->gc=MIN(inst->gc, 20);
			inst->gc=MAX(inst->gc, 0);

			if (inst->gc < gs){
				gs = r_a * gs + (1.0 - r_a) * inst->gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * inst->gc;
			}

			inst->inputgain = powf (10, gs*0.05);
			

 		temp_gain=inst->inputgain; //*gate;
		xout[m]=(short)((float)xin[m]*temp_gain);
		xout2[m]=(short)((float)xin2[m]*temp_gain);
		xout3[m]=(short)((float)xin3[m]*temp_gain);
		xout4[m]=(short)((float)xin4[m]*temp_gain);
		xout5[m]=(short)((float)xin5[m]);

		abs_xout_Q15=((int)(abs(xout[m])))<<15;

    	if(abs_xout_Q15 > inst->xout_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	inst->xout_fast=inst->xout_fast-((inst->xout_fast+roundbit_f)>>beta_f);
		inst->xout_fast=inst->xout_fast+((abs_xout_Q15+roundbit_f)>>beta_f);

#ifdef DEBUG_AGC_MATLAB
	if ((g_agc_debug_on==1)&&(g_agc_debug_snd_idx==0)){    		
			inst->idx--;
			if (inst->idx<=0){
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_fast, 0);
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_peak, 1);
				
				debug_matlab_float(DEBUG_NUM_AGC, threshold, 2);
				debug_matlab_float(DEBUG_NUM_AGC, inst->gc, 3);
				debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

				debug_matlab_int(DEBUG_NUM_AGC, vad_short, 5);
				debug_matlab_int(DEBUG_NUM_AGC, vad_long, 6);

				// debug_matlab_int(DEBUG_NUM_AGC, inst->x_slow, 7);
				debug_matlab_int(DEBUG_NUM_AGC, inst->xout_fast, 7);
				// debug_matlab_int(DEBUG_NUM_AGC, inst->xref_slow, 9);

				debug_matlab_send(DEBUG_NUM_AGC);
				inst->idx=32;
			}
	}

	if (g_agc_debug_on==1){
		g_agc_debug_snd_idx++;
		if (g_agc_debug_snd_idx>=g_agc_debug_snd_period) {
			g_agc_debug_snd_idx = 0;
		}
	}

#endif

    }

	// inst->idx--;
	// if (inst->idx<=0){
	// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%.2f, gs=%.2f gain=%.2f cnt=%d vad=%d,\n", inst->x_peak, threshold-180, xdB-180, inst->gc, gs, inst->inputgain, inst->cnt, vad_short);
	// 	inst->idx=20;
	// }	
}

void AGC_input_5ch_2(agcInst_t *agcInst, int leng, void *in1, void *in2, void *in3, void *in4, void *in5, void *out, void *out2, void *out3, void *out4, void *out5, float globalMakeupGain_dB, float threshold_dBFS) {

    int m;
    float gmod, temp_gain;

    agcInst_t *inst = (agcInst_t *)agcInst;

    float xdB;
    int abs_x1_Q15, abs_x2_Q15, abs_x3_Q15, abs_x4_Q15, abs_x5_Q15;
    int max_x1_Q15, max_x2_Q15, max_x3_Q15, max_x4_Q15, max_x5_Q15;	
    int abs_x_Q15, abs_xout_Q15, beta_f, roundbit_f, beta_p, roundbit_p;

    short *xin = (short *)in1;
    short *xin2 = (short *)in2;
    short *xin3 = (short *)in3;
    short *xin4 = (short *)in4;
    short *xin5 = (short *)in5;
    short *xout = (short *)out;
    short *xout2 = (short *)out2;
    short *xout3 = (short *)out3;
    short *xout4 = (short *)out4;
    short *xout5 = (short *)out5;

    float gs = 20.0*log10f((float)inst->inputgain);
    float r_a = gamma16k[1];  // Attack time constant
    float r_r = gamma16k[7]; // Release time constant

	float threshold_dB = Q30_dB + threshold_dBFS;
    float softClipStart = threshold_dB - 3.0f; // 쓰레숄드 3dB 전부터 소프트 클리핑 시작

    max_x1_Q15 = 0;
    max_x2_Q15 = 0;
    max_x3_Q15 = 0;
    max_x4_Q15 = 0;
    max_x5_Q15 = 0;

    for (m = 0 ; m < leng ; m++) {
        // 각 채널의 절대값 최대값을 계산
        abs_x1_Q15 = ((int)(abs(xin[m]))) << 15;
        abs_x2_Q15 = ((int)(abs(xin2[m]))) << 15;
        abs_x3_Q15 = ((int)(abs(xin3[m]))) << 15;
        abs_x4_Q15 = ((int)(abs(xin4[m]))) << 15;
        abs_x5_Q15 = ((int)(abs(xin5[m]))) << 15;

        if (abs_x1_Q15 > max_x1_Q15) max_x1_Q15 = abs_x1_Q15;
        if (abs_x2_Q15 > max_x2_Q15) max_x2_Q15 = abs_x2_Q15;
        if (abs_x3_Q15 > max_x3_Q15) max_x3_Q15 = abs_x3_Q15;
        if (abs_x4_Q15 > max_x4_Q15) max_x4_Q15 = abs_x4_Q15;
        if (abs_x5_Q15 > max_x5_Q15) max_x5_Q15 = abs_x5_Q15;
    }

    // 5개의 채널 최대값을 평균하여 x_fast 계산
    inst->x_fast = (max_x1_Q15 + max_x2_Q15 + max_x3_Q15 + max_x4_Q15 + max_x5_Q15) / 5;


    for (m = 0 ; m < leng ; m++) {

        if (inst->x_fast > inst->x_peak) {        
            inst->x_peak = inst->x_fast;
        } else {
            beta_p = beta16k[7];
            roundbit_p = round_bit16k[7];        
            inst->x_peak = inst->x_peak - ((inst->x_peak + roundbit_p) >> beta_p);
            inst->x_peak = inst->x_peak + ((inst->x_fast + roundbit_p) >> beta_p);                
        }           

        xdB = 20.0f * log10f((float)inst->x_peak);

        if (xdB > threshold_dB) {                
            inst->gc = threshold_dB - xdB + globalMakeupGain_dB; 
        } else if (xdB > softClipStart) {
            // 서서히 게인을 줄이는 부분
            float delta = (threshold_dB - xdB) / 3.0f;
            inst->gc = delta + globalMakeupGain_dB; 
        } else {
            inst->gc = globalMakeupGain_dB;
        }

        inst->gc = MIN(inst->gc, globalMakeupGain_dB);
        inst->gc = MAX(inst->gc, -100);

        if (inst->gc < gs) {
            gs = r_a * gs + (1.0 - r_a) * inst->gc;
        } else {
            gs = r_r * gs + (1.0 - r_r) * inst->gc;
        }

        inst->inputgain = powf(10, gs * 0.05f);

        temp_gain = inst->inputgain;
        xout[m] = (short)((float)xin[m] * temp_gain);
        xout2[m] = (short)((float)xin2[m] * temp_gain);
        xout3[m] = (short)((float)xin3[m] * temp_gain);
        xout4[m] = (short)((float)xin4[m] * temp_gain);
        xout5[m] = (short)((float)xin5[m] * temp_gain);

        abs_xout_Q15 = ((int)(abs(xout[m]))) << 15;

        if (abs_xout_Q15 > inst->xout_fast) {            
            beta_f = inst->beta_f_r;
            roundbit_f = inst->round_bit_f_r;
        } else {
            beta_f = inst->beta_f_f;
            roundbit_f = inst->round_bit_f_f;
        }
        inst->xout_fast = inst->xout_fast - ((inst->xout_fast + roundbit_f) >> beta_f);
        inst->xout_fast = inst->xout_fast + ((abs_xout_Q15 + roundbit_f) >> beta_f);

#ifdef DEBUG_AGC_MATLAB
        if ((g_agc_debug_on == 1) && (g_agc_debug_snd_idx == 0)){            
            inst->idx--;
            if (inst->idx <= 0){
                debug_matlab_int(DEBUG_NUM_AGC, inst->x_fast, 0);
                debug_matlab_int(DEBUG_NUM_AGC, inst->x_peak, 1);
                
                debug_matlab_float(DEBUG_NUM_AGC, threshold_dB, 2);
                debug_matlab_float(DEBUG_NUM_AGC, inst->gc, 3);
                debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

                debug_matlab_int(DEBUG_NUM_AGC, inst->xout_fast, 7);

                debug_matlab_send(DEBUG_NUM_AGC);
                inst->idx = 32;
            }
        }

        if (g_agc_debug_on == 1){
            g_agc_debug_snd_idx++;
            if (g_agc_debug_snd_idx >= g_agc_debug_snd_period) {
                g_agc_debug_snd_idx = 0;
            }
        }

#endif

    }

	// inst->idx--;
	// if (inst->idx<=0){
	// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%.2f, gs=%.2f gain=%.2f\n", inst->x_peak, threshold_dB-180, xdB-180, inst->gc, gs, inst->inputgain);
	// 	inst->idx=20;
	// }		
}


void AGC_input_2ch(agcInst_t *agcInst, int leng, void *in1, void *in2, void *out, void *out2, float globalMakeupGain_dB, float threshold_dBFS) {

    int m;
    float gmod, temp_gain;

    agcInst_t *inst = (agcInst_t *)agcInst;

    float xdB;
    int abs_x1_Q15, abs_x2_Q15, abs_x3_Q15, abs_x4_Q15, abs_x5_Q15;
    int max_x1_Q15, max_x2_Q15, max_x3_Q15, max_x4_Q15, max_x5_Q15;	
    int abs_x_Q15, abs_xout_Q15, beta_f, roundbit_f, beta_p, roundbit_p;

    short *xin = (short *)in1;
    short *xin2 = (short *)in2;

    short *xout = (short *)out;
    short *xout2 = (short *)out2;


	float gs = 20.0*log10f((float)inst->inputgain);	
    float r_a = gamma16k[1];  // Attack time constant
    float r_r = gamma16k[7]; // Release time constant

	float threshold_dB = Q30_dB + threshold_dBFS;
    float softClipStart = threshold_dB - 3.0f; // 쓰레숄드 3dB 전부터 소프트 클리핑 시작

    max_x1_Q15 = 0;
    max_x2_Q15 = 0;

    for (m = 0 ; m < leng ; m++) {
        // 각 채널의 절대값 최대값을 계산
        abs_x1_Q15 = ((int)(abs(xin[m]))) << 15;
        abs_x2_Q15 = ((int)(abs(xin2[m]))) << 15;

        if (abs_x1_Q15 > max_x1_Q15) max_x1_Q15 = abs_x1_Q15;
        if (abs_x2_Q15 > max_x2_Q15) max_x2_Q15 = abs_x2_Q15;
    }

    // 5개의 채널 최대값을 평균하여 x_fast 계산
	inst->x_fast = (max_x1_Q15 + max_x2_Q15 ) / 2;


    for (m = 0 ; m < leng ; m++) {

        if (inst->x_fast > inst->x_peak) {        
            inst->x_peak = inst->x_fast;
        } else {
            beta_p = beta16k[7];
            roundbit_p = round_bit16k[7];        
            inst->x_peak = inst->x_peak - ((inst->x_peak + roundbit_p) >> beta_p);
            inst->x_peak = inst->x_peak + ((inst->x_fast + roundbit_p) >> beta_p);                
        }           

        xdB = 20.0f * log10f((float)inst->x_peak);

        if (xdB > threshold_dB) {                
            inst->gc = threshold_dB - xdB + globalMakeupGain_dB; 
        } else if (xdB > softClipStart) {
            // 서서히 게인을 줄이는 부분
            float delta = (threshold_dB - xdB) / 3.0f;
            inst->gc = delta + globalMakeupGain_dB; 
        } else {
            inst->gc = globalMakeupGain_dB;
        }

        inst->gc = MIN(inst->gc, globalMakeupGain_dB);
        inst->gc = MAX(inst->gc, -100);

        if (inst->gc < gs) {
            gs = r_a * gs + (1.0 - r_a) * inst->gc;
        } else {
            gs = r_r * gs + (1.0 - r_r) * inst->gc;
        }

        inst->inputgain = powf(10, gs * 0.05f);

        temp_gain = inst->inputgain;
        xout[m] = (short)((float)xin[m] * temp_gain);
        xout2[m] = (short)((float)xin2[m] * temp_gain);

        abs_xout_Q15 = ((int)(abs(xout[m]))) << 15;

        if (abs_xout_Q15 > inst->xout_fast) {            
            beta_f = inst->beta_f_r;
            roundbit_f = inst->round_bit_f_r;
        } else {
            beta_f = inst->beta_f_f;
            roundbit_f = inst->round_bit_f_f;
        }
        inst->xout_fast = inst->xout_fast - ((inst->xout_fast + roundbit_f) >> beta_f);
        inst->xout_fast = inst->xout_fast + ((abs_xout_Q15 + roundbit_f) >> beta_f);

#ifdef DEBUG_AGC_MATLAB
		if ((g_agc_debug_on == 1) && (g_agc_debug_snd_idx == 0)){            
			inst->idx--;
			if (inst->idx <= 0){
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_fast, 0);
				debug_matlab_int(DEBUG_NUM_AGC, inst->x_peak, 1);
				
				debug_matlab_float(DEBUG_NUM_AGC, threshold_dB, 2);
				debug_matlab_float(DEBUG_NUM_AGC, inst->gc, 3);
				debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

				debug_matlab_int(DEBUG_NUM_AGC, inst->xout_fast, 7);

				debug_matlab_send(DEBUG_NUM_AGC);
				inst->idx = 32;
			}
		}

		if (g_agc_debug_on == 1){
			g_agc_debug_snd_idx++;
			if (g_agc_debug_snd_idx >= g_agc_debug_snd_period) {
				g_agc_debug_snd_idx = 0;
			}
		}

#endif

    }

	// idx--;
	// if (idx<=0){
	// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%.2f, gs=%.2f gain=%.2f\n", x_peak, threshold_dB-180, xdB-180, gc, gs, inputgain);
	// 	idx=20;
	// }		
}



void AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat) {

	int k, m, n;
	float gmod;

	agcInst_t *inst = (agcInst_t *)agcInst;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;

	int x_pow, abs_x, beta_f, roundbit_f,  beta_p, roundbit_p;
	float x_pow_f, abs_x_f;
	
	// float Kp_dB = 20.0*log10f((float)inst->Kp_band);

	// float gs[256];
	// for (n=0; n<128; n++){
	// 	gs [n]= 20.0*log10f((float)inst->gain_band[n]);
	// }
	
	// float r_a = 0.96875;
	// float r_r = 0.96875;

	float deno = 1.0/(32768.0);

	for (m=0 ; m<PolyL ; m++){
		for (k=m*2, n=0; k<PolyM*PolyL+m*2; k+=PolyL*2){
			// x_pow=(int)fft_xin_mat[k]*(int)fft_xin_mat[k]+(int)fft_xin_mat[k+1]*(int)fft_xin_mat[k+1];
			// abs_x=((int)(sqrt((double)x_pow)))<<15;
			x_pow_f=(float)fft_xin_mat[k]*(float)fft_xin_mat[k]+(float)fft_xin_mat[k+1]*(float)fft_xin_mat[k+1];
			abs_x=((int)(sqrtf((float)x_pow_f)));

			
			if(abs_x > inst->x_fast_band[n]){			 
				beta_f=inst->beta_f_r_band;
				roundbit_f=inst->round_bit_f_r_band;
			}
			else {
				beta_f=inst->beta_f_f_band;
				roundbit_f=inst->round_bit_f_f_band;
			}
			inst->x_fast_band[n]=inst->x_fast_band[n]-((inst->x_fast_band[n]+roundbit_f)>>beta_f);
			inst->x_fast_band[n]=inst->x_fast_band[n]+((abs_x+roundbit_f)>>beta_f);

			if(inst->x_fast_band[n] > inst->x_peak_band[n]) { 		 
				beta_p=inst->beta_p_r_band;
				roundbit_p=inst->round_bit_p_r_band;
			}
			else {
				beta_p=inst->beta_p_f_band;
				roundbit_p=inst->round_bit_p_f_band;
			}		   
			inst->x_peak_band[n]=inst->x_peak_band[n] - ((inst->x_peak_band[n]+roundbit_p)>>beta_p);
			inst->x_peak_band[n]=inst->x_peak_band[n] + ((inst->x_fast_band[n]+roundbit_p)>>beta_p);

			// float xdB = 20.0*log10f((float)inst->x_peak_band[n]);

			// float gc = Kp_dB - xdB;

			// if (gs[n] > gc){
			// 	gs[n] = r_a * gs[n] + (1.0 - r_a) * gc;
			// } else {
			// 	gs[n] = r_r * gs[n] + (1.0 - r_r) * gc;
			// }

			// inst->gain_band[n] = powf (10, gs[n]*0.05);

			if ((((float)inst->x_peak_band[n])*inst->gain_band[n])<inst->Kp_band) {
				gmod=inst->ginc_band;
			}
			else {
				gmod=inst->gdec_band;
			}
			inst->gain_band[n]=inst->gain_band[n]*gmod; 
			inst->gain_band[n]=MIN(inst->gain_band[n], inst->max_gain_band);
			inst->gain_band[n]=MAX(inst->gain_band[n], inst->min_gain_band);

			inst->gain_band_hist[m][n]=inst->gain_band[n];

			fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k] * inst->gain_band[n]);
			fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1] * inst->gain_band[n]);
			n++;
		}
	}
}


void en_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat) {

	int k, m, n;

	agcInst_t *inst = (agcInst_t *)agcInst;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
	
	for (m=0 ; m<PolyL ; m++){
		for (k=m*2, n=0; k<PolyM*PolyL+m*2; k+=PolyL*2){
            fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k] * inst->gain_band_hist[m][n]);
            fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1] * inst->gain_band_hist[m][n]);
            n++;
		}
	}
}


void de_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat) {

	int k, m, n;

	agcInst_t *inst = (agcInst_t *)agcInst;	

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
	
	for (m=0 ; m<PolyL ; m++){
		for (k=m*2, n=0; k<PolyM*PolyL+m*2; k+=PolyL*2){
            fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k]/ inst->gain_band_hist[m][n]);
            fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1]/ inst->gain_band_hist[m][n]);
            n++;
		}
	}
}
