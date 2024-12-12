

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "sys.h"
#include "time_constant.h"
#include "audio_utils.h"
#include "level_mon.h"
#include "ssl_vad.h"

#define Mon_avgsize 64
#define Mon_blocksize 256

#define tau_s_r 0.00005
#define tau_s_f 0.6

int e_hist_Buf_Mon[2][Mon_avgsize];
double g_x_slow_Mon[2];
int x_peak_Mon=9000000;

short prev_state=0;

float in_scratch[Mon_blocksize];
float scratch_A[Mon_blocksize];
float scratch_BPF[NUM_FRAMES];

float in_scratch_after[Mon_blocksize];
float scratch_A_after[Mon_blocksize];	
float scratch_BPF_after[NUM_FRAMES];	

float noise_level_buf[1024];
float target_level_buf[1024];
float noise_bpf_level_buf[1024];
float target_bpf_level_buf[1024];

float noise_after_level_buf[1024];
float target_after_level_buf[1024];
float noise_bpf_after_level_buf[1024];
float target_bpf_after_level_buf[1024];


float LAeq_noise, LAeq_target, LAeq_noise_after, LAeq_target_after;
float LBPFeq_noise, LBPFeq_target, LBPFeq_noise_after, LBPFeq_target_after;
float LAeq_noise_dB, LAeq_target_dB, LAeq_noise_after_dB, LAeq_target_after_dB;
float LBPFeq_noise_dB, LBPFeq_target_dB, LBPFeq_noise_after_dB, LBPFeq_target_after_dB;
uint32_t cnt_noise = 0, cnt_target = 0;
uint32_t noise_calc_done = 0, target_calc_done = 0;


int32_t speech_BPF_Q15[sNWz]= {467,	691,	434,	-136,	-458,	-215,	234,	209,
						-468,	-1165,	-1122,	-414,	4,	-611,	-1779,	-2176,
						-1189,	95,	-127,	-2201,	-4044,	-2800,	2186,	8112,
						10755,	
						8112,	2186,	-2800,	-4044,	-2201,	-127,	95, -1189,
						-2176,	-1779,	-611,	4,	-414,	-1122,	-1165,	-468,
						209,	234,	-215,	-458,	-136,	434,	691,	467};

int32_t speech_BPF300_5k_Q15[sNWz]= {635,	624,	-196,	60,	511,	-377,	-534,	266,
									-483,	-1134,	-88,	-484,	-1702,	-555,	-331,	-2197,
									-1180,	83,	-2580,	-2175,	1160,	-2823,	-5179,	8120,	
									18938,
									8120,	-5179,	-2823,	1160,	-2175,	-2580,	83,	-1180,	
									-2197,	-331,	-555,	-1702,	-484,	-88,	-1134,	-483,	
									266,	-534,	-377,	511,	60,	-196,	624,	635};

int speech_BPF_Buf[2][sNWz+NUM_FRAMES-1];

MonInst_t Sys_MonInst;

MonInst_t * sysMonCreate()
{
	int i, j;
    Mon_Init(&Sys_MonInst);

	return &Sys_MonInst;
}

void Mon_Init(MonInst_t *MonInst){

    MonInst_t *inst = (MonInst_t *)MonInst;

    inst->blocksize=NUM_FRAMES;
    inst->tau_rise = 0.00005;
    inst->tau_fall = 0.6;

    inst->beta_p_r_num=10;
    inst->beta_p_f_num=9;
    inst->beta_p_r=beta16k[inst->beta_p_r_num];
    inst->beta_p_f=beta16k[inst->beta_p_f_num];
    inst->round_bit_p_r=round_bit16k[inst->beta_p_r_num]; //512; //(short)1<<((inst->beta_p_r)-1);
	inst->round_bit_p_f=round_bit16k[inst->beta_p_f_num]; //4096; //(short)1<<((inst->beta_f_f)-1);


    inst->noise_level = -1;
    inst->target_level = -1;
    inst->noise_bpf_level = -1;
    inst->target_bpf_level = -1;

	inst->noise_dB = -1; //FS_SPL + 10*log10f(inst->noise_level) - Q15_dB;
    inst->target_dB = -1; // FS_SPL + 10*log10f(inst->target_level) - Q15_dB;
	inst->noise_bpf_dB = -1; // FS_SPL + 10*log10f(inst->noise_bpf_level) - Q15_dB;
    inst->target_bpf_dB = -1; // FS_SPL + 10*log10f(inst->target_bpf_level) - Q15_dB;

    int ProcBufSize = Mon_blocksize * sizeof(float);
    audioEQ_allocate_bufs(ProcBufSize);

}

