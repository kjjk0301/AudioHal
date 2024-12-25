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
		
agcInst_t Sys_agcInst;


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


agcInst_t * sysAGCCreate()
{
	AGC_Init(&Sys_agcInst);
	return &Sys_agcInst;
}

float gc=0;

void AGC_Init(agcInst_t *agcInst){

	int i;

	gc=0;

	agcInst_t *inst = (agcInst_t *)agcInst;

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

	inst->min_vcnt = 500;

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
		gain_band[i] = 1.0;
		x_fast_band[i] = 0;
		x_slow_band[i] = 0;
		x_peak_band[i] = 0;
	}

	inst->globalMakeupGain_dB = 5.0;
	inst->threshold_dBFS = -8.0;

// #ifdef DEBUG_AGC_MATLAB
// 	debug_matlab_open();
// #endif	
}

void AGC_DeInit(){

// #ifdef DEBUG_AGC_MATLAB
// 	debug_matlab_close();
// #endif	
}

int idx=0;
int v_cnt = 0;

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
	
	float gs= 20.0*log10f((float)gain);

	float r_a = inst->r_att;
	float r_r = inst->r_rel;
		
    for (m = 0 ; m<polyblocksize ; m++) {

		if (m>=(polyblocksize>>1)) vad_short = (vad && 0x0010)>>1;

		abs_x_Q15=((int)(abs(xin[m])))<<15;

    	if(abs_x_Q15>x_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	x_fast=x_fast-((x_fast+roundbit_f)>>beta_f);
		x_fast=x_fast+((abs_x_Q15+roundbit_f)>>beta_f);

		abs_xref_Q15=((int)(abs(xref[m])))<<15;	

#ifdef DEBUG_AGC_MATLAB
    	if(abs_xref_Q15>xref_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	xref_fast=xref_fast-((xref_fast+roundbit_f)>>beta_f);
		xref_fast=xref_fast+((abs_xref_Q15+roundbit_f)>>beta_f);		
#endif

		if(vad_short>0) {

			v_cnt++;

			if(x_fast>x_peak) {			 
				beta_p=inst->beta_p_r;
				roundbit_p=inst->round_bit_p_r;
			}
			else {
				beta_p=inst->beta_p_f;
				roundbit_p=inst->round_bit_p_f;
			} 		   
			x_peak=x_peak - ((x_peak+roundbit_p)>>beta_p);
			x_peak=x_peak + ((x_fast+roundbit_p)>>beta_p); 
		
			xdB = 20.0*log10f((float)x_peak);

			// if (((v_cnt%500)==0)&&(v_cnt>3000)) printf("v_cnt=%d\n", v_cnt);
			if ((xdB>(inst->Kp_dB-40))&&(v_cnt>(inst->min_vcnt))){				
				gc = inst->Kp_dB - xdB; // + gatedB;
			}

			gc=MIN(gc, inst->max_gain_dB);
			gc=MAX(gc, inst->min_gain_dB);

			if (gs < gc){
				gs = r_a * gs + (1.0 - r_a) * gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * gc;
			}

			gain = powf (10, gs*0.05);
			
		}
		else {

			if(vad_long==0){
				v_cnt=0;
			}

			xdB = 20.0*log10f((float)x_peak);


			// if (((v_cnt%500)==0)&&(v_cnt>3000)) printf("v_cnt=%d\n", v_cnt);

			if ((xdB>(inst->Kp_dB-40))&&(v_cnt>5000)){
				gc = inst->Kp_dB - xdB; // + gatedB;
			}

			gc=MIN(gc, inst->max_gain_dB);
			gc=MAX(gc, inst->min_gain_dB);

			if (gs < gc){
				gs = r_a * gs + (1.0 - r_a) * gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * gc;
			}

			gain = powf (10, gs*0.05);

		}

 		temp_gain=gain; //*gate;
		xout[m]=(short)((float)xin[m]*temp_gain);

		abs_xout_Q15=((int)(abs(xout[m])))<<15;

    	if(abs_xout_Q15>xout_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	xout_fast=xout_fast-((xout_fast+roundbit_f)>>beta_f);
		xout_fast=xout_fast+((abs_xout_Q15+roundbit_f)>>beta_f);

#ifdef DEBUG_AGC_MATLAB
	if ((g_agc_debug_on==1)&&(g_agc_debug_snd_idx==0)){    		
			idx--;
			if (idx<=0){
				debug_matlab_int(DEBUG_NUM_AGC, x_fast, 0);
				debug_matlab_int(DEBUG_NUM_AGC, x_peak, 1);
				
				debug_matlab_float(DEBUG_NUM_AGC, inst->Kp_dB, 2);
				debug_matlab_float(DEBUG_NUM_AGC, gc, 3);
				debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

				debug_matlab_int(DEBUG_NUM_AGC, vad_short, 5);
				debug_matlab_int(DEBUG_NUM_AGC, vad_long, 6);

				// debug_matlab_int(DEBUG_NUM_AGC, x_slow, 7);
				debug_matlab_int(DEBUG_NUM_AGC, xout_fast, 7);
				
				debug_matlab_int(DEBUG_NUM_AGC, xref_fast, 8);
				// debug_matlab_int(DEBUG_NUM_AGC, xref_slow, 9);

				debug_matlab_send(DEBUG_NUM_AGC);
				idx=32;
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

		// idx--;
		// if (idx<=0){
		// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%f, gs=%f gain=%f cnt=%d vad=%d,\n", x_peak, inst->Kp_dB, xdB, gc, gs, gain, cnt, vad);
		// 	idx=20;
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

	float gs= 20.0*log10f((float)inputgain);
	float r_a = gamma16k[1];
	float r_r = gamma16k[11];	

	short vad_short, vad_long;

	float threshold = (float)Q15_dB + (float)Q15_dB - 26.0f;

	x_fast = 0;
	for (m = 0 ; m<leng ; m++) {
		abs_x_Q15=((int)(abs(xin[m])))<<15;
		if (x_fast<=abs_x_Q15) {
			x_fast = abs_x_Q15;
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

    	// if(abs_x_Q15>x_fast){            
        // 	beta_f=beta16k[1];
        // 	roundbit_f=round_bit16k[1]; //128; //(short)1<<((inst->beta_f_r)-1);	
    	// }
    	// else {
        // 	beta_f=beta16k[8];
        // 	roundbit_f=round_bit16k[8]; //1024; //(short)1<<((inst->beta_f_f)-1);
    	// }
    	// x_fast=x_fast-((x_fast+roundbit_f)>>beta_f);
		// x_fast=x_fast+((abs_x_Q15+roundbit_f)>>beta_f);

		// if(vad_short>0) {

		// 	v_cnt++;

			if(x_fast>x_peak) {		
				if(vad_short>0) {
					x_peak=x_fast;
				} else {
					beta_p=beta16k[1];
					roundbit_p=round_bit16k[1]; //512; //(short)1<<((inst->beta_p_r)-1);
					x_peak=x_peak - ((x_peak+roundbit_p)>>beta_p);
					x_peak=x_peak + ((x_fast+roundbit_p)>>beta_p); 
				}	 
			}
			else {
				beta_p=beta16k[12];
				roundbit_p=round_bit16k[12]; //4096; //(short)1<<((inst->beta_f_f)-1);		
				x_peak=x_peak - ((x_peak+roundbit_p)>>beta_p);
				x_peak=x_peak + ((x_fast+roundbit_p)>>beta_p); 				
			} 		   
		
			xdB = 20.0*log10f((float)x_peak);

			// if (((v_cnt%500)==0)&&(v_cnt>3000)) printf("v_cnt=%d\n", v_cnt);
			if ((xdB>(threshold))){				
				gc = threshold - xdB + 20; // + gatedB;
			} else {
				gc = 20;
			}

			gc=MIN(gc, 20);
			gc=MAX(gc, 0);

			if (gc < gs){
				gs = r_a * gs + (1.0 - r_a) * gc;
			} else {
				gs = r_r * gs + (1.0 - r_r) * gc;
			}

			inputgain = powf (10, gs*0.05);
			

 		temp_gain=inputgain; //*gate;
		xout[m]=(short)((float)xin[m]*temp_gain);
		xout2[m]=(short)((float)xin2[m]*temp_gain);
		xout3[m]=(short)((float)xin3[m]*temp_gain);
		xout4[m]=(short)((float)xin4[m]*temp_gain);
		xout5[m]=(short)((float)xin5[m]);

		abs_xout_Q15=((int)(abs(xout[m])))<<15;

    	if(abs_xout_Q15>xout_fast){            
        	beta_f=inst->beta_f_r;
        	roundbit_f=inst->round_bit_f_r;
    	}
    	else {
        	beta_f=inst->beta_f_f;
        	roundbit_f=inst->round_bit_f_f;
    	}
    	xout_fast=xout_fast-((xout_fast+roundbit_f)>>beta_f);
		xout_fast=xout_fast+((abs_xout_Q15+roundbit_f)>>beta_f);

#ifdef DEBUG_AGC_MATLAB
	if ((g_agc_debug_on==1)&&(g_agc_debug_snd_idx==0)){    		
			idx--;
			if (idx<=0){
				debug_matlab_int(DEBUG_NUM_AGC, x_fast, 0);
				debug_matlab_int(DEBUG_NUM_AGC, x_peak, 1);
				
				debug_matlab_float(DEBUG_NUM_AGC, threshold, 2);
				debug_matlab_float(DEBUG_NUM_AGC, gc, 3);
				debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

				debug_matlab_int(DEBUG_NUM_AGC, vad_short, 5);
				debug_matlab_int(DEBUG_NUM_AGC, vad_long, 6);

				// debug_matlab_int(DEBUG_NUM_AGC, x_slow, 7);
				debug_matlab_int(DEBUG_NUM_AGC, xout_fast, 7);
				// debug_matlab_int(DEBUG_NUM_AGC, xref_slow, 9);

				debug_matlab_send(DEBUG_NUM_AGC);
				idx=32;
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

	// idx--;
	// if (idx<=0){
	// 	printf("AGC : x_peak = %d, inst->Kp_dB = %3.1f, xdB = %3.1f, gc=%.2f, gs=%.2f gain=%.2f cnt=%d vad=%d,\n", x_peak, threshold-180, xdB-180, gc, gs, inputgain, cnt, vad_short);
	// 	idx=20;
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

    float gs = 20.0*log10f((float)inputgain);
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
    x_fast = (max_x1_Q15 + max_x2_Q15 + max_x3_Q15 + max_x4_Q15 + max_x5_Q15) / 5;


    for (m = 0 ; m < leng ; m++) {

        if (x_fast > x_peak) {        
            x_peak = x_fast;
        } else {
            beta_p = beta16k[7];
            roundbit_p = round_bit16k[7];        
            x_peak = x_peak - ((x_peak + roundbit_p) >> beta_p);
            x_peak = x_peak + ((x_fast + roundbit_p) >> beta_p);                
        }           

        xdB = 20.0f * log10f((float)x_peak);

        if (xdB > threshold_dB) {                
            gc = threshold_dB - xdB + globalMakeupGain_dB; 
        } else if (xdB > softClipStart) {
            // 서서히 게인을 줄이는 부분
            float delta = (threshold_dB - xdB) / 3.0f;
            gc = delta + globalMakeupGain_dB; 
        } else {
            gc = globalMakeupGain_dB;
        }

        gc = MIN(gc, globalMakeupGain_dB);
        gc = MAX(gc, -100);

        if (gc < gs) {
            gs = r_a * gs + (1.0 - r_a) * gc;
        } else {
            gs = r_r * gs + (1.0 - r_r) * gc;
        }

        inputgain = powf(10, gs * 0.05f);

        temp_gain = inputgain;
        xout[m] = (short)((float)xin[m] * temp_gain);
        xout2[m] = (short)((float)xin2[m] * temp_gain);
        xout3[m] = (short)((float)xin3[m] * temp_gain);
        xout4[m] = (short)((float)xin4[m] * temp_gain);
        xout5[m] = (short)((float)xin5[m] * temp_gain);

        abs_xout_Q15 = ((int)(abs(xout[m]))) << 15;

        if (abs_xout_Q15 > xout_fast) {            
            beta_f = inst->beta_f_r;
            roundbit_f = inst->round_bit_f_r;
        } else {
            beta_f = inst->beta_f_f;
            roundbit_f = inst->round_bit_f_f;
        }
        xout_fast = xout_fast - ((xout_fast + roundbit_f) >> beta_f);
        xout_fast = xout_fast + ((abs_xout_Q15 + roundbit_f) >> beta_f);

#ifdef DEBUG_AGC_MATLAB
        if ((g_agc_debug_on == 1) && (g_agc_debug_snd_idx == 0)){            
            idx--;
            if (idx <= 0){
                debug_matlab_int(DEBUG_NUM_AGC, x_fast, 0);
                debug_matlab_int(DEBUG_NUM_AGC, x_peak, 1);
                
                debug_matlab_float(DEBUG_NUM_AGC, threshold_dB, 2);
                debug_matlab_float(DEBUG_NUM_AGC, gc, 3);
                debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

                debug_matlab_int(DEBUG_NUM_AGC, xout_fast, 7);

                debug_matlab_send(DEBUG_NUM_AGC);
                idx = 32;
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


    float gs = 20.0*log10f((float)inputgain);
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
    x_fast = (max_x1_Q15 + max_x2_Q15 ) / 2;


    for (m = 0 ; m < leng ; m++) {

        if (x_fast > x_peak) {        
            x_peak = x_fast;
        } else {
            beta_p = beta16k[7];
            roundbit_p = round_bit16k[7];        
            x_peak = x_peak - ((x_peak + roundbit_p) >> beta_p);
            x_peak = x_peak + ((x_fast + roundbit_p) >> beta_p);                
        }           

        xdB = 20.0f * log10f((float)x_peak);

        if (xdB > threshold_dB) {                
            gc = threshold_dB - xdB + globalMakeupGain_dB; 
        } else if (xdB > softClipStart) {
            // 서서히 게인을 줄이는 부분
            float delta = (threshold_dB - xdB) / 3.0f;
            gc = delta + globalMakeupGain_dB; 
        } else {
            gc = globalMakeupGain_dB;
        }

        gc = MIN(gc, globalMakeupGain_dB);
        gc = MAX(gc, -100);

        if (gc < gs) {
            gs = r_a * gs + (1.0 - r_a) * gc;
        } else {
            gs = r_r * gs + (1.0 - r_r) * gc;
        }

        inputgain = powf(10, gs * 0.05f);

        temp_gain = inputgain;
        xout[m] = (short)((float)xin[m] * temp_gain);
        xout2[m] = (short)((float)xin2[m] * temp_gain);

        abs_xout_Q15 = ((int)(abs(xout[m]))) << 15;

        if (abs_xout_Q15 > xout_fast) {            
            beta_f = inst->beta_f_r;
            roundbit_f = inst->round_bit_f_r;
        } else {
            beta_f = inst->beta_f_f;
            roundbit_f = inst->round_bit_f_f;
        }
        xout_fast = xout_fast - ((xout_fast + roundbit_f) >> beta_f);
        xout_fast = xout_fast + ((abs_xout_Q15 + roundbit_f) >> beta_f);

#ifdef DEBUG_AGC_MATLAB
        if ((g_agc_debug_on == 1) && (g_agc_debug_snd_idx == 0)){            
            idx--;
            if (idx <= 0){
                debug_matlab_int(DEBUG_NUM_AGC, x_fast, 0);
                debug_matlab_int(DEBUG_NUM_AGC, x_peak, 1);
                
                debug_matlab_float(DEBUG_NUM_AGC, threshold_dB, 2);
                debug_matlab_float(DEBUG_NUM_AGC, gc, 3);
                debug_matlab_float(DEBUG_NUM_AGC, gs, 4);

                debug_matlab_int(DEBUG_NUM_AGC, xout_fast, 7);

                debug_matlab_send(DEBUG_NUM_AGC);
                idx = 32;
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
	// 	gs [n]= 20.0*log10f((float)gain_band[n]);
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

			
			if(abs_x>x_fast_band[n]){			 
				beta_f=inst->beta_f_r_band;
				roundbit_f=inst->round_bit_f_r_band;
			}
			else {
				beta_f=inst->beta_f_f_band;
				roundbit_f=inst->round_bit_f_f_band;
			}
			x_fast_band[n]=x_fast_band[n]-((x_fast_band[n]+roundbit_f)>>beta_f);
			x_fast_band[n]=x_fast_band[n]+((abs_x+roundbit_f)>>beta_f);

			if(x_fast_band[n]>x_peak_band[n]) { 		 
				beta_p=inst->beta_p_r_band;
				roundbit_p=inst->round_bit_p_r_band;
			}
			else {
				beta_p=inst->beta_p_f_band;
				roundbit_p=inst->round_bit_p_f_band;
			}		   
			x_peak_band[n]=x_peak_band[n] - ((x_peak_band[n]+roundbit_p)>>beta_p);
			x_peak_band[n]=x_peak_band[n] + ((x_fast_band[n]+roundbit_p)>>beta_p);

			// float xdB = 20.0*log10f((float)x_peak_band[n]);

			// float gc = Kp_dB - xdB;

			// if (gs[n] > gc){
			// 	gs[n] = r_a * gs[n] + (1.0 - r_a) * gc;
			// } else {
			// 	gs[n] = r_r * gs[n] + (1.0 - r_r) * gc;
			// }

			// gain_band[n] = powf (10, gs[n]*0.05);

			if ((((float)x_peak_band[n])*gain_band[n])<inst->Kp_band) {
				gmod=inst->ginc_band;
			}
			else {
				gmod=inst->gdec_band;
			}
			gain_band[n]=gain_band[n]*gmod; 
			gain_band[n]=MIN(gain_band[n], inst->max_gain_band);
			gain_band[n]=MAX(gain_band[n], inst->min_gain_band);

			gain_band_hist[m][n]=gain_band[n];

			fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k]*gain_band[n]);
			fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1]*gain_band[n]);
			n++;
		}
	}
}


void en_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat) {

	int k, m, n;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
	
	for (m=0 ; m<PolyL ; m++){
		for (k=m*2, n=0; k<PolyM*PolyL+m*2; k+=PolyL*2){
            fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k]*gain_band_hist[m][n]);
            fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1]*gain_band_hist[m][n]);
            n++;
		}
	}
}


void de_AGC_band(agcInst_t *agcInst, void *fft_in_mat, void *fft_out_mat) {

	int k, m, n;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
	
	for (m=0 ; m<PolyL ; m++){
		for (k=m*2, n=0; k<PolyM*PolyL+m*2; k+=PolyL*2){
            fft_xout_mat[k]=(int32_t)((float)fft_xin_mat[k]/gain_band_hist[m][n]);
            fft_xout_mat[k+1]=(int32_t)((float)fft_xin_mat[k+1]/gain_band_hist[m][n]);
            n++;
		}
	}
}
