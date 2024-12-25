/*
 * ssl_vad.c
 *
 *	Created on: 2019. 6. 24.
 *		Author: Seong Pil Moon
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

	
#include "sys.h"
#include "ssl_vad.h"

#include "debug_file.h"


vadInst_t Sys_vadInst;

double avg_time=1.0;
double fs=16000;

//	int avgsize =(int)(avg_time*fs/ssl_blocksize);
#define avgsize 32 //128 //64
#define avgsize2 64 //128 //64
#define vad_idx_total 6 // NUM_FRAMES/ssl_blocksize
#define tau_s_r_def 0.0005 //0.05 //0.001 //0.0001
#define tau_s_f_def 0.22 //0.1 //0.05 //0.6
#define d_SNR_def 4.0
#define d_SNR_vad_def 4.0// 10//6
#define n_floor_min_def 30 //30//20


int e_hist_Buf[5][avgsize2+vad_idx_total-1];
int BPF_Buf[5][NWz+NUM_FRAMES-1];

// double x_slow_vad=0;
// double t_chg=1.511886431509580e+02;
// double t_vad=4.011872336272725e+02;
// int n_floor=n_floor_min_def;
// int state=0;

double g_x_slow_vad[2];
double g_t_chg[2];
double g_t_vad[2];
int g_n_floor[2];
int g_state[2];

int vad_hist1[80];
int vad_hist2[80];
// int vad_hist3[80];
// int vad_hist4[80];   

vadInst_t * sysSSLVADCreate()
{
	int i, j;
    ssl_vad_Init(&Sys_vadInst);

	return &Sys_vadInst;
}


void ssl_vad_Init(vadInst_t *vadInst)
{

	int i, j, m;
    vadInst_t *inst = (vadInst_t *)vadInst;

    inst->blocksize=NUM_FRAMES;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block

	inst->tau_s_r = tau_s_r_def;
	inst->tau_s_f = tau_s_f_def;

	inst->d_SNR = d_SNR_def;
	inst->d_SNR_vad = d_SNR_vad_def;
	inst->d_SNR_ratio = pow(10, inst->d_SNR/10.0)-1.0;
	inst->d_SNR_vad_ratio = pow(10, inst->d_SNR_vad/10.0)-1.0;

	inst->n_floor = 50000;
	inst->n_floor_min = n_floor_min_def;
	inst->t_chg_min=pow(10,(((double)inst->n_floor_min)/10));

	inst->vad_hold_max = vad_hold_max_def;
	inst->vad_hold_min = vad_hold_min_def;

	for (j=0; j<2; j++){
		g_t_chg[j] = inst->t_chg_min * 20 * inst->d_SNR_ratio;
		g_t_vad[j] = inst->t_chg_min * 20 * inst->d_SNR_vad_ratio;
		g_x_slow_vad[j]=50000;
		for (i=0; i<avgsize2+vad_idx_total-1 ; i++){
			e_hist_Buf[j][i]=50000;
		}
		g_n_floor[j] = inst->n_floor_min;
		g_state[j] = 0;

		for (m=0 ; m<NWz+NUM_FRAMES-1 ; m++){
			BPF_Buf[j][m]=200;
		}
	}

	for (j=0; j<5; j++){
		for (m=0 ; m<NWz+NUM_FRAMES-1 ; m++){
			BPF_Buf[j][m]=200;
		}
	}

    for (i=0; i<80; i++){
        vad_hist1[i]=0;
        vad_hist2[i]=0;
        // vad_hist3[i]=0;
        // vad_hist4[i]=0;
    }

#ifdef DEBUG_SSL_VAD
	debug_file_open();
#endif	

// #ifdef DEBUG_SSL_VAD_MATLAB
// 	debug_matlab_open();
// #endif	

}


void ssl_vad_DeInit(){
#ifdef DEBUG_SSL_VAD
	debug_file_close();
#endif	

// #ifdef DEBUG_SSL_VAD_MATLAB
// 	debug_matlab_close();
// #endif	

}

int filter_Q15(int * x_p, int * filtQ15_p, short filt_leng)
{
    // int temp=16384;
	int temp=0;
    short i;
	// float temp_f=0;

    for(i=0; i<filt_leng; i++){
        temp += (x_p[i]*filtQ15_p[i]+16384)>>15;
		// temp += (x_p[i]*filtQ15_p[i])>>15;
    }

    return (temp);
}

int BPF_coef_Q15[NWz]={379	, -700	,92	,-214	,-665	,-514	,63	,316	,
36	,-188	,18	,257	,169	,29	,105	,137	,
-54	,-153	,-18	,-9	,-243	,-295	,-40	,62	,
-124	,-118	,199	,304	,72	,41	,305	,293	,
-71	,-170	,66	,0	,-396	,-419	,-51	,-29	,
-342	,-210	,309	,358	,7	,139	,621	,487	,
-95	,-83	,314	,0	,-721	,-623	,-41	,-275	,
-885	,-434	,533	,396	,-221	,442	,1554	,1038	,
-147	,374	,1382	,0	,-2222	,-1566	,80	,-2392	,
-6595	,-3744	,6626	,12697	,6626	,-3744	,-6595	,-2392	,
80	,-1566	,-2222	,0	,1382	,374	,-147	,1038,
1554	, 442	,-221	,396	,533	,-434	,-885	,-275	,
-41	,-623	,-721	,0	,314	,-83	,-95	,487	,
621	,139	,7	,358	,309	,-210	,-342	,-29	,
-51	,-419	,-396	,0	,66	,-170	,-71	,293	,
305	,41	,72	,304	,199	,-118	,-124	,62	,
-40	,-295	,-243	,-9	,-18	,-153	,-54	,137	,
105	,29	,169	,257	,18	,-188	,36	,316	,
63	,-514	,-665	,-214	,92	,-700	,379	};

int BPF_coef2_Q15[NWz]={-2932,	1880,	-1582,	-364,	2283,	2653,	907,	-421,	-76,	930,
1177,	542,	-65,	-13,	438,	659,	426,	58,		-29,	204,
439,	388,	114,	-69,	38,		284,	354,	143,	-114,	-121,
118,	289,	154,	-154,	-278,	-82,	177,	145,	-180,	-429,
-310,	13,		110,	-188,	-553,	-558,	-203,	47,		-166,	-638,	-810,
-471,	-48,	-112,	-655,	-1050,	-797,	-186,	-5,		-570,	-1258,
-1205,	-396,	190,	-295,	-1420,	-1810,	-775,	616,	507,	-1522,
-3375,	-2141,	2884,	8874,	11551,	8874,	2884,	-2141,	-3375,	-1522,
507,	616,	-775,	-1810,	-1420,	-295,	190,	-396,	-1205,	-1258,
-570,	-5,		-186,	-797,	-1050,	-655,	-112,	-48,	-471,	-810,	-638,
-166,	47,		-203,	-558,	-553,	-188,	110,	13,		-310,	-429,
-180,	145,	177,	-82,	-278,	-154,	154,	289,	118,	-121,
-114,	143,	354,	284,	38,		-69,	114,	388,	439,	204,
-29,	58,		426,	659,	438,	-13,	-65,	542,	1177,	930,
-76,	-421,	907,	2653,	2283,	-364,	-1582,	1880,	-2932};

void ssl_filtering(vadInst_t *vadInst, short *in, int *inBuf, short *out){
	int i, k;
	vadInst_t *inst = (vadInst_t *)vadInst;
	short *xin = in;
	short *xout = out;	
	int *Buf = inBuf;
	int *coef_Q15 = BPF_coef_Q15;

	// memcpy(&Buf[0], &Buf[NUM_FRAMES], sizeof(int)*(NWz-1));

	for (i=0, k=NUM_FRAMES; i<(NWz-1); i++){
		Buf[i]=Buf[k++];
	}

	for (i=0, k=NWz-1; i<NUM_FRAMES; i++){
		Buf[k++]=(int)xin[i];
	}
	
	for (i=0; i<NUM_FRAMES; i++){
	    xout[i]=(short)filter_Q15(&Buf[i], coef_Q15, NWz);
	}
	
}

void ssl_vad_process(vadInst_t *vadInst, short *in, short *ssl_vad_p, short *min_vad_p, short *max_vad_p, int channel) {

	int i,m,n,k,avg_temp, e_curr, temp;
	
	vadInst_t *inst = (vadInst_t *)vadInst;

	short *xin = in;
	int *e_hist = &e_hist_Buf[channel][0];

	double x_slow_vad = g_x_slow_vad[channel];
	double t_chg = g_t_chg[channel];
	double t_vad = g_t_vad[channel];

	int n_floor = g_n_floor[channel];
	int state = g_state[channel];

	double t_chg_min_tmp = inst->t_chg_min;

	double tau_s;
	double temp_double;

	for (i=avgsize2-2; i>=0; i--){
		e_hist[i+1] = e_hist[i];
	}

	e_curr=0;
	for (k=0; k<ssl_blocksize; k++){
		e_curr = e_curr + ((((int)xin[k]*(int)xin[k])+2)>>2);
		if ((e_curr>(1<<30))||(e_curr<0)){
			e_curr=(1<<30);
		}
	}
	e_curr = (e_curr+4)>>3;

	e_hist[0]=e_curr;

#ifdef DEBUG_SSL_VAD
if (channel==0){
		debug_file_write_int(e_curr, 0);
}
#endif

#ifdef DEBUG_SSL_VAD_MATLAB
	if (((channel==0)&&(g_vad_debug_on==1))&&(g_vad_debug_snd_idx==0)){   
			debug_matlab_int(DEBUG_NUM_VAD, e_curr, 0);
	}
#endif

	int e_noise=0;
	for (i=0; i<avgsize2; i++){
		e_noise+=(e_hist[i]+2)>>2;
	}
	e_noise = (e_noise+8)>>4;
	

	// // temp_double=pow(10, ((double)(d_SNR+n_floor))/10)-pow(10, (double)n_floor/10);

	// temp_double=x_slow_vad*inst->d_SNR_ratio;
	// t_chg=t_chg*(1-0.125) +temp_double*0.125;

	// // temp_double=pow(10, ((double)(d_SNR_vad+n_floor))/10)-pow(10, (double)n_floor/10);		

	// temp_double=x_slow_vad*inst->d_SNR_vad_ratio;	
	// t_vad=t_vad*(1-0.125) +temp_double*0.125;


	if ((double)e_curr>x_slow_vad){
		tau_s=inst->tau_s_r;
		x_slow_vad=((double)e_curr*tau_s)+x_slow_vad*(1.0-tau_s);
	}			 
	else {
		tau_s=inst->tau_s_f;
		x_slow_vad=((double)e_curr*tau_s)+x_slow_vad*(0.9-tau_s);
	}
		
	// x_slow_vad=(double)e_curr*tau_s+x_slow_vad*(1.0-tau_s);
	
	if (x_slow_vad<(double)e_noise){
		x_slow_vad=(double)e_noise;
	}

	if (x_slow_vad>(double)e_noise*5.0){
		// e_noise=(int)(x_slow_vad*0.3);
		// e_noise=(int)(x_slow_vad*0.1);
		e_noise=(inst->n_floor)>>3;
	}

		

#ifdef DEBUG_SSL_VAD
	if (channel==0){
			debug_file_write_double(x_slow_vad, 2);		
	}
#endif
#ifdef DEBUG_SSL_VAD_MATLAB
	if (((channel==0)&&(g_vad_debug_on==1))&&(g_vad_debug_snd_idx==0)){   
		debug_matlab_float(DEBUG_NUM_VAD, (float)x_slow_vad, 2);
	}
#endif

	if (e_curr>e_noise+(int)t_chg){
		tau_s=0.001; //0.125; //0.01;
	}			 
	else {
		tau_s=0.125; //0.01;
	}

	// temp_double=pow(10, ((double)(d_SNR+n_floor))/10)-pow(10, (double)n_floor/10);

	temp_double=x_slow_vad*inst->d_SNR_ratio;
	t_chg=t_chg*(1.0-tau_s) +temp_double*tau_s;

	// temp_double=pow(10, ((double)(d_SNR_vad+n_floor))/10)-pow(10, (double)n_floor/10);		

	temp_double=x_slow_vad*inst->d_SNR_vad_ratio;	
	t_vad=t_vad*(1.0-tau_s) +temp_double*tau_s;

	if ((e_curr>e_noise+(int)t_chg) && (x_slow_vad>t_chg_min_tmp)) {
		if (state==0) {
			// memset(&e_hist[0],0, sizeof(int)*(avgsize2));

			for (i=0; i<avgsize2; i++){
				e_hist[i] = 0;
				// printf("e_hist[%d]=%d\n", i, e_hist[i]);
			}

			// printf("state 0 -> 1\n");

			e_noise=0;
			// printf("e_noise=%d\n", e_noise);
		}
		state=1;
		
		if (e_curr>e_noise+(int)t_vad)
			*ssl_vad_p=1;
		else
			*ssl_vad_p=0;
		
		// inst->n_floor=(int)(10*log10(x_slow_vad));	
	}
	else {
		if (state==1) {
			// memset(&e_hist[0],0, sizeof(int)*(avgsize2));
			for (i=0; i<avgsize2; i++){
				e_hist[i] = 0;
				// printf("e_hist[%d]=%d\n", i, e_hist[i]);
			}



			// printf("state 1 -> 0\n");

			// printf("e_noise=%d\n", e_noise);


		}			
		state=0;
		*ssl_vad_p=0;	 
		// inst->n_floor=(int)(10*log10(x_slow_vad));		 
	}

	// if (x_slow_vad<(double)e_noise){
	// 	x_slow_vad=(double)e_noise;
	// }

	for (i=0; i<inst->vad_hold_max-1; i++){
		vad_hist1[i]=vad_hist1[i+1];
		vad_hist2[i]=vad_hist2[i+1];
	}

	vad_hist1[inst->vad_hold_max-1]=*ssl_vad_p;

	vad_hist2[inst->vad_hold_max-1]=MIN( 1,  vad_hist1[inst->vad_hold_max-1]);
	for (i=0; i<(2-1); i++){
		vad_hist2[inst->vad_hold_max-1]=MIN(vad_hist2[inst->vad_hold_max-1], vad_hist1[inst->vad_hold_max-1-(i+1)]);
	}
	
	int count_pre=MAX(0,  vad_hist2[inst->vad_hold_max-1]);
	for (i=0; i<inst->vad_hold_max-1; i++){
		count_pre=MAX( count_pre, vad_hist2[inst->vad_hold_max-1-(i+1)]);
	}

	*max_vad_p = count_pre;

	int count_post=MAX(0,  vad_hist2[inst->vad_hold_max-1]);
	for (i=0; i<inst->vad_hold_min-1; i++){
		count_post=MAX( count_post, vad_hist2[inst->vad_hold_max-1-(i+1)]);
	}	

	*min_vad_p = count_post;

	if (((state==0)&&(*max_vad_p==0))&&(*min_vad_p == 0)){
		inst->n_floor=(int)(x_slow_vad);	
	}
	

	// if (*min_vad_p>0) {
	// 	// memset(&e_hist[0],0, sizeof(int)*(avgsize2));
	// 	for (i=0; i<avgsize2; i++){
	// 		e_hist[i] = 0;
	// 		// printf("e_hist[%d]=%d\n", i, e_hist[i]);
	// 	}
	// 	e_noise=0;
	// }

#ifdef DEBUG_SSL_VAD
	if (channel==0){
		debug_file_write_int(e_noise, 1);
		debug_file_write_int(n_floor, 3);
		debug_file_write_double(t_chg, 4);
		debug_file_write_double(t_vad, 5);
		debug_file_write_int(state, 6);
		debug_file_write_int(*ssl_vad_p, 7);
		debug_file_write_int(*min_vad_p, 8);
		debug_file_write_int(*max_vad_p, 9);
	}	
#endif	

#ifdef DEBUG_SSL_VAD_MATLAB
	if (((channel==0)&&(g_vad_debug_on==1))&&(g_vad_debug_snd_idx==0)){   
			debug_matlab_int(DEBUG_NUM_VAD, e_noise, 1);
			// debug_matlab_int(DEBUG_NUM_VAD, t_chg_min_tmp, 3);
			debug_matlab_float(DEBUG_NUM_VAD, (float)t_chg_min_tmp, 3);
			// debug_matlab_int(DEBUG_NUM_VAD, inst->n_floor, 3);
			debug_matlab_float(DEBUG_NUM_VAD, t_chg, 4);
			debug_matlab_float(DEBUG_NUM_VAD, t_vad, 5);
			debug_matlab_int(DEBUG_NUM_VAD, state, 6);
			debug_matlab_int(DEBUG_NUM_VAD, *ssl_vad_p, 7);
			debug_matlab_int(DEBUG_NUM_VAD, *min_vad_p, 8);
			debug_matlab_int(DEBUG_NUM_VAD, *max_vad_p, 9);	
	}
#endif

#ifdef DEBUG_SSL_VAD_MATLAB
	if (((channel==0)&&(g_vad_debug_on==1))&&(g_vad_debug_snd_idx==0)){   
		debug_matlab_send(DEBUG_NUM_VAD);
		// printf("e_curr = %d, e_noise=%d, inst->n_floor=%d, t_chg=%3.1f, t_vad==%3.1f, vad = %d, x_slow_vad=%f t_chg_min_tmp=%f\n", e_curr, e_noise, inst->n_floor, t_chg, t_vad, *ssl_vad_p, x_slow_vad, t_chg_min_tmp);
	}
	if ((channel==0)&&(g_vad_debug_on==1)){
		g_vad_debug_snd_idx++;
		if (g_vad_debug_snd_idx>=g_vad_debug_snd_period) {
			g_vad_debug_snd_idx = 0;
		}
	}
#endif

	g_x_slow_vad[channel] = x_slow_vad;
	g_t_chg[channel] = t_chg;
	g_t_vad[channel] = t_vad;

	g_n_floor[channel] = inst->n_floor;
	g_state[channel] = state;

    //  
}