void Mon_DeInit(){
	audioEQ_free_bufs();
}

void Mon_filtering(MonInst_t *MonInst, short *in, int *inBuf, float *out){
	int i, k;
	MonInst_t *inst = (MonInst_t *)MonInst;
	short *xin = in;
	float *xout = out;	
	int *Buf = inBuf;
	int *coef_Q15 = speech_BPF300_5k_Q15; //speech_BPF_Q15;

	// memcpy(&Buf[0], &Buf[NUM_FRAMES], sizeof(int)*(NWz-1));

	for (i=0, k=Mon_blocksize; i<(sNWz-1); i++){
		Buf[i]=Buf[k++];
	}

	for (i=0, k=sNWz-1; i<Mon_blocksize; i++){
		Buf[k++]=(int)xin[i];
	}
	
	for (i=0; i<Mon_blocksize; i++){
	    xout[i]=Q15_TO_FLOAT(filter_Q15(&Buf[i], coef_Q15, sNWz));
	}
	
}

int get_SDNR_level(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	float target_only = inst->target_bpf_level - inst->noise_bpf_level;

	// printf("target_bpf_level = %f dB\n", FS_SPL +  10*log10f(inst->target_bpf_level) + 5);
	// printf("noise_bpf_level = %f dB\n", FS_SPL +  10*log10f(inst->noise_bpf_level) + 5);

	// printf("target_bpf_after_level = %f dB\n", FS_SPL +  10*log10f(inst->target_bpf_after_level) + 5);
	// printf("noise_bpf_after_level = %f dB\n", FS_SPL +  10*log10f(inst->noise_bpf_after_level) + 5);

	// printf("target_only = %f dB\n", FS_SPL +  10*log10f(target_only) + 5);

	float target_after_only = inst->target_bpf_after_level - inst->noise_bpf_after_level;

	// printf("dist target_only = %f dB\n",FS_SPL +  10*log10f(target_after_only) + 5);

	float dist_level = target_only - target_after_only;

	if (dist_level<0) dist_level = 0;

	// printf("dist_only = %f dB\n",FS_SPL +  10*log10f(dist_level) + 5);

	float SDNR = 10*log10f(dist_level/target_only);

	// printf("SDNR = %f dB\n", SDNR);

	return SDNR;
}

int get_noise_level(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->noise_dB;
}

int get_target_level(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->target_dB;
}

int get_noise_after_level(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->noise_after_dB;
}

int get_target_after_level(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->target_after_dB;
}

int get_noise_calc_percent(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->noise_calc_percent;
}

int get_target_calc_percent(MonInst_t *MonInst){
	MonInst_t *inst = (MonInst_t *)MonInst;

	return inst->target_calc_percent;
}


void reset_noise_levels(MonInst_t *MonInst){

	for(int i =0; i<1024; i++){
		noise_level_buf[i] = 0.0;
		noise_after_level_buf[i] = 0.0;
		noise_bpf_level_buf[i] = 0.0;
		noise_bpf_after_level_buf[i] = 0.0;		
	}
	LAeq_noise = 0;
	LAeq_noise_dB = -1;

	LAeq_noise_after = 0;
	LAeq_noise_after_dB = -1;

	LBPFeq_noise = 0;
	LBPFeq_noise_dB = -1;

	LBPFeq_noise_after = 0;
	LBPFeq_noise_after_dB = -1;	

	cnt_noise = 0;
	noise_calc_done = 0;

	MonInst_t *inst = (MonInst_t *)MonInst;
	inst->noise_level = -1;
	inst->noise_dB = -1;
	inst->noise_after_level -1;
	inst->noise_after_dB = -1;	
	inst->noise_bpf_level = -1;
	inst->noise_bpf_dB = -1;
	inst->noise_bpf_after_level -1;
	inst->noise_bpf_after_dB = -1;	

	inst->noise_calc_percent = 0;
}

void reset_target_levels(MonInst_t *MonInst){

	for(int i =0; i<1024; i++){
		target_level_buf[i] = 0.0;
		target_after_level_buf[i] = 0.0;
		target_bpf_level_buf[i] = 0.0;
		target_bpf_after_level_buf[i] = 0.0;		
	}
	LAeq_target = 0;
	LAeq_target_dB = -1;

	LAeq_target_after = 0;
	LAeq_target_after_dB = -1;

	LBPFeq_target = 0;
	LBPFeq_target_dB = -1;

	LBPFeq_target_after = 0;
	LBPFeq_target_after_dB = -1;	

	cnt_target = 0;
	target_calc_done = 0;

	MonInst_t *inst = (MonInst_t *)MonInst;
	inst->target_level = -1;
	inst->target_dB = -1;
	inst->target_after_level = -1;
	inst->target_after_dB = -1;		

	inst->target_bpf_level = -1;
	inst->target_bpf_dB = -1;
	inst->target_bpf_after_level = -1;
	inst->target_bpf_after_dB = -1;		

	inst->target_calc_percent = 0;		

}

float calc_Leq(float *Leq, void *calc_buf, float *rx_buf, uint32_t *buf_cnt, uint32_t input_len);

float calc_Leq(float *Leq, void *calc_buf, float *rx_buf, uint32_t *buf_cnt, uint32_t input_len)
{
    int i, j;
    float temp, LeqdB;
    float *buf=calc_buf;
	uint32_t cnt = *buf_cnt;


    temp=0;
    for (i = 0; i<input_len; i++){
        temp+=rx_buf[i]*rx_buf[i];
    }

    buf[cnt] = temp/((float)input_len);

    temp=0;
    for (i = 0; i<=cnt; i++){
        temp+=buf[i];
    }

    *Leq = temp/((float)cnt+1);
	LeqdB = FS_SPL + 10*log10f(*Leq) + 5;

    return LeqdB;
}



void Mon_process_LEQ(MonInst_t *MonInst, short *in, short *in_after){

	MonInst_t *inst = (MonInst_t *)MonInst;
    int i;

	for(i=0; i<Mon_blocksize; i++){
		in_scratch[i] = Q15_TO_FLOAT(in[i]); //(float) txOutFrame1[i] * (float)cali_factor_mic;
		in_scratch_after[i] = Q15_TO_FLOAT(in_after[i]); //(float) txOutFrame1[i] * (float)cali_factor_mic;
	}

	audioEQ_process_samples_float((void *)&scratch_A[0], (void *)&in_scratch[0], Mon_blocksize, 3, 0); 
	audioEQ_process_samples_float((void *)&scratch_A_after[0], (void *)&in_scratch_after[0], Mon_blocksize, 3, 1);
}

void Mon_process_BPF(MonInst_t *MonInst, short *in, short *in_after){

	Mon_filtering(MonInst, in, &speech_BPF_Buf[0][0], &scratch_BPF[0]);
    Mon_filtering(MonInst, in_after, &speech_BPF_Buf[1][0], &scratch_BPF_after[0]);

	// for(int i=0; i<Mon_blocksize; i++){
	// 	in[i] = FLOAT_TO_Q15(scratch_BPF[i]);
	// 	in_after[i] = FLOAT_TO_Q15(scratch_BPF_after[i]);
	// }	
}

int Mon_process_noise(MonInst_t *MonInst, short vad_short, short vad_long, int cnt_done){

	MonInst_t *inst = (MonInst_t *)MonInst;
    int i;

	if (vad_long == 0){
		LAeq_noise_dB = calc_Leq(&LAeq_noise, noise_level_buf, scratch_A, &cnt_noise, Mon_blocksize);
		LAeq_noise_after_dB = calc_Leq(&LAeq_noise_after, noise_after_level_buf, scratch_A_after, &cnt_noise, Mon_blocksize);

		LBPFeq_noise_dB = calc_Leq(&LBPFeq_noise, noise_bpf_level_buf, scratch_BPF, &cnt_noise, Mon_blocksize);
		LBPFeq_noise_after_dB = calc_Leq(&LBPFeq_noise_after, noise_bpf_after_level_buf, scratch_BPF_after, &cnt_noise, Mon_blocksize);
		
		cnt_noise++;
		if (cnt_noise>=1024){
			cnt_noise=0;
			noise_calc_done=1;
		}

		if (cnt_noise>=cnt_done){
			noise_calc_done=1;
		}

		if (noise_calc_done==1){
			inst->noise_level = LAeq_noise;
			inst->noise_dB = LAeq_noise_dB;
			inst->noise_after_level = LAeq_noise_after;
			inst->noise_after_dB = LAeq_noise_after_dB;

			inst->noise_bpf_level = LBPFeq_noise;
			inst->noise_bpf_dB = LBPFeq_noise_dB;
			inst->noise_bpf_after_level = LBPFeq_noise_after;
			inst->noise_bpf_after_dB = LBPFeq_noise_after_dB;

		}  else {
			inst->noise_level = -1;
			inst->noise_dB = -1;
			inst->noise_after_level -1;
			inst->noise_after_dB = -1;	

			inst->noise_bpf_level = -1;
			inst->noise_bpf_dB = -1;
			inst->noise_bpf_after_level = -1;
			inst->noise_bpf_after_dB = -1;			
		}
	} 

	inst->noise_calc_percent=(int)(((float)cnt_noise/(float)(cnt_done))*100.0);

	return inst->noise_calc_percent;	
}

int Mon_process_target(MonInst_t *MonInst, short vad_short, short vad_long, int cnt_done){

	MonInst_t *inst = (MonInst_t *)MonInst;
    int i;

if (vad_short == 1){
		LAeq_target_dB = calc_Leq(&LAeq_target, target_level_buf, scratch_A, &cnt_target, Mon_blocksize);
		LAeq_target_after_dB = calc_Leq(&LAeq_target_after, target_after_level_buf, scratch_A_after, &cnt_target, Mon_blocksize);

		LBPFeq_target_dB = calc_Leq(&LBPFeq_target, target_bpf_level_buf, scratch_BPF, &cnt_target, Mon_blocksize);
		LBPFeq_target_after_dB = calc_Leq(&LBPFeq_target_after, target_bpf_after_level_buf, scratch_BPF_after, &cnt_target, Mon_blocksize);

		cnt_target++;
		if (cnt_target>=1024){
			cnt_target=0;
			target_calc_done=1;
		}

		if (cnt_target>=cnt_done){
			target_calc_done=1;
		}

		if (target_calc_done==1){
			inst->target_level = LAeq_target;
			inst->target_dB = LAeq_target_dB;
			inst->target_after_level = LAeq_target_after;
			inst->target_after_dB = LAeq_target_after_dB;

			inst->target_bpf_level = LBPFeq_target;
			inst->target_bpf_dB = LBPFeq_target_dB;
			inst->target_bpf_after_level = LBPFeq_target_after;
			inst->target_bpf_after_dB = LBPFeq_target_after_dB;

		} else {
			inst->target_level = -1;
			inst->target_dB = -1;
			inst->target_after_level = -1;
			inst->target_after_dB = -1;	

			inst->target_bpf_level = -1;
			inst->target_bpf_dB = -1;
			inst->target_bpf_after_level = -1;
			inst->target_bpf_after_dB = -1;		
		}
	}

	inst->target_calc_percent=(int)(((float)cnt_target/(float)(cnt_done))*100.0);

	return inst->target_calc_percent;
}

