/*
 * ssl_core.c
 *
 *	Created on: 2019. 7. 3.
 *		Author: Seong Pil Moon
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

// #include "fftw3.h"
#include "pffft.h"

#include "sys.h"
#include "ssl_vad.h"
#include "ssl_core.h"

#include "debug_file.h"

#include "DoA_4m.h"

// #define PRINT_DEBUG_SSL_INPUT
// #define PRINT_DEBUG_SSL
// #define PRINT_DEBUG_SSL2
#define SEND_DEBUG_DOA
// #define SEND_DEBUG_CCVF
// #define PRINT_DEBUG_NOISEFLOOR
// #define PRINT_DEBUG_PEAKMAX

#define ccv_avg_size 32
#define ccv_stored_min_def 2
#define ccv_stored_min2_def 2

#define DoA_min -90
#define DoA_max 90

#define Cth 0.004

#define max_DoA_local 90
#define max_DoA_local_wall 80

#define E_avr_max 60.0


int hanning_w[ssl_blocksize]=
	{5	, 20	,44 ,78 ,122	,176	,239	,312	,
	395 ,487	,589	,700	,821	,950	,1089	,1238	,
	1395	,1561	,1736	,1920	,2112	,2313	,2523	,2740	,
	2966	,3200	,3442	,3691	,3948	,4213	,4484	,4763	,
	5049	,5342	,5641	,5946	,6258	,6576	,6900	,7229	,
	7564	,7904	,8250	,8600	,8954	,9314	,9677	,10044	,
	10416	,10790	,11169	,11550	,11934	,12321	,12710	,13101	,
	13495	,13890	,14286	,14684	,15083	,15483	,15883	,16283	,
	16684	,17084	,17484	,17883	,18282	,18679	,19075	,19469	,
	19862	,20252	,20640	,21025	,21408	,21788	,22164	,22537	,
	22907	,23272	,23633	,23990	,24343	,24691	,25033	,25371	,
	25703	,26030	,26351	,26665	,26974	,27277	,27572	,27862	,
	28144	,28419	,28688	,28948	,29202	,29447	,29685	,29915	,
	30137	,30350	,30555	,30752	,30940	,31120	,31290	,31452	,
	31605	,31748	,31883	,32008	,32124	,32230	,32327	,32415	,
	32492	,32561	,32619	,32668	,32707	,32736	,32756	,32766	,
	32766	,32756	,32736	,32707	,32668	,32619	,32561	,32492	,
	32415	,32327	,32230	,32124	,32008	,31883	,31748	,31605	,
	31452	,31290	,31120	,30940	,30752	,30555	,30350	,30137	,
	29915	,29685	,29447	,29202	,28948	,28688	,28419	,28144	,
	27862	,27572	,27277	,26974	,26665	,26351	,26030	,25703	,
	25371	,25033	,24691	,24343	,23990	,23633	,23272	,22907	,
	22537	,22164	,21788	,21408	,21025	,20640	,20252	,19862	,
	19469	,19075	,18679	,18282	,17883	,17484	,17084	,16684	,
	16283	,15883	,15483	,15083	,14684	,14286	,13890	,13495	,
	13101	,12710	,12321	,11934	,11550	,11169	,10790	,10416	,
	10044	,9677	,9314	,8954	,8600	,8250	,7904	,7564	,
	7229	,6900	,6576	,6258	,5946	,5641	,5342	,5049	,
	4763	,4484	,4213	,3948	,3691	,3442	,3200	,2966	,
	2740	,2523	,2313	,2112	,1920	,1736	,1561	,1395	,
	1238	,1089	,950	,821	,700	,589	,487	,395	,
	312 ,239	,176	,122	,78 ,44 ,20 ,5	};

int hanning_w512[ssl_blocksize512] = {
    1, 5, 11, 20, 31, 44, 60, 79, 99, 123, 148, 177, 207, 240, 276, 314, 
    354, 397, 442, 489, 539, 591, 646, 703, 762, 824, 888, 954, 1023, 1094, 1167, 1242, 
    1320, 1400, 1482, 1567, 1654, 1743, 1834, 1927, 2023, 2120, 2220, 2322, 2426, 2532, 2640, 2751, 
    2863, 2977, 3094, 3212, 3332, 3455, 3579, 3705, 3833, 3963, 4095, 4228, 4364, 4501, 4640, 4781, 
    4923, 5068, 5214, 5361, 5511, 5661, 5814, 5968, 6124, 6281, 6440, 6600, 6762, 6925, 7089, 7255, 
    7423, 7591, 7761, 7932, 8105, 8279, 8454, 8630, 8807, 8986, 9165, 9346, 9528, 9711, 9894, 10079, 
    10265, 10451, 10639, 10827, 11016, 11206, 11397, 11589, 11781, 11974, 12167, 12362, 12556, 12752, 12948, 13144, 
    13341, 13539, 13736, 13935, 14133, 14332, 14531, 14731, 14931, 15131, 15331, 15531, 15732, 15932, 16133, 16333, 
    16534, 16735, 16935, 17136, 17336, 17536, 17736, 17936, 18136, 18335, 18534, 18733, 18932, 19130, 19327, 19524, 
    19721, 19917, 20113, 20308, 20503, 20697, 20890, 21082, 21274, 21465, 21656, 21845, 22034, 22222, 22409, 22595, 
    22780, 22965, 23148, 23330, 23511, 23692, 23871, 24048, 24225, 24401, 24575, 24748, 24920, 25091, 25260, 25428, 
    25595, 25760, 25924, 26086, 26247, 26407, 26565, 26721, 26876, 27029, 27181, 27331, 27480, 27627, 27772, 27915, 
    28057, 28197, 28335, 28471, 28606, 28738, 28869, 28998, 29125, 29251, 29374, 29495, 29614, 29732, 29847, 29960, 
    30072, 30181, 30288, 30393, 30496, 30597, 30696, 30792, 30887, 30979, 31069, 31157, 31243, 31326, 31407, 31486, 
    31563, 31637, 31709, 31779, 31846, 31912, 31974, 32035, 32093, 32149, 32202, 32253, 32302, 32348, 32392, 32434, 
    32473, 32509, 32544, 32575, 32605, 32632, 32656, 32678, 32698, 32715, 32730, 32742, 32752, 32759, 32764, 32767, 
    32767, 32764, 32759, 32752, 32742, 32730, 32715, 32698, 32678, 32656, 32632, 32605, 32575, 32544, 32509, 32473, 
    32434, 32392, 32348, 32302, 32253, 32202, 32149, 32093, 32035, 31974, 31912, 31846, 31779, 31709, 31637, 31563, 
    31486, 31407, 31326, 31243, 31157, 31069, 30979, 30887, 30792, 30696, 30597, 30496, 30393, 30288, 30181, 30072, 
    29960, 29847, 29732, 29614, 29495, 29374, 29251, 29125, 28998, 28869, 28738, 28606, 28471, 28335, 28197, 28057, 
    27915, 27772, 27627, 27480, 27331, 27181, 27029, 26876, 26721, 26565, 26407, 26247, 26086, 25924, 25760, 25595, 
    25428, 25260, 25091, 24920, 24748, 24575, 24401, 24225, 24048, 23871, 23692, 23511, 23330, 23148, 22965, 22780, 
    22595, 22409, 22222, 22034, 21845, 21656, 21465, 21274, 21082, 20890, 20697, 20503, 20308, 20113, 19917, 19721, 
    19524, 19327, 19130, 18932, 18733, 18534, 18335, 18136, 17936, 17736, 17536, 17336, 17136, 16935, 16735, 16534, 
    16333, 16133, 15932, 15732, 15531, 15331, 15131, 14931, 14731, 14531, 14332, 14133, 13935, 13736, 13539, 13341, 
    13144, 12948, 12752, 12556, 12362, 12167, 11974, 11781, 11589, 11397, 11206, 11016, 10827, 10639, 10451, 10265, 
    10079, 9894, 9711, 9528, 9346, 9165, 8986, 8807, 8630, 8454, 8279, 8105, 7932, 7761, 7591, 7423, 
    7255, 7089, 6925, 6762, 6600, 6440, 6281, 6124, 5968, 5814, 5661, 5511, 5361, 5214, 5068, 4923, 
    4781, 4640, 4501, 4364, 4228, 4095, 3963, 3833, 3705, 3579, 3455, 3332, 3212, 3094, 2977, 2863, 
    2751, 2640, 2532, 2426, 2322, 2220, 2120, 2023, 1927, 1834, 1743, 1654, 1567, 1482, 1400, 1320, 
    1242, 1167, 1094, 1023, 954, 888, 824, 762, 703, 646, 591, 539, 489, 442, 397, 354, 
    314, 276, 240, 207, 177, 148, 123, 99, 79, 60, 44, 31, 20, 11, 5, 1
};

int E_idx_wall[8][3]={{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1},
			          {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};

int E_idx[16][4]={{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0}, {0, 0, 1, 1},
			   {0, 1, 0, 0}, {0, 1, 0, 1}, {0, 1, 1, 0}, {0, 1, 1, 1},			   
			   {1, 0, 0, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}, {1, 0, 1, 1},
			   {1, 1, 0, 0}, {1, 1, 0, 1}, {1, 1, 1, 0}, {1, 1, 1, 1}};		

int E_idx64[64][6] = {
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 1},
    {0, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 0, 1}, {0, 0, 0, 1, 1, 0}, {0, 0, 0, 1, 1, 1},
    {0, 0, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1}, {0, 0, 1, 0, 1, 0}, {0, 0, 1, 0, 1, 1},
    {0, 0, 1, 1, 0, 0}, {0, 0, 1, 1, 0, 1}, {0, 0, 1, 1, 1, 0}, {0, 0, 1, 1, 1, 1},
    {0, 1, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 1}, {0, 1, 0, 0, 1, 0}, {0, 1, 0, 0, 1, 1},
    {0, 1, 0, 1, 0, 0}, {0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 0}, {0, 1, 0, 1, 1, 1},
    {0, 1, 1, 0, 0, 0}, {0, 1, 1, 0, 0, 1}, {0, 1, 1, 0, 1, 0}, {0, 1, 1, 0, 1, 1},
    {0, 1, 1, 1, 0, 0}, {0, 1, 1, 1, 0, 1}, {0, 1, 1, 1, 1, 0}, {0, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 1, 0}, {1, 0, 0, 0, 1, 1},
    {1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 1}, {1, 0, 0, 1, 1, 0}, {1, 0, 0, 1, 1, 1},
    {1, 0, 1, 0, 0, 0}, {1, 0, 1, 0, 0, 1}, {1, 0, 1, 0, 1, 0}, {1, 0, 1, 0, 1, 1},
    {1, 0, 1, 1, 0, 0}, {1, 0, 1, 1, 0, 1}, {1, 0, 1, 1, 1, 0}, {1, 0, 1, 1, 1, 1},
    {1, 1, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 1}, {1, 1, 0, 0, 1, 0}, {1, 1, 0, 0, 1, 1},
    {1, 1, 0, 1, 0, 0}, {1, 1, 0, 1, 0, 1}, {1, 1, 0, 1, 1, 0}, {1, 1, 0, 1, 1, 1},
    {1, 1, 1, 0, 0, 0}, {1, 1, 1, 0, 0, 1}, {1, 1, 1, 0, 1, 0}, {1, 1, 1, 0, 1, 1},
    {1, 1, 1, 1, 0, 0}, {1, 1, 1, 1, 0, 1}, {1, 1, 1, 1, 1, 0}, {1, 1, 1, 1, 1, 1}
};

int X_L[ssl_blocksize];
int X_R[ssl_blocksize];
int X_F[ssl_blocksize];
int X_B[ssl_blocksize];
int X_C[ssl_blocksize];

int X_L512[ssl_blocksize512];
int X_R512[ssl_blocksize512];
int X_F512[ssl_blocksize512];
int X_B512[ssl_blocksize512];

// fftwf_complex *sslFFTinput_L, *sslFFTinput_R, *sslFFTinput_F, *sslFFT_L, *sslFFT_R, *sslFFT_F;
// fftwf_complex *CCVF, *ccv_temp;

// fftwf_plan ssl_L_p,ssl_R_p,ssl_F_p, ssl_ccv_p;

PFFFT_Setup *ssl_L_p = NULL;
PFFFT_Setup *ssl_R_p = NULL;
PFFFT_Setup *ssl_F_p = NULL;
PFFFT_Setup *ssl_B_p = NULL;
PFFFT_Setup *ssl_C_p = NULL;

PFFFT_Setup *ssl_ccv_RL_p = NULL;
PFFFT_Setup *ssl_ccv_LF_p = NULL;
PFFFT_Setup *ssl_ccv_FR_p = NULL;
PFFFT_Setup *ssl_ccv_FB_p = NULL;

PFFFT_Setup *ssl_ccv_BR_p = NULL;
PFFFT_Setup *ssl_ccv_LB_p = NULL;

PFFFT_Setup *ssl_ccv_LR_p = NULL;
PFFFT_Setup *ssl_ccv_LC_p = NULL;
PFFFT_Setup *ssl_ccv_CR_p = NULL;

float *sslFFTinput_L = NULL;
float *sslFFTinput_R = NULL;
float *sslFFTinput_F = NULL;
float *sslFFTinput_B = NULL;
float *sslFFTinput_C = NULL;

float *sslFFT_L = NULL;
float *sslFFT_R = NULL;
float *sslFFT_F = NULL;
float *sslFFT_B = NULL;
float *sslFFT_C = NULL;

float *pffftsslwork = NULL;

float *CCVF = NULL;
float *ccv_temp = NULL;
float *pffftccvwork = NULL;

int fbin_min;
int fbin_max;

int fsup_ssl_blocksize;

double ccv_RL[ccv_avg_size][ITD_size2];
double ccv_LF[ccv_avg_size][ITD_size];
double ccv_FR[ccv_avg_size][ITD_size];
double ccv_FB[ccv_avg_size][ITD_size2];

double ccv_BR[ccv_avg_size][ITD_size];
double ccv_LB[ccv_avg_size][ITD_size];

double ccv_LR[ccv_avg_size][ITD_size2];
double ccv_LC[ccv_avg_size][ITD_size3];
double ccv_CR[ccv_avg_size][ITD_size3];

double peaktemp[ccv_avg_size];
double peaklevel[ccv_avg_size];


#define CLUSTER_LENG 32

double cluster[5][CLUSTER_LENG];
double cluster_power[5][CLUSTER_LENG];
int16_t cluster_idx[5] = {0, 0, 0, 0, 0};
int16_t cluster_ccvidx[5][CLUSTER_LENG];

float deno;
float deno512;
double deno2;


sslInst_t Sys_sslInst;

sslInst_t* sysSSLCORECreate()
{
    // printf("ssl_core_Init in\n");
    // printf("sslFFTinput_L = %p\n", sslFFTinput_L);
    // printf("sslFFTinput_R = %p\n", sslFFTinput_R);
    // printf("sslFFTinput_F = %p\n", sslFFTinput_F);
    // printf("sslFFTinput_B = %p\n", sslFFTinput_B);
    // printf("sslFFT_L = %p\n", sslFFT_L);
	// printf("sslFFT_R = %p\n", sslFFT_R);
	// printf("sslFFT_F = %p\n", sslFFT_F);
	// printf("sslFFT_B = %p\n", sslFFT_B);

    // printf("pffftsslwork = %p\n", pffftsslwork);

    // // PFFFT benchmark
    // printf("ssl_L_p = %p\n", ssl_L_p);
	// printf("ssl_R_p = %p\n", ssl_R_p);
	// printf("ssl_F_p = %p\n", ssl_F_p);
	// printf("ssl_B_p = %p\n", ssl_B_p);


    // printf("CCVF = %p\n", CCVF);
    // printf("ccv_temp = %p\n", ccv_temp);

	// printf("pffftccvwork = %p\n", pffftccvwork);

	// printf("ssl_ccv_RL_p = %p\n", ssl_ccv_RL_p);
	// printf("ssl_ccv_LF_p = %p\n", ssl_ccv_LF_p);
	// printf("ssl_ccv_FR_p = %p\n", ssl_ccv_FR_p);
	// printf("ssl_ccv_FB_p = %p\n", ssl_ccv_FB_p);

    ssl_core_Init(&Sys_sslInst);
	// ssl_core_Init_wall(&Sys_sslInst);
	// ssl_core_Init512(&Sys_sslInst);

    // printf("ssl_core_Init out\n");
    // printf("sslFFTinput_L = %p\n", sslFFTinput_L);
    // printf("sslFFTinput_R = %p\n", sslFFTinput_R);
    // printf("sslFFTinput_F = %p\n", sslFFTinput_F);
    // printf("sslFFTinput_B = %p\n", sslFFTinput_B);
    // printf("sslFFT_L = %p\n", sslFFT_L);
	// printf("sslFFT_R = %p\n", sslFFT_R);
	// printf("sslFFT_F = %p\n", sslFFT_F);
	// printf("sslFFT_B = %p\n", sslFFT_B);

    // printf("pffftsslwork = %p\n", pffftsslwork);

    // // PFFFT benchmark
    // printf("ssl_L_p = %p\n", ssl_L_p);
	// printf("ssl_R_p = %p\n", ssl_R_p);
	// printf("ssl_F_p = %p\n", ssl_F_p);
	// printf("ssl_B_p = %p\n", ssl_B_p);


    // printf("CCVF = %p\n", CCVF);
    // printf("ccv_temp = %p\n", ccv_temp);

	// printf("pffftccvwork = %p\n", pffftccvwork);

	// printf("ssl_ccv_RL_p = %p\n", ssl_ccv_RL_p);
	// printf("ssl_ccv_LF_p = %p\n", ssl_ccv_LF_p);
	// printf("ssl_ccv_FR_p = %p\n", ssl_ccv_FR_p);
	// printf("ssl_ccv_FB_p = %p\n", ssl_ccv_FB_p);

	return &Sys_sslInst;
}

sslInst_t* sysSSLCORECreate_wall()
{

    // ssl_core_Init(&Sys_sslInst);
	ssl_core_Init_wall(&Sys_sslInst);
	// ssl_core_Init512(&Sys_sslInst);

	return &Sys_sslInst;
}

int ITD_local_min;
int ITD_local_max;
int ITD2_local_min;
int ITD2_local_max;
int ITD3_local_min;
int ITD3_local_max;


void ssl_core_Init(sslInst_t *sslInst){

	int n, m;

	double fmin=50;
	double fmax=5000;
	double fs=16000;

	sslInst->fbin_min=(int)((double)ssl_blocksize*fmin/fs);
	sslInst->fbin_max=(int)((double)ssl_blocksize*fmax/fs+0.5);

	// float thresold_dB = 30.0;
	// float thresold_f = powf(10.0f, thresold_dB / 10.0f);
	// sslInst->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
	// sslInst->gun_trig_thresold=-1;
	sslInst->gun_trig_thresold=-3;
	sslInst->gun_trig_thresold2=30;	

	fsup_ssl_blocksize=ssl_blocksize*16;

	sslInst->CCVth = 0.000200;

	sslInst->ccv_stored_min = 1;

	sslInst->ccv_last_num = 2;

	sslInst->gate_dB = -130.0;

	deno=(1.0/((float)((int)ssl_blocksize*(int)32768)));	
	deno2=(1.0/((double)fsup_ssl_blocksize));	
	
    int cplx = 0;
    int N = ssl_blocksize;
    int Nfloat = (cplx ? N*2 : N);
    int Nbytes = Nfloat * sizeof(float);
    sslFFTinput_L = pffft_aligned_malloc(Nbytes);
    sslFFTinput_R = pffft_aligned_malloc(Nbytes);
	sslFFTinput_F = pffft_aligned_malloc(Nbytes);
	sslFFTinput_B = pffft_aligned_malloc(Nbytes);
    sslFFT_L = pffft_aligned_malloc(Nbytes);
	sslFFT_R = pffft_aligned_malloc(Nbytes);
	sslFFT_F = pffft_aligned_malloc(Nbytes);
	sslFFT_B = pffft_aligned_malloc(Nbytes);

    pffftsslwork = pffft_aligned_malloc(Nbytes);

    // PFFFT benchmark
    ssl_L_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_R_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_F_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_B_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);


    cplx = 1;
    N = fsup_ssl_blocksize;
    Nfloat = (cplx ? N*2 : N);
    Nbytes = Nfloat * sizeof(float);
    CCVF = pffft_aligned_malloc(Nbytes);
    ccv_temp = pffft_aligned_malloc(Nbytes);

	pffftccvwork = pffft_aligned_malloc(Nbytes);

	ssl_ccv_RL_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_LF_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_FR_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_FB_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);

	ssl_ccv_BR_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_LB_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);

	// CCVF = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fsup_ssl_blocksize);	
	// ccv_temp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fsup_ssl_blocksize);	

	for (m=0; m<ITD_size; m++){
		if (DoA_range[m]<-1*max_DoA_local){
			ITD_local_min=m+1;
		} else if (DoA_range[m]>max_DoA_local){
			break;
		} else {
			ITD_local_max=m;
		}
	}

	for (m=0; m<ITD_size2; m++){
		if (DoA_range2[m]<-1*max_DoA_local){
			ITD2_local_min=m+1;
		} else if (DoA_range2[m]>max_DoA_local){
			break;
		} else {
			ITD2_local_max=m;
		}
	}

	printf("ITD_local_min=%d, %.2f ", ITD_local_min, DoA_range[ITD_local_min]);
	printf("ITD_local_max=%d, %.2f \r\n", ITD_local_max, DoA_range[ITD_local_max]);
	printf("ITD2_local_min=%d, %.2f ", ITD2_local_min, DoA_range2[ITD2_local_min]);
	printf("ITD2_local_max=%d, %.2f \r\n\n", ITD2_local_max, DoA_range2[ITD2_local_max]);	

	for (n = 0; n<fsup_ssl_blocksize; n++) {
		CCVF[2*n] = 0.0;
		CCVF[2*n+1] = 0.0;	
		ccv_temp[2*n] = 0.0;
		ccv_temp[2*n+1] = 0.0;
	}	

	for (n=0; n<ccv_avg_size; n++){
		for (m=0; m<ITD_size2; m++){
			ccv_RL[n][m]=0.0;
			ccv_FB[n][m]=0.0;
		}
	}

	for (n=0; n<ccv_avg_size; n++){
		for (m=0; m<ITD_size; m++){
			// ccv_RL[n][m]=0.0;			
			ccv_LF[n][m]=0.0;
			ccv_FR[n][m]=0.0;
		}
	}

}

void ssl_core_Init_wall(sslInst_t *sslInst){

	int n, m;

	double fmin=200;
	double fmax=1500;
	double fs=16000;

	sslInst->fbin_min=(int)((double)ssl_blocksize*fmin/fs);
	sslInst->fbin_max=(int)((double)ssl_blocksize*fmax/fs+0.5);

	// float thresold_dB = 30.0;
	// float thresold_dB2 = 30.0;
	// float thresold_f = powf(10.0f, thresold_dB / 10.0f);
	// sslInst->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
	sslInst->gun_trig_thresold=-3;
	sslInst->gun_trig_thresold2=20;

	fsup_ssl_blocksize=ssl_blocksize*16;

	sslInst->CCVth = 0.00005;

	sslInst->ccv_stored_min = 1;

	sslInst->ccv_last_num = 1;

	deno=(1.0/((float)((int)ssl_blocksize*(int)32768)));	
	deno2=(1.0/((double)fsup_ssl_blocksize));	
	
    int cplx = 0;
    int N = ssl_blocksize;
    int Nfloat = (cplx ? N*2 : N);
    int Nbytes = Nfloat * sizeof(float);
    sslFFTinput_L = pffft_aligned_malloc(Nbytes);
    sslFFTinput_R = pffft_aligned_malloc(Nbytes);
	sslFFTinput_C = pffft_aligned_malloc(Nbytes);
	
    sslFFT_L = pffft_aligned_malloc(Nbytes);
	sslFFT_R = pffft_aligned_malloc(Nbytes);
	sslFFT_C = pffft_aligned_malloc(Nbytes);

    pffftsslwork = pffft_aligned_malloc(Nbytes);

    // PFFFT benchmark
    ssl_L_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_R_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_C_p = pffft_new_setup(ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);


    cplx = 1;
    N = fsup_ssl_blocksize;
    Nfloat = (cplx ? N*2 : N);
    Nbytes = Nfloat * sizeof(float);
    CCVF = pffft_aligned_malloc(Nbytes);
    ccv_temp = pffft_aligned_malloc(Nbytes);

	pffftccvwork = pffft_aligned_malloc(Nbytes);

	ssl_ccv_LR_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_LC_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	ssl_ccv_CR_p = pffft_new_setup(fsup_ssl_blocksize, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
	
	// CCVF = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fsup_ssl_blocksize);	
	// ccv_temp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fsup_ssl_blocksize);	

	for (m=0; m<ITD_size3; m++){
		if (DoA_range3[m]<-1*max_DoA_local){
			ITD3_local_min=m+1;
		} else if (DoA_range3[m]>max_DoA_local){
			break;
		} else {
			ITD3_local_max=m;
		}
	}

	for (m=0; m<ITD_size2; m++){
		if (DoA_range2[m]<-1*max_DoA_local){
			ITD2_local_min=m+1;
		} else if (DoA_range2[m]>max_DoA_local){
			break;
		} else {
			ITD2_local_max=m;
		}
	}

	printf("ITD3_local_min=%d, %.2f ", ITD3_local_min, DoA_range3[ITD3_local_min]);
	printf("ITD3_local_max=%d, %.2f \r\n", ITD3_local_max, DoA_range3[ITD3_local_max]);
	printf("ITD2_local_min=%d, %.2f ", ITD2_local_min, DoA_range2[ITD2_local_min]);
	printf("ITD2_local_max=%d, %.2f \r\n\n", ITD2_local_max, DoA_range2[ITD2_local_max]);	

	for (n = 0; n<fsup_ssl_blocksize; n++) {
		CCVF[2*n] = 0.0;
		CCVF[2*n+1] = 0.0;	
		ccv_temp[2*n] = 0.0;
		ccv_temp[2*n+1] = 0.0;
	}	

	for (n=0; n<ccv_avg_size; n++){
		for (m=0; m<ITD_size2; m++){
			ccv_LR[n][m]=0.0;
		}
	}

	for (n=0; n<ccv_avg_size; n++){
		for (m=0; m<ITD_size3; m++){
			ccv_LC[n][m]=0.0;
			ccv_CR[n][m]=0.0;
		}
	}

}

void free_pfft_alined(float **X);
void destroy_pfft_setup(PFFFT_Setup **s);

void ssl_core_DeInit(){

    // printf("ssl_core_DeInit in\n");
    // printf("sslFFTinput_L = %p\n", sslFFTinput_L);
    // printf("sslFFTinput_R = %p\n", sslFFTinput_R);
    // printf("sslFFTinput_F = %p\n", sslFFTinput_F);
    // printf("sslFFTinput_B = %p\n", sslFFTinput_B);
    // printf("sslFFT_L = %p\n", sslFFT_L);
	// printf("sslFFT_R = %p\n", sslFFT_R);
	// printf("sslFFT_F = %p\n", sslFFT_F);
	// printf("sslFFT_B = %p\n", sslFFT_B);

    // printf("pffftsslwork = %p\n", pffftsslwork);

    // // PFFFT benchmark
    // printf("ssl_L_p = %p\n", ssl_L_p);
	// printf("ssl_R_p = %p\n", ssl_R_p);
	// printf("ssl_F_p = %p\n", ssl_F_p);
	// printf("ssl_B_p = %p\n", ssl_B_p);


    // printf("CCVF = %p\n", CCVF);
    // printf("ccv_temp = %p\n", ccv_temp);

	// printf("pffftccvwork = %p\n", pffftccvwork);

	// printf("ssl_ccv_RL_p = %p\n", ssl_ccv_RL_p);
	// printf("ssl_ccv_LF_p = %p\n", ssl_ccv_LF_p);
	// printf("ssl_ccv_FR_p = %p\n", ssl_ccv_FR_p);
	// printf("ssl_ccv_FB_p = %p\n", ssl_ccv_FB_p);

    free_pfft_alined(&sslFFTinput_L);
    free_pfft_alined(&sslFFTinput_R);
	free_pfft_alined(&sslFFTinput_F);
	free_pfft_alined(&sslFFTinput_B);
    free_pfft_alined(&sslFFT_L);
	free_pfft_alined(&sslFFT_R);
	free_pfft_alined(&sslFFT_F);
	free_pfft_alined(&sslFFT_B);

    free_pfft_alined(&pffftsslwork);

    // PFFFT benchmark
    destroy_pfft_setup(&ssl_L_p);
	destroy_pfft_setup(&ssl_R_p);
	destroy_pfft_setup(&ssl_F_p);
	destroy_pfft_setup(&ssl_B_p);


    free_pfft_alined(&CCVF);
    free_pfft_alined(&ccv_temp);

	free_pfft_alined(&pffftccvwork);

	destroy_pfft_setup(&ssl_ccv_RL_p);
	destroy_pfft_setup(&ssl_ccv_LF_p);
	destroy_pfft_setup(&ssl_ccv_FR_p);
	destroy_pfft_setup(&ssl_ccv_FB_p);

    // printf("ssl_core_DeInit out\n");
    // printf("sslFFTinput_L = %p\n", sslFFTinput_L);
    // printf("sslFFTinput_R = %p\n", sslFFTinput_R);
    // printf("sslFFTinput_F = %p\n", sslFFTinput_F);
    // printf("sslFFTinput_B = %p\n", sslFFTinput_B);
    // printf("sslFFT_L = %p\n", sslFFT_L);
	// printf("sslFFT_R = %p\n", sslFFT_R);
	// printf("sslFFT_F = %p\n", sslFFT_F);
	// printf("sslFFT_B = %p\n", sslFFT_B);

    // printf("pffftsslwork = %p\n", pffftsslwork);

    // // PFFFT benchmark
    // printf("ssl_L_p = %p\n", ssl_L_p);
	// printf("ssl_R_p = %p\n", ssl_R_p);
	// printf("ssl_F_p = %p\n", ssl_F_p);
	// printf("ssl_B_p = %p\n", ssl_B_p);


    // printf("CCVF = %p\n", CCVF);
    // printf("ccv_temp = %p\n", ccv_temp);

	// printf("pffftccvwork = %p\n", pffftccvwork);

	// printf("ssl_ccv_RL_p = %p\n", ssl_ccv_RL_p);
	// printf("ssl_ccv_LF_p = %p\n", ssl_ccv_LF_p);
	// printf("ssl_ccv_FR_p = %p\n", ssl_ccv_FR_p);
	// printf("ssl_ccv_FB_p = %p\n", ssl_ccv_FB_p);	
}

void destroy_pfft_setup(PFFFT_Setup **s){
    if (*s!=NULL) {
        pffft_destroy_setup(*s);
        *s=NULL;
    }
}

void free_pfft_alined(float **X){

    if (*X!=NULL) {
        pffft_aligned_free(*X);
        *X=NULL;
    }
}

static int ccv_idx=0;
static int ccv_stored=0;

static int prev_vad=0;

void ssl_core_process(sslInst_t *sslInst, short *in_L, short *in_R, short *in_F, short *in_B, short ssl_vad, short ssl_vad_long, double *DoA_mean_p) {
	
	int i, j, n, m, p, q, r, s;
	int idx_pick = -1;
	double temp_real, temp_imaj, temp_abs, DoA_temp, DoA_mean_temp;
	double ccv_thre_RL_mean=0.0;
	double ccv_thre_RL_max=0.0;
	double ccv_thre_LF_mean=0.0;
	double ccv_thre_LF_max=0.0;
	double ccv_thre_FR_mean=0.0;
	double ccv_thre_FR_max=0.0;	
	double ccv_thre_FB_mean=0.0;
	double ccv_thre_FB_max=0.0;		

	double ccv_RL_mean[ITD_size2];
	double ccv_LF_mean[ITD_size];
	double ccv_FR_mean[ITD_size];
	double ccv_FB_mean[ITD_size2];	
	
	short *x_L = in_L;
	short *x_R = in_R;
	short *x_F = in_F;
	short *x_B = in_B;

	int ccv_RL_max_idx=-1;
	int ccv_LF_max_idx=-1;
	int ccv_FR_max_idx=-1;
	int ccv_FB_max_idx=-1;

	double D_l_RL;
	double D_l_LF;
	double D_l_FR;
	double D_l_FB;

	double D_g_RL[2];
	double D_g_LF[2];
	double D_g_FR[2];	
	double D_g_FB[2];	


	double E[16];
	double E_min_temp=1000.0;

	*DoA_mean_p = -1.0;

	// for (i=0; i<16; i++) {
	// 	E[i] = E_min_temp;
	// }

	if (ssl_vad>0){

		/*-------------------- make FFTs --------------------*/

		for (i=0; i<ssl_blocksize; i++)
			X_L[i]=(int)x_L[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_R[i]=(int)x_R[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_F[i]=(int)x_F[i]*hanning_w[i];	
		for (i=0; i<ssl_blocksize; i++)
			X_B[i]=(int)x_B[i]*hanning_w[i];				

		
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_L[i] = ((float)X_L[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_R[i] = ((float)X_R[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_F[i] = ((float)X_F[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_B[i] = ((float)X_B[i])*deno;			

		pffft_transform_ordered(ssl_L_p, sslFFTinput_L, sslFFT_L, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_R_p, sslFFTinput_R, sslFFT_R, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_F_p, sslFFTinput_F, sslFFT_F, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_B_p, sslFFTinput_B, sslFFT_B, pffftsslwork, PFFFT_FORWARD);

        ccv_stored++;
        if (ccv_stored>ccv_avg_size)
            ccv_stored=ccv_avg_size;

		/*-------------------- make ccv_LF --------------------*/
		int Nbytes = 2 * fsup_ssl_blocksize * sizeof(float);
		memset(CCVF, 0, Nbytes);

		m=fsup_ssl_blocksize-sslInst->fbin_min;	
		for (i=sslInst->fbin_min; i<=sslInst->fbin_max; i++){
			temp_real=sslFFT_L[2*i]*sslFFT_F[2*i];
			temp_real=temp_real+(sslFFT_L[2*i+1]*sslFFT_F[2*i+1]);
			
			temp_imaj=sslFFT_L[2*i+1]*sslFFT_F[2*i];
			temp_imaj=temp_imaj-sslFFT_L[2*i]*sslFFT_F[2*i+1];

			temp_abs=temp_real*temp_real;
			temp_abs=temp_abs+(temp_imaj*temp_imaj);
			temp_abs=sqrt(temp_abs);
			CCVF[2*i]=temp_real/temp_abs;
			CCVF[2*i+1]=temp_imaj/temp_abs;		

			CCVF[2*m]=CCVF[2*i];
			CCVF[2*m+1]=-1*CCVF[2*i+1];	
			m--;				
		}

		pffft_transform_ordered(ssl_ccv_LF_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max; i++){			
			ccv_LF[ccv_idx][ITD_max+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			ccv_LF[ccv_idx][ITD_max+i]=sqrt(ccv_LF[ccv_idx][ITD_max+i])*deno2;
		}

		m=fsup_ssl_blocksize-ITD_max;
		for (i=0; i<ITD_max; i++){
			ccv_LF[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			ccv_LF[ccv_idx][i]=sqrt(ccv_LF[ccv_idx][i])*deno2;
			m++;
		}


		/*-------------------- make ccv_FR --------------------*/
		m=fsup_ssl_blocksize-sslInst->fbin_min;	
		for (i=sslInst->fbin_min; i<=sslInst->fbin_max; i++){
			temp_real=sslFFT_F[2*i]*sslFFT_R[2*i];
			temp_real=temp_real+(sslFFT_F[2*i+1]*sslFFT_R[2*i+1]);
			
			temp_imaj=sslFFT_F[2*i+1]*sslFFT_R[2*i];
			temp_imaj=temp_imaj-sslFFT_F[2*i]*sslFFT_R[2*i+1];

			temp_abs=temp_real*temp_real;
			temp_abs=temp_abs+(temp_imaj*temp_imaj);
			temp_abs=sqrt(temp_abs);
			CCVF[2*i]=temp_real/temp_abs;
			CCVF[2*i+1]=temp_imaj/temp_abs;		

			CCVF[2*m]=CCVF[2*i];
			CCVF[2*m+1]=-1*CCVF[2*i+1];		
			m--;			

		}

		pffft_transform_ordered(ssl_ccv_FR_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max; i++){
			ccv_FR[ccv_idx][ITD_max+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			ccv_FR[ccv_idx][ITD_max+i]=sqrt(ccv_FR[ccv_idx][ITD_max+i])*deno2;
		}

		m=fsup_ssl_blocksize-ITD_max;
		for (i=0; i<ITD_max; i++){
			ccv_FR[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			ccv_FR[ccv_idx][i]=sqrt(ccv_FR[ccv_idx][i])*deno2;
			m++;
		}



		/*-------------------- make ccv_RL --------------------*/
		m=fsup_ssl_blocksize-sslInst->fbin_min;	
		for (i=sslInst->fbin_min; i<=sslInst->fbin_max; i++){
			temp_real=sslFFT_R[2*i]*sslFFT_L[2*i];
			temp_real=temp_real+(sslFFT_R[2*i+1]*sslFFT_L[2*i+1]);
			
			temp_imaj=sslFFT_R[2*i+1]*sslFFT_L[2*i];
			temp_imaj=temp_imaj-sslFFT_R[2*i]*sslFFT_L[2*i+1];

			temp_abs=temp_real*temp_real;
			temp_abs=temp_abs+(temp_imaj*temp_imaj);
			temp_abs=sqrt(temp_abs);
			CCVF[2*i]=temp_real/temp_abs;
			CCVF[2*i+1]=temp_imaj/temp_abs;		
			
			CCVF[2*m]=CCVF[2*i];
			CCVF[2*m+1]=-1*CCVF[2*i+1];		
			m--;				
		}

		pffft_transform_ordered(ssl_ccv_RL_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max2; i++){
			ccv_RL[ccv_idx][ITD_max2+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			ccv_RL[ccv_idx][ITD_max2+i]=sqrt(ccv_RL[ccv_idx][ITD_max2+i])*deno2;
		}

		m=fsup_ssl_blocksize-ITD_max2;
		for (i=0; i<ITD_max2; i++){
			ccv_RL[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			ccv_RL[ccv_idx][i]=sqrt(ccv_RL[ccv_idx][i])*deno2;
			m++;
		}

		/*-------------------- make ccv_FB --------------------*/
		m=fsup_ssl_blocksize-sslInst->fbin_min;	
		for (i=sslInst->fbin_min; i<=sslInst->fbin_max; i++){
			temp_real=sslFFT_F[2*i]*sslFFT_B[2*i];
			temp_real=temp_real+(sslFFT_F[2*i+1]*sslFFT_B[2*i+1]);
			
			temp_imaj=sslFFT_F[2*i+1]*sslFFT_B[2*i];
			temp_imaj=temp_imaj-sslFFT_F[2*i]*sslFFT_B[2*i+1];

			temp_abs=temp_real*temp_real;
			temp_abs=temp_abs+(temp_imaj*temp_imaj);
			temp_abs=sqrt(temp_abs);
			CCVF[2*i]=temp_real/temp_abs;
			CCVF[2*i+1]=temp_imaj/temp_abs;		
			
			CCVF[2*m]=CCVF[2*i];
			CCVF[2*m+1]=-1*CCVF[2*i+1];		
			m--;				
		}

		pffft_transform_ordered(ssl_ccv_FB_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max2; i++){
			ccv_FB[ccv_idx][ITD_max2+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			ccv_FB[ccv_idx][ITD_max2+i]=sqrt(ccv_FB[ccv_idx][ITD_max2+i])*deno2;
		}

		m=fsup_ssl_blocksize-ITD_max2;
		for (i=0; i<ITD_max2; i++){
			ccv_FB[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			ccv_FB[ccv_idx][i]=sqrt(ccv_FB[ccv_idx][i])*deno2;
			m++;
		}		

        if (ccv_stored>=sslInst->ccv_stored_min){

			for (n=0; n<ITD_size2; n++){ 
				ccv_RL_mean[n]=0.0;
				ccv_FB_mean[n]=0.0;

				for (i=0; i<ccv_stored; i++){
					ccv_RL_mean[n]+=ccv_RL[i][n];
					ccv_FB_mean[n]+=ccv_FB[i][n];
				}
				ccv_RL_mean[n]=ccv_RL_mean[n]/ccv_stored;
				ccv_FB_mean[n]=ccv_FB_mean[n]/ccv_stored;
			}

			for (n=0; n<ITD_size; n++){ 
				ccv_LF_mean[n]=0.0;
				ccv_FR_mean[n]=0.0;
				for (i=0; i<ccv_stored; i++){
					ccv_LF_mean[n]+=ccv_LF[i][n];
					ccv_FR_mean[n]+=ccv_FR[i][n];
				}
				ccv_LF_mean[n]=ccv_LF_mean[n]/ccv_stored;
				ccv_FR_mean[n]=ccv_FR_mean[n]/ccv_stored;		
			}			

			for (i=0; i<ITD_size2; i++){
				ccv_thre_RL_mean+=ccv_RL_mean[i];	
				ccv_thre_FB_mean+=ccv_FB_mean[i];	
				
				if (ccv_thre_RL_max<=ccv_RL_mean[i]){
					ccv_thre_RL_max=ccv_RL_mean[i];
					ccv_RL_max_idx=i;
				}

				if (ccv_thre_FB_max<=ccv_FB_mean[i]){
					ccv_thre_FB_max=ccv_FB_mean[i];
					ccv_FB_max_idx=i;
				}				
			}

			for (i=0; i<ITD_size; i++){
				ccv_thre_LF_mean+=ccv_LF_mean[i];	
				ccv_thre_FR_mean+=ccv_FR_mean[i];	

				if (ccv_thre_LF_max<=ccv_LF_mean[i]){
					ccv_thre_LF_max=ccv_LF_mean[i];
					ccv_LF_max_idx=i;
				}

				if (ccv_thre_FR_max<=ccv_FR_mean[i]){
					ccv_thre_FR_max=ccv_FR_mean[i];
					ccv_FR_max_idx=i;
				}
			}

			ccv_thre_RL_mean=ccv_thre_RL_mean/(double)fsup_ssl_blocksize;
			ccv_thre_LF_mean=ccv_thre_LF_mean/(double)fsup_ssl_blocksize;
			ccv_thre_FR_mean=ccv_thre_FR_mean/(double)fsup_ssl_blocksize;
			ccv_thre_FB_mean=ccv_thre_FB_mean/(double)fsup_ssl_blocksize;

			// printf("ccv_thre_RL_max=%.3f(%d), ccv_thre_RL_mean(%f)+Cth=%f\n", ccv_thre_RL_max, ccv_RL_max_idx, (ccv_thre_RL_mean),  (ccv_thre_RL_mean+Cth));
			// printf("ccv_thre_LF_max=%.3f(%d), ccv_thre_LF_mean(%f)+Cth=%f\n", ccv_thre_LF_max, ccv_LF_max_idx, (ccv_thre_LF_mean),  (ccv_thre_LF_mean+Cth) );
			// printf("ccv_thre_FR_max=%.3f(%d), ccv_thre_FR_mean(%f)+Cth=%f\n", ccv_thre_FR_max, ccv_FR_max_idx, (ccv_thre_FR_mean),  (ccv_thre_FR_mean+Cth) );
			// printf("ccv_thre_FB_max=%.3f(%d), ccv_thre_FB_mean(%f)+Cth=%f\n", ccv_thre_FB_max, ccv_FB_max_idx, (ccv_thre_FB_mean),  (ccv_thre_FB_mean+Cth) );

			double tempmin = ccv_thre_RL_max;
			int tempmin_idx = 0;
			if (tempmin>=ccv_thre_LF_max) {
				tempmin = ccv_thre_LF_max;
				tempmin_idx = 1;
			}
			if (tempmin>=ccv_thre_FR_max) {
				tempmin = ccv_thre_FR_max;
				tempmin_idx = 2;
			}
			if (tempmin>=ccv_thre_FB_max) {
				tempmin = ccv_thre_FB_max;
				tempmin_idx = 3;
			}		

			double tempmax = ccv_thre_RL_max;
			int tempmax_idx = 0;
			if (tempmax<=ccv_thre_LF_max) {
				tempmax = ccv_thre_LF_max;
				tempmax_idx = 1;
			}
			if (tempmax<=ccv_thre_FR_max) {
				tempmax = ccv_thre_FR_max;
				tempmax_idx = 2;
			}
			if (tempmax<=ccv_thre_FB_max) {
				tempmax = ccv_thre_FB_max;
				tempmax_idx = 3;
			}		

			// if (tempmin_idx==0){
			// 	printf("ccv_thre_RL_max(%f)	is minimum\n", ccv_thre_RL_max); 
			// } else if (tempmin_idx==1){
			// 	printf("ccv_thre_LF_max(%f)	is minimum\n", ccv_thre_LF_max); 
			// } else if (tempmin_idx==2){
			// 	printf("ccv_thre_FR_max(%f)	is minimum\n", ccv_thre_FR_max); 
			// } else if (tempmin_idx==3){
			// 	printf("ccv_thre_FB_max(%f)	is minimum\n", ccv_thre_FB_max); 
			// } 

			// if (tempmax_idx==0){
			// 	printf("ccv_thre_RL_max(%f)	is maximum\n", ccv_thre_RL_max); 
			// } else if (tempmax_idx==1){
			// 	printf("ccv_thre_LF_max(%f)	is maximum\n", ccv_thre_LF_max); 
			// } else if (tempmax_idx==2){
			// 	printf("ccv_thre_FR_max(%f)	is maximum\n", ccv_thre_FR_max); 
			// } else if (tempmax_idx==3){
			// 	printf("ccv_thre_FB_max(%f)	is maximum\n", ccv_thre_FB_max); 
			// } 			

			int except_RL = 0;
			int except_LF = 0;
			int except_FR = 0;
			int except_FB = 0;

			if ((ccv_thre_RL_max>ccv_thre_RL_mean+Cth) && (tempmin_idx!=0)){
				D_l_RL=DoA_range2[ccv_RL_max_idx];
				if (abs(D_l_RL)>=max_DoA_local){
					except_RL = 1;
				}
			} else {
				D_l_RL=DoA_range2[ccv_RL_max_idx];
				except_RL = 1;
			}

			if ((ccv_thre_LF_max>ccv_thre_LF_mean+Cth) &&  (tempmin_idx!=1)){
				D_l_LF=DoA_range[ccv_LF_max_idx];
				if (abs(D_l_LF)>=max_DoA_local){
					except_LF = 1;
				}				
			} else {
				D_l_LF=DoA_range[ccv_LF_max_idx];
				except_LF = 1;
			}

			if ((ccv_thre_FR_max>ccv_thre_FR_mean+Cth) &&  (tempmin_idx!=2)){
				D_l_FR=DoA_range[ccv_FR_max_idx];
				if (abs(D_l_FR)>=max_DoA_local){
					except_FR = 1;
				}					
			} else {
				D_l_FR=DoA_range[ccv_FR_max_idx];
				except_FR = 1;
			}	

			if ((ccv_thre_FB_max>ccv_thre_FB_mean+Cth) && (tempmin_idx!=3)){
				D_l_FB=DoA_range2[ccv_FB_max_idx];
				if (abs(D_l_FB)>=max_DoA_local){
					except_FB = 1;
				}					
			} else {
				D_l_FB=DoA_range2[ccv_FB_max_idx];
				except_FB = 1;
			}			

			// printf("except = %d, %d, %d, %d\n", except_RL, except_LF, except_FR, except_FB);

			if (D_l_RL==0.0)
				 D_g_RL[1]=0.0;
			else
				 D_g_RL[1]=-1.0*D_l_RL;

			if (D_l_RL>0.0) 
				 D_g_RL[0]=D_l_RL-180.0;
			else
				 D_g_RL[0]=D_l_RL+180.0;

			if (D_l_LF<-45.0)
				 D_g_LF[1]=-1.0*D_l_LF-225.0;
			else
				 D_g_LF[1]=135.0+(-1.0*D_l_LF);

			D_g_LF[0]=D_l_LF-45.0;

			if (D_l_FR>45.0)
				 D_g_FR[1]=-1.0*D_l_FR+225.0;
			else
				 D_g_FR[1]=-135.0+(-1*D_l_FR);

			D_g_FR[0]=D_l_FR+45.0;

			D_g_FB[1]=-90.0+(-1*D_l_FB);

			D_g_FB[0]=D_l_FB+90.0;			

			idx_pick=-1;
			double temp_diff;
			for (i=0; i<16 ; i++){
				p=E_idx[i][0];
				q=E_idx[i][1];
				r=E_idx[i][2];
				s=E_idx[i][3];
				E[i] = 0;

				if ((except_RL + except_LF) == 0){
					temp_diff=abs(D_g_RL[p]-D_g_LF[q]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}

				if ((except_LF + except_FR) == 0){
					temp_diff=abs(D_g_LF[q]-D_g_FR[r]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}

				if ((except_FR + except_RL) == 0){
					temp_diff=abs(D_g_FR[r]-D_g_RL[p]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}

				if ((except_FB + except_LF) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_LF[q]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}

				if ((except_FB + except_FR) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_FR[r]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}

				if ((except_FB + except_RL) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_RL[p]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					E[i] += temp_diff;
				}
		
				// E[i]=abs(D_g_RL[p]-D_g_LF[q])+abs(D_g_LF[q]-D_g_FR[r])+abs(D_g_FR[r]-D_g_RL[p]);
				if (E[i]<E_min_temp){
					E_min_temp=E[i];
					idx_pick=i;
				}
			}


			if (idx_pick>-1){
				p=E_idx[idx_pick][0];
				q=E_idx[idx_pick][1];
				r=E_idx[idx_pick][2];
				s=E_idx[idx_pick][3];

				if ((except_RL + except_LF) == 0){
					temp_diff=abs(D_g_RL[p]-D_g_LF[q]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {
							except_LF = 1;
						} else if (tempmax_idx==1) {
							except_RL = 1;
						} else if (tempmax_idx==2) {
							
						} else if (tempmax_idx==3) {
							
						}
					}
				}

				if ((except_LF + except_FR) == 0){
					temp_diff=abs(D_g_LF[q]-D_g_FR[r]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {

						} else if (tempmax_idx==1) {
							except_FR = 1;
						} else if (tempmax_idx==2) {
							except_LF = 1;
						} else if (tempmax_idx==3) {
							
						}
					}
				}

				if ((except_FR + except_RL) == 0){
					temp_diff=abs(D_g_FR[r]-D_g_RL[p]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {
							except_FR = 1;
						} else if (tempmax_idx==1) {

						} else if (tempmax_idx==2) {
							except_RL = 1;
						} else if (tempmax_idx==3) {
							
						}
					}
				}

				if ((except_FB + except_LF) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_LF[q]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {

						} else if (tempmax_idx==1) {
							except_FB = 1;
						} else if (tempmax_idx==2) {

						} else if (tempmax_idx==3) {
							except_LF = 1;
						}
					}
				}

				if ((except_FB + except_FR) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_FR[r]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {

						} else if (tempmax_idx==1) {

						} else if (tempmax_idx==2) {
							except_FB = 1;
						} else if (tempmax_idx==3) {
							except_FR = 1;
						}
					}
				}

				if ((except_FB + except_RL) == 0){
					temp_diff=abs(D_g_FB[s]-D_g_RL[p]);
					if (temp_diff>320.0) {
						temp_diff = (360.0-temp_diff);
					}
					if (temp_diff>30){
						if (tempmax_idx==0) {
							except_FB = 1;
						} else if (tempmax_idx==1) {

						} else if (tempmax_idx==2) {

						} else if (tempmax_idx==3) {
							except_RL = 1;
						}
					}
				}
			}			

			// printf("except = %d, %d, %d, %d\n", except_RL, except_LF, except_FR, except_FB);
				// printf("idx_pick=%d ", idx_pick);
//	            idx_pick=find(E==min(E), 1, 'last');			

			if ((except_RL+except_LF+except_FR+except_FB)>=3){
				idx_pick = -1;
			}

			int DoA_cnt=0;
			double DoA=0.0;

			if (idx_pick>-1){
				
				if (except_RL == 0){
					
					if (abs(D_l_RL)<max_DoA_local){
						
						// if (D_g_RL[E_idx[idx_pick][0]]<0) D_g_RL[E_idx[idx_pick][0]]=D_g_RL[E_idx[idx_pick][0]]+360.0;

						if (((160<D_g_LF[E_idx[idx_pick][1]]) && (D_g_LF[E_idx[idx_pick][1]]<=180)) && ((-180<=D_g_RL[E_idx[idx_pick][0]]) && (D_g_RL[E_idx[idx_pick][0]]<-160))) {
							D_g_RL[E_idx[idx_pick][0]] += 360.0;
						}

						DoA+=D_g_RL[E_idx[idx_pick][0]];
						DoA_cnt++;
					}
				}
				

				
				if (except_FR == 0){
					
					if (abs(D_l_FR)<max_DoA_local){
						
						// if (D_g_FR[E_idx[idx_pick][2]]<0) D_g_FR[E_idx[idx_pick][2]]=D_g_FR[E_idx[idx_pick][2]]+360.0;
						if (((160<D_g_LF[E_idx[idx_pick][1]]) && (D_g_LF[E_idx[idx_pick][1]]<=180)) && ((-180<=D_g_FR[E_idx[idx_pick][2]]) && (D_g_FR[E_idx[idx_pick][2]]<-160))) {
							D_g_FR[E_idx[idx_pick][2]] += 360.0;
						}
						
						DoA+=D_g_FR[E_idx[idx_pick][2]];
						DoA_cnt++;
					} 	 
				}
				
				
				if (except_FB == 0){
					
					if (abs(D_l_FB)<max_DoA_local){
						
						// if (D_g_LF[E_idx[idx_pick][1]]<0) D_g_LF[E_idx[idx_pick][1]]=D_g_LF[E_idx[idx_pick][1]]+360.0;
						DoA+=D_g_FB[E_idx[idx_pick][3]];
						DoA_cnt++;
					}		
				}
				
		 
				
				if (except_LF == 0){
					
					if (abs(D_l_LF)<max_DoA_local){
						
						// if (D_g_LF[E_idx[idx_pick][1]]<0) D_g_LF[E_idx[idx_pick][1]]=D_g_LF[E_idx[idx_pick][1]]+360.0;
						if (((160<D_g_FR[E_idx[idx_pick][2]]) && (D_g_FR[E_idx[idx_pick][2]]<=180)) && ((-180<=D_g_LF[E_idx[idx_pick][1]]) && (D_g_LF[E_idx[idx_pick][1]]<-160))) {
							D_g_LF[E_idx[idx_pick][1]] += 360.0;
						}

						DoA+=D_g_LF[E_idx[idx_pick][1]];
						DoA_cnt++;
					}
				}

				// printf("D_l_RL=[%3.1f, %3.1f, %3.1f] ", D_l_RL, D_g_RL[0], D_g_RL[1]);
				// printf("D_l_LF=[%3.1f, %3.1f, %3.1f] ", D_l_LF, D_g_LF[0], D_g_LF[1]);
				// printf("D_l_FR=[%3.1f, %3.1f, %3.1f] ", D_l_FR, D_g_FR[0], D_g_FR[1]);
				// printf("D_l_FB=[%3.1f, %3.1f, %3.1f] ", D_l_FB, D_g_FB[0], D_g_FB[1]);
				
				if (DoA_cnt>0)
					DoA_mean_temp=DoA/(double)DoA_cnt;

				if (DoA_mean_temp<0) DoA_mean_temp+=360.0;
								
				// printf("idx_pick=%d, E_idx=[%d, %d, %d, %d] ",idx_pick, E_idx[idx_pick][0], E_idx[idx_pick][1], E_idx[idx_pick][2], E_idx[idx_pick][3]);
				// printf("DoA_mean_temp= %.2f\r\n", DoA_mean_temp);
			}

			// if (g_ssl_debug_on==1){
			// 	// debug_matlab_ccv_double(ccv_RL_mean, 0, ITD_size2);
			// 	// debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
			// 	// debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
			// 	// debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);

			// 	// debug_matlab_ccv_send(0, ITD_size2);
			// 	// debug_matlab_ccv_send(1, ITD_size);
			// 	// debug_matlab_ccv_send(2, ITD_size);	
			// 	// debug_matlab_ccv_send(3, ITD_size2);	

			// 	debug_matlab_doa_send(7, DoA_mean_temp);
			// 	// for (i=0; i<ITD_size; i++){
			// 	// 	printf("ccv_RL_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_RL_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
			// 	// }
				
			// }

        }


		ccv_idx++;
		if (ccv_idx>=ccv_avg_size)
			ccv_idx=0;
	
	} else {

		// if (g_ssl_debug_on==1){
		// 	debug_matlab_doa_send(7, -60.0);
		// }


		for (i=0; i<ccv_avg_size; i++){
			for (m=0; m<ITD_size2; m++){
				ccv_RL[i][m]=0.0;
				ccv_FB[i][m]=0.0;
			}

			for (m=0; m<ITD_size; m++){		
				ccv_LF[i][m]=0.0;
				ccv_FR[i][m]=0.0;				
			}			
		}
        ccv_idx=0;
        ccv_stored=0;
	
	}

	// if (g_ssl_debug_on==1){
	// 	debug_matlab_ssl_vad_send(ssl_vad);
	// }

	if ((prev_vad == 0) && (ssl_vad_long==1)){
		prev_vad = 1;

		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}					
					cluster[i][0] = DoA_temp;
					
					// printf("cluster[%d][%d]=%f\r\n", i, 0, cluster[i][0]);

					cluster_idx[i]++;
					break;
				} else {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}
					
					if (fabs(cluster[i][cluster_idx[i]-1]-DoA_temp) <= 30.0){
						cluster[i][cluster_idx[i]] = DoA_temp;
						// printf("cluster[%d][%d]=%f\r\n", i, cluster_idx[i], cluster[i][cluster_idx[i]]);

						cluster_idx[i]++;
						break;
					}
				}

			}
		}		

	} else if ((prev_vad == 1) && (ssl_vad_long==1)){
		prev_vad = 1;
		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}

					cluster[i][0] = DoA_temp;
					
					// printf("cluster[%d][%d]=%f\r\n", i, 0, cluster[i][0]);
					
					cluster_idx[i]++;
					
					break;
				} else {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}

					if (fabs(cluster[i][cluster_idx[i]-1]-DoA_temp) <= 30.0){
				
						cluster[i][cluster_idx[i]] = DoA_temp;
						// printf("cluster[%d][%d]=%f\r\n", i, cluster_idx[i], cluster[i][cluster_idx[i]]);

						cluster_idx[i]++;
						break;
					}
				}

			}
		}
	} else if ((prev_vad == 1) && (ssl_vad_long==0)){
		prev_vad = 0;

		int max_cluster = 0;
		int max_cluster_idx = 0;
		for (i=0; i<5 ; i++){
			if (max_cluster <= cluster_idx[i]){
				max_cluster = cluster_idx[i];
				max_cluster_idx = i;
			}
		}

		if (max_cluster>0){
			double mean_Doa = 0.0;
			for (i=0; i<max_cluster; i++){
				mean_Doa += cluster[max_cluster_idx][i];
			}
			mean_Doa /= max_cluster;

			if (mean_Doa<0) mean_Doa=mean_Doa+360.0;

			*DoA_mean_p = mean_Doa;
			// printf("mean DoA = %.2f \r\n", mean_Doa);
			// int Beamno = ((int)(*DoA_mean_p + 22.5) / (int)45) % 8;
			// printf("DoA = %.2f degree, Beam no = %d \r\n", *DoA_mean_p, Beamno );

			if (g_ssl_debug_on==1){
				// debug_matlab_ccv_double(ccv_RL_mean, 0, ITD_size2);
				// debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
				// debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
				// debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);

				// debug_matlab_ccv_send(0, ITD_size2);
				// debug_matlab_ccv_send(1, ITD_size);
				// debug_matlab_ccv_send(2, ITD_size);	
				// debug_matlab_ccv_send(3, ITD_size2);	

				debug_matlab_doa_send(7, *DoA_mean_p);
				// for (i=0; i<ITD_size; i++){
				// 	printf("ccv_RL_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_RL_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
				// }
				
			}

		}



		for (i=0 ; i<5 ; i++){
			// for (j=0; j<cluster_idx[i]; j++){
			// 	printf("final cluster[%d][%d]=%.2f\r\n", i, j, cluster[i][j]);
			// }
			cluster_idx[i] = 0;
		}
	}

	int max_cluster = 0;
	int max_cluster_idx = 0;
	for (i=0; i<5 ; i++){
		if (max_cluster <= cluster_idx[i]){
			max_cluster = cluster_idx[i];
			max_cluster_idx = i;
		}
	}	

	if (max_cluster>10) {
		prev_vad = 0;


		double mean_Doa = 0.0;
		for (i=0; i<max_cluster; i++){
			mean_Doa += cluster[max_cluster_idx][i];
		}
		mean_Doa /= max_cluster;

		if (mean_Doa<0) mean_Doa=mean_Doa+360.0;
		
		*DoA_mean_p = mean_Doa;
		// printf("mean DoA = %.2f \r\n", mean_Doa);
		// int Beamno = ((int)(*DoA_mean_p + 22.5) / (int)45) % 8;
    	// printf("DoA = %.2f degree, Beam no = %d \r\n", *DoA_mean_p, Beamno );

		if (g_ssl_debug_on==1){
			// debug_matlab_ccv_double(ccv_RL_mean, 0, ITD_size2);
			// debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
			// debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
			// debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);

			// debug_matlab_ccv_send(0, ITD_size2);
			// debug_matlab_ccv_send(1, ITD_size);
			// debug_matlab_ccv_send(2, ITD_size);	
			// debug_matlab_ccv_send(3, ITD_size2);	

			debug_matlab_doa_send(7, *DoA_mean_p);
			// for (i=0; i<ITD_size; i++){
			// 	printf("ccv_RL_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_RL_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
			// }
			
		}



		for (i=0 ; i<5 ; i++){
			// for (j=0; j<cluster_idx[i]; j++){
			// 	printf("final cluster[%d][%d]=%.2f\r\n", i, j, cluster[i][j]);
			// }
			cluster_idx[i] = 0;
		}

		

	}


}

#define avgsize 64 //128 //64
#define avgsize2 128 //128 //64
#define vad_idx_total 6 // NUM_FRAMES/ssl_blocksize

extern int e_hist_Buf[4][avgsize2+vad_idx_total-1];
extern double g_x_slow_vad[2];

// // 결과 값을 담을 구조체 정의
// typedef struct {
//     float dBFS;
//     float ratio;
// } dBFSResult;

// void calculate_CCVF(float* FFT1, float* FFT2, float* CCVF, int fbin_min, int fbin_max, int fsup_ssl_blocksize);
// float calculate_dBFS_peak(int16_t samples[], size_t num_samples);
// dBFSResult calculate_dBFS_peak_and_ratio(int16_t samples[], size_t num_samples);
// dBFSResult calculate_dBFS_peak_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers);
// float calculate_dBFS_rms(int16_t samples[], size_t num_samples);
// dBFSResult calculate_dBFS_rms_and_ratio(int16_t samples[], size_t num_samples);
// dBFSResult calculate_dBFS_rms_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers);
// int countZeroCrossings(int16_t *buffer, int size);
// void find_outlier_and_average(double arr[], int size, double *average);
// int find_outlier(double arr[], int size);
// double update_noise_floor(double new_rms, double new_peak, double *p_noise_floor, double threshold_dB) ;
// double update_peak_max(double noisefloor_dB, double new_rms, double new_peak, double *p_peak_max, double threshold_dB);
// double calculate_dbfs(double rms);
// double calculate_ratio(double dbfs);
// double phase_difference(double phase1, double phase2);
// double phase_average(double phases[], int n);

double g_noise_floor = 1.0000e-03;
double g_max_peak  = 1.0000e-03;
float g_e_curr_dB = -60;
float g_max_peak_dB = -60;
float g_max_rms_dB  = -60;
int peakmax_ccv_idx =0;
int sslhold = 0;

int g_ssl_ccf_sent = 1;

void ssl_core_process_gun(sslInst_t *sslInst, short *in_L, short *in_R, short *in_F, short *in_B, double *DoA_mean_p, int mode ) {
	
	// int mode = mode_normal;
	// int mode = mode_gun;

	int i, j, n, m;
	int idx_pick = -1;
	double temp_real, temp_imaj, temp_abs, DoA_temp, DoA_mean_temp;
	double ccv_thre_RL_mean=0.0;
	double ccv_thre_RL_max=-INFINITY;
	double ccv_thre_LF_mean=0.0;
	double ccv_thre_LF_max=-INFINITY;
	double ccv_thre_FR_mean=0.0;
	double ccv_thre_FR_max=-INFINITY;	
	double ccv_thre_FB_mean=0.0;
	double ccv_thre_FB_max=-INFINITY;		
	double ccv_thre_BR_mean=0.0;
	double ccv_thre_BR_max=-INFINITY;
	double ccv_thre_LB_mean=0.0;
	double ccv_thre_LB_max=-INFINITY;	

	double ccv_RL_mean[ITD_size2];
	double ccv_LF_mean[ITD_size];
	double ccv_FR_mean[ITD_size];
	double ccv_FB_mean[ITD_size2];	
	double ccv_BR_mean[ITD_size];
	double ccv_LB_mean[ITD_size];	
	
	short *x_L = in_L;
	short *x_R = in_R;
	short *x_F = in_F;
	short *x_B = in_B;

	int ccv_RL_max_idx=-1;
	int ccv_LF_max_idx=-1;
	int ccv_FR_max_idx=-1;
	int ccv_FB_max_idx=-1;
	int ccv_BR_max_idx=-1;
	int ccv_LB_max_idx=-1;	

	double D_l_RL, D_l_LF, D_l_FR, D_l_FB, D_l_BR, D_l_LB;

	double D_g_RL[2];
	double D_g_LF[2];
	double D_g_FR[2];	
	double D_g_FB[2];	
	double D_g_BR[2];
	double D_g_LB[2];	

	double E[64];
	double E_min_temp=5000.0;

	*DoA_mean_p = -1.0;

	float thresold_dB_ssl_off = -6;
	float thresold_dB_ssl_on = 20;
	int ccv_last_num = 4;

	if (mode == mode_gun) {
		thresold_dB_ssl_off = -1;
		thresold_dB_ssl_on = 30;
		ccv_last_num = 2;
	} else {
		thresold_dB_ssl_off = -10;
		thresold_dB_ssl_on = 20;
		ccv_last_num = 4;
	}

	double gate_dB = sslInst->gate_dB;

	int peak = 0;

	int16_t* buffers[] = { in_L, in_R, in_F, in_B };
	dBFSResult result = calculate_dBFS_peak_and_ratio_multibuf(buffers, ssl_blocksize, 4);
	float e_curr_peak_dB = result.dBFS;
	float e_curr_peak_ratio = result.ratio;

	result = calculate_dBFS_rms_and_ratio_multibuf(buffers, ssl_blocksize, 4);
	float  e_curr_rms_dB = result.dBFS;
	float  e_curr_rms_ratio = result.ratio;

	g_noise_floor = update_noise_floor(e_curr_rms_ratio, e_curr_peak_ratio, &g_noise_floor, thresold_dB_ssl_on);
	double noise_floor_dbfs = calculate_dbfs(g_noise_floor);

	double max_peak_dbfs_prev = calculate_dbfs(g_max_peak);
	g_max_peak = update_peak_max(noise_floor_dbfs, e_curr_rms_ratio, e_curr_peak_ratio, &g_max_peak, thresold_dB_ssl_on);
	double max_peak_dbfs = calculate_dbfs(g_max_peak);
	
	if (e_curr_peak_dB < (noise_floor_dbfs + thresold_dB_ssl_on/4.0)) max_peak_dbfs = -90;
	
	if (g_e_curr_dB<e_curr_peak_dB) g_e_curr_dB=e_curr_peak_dB;

	if (g_max_peak_dB<max_peak_dbfs) g_max_peak_dB=max_peak_dbfs;

	if ((g_ssl_debug_on==1)&&(g_ssl_debug_snd_idx==0)){

		FloatPacket packet;
		packet.val1 = (float)g_e_curr_dB;
		packet.val2 = (float)g_max_peak_dB;
		packet.val3 = (float)e_curr_rms_dB;
		packet.val4 = (float)noise_floor_dbfs;
		packet.val5 = (float)thresold_dB_ssl_on;

		debug_matlab_ssl_float_packet_send(8, packet);
		g_e_curr_dB = -100.0;
		g_max_peak_dB = -100.0;
	}

	int ssl_vad=0;
	sslhold--;
	if (sslhold<=0) sslhold=0;
	if (sslhold==0) {
		if (((e_curr_rms_dB > (noise_floor_dbfs+thresold_dB_ssl_on))&&(e_curr_peak_dB>max_peak_dbfs+thresold_dB_ssl_off))||((ccv_idx>0)&&(ccv_idx<ccv_last_num)))
		{	
			ssl_vad = 1;		
			if (e_curr_peak_dB>-3.0) peak = 1;	
	#ifdef PRINT_DEBUG_SSL_INPUT	
			printf("\n\n");
			printf("*************************************************************************\r\n");	
			printf("peakmax_ccv_idx=%d ", peakmax_ccv_idx);
			printf("ccv_idx = %d, ccv_stored= %d ", ccv_idx, ccv_stored);
			printf("g_max_peak=%.2f, e_curr_peak_dB=%.2f, e_curr_rms_dB=%.2f noise_floor_dbfs=%.2f\r\n", max_peak_dbfs, e_curr_peak_dB, e_curr_rms_dB, noise_floor_dbfs);
			printf("thresold_dB_ssl_on=%f, thresold_dB_ssl_off=%f \r\n",thresold_dB_ssl_on,thresold_dB_ssl_off  );
			if (peak == 1) printf(" input peak!!");		
			printf(" \n\n");		
	#endif				
		}
	} 


	if ((ssl_vad>0)){		

        ccv_stored++;
        if (ccv_stored>ccv_avg_size) ccv_stored=ccv_avg_size;

		if ((max_peak_dbfs_prev+0.5)<=e_curr_peak_dB) peakmax_ccv_idx=ccv_idx;

		peaklevel[ccv_idx]=(double)(e_curr_rms_ratio);

		/*-------------------- make FFTs --------------------*/
		for (i=0; i<ssl_blocksize; i++)
			X_L[i]=(int)x_L[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_R[i]=(int)x_R[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_F[i]=(int)x_F[i]*hanning_w[i];	
		for (i=0; i<ssl_blocksize; i++)
			X_B[i]=(int)x_B[i]*hanning_w[i];				

		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_L[i] = ((float)X_L[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_R[i] = ((float)X_R[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_F[i] = ((float)X_F[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_B[i] = ((float)X_B[i])*deno;			

		pffft_transform_ordered(ssl_L_p, sslFFTinput_L, sslFFT_L, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_R_p, sslFFTinput_R, sslFFT_R, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_F_p, sslFFTinput_F, sslFFT_F, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_B_p, sslFFTinput_B, sslFFT_B, pffftsslwork, PFFFT_FORWARD);

		/*-------------------- make ccv --------------------*/
		calculate_ccv(ssl_ccv_LF_p, ccv_LF[ccv_idx], sslFFT_L, sslFFT_F, CCVF, ITD_max, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);
		calculate_ccv(ssl_ccv_BR_p, ccv_BR[ccv_idx], sslFFT_B, sslFFT_R, CCVF, ITD_max, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);
		calculate_ccv(ssl_ccv_FR_p, ccv_FR[ccv_idx], sslFFT_F, sslFFT_R, CCVF, ITD_max, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);
		calculate_ccv(ssl_ccv_LB_p, ccv_LB[ccv_idx], sslFFT_L, sslFFT_B, CCVF, ITD_max, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);
		calculate_ccv(ssl_ccv_RL_p, ccv_RL[ccv_idx], sslFFT_R, sslFFT_L, CCVF, ITD_max2, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);
		calculate_ccv(ssl_ccv_FB_p, ccv_FB[ccv_idx], sslFFT_F, sslFFT_B, CCVF, ITD_max2, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, gate_dB);

        if (ccv_stored>=sslInst->ccv_stored_min){

			double peaktotal;
			calculate_peak_temp_total(peaklevel, peaktemp, &peaktotal, 0.5, ccv_stored, peakmax_ccv_idx);

			calculate_ccv_mean(mode, &ccv_LF[0][0], ccv_LF_mean, peaktemp, peaktotal, &ccv_thre_LF_mean, &ccv_thre_LF_max, &ccv_LF_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size);
			calculate_ccv_mean(mode, &ccv_BR[0][0], ccv_BR_mean, peaktemp, peaktotal, &ccv_thre_BR_mean, &ccv_thre_BR_max, &ccv_BR_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size);
			calculate_ccv_mean(mode, &ccv_FR[0][0], ccv_FR_mean, peaktemp, peaktotal, &ccv_thre_FR_mean, &ccv_thre_FR_max, &ccv_FR_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size);			
			calculate_ccv_mean(mode, &ccv_LB[0][0], ccv_LB_mean, peaktemp, peaktotal, &ccv_thre_LB_mean, &ccv_thre_LB_max, &ccv_LB_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size);
			calculate_ccv_mean(mode, &ccv_RL[0][0], ccv_RL_mean, peaktemp, peaktotal, &ccv_thre_RL_mean, &ccv_thre_RL_max, &ccv_RL_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size2);
			calculate_ccv_mean(mode, &ccv_FB[0][0], ccv_FB_mean, peaktemp, peaktotal, &ccv_thre_FB_mean, &ccv_thre_FB_max, &ccv_FB_max_idx, ccv_stored, peakmax_ccv_idx, ITD_size2);

#ifdef SEND_DEBUG_CCVF	
			// if ((g_ssl_debug_on==1)&&(g_ssl_ccf_sent==0)){			
			if ((g_ssl_debug_on==1)){				
				debug_matlab_ccv_double(ccv_RL_mean, 0, ITD_size2);
				debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
				debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
				debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);			

				// debug_matlab_ccv_double2(ccv_RL[ccv_idx], peaktemp[ccv_idx], 0, ITD_size2);
				// debug_matlab_ccv_double2(ccv_LF[ccv_idx], peaktemp[ccv_idx], 1, ITD_size);
				// debug_matlab_ccv_double2(ccv_FR[ccv_idx], peaktemp[ccv_idx], 2, ITD_size);
				// debug_matlab_ccv_double2(ccv_FB[ccv_idx], peaktemp[ccv_idx], 3, ITD_size2);		

				debug_matlab_ccv_send(0, ITD_size2);
				debug_matlab_ccv_send(1, ITD_size);
				debug_matlab_ccv_send(2, ITD_size);	
				debug_matlab_ccv_send(3, ITD_size2);	

				debug_matlab_ssl_float_send(10, ccv_thre_RL_mean+sslInst->CCVth);
				debug_matlab_ssl_float_send(11, ccv_thre_LF_mean+sslInst->CCVth);
				debug_matlab_ssl_float_send(12, ccv_thre_FR_mean+sslInst->CCVth);
				debug_matlab_ssl_float_send(13, ccv_thre_FB_mean+sslInst->CCVth);
				// g_ssl_ccf_sent = 1;
			}
#endif			
		
			int except_RL = 0;
			int except_LF = 0;
			int except_FR = 0;
			int except_FB = 0;
			int except_BR = 0;
			int except_LB = 0;

			if ((ccv_thre_RL_max>ccv_thre_RL_mean+sslInst->CCVth)){
				D_l_RL=DoA_range2[ccv_RL_max_idx];
				if (abs(D_l_RL)>=max_DoA_local){
					except_RL = 1;
				}
			} else {
				D_l_RL=DoA_range2[ccv_RL_max_idx];
				except_RL = 1;
			}
			if ((ccv_thre_FB_max>ccv_thre_FB_mean+sslInst->CCVth)){
				D_l_FB=DoA_range2[ccv_FB_max_idx];
				if (abs(D_l_FB)>=max_DoA_local){
					except_FB = 1;
				}					
			} else {
				D_l_FB=DoA_range2[ccv_FB_max_idx];
				except_FB = 1;
			}					

			if ((ccv_thre_LF_max>ccv_thre_LF_mean+sslInst->CCVth)){
				D_l_LF=DoA_range[ccv_LF_max_idx];
				if (abs(D_l_LF)>=max_DoA_local){
					except_LF = 1;
				}				
			} else {
				D_l_LF=DoA_range[ccv_LF_max_idx];
				except_LF = 1;
			}
			if ((ccv_thre_FR_max>ccv_thre_FR_mean+sslInst->CCVth)){
				D_l_FR=DoA_range[ccv_FR_max_idx];
				if (abs(D_l_FR)>=max_DoA_local){
					except_FR = 1;
				}					
			} else {
				D_l_FR=DoA_range[ccv_FR_max_idx];
				except_FR = 1;
			}	
			if ((ccv_thre_BR_max>ccv_thre_BR_mean+sslInst->CCVth)){
				D_l_BR=DoA_range[ccv_BR_max_idx];
				if (abs(D_l_BR)>=max_DoA_local){
					except_BR = 1;
				}					
			} else {
				D_l_BR=DoA_range[ccv_BR_max_idx];
				except_BR = 1;
			}		
			if ((ccv_thre_LB_max>ccv_thre_LB_mean+sslInst->CCVth)){
				D_l_LB=DoA_range[ccv_LB_max_idx];
				if (abs(D_l_LB)>=max_DoA_local){
					except_LB = 1;
				}					
			} else {
				D_l_LB=DoA_range[ccv_LB_max_idx];
				except_LB = 1;
			}						

			if (D_l_RL==0.0) D_g_RL[1]=0.0;
			else D_g_RL[1]=-1.0*D_l_RL;

			if (D_l_RL>0.0) D_g_RL[0]=D_l_RL-180.0;
			else  D_g_RL[0]=D_l_RL+180.0;

			if (D_l_LF<-45.0) D_g_LF[1]=-1.0*D_l_LF-225.0;
			else D_g_LF[1]=135.0+(-1.0*D_l_LF);

			D_g_LF[0]=D_l_LF-45.0;

			if (D_l_BR<-45.0) D_g_BR[1]=-1.0*D_l_BR-225.0;
			else D_g_BR[1]=135.0+(-1.0*D_l_BR);

			D_g_BR[0]=D_l_BR-45.0;			

			if (D_l_FR>45.0) D_g_FR[1]=-1.0*D_l_FR+225.0;
			else D_g_FR[1]=-135.0+(-1*D_l_FR);

			D_g_FR[0]=D_l_FR+45.0;

			if (D_l_LB>45.0) D_g_LB[1]=-1.0*D_l_LB+225.0;
			else D_g_LB[1]=-135.0+(-1*D_l_LB);

			D_g_LB[0]=D_l_LB+45.0;			

			D_g_FB[1]=-90.0+(-1*D_l_FB);
			D_g_FB[0]=D_l_FB+90.0;			

#ifdef PRINT_DEBUG_SSL	
				printf("ccv_RL_max_idx=%d, D_l_RL=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_RL_max_idx-ITD_max2, D_l_RL, D_g_RL[0], D_g_RL[1], except_RL);
				printf("ccv_LF_max_idx=%d, D_l_LF=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_LF_max_idx-ITD_max, D_l_LF, D_g_LF[0], D_g_LF[1], except_LF);
				printf("ccv_FR_max_idx=%d, D_l_FR=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_FR_max_idx-ITD_max, D_l_FR, D_g_FR[0], D_g_FR[1], except_FR);
				printf("ccv_FB_max_idx=%d, D_l_FB=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_FB_max_idx-ITD_max2, D_l_FB, D_g_FB[0], D_g_FB[1], except_FB);
				printf("ccv_BR_max_idx=%d, D_l_BR=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_BR_max_idx-ITD_max, D_l_BR, D_g_BR[0], D_g_BR[1], except_BR);
				printf("ccv_LB_max_idx=%d, D_l_LB=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_LB_max_idx-ITD_max, D_l_LB, D_g_LB[0], D_g_LB[1], except_LB);

				printf("\n");
#endif			

			idx_pick=-1;
			double temp_diff, temp1, temp2;
			int e_count = 0;
			int e_counttemp = 0;
			int p, q, r, s, t, u;

			for (i=0; i<64 ; i++){
				p=E_idx64[i][0];
				q=E_idx64[i][1];
				r=E_idx64[i][2];
				s=E_idx64[i][3];
				t=E_idx64[i][4];
				u=E_idx64[i][5];
				E[i] = -1;
				e_counttemp = 0;

				int outlier_idx1;
				int outlier_idx2;
				double ourlier_avr;			
				
				if ((except_RL + except_LF) == 0){
					temp1 = D_g_RL[p];
					temp2 = D_g_LF[q];
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_FR) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_FR[r];			
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_FB) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_FB[s];
					temp_diff=phase_difference(temp1,  temp2);					
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_BR) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_BR[t];
					temp_diff=phase_difference(temp1,  temp2);					
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_LB) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_LB[u];
					temp_diff=phase_difference(temp1,  temp2);					
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_FR) == 0){
					temp1 = D_g_LF[q];
					temp2 = D_g_FR[r];					
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_FB) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_FB[s];
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_BR) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_BR[t];
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_LB) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_LB[u];
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_FR) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_FR[r];	
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_BR) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_BR[t];	
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_LB) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_LB[u];	
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}		

				if ((except_BR + except_LB) == 0){
					temp1 = D_g_BR[t];
					temp2 = D_g_LB[u];	
					temp_diff=phase_difference(temp1,  temp2);
					E[i] += temp_diff;
					e_counttemp++;
				}												

				if (E[i]==-1) continue;
		
				if (E[i]<E_min_temp){
					E_min_temp=E[i];
					idx_pick=i;
					e_count=e_counttemp;
				} 
				// else {
				// 	printf("E[%d]=%f\r\n", i, E[i]);
				// }
			}

#ifdef PRINT_DEBUG_SSL	
			if (idx_pick>-1){
				printf("idx_pick=%d, E_idx=[%d, %d, %d, %d, %d, %d] ",idx_pick, E_idx64[idx_pick][0], E_idx64[idx_pick][1], E_idx64[idx_pick][2], E_idx64[idx_pick][3], E_idx64[idx_pick][4], E_idx64[idx_pick][5]);
				printf("E[idx_pick]=%f e_count=%d\r\n\n", E[idx_pick], e_count);
			} else {
				printf("idx_pick=%d\r\n");
			}
#endif							

			double arr_D_g[6];
			double D_l[6];

			if (idx_pick>-1){
				
				p=E_idx64[idx_pick][0];
				q=E_idx64[idx_pick][1];
				r=E_idx64[idx_pick][2];
				s=E_idx64[idx_pick][3];
				t=E_idx64[idx_pick][4];
				u=E_idx64[idx_pick][5];
				int size=0;
				if (except_RL==0) arr_D_g[size++]=D_g_RL[p];
				if (except_LF==0) arr_D_g[size++]=D_g_LF[q];
				if (except_FR==0) arr_D_g[size++]=D_g_FR[r];
				if (except_FB==0) arr_D_g[size++]=D_g_FB[s];
				if (except_BR==0) arr_D_g[size++]=D_g_BR[t];
				if (except_LB==0) arr_D_g[size++]=D_g_LB[u];

				D_l[0]=D_l_RL;
				D_l[1]=D_l_LF;
				D_l[2]=D_l_FR;
				D_l[3]=D_l_FB;
				D_l[4]=D_l_BR;
				D_l[5]=D_l_LB;

				if ((except_RL + except_LF + except_FR + except_FB + except_BR + except_LB)<=2){
					int outlier_idx1;
					int outlier_idx2;
					double ourlier_avr = find_two_outliers(arr_D_g, size, &outlier_idx1, &outlier_idx2);

					if (ourlier_avr > -1000){
	
						if (D_g_RL[p]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 0;
						}
						if (D_g_LF[q]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 1;
						}
						if (D_g_FR[r]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 2;
						}
						if (D_g_FB[s]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 3;
						}
						if (D_g_BR[t]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 4;
						}
						if (D_g_LB[u]==arr_D_g[outlier_idx1]){
							outlier_idx1 = 5;
						}

						if (D_g_RL[p]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 0;
						}
						if (D_g_LF[q]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 1;
						}
						if (D_g_FR[r]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 2;
						}
						if (D_g_FB[s]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 3;
						}
						if (D_g_BR[t]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 4;
						}
						if (D_g_LB[u]==arr_D_g[outlier_idx2]){
							outlier_idx2 = 5;
						}

						arr_D_g[0]=D_g_RL[p];
						arr_D_g[1]=D_g_LF[q];
					 	arr_D_g[2]=D_g_FR[r];
						arr_D_g[3]=D_g_FB[s];
						arr_D_g[4]=D_g_BR[t];
						arr_D_g[5]=D_g_LB[u];					

	#ifdef PRINT_DEBUG_SSL						
						printf("ourlier_avr = %.2f\r\n", ourlier_avr);
						printf("outlier_idx1 = %d degree = %.2f\r\n", outlier_idx1, arr_D_g[outlier_idx1]);
						printf("outlier_idx2 = %d degree = %.2f\r\n", outlier_idx2, arr_D_g[outlier_idx2]);
	#endif
						if ( (phase_difference(arr_D_g[outlier_idx1],ourlier_avr) > 60.0) || (phase_difference(arr_D_g[outlier_idx2],ourlier_avr) > 60.0) ) {
							if ((phase_difference(arr_D_g[outlier_idx1],ourlier_avr))> 60.0){
								if (outlier_idx1==0) except_RL = 1;
								else if (outlier_idx1==1) except_LF = 1;
								else if (outlier_idx1==2) except_FR = 1;
								else if (outlier_idx1==3) except_FB = 1;
								else if (outlier_idx1==4) except_BR = 1;
								else if (outlier_idx1==5) except_LB = 1;
							}
							if ((phase_difference(arr_D_g[outlier_idx2],ourlier_avr))> 60.0){
								if (outlier_idx2==0) except_RL = 1;
								else if (outlier_idx2==1) except_LF = 1;
								else if (outlier_idx2==2) except_FR = 1;
								else if (outlier_idx2==3) except_FB = 1;	
								else if (outlier_idx2==4) except_BR = 1;	
								else if (outlier_idx2==5) except_LB = 1;	
							}
						} else 	{
							// if ( phase_difference(arr_D_g[outlier_idx1],ourlier_avr) > phase_difference(arr_D_g[outlier_idx2],ourlier_avr) ) {
							if (fabs(D_l[outlier_idx1]) > fabs(D_l[outlier_idx2])){
								if (outlier_idx1==0) except_RL = 1;
								else if (outlier_idx1==1) except_LF = 1;
								else if (outlier_idx1==2) except_FR = 1;
								else if (outlier_idx1==3) except_FB = 1;
								else if (outlier_idx1==4) except_BR = 1;
								else if (outlier_idx1==5) except_LB = 1;
							} else {
								if (outlier_idx2==0) except_RL = 1;
								else if (outlier_idx2==1) except_LF = 1;
								else if (outlier_idx2==2) except_FR = 1;
								else if (outlier_idx2==3) except_FB = 1;
								else if (outlier_idx2==4) except_BR = 1;
								else if (outlier_idx2==5) except_LB = 1;
							}
						}
					}



#ifdef PRINT_DEBUG_SSL	
				printf("ccv_RL_max_idx=%d, D_l_RL=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_RL_max_idx-ITD_max2, D_l_RL, D_g_RL[0], D_g_RL[1], except_RL);
				printf("ccv_LF_max_idx=%d, D_l_LF=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_LF_max_idx-ITD_max, D_l_LF, D_g_LF[0], D_g_LF[1], except_LF);
				printf("ccv_FR_max_idx=%d, D_l_FR=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_FR_max_idx-ITD_max, D_l_FR, D_g_FR[0], D_g_FR[1], except_FR);
				printf("ccv_FB_max_idx=%d, D_l_FB=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_FB_max_idx-ITD_max2, D_l_FB, D_g_FB[0], D_g_FB[1], except_FB);
				printf("ccv_BR_max_idx=%d, D_l_BR=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_BR_max_idx-ITD_max, D_l_BR, D_g_BR[0], D_g_BR[1], except_BR);
				printf("ccv_LB_max_idx=%d, D_l_LB=[%3.1f, %3.1f, %3.1f] except = %d\n",ccv_LB_max_idx-ITD_max, D_l_LB, D_g_LB[0], D_g_LB[1], except_LB);
				printf("\n");
#endif					
				}	

				E[idx_pick] = 0;
				e_counttemp = 0;
				
				if ((except_RL + except_LF) == 0){
					temp1 = D_g_RL[p];
					temp2 = D_g_LF[q];
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_FR) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_FR[r];			
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_FB) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_FB[s];
					temp_diff=phase_difference(temp1,  temp2);					
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_BR) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_BR[t];
					temp_diff=phase_difference(temp1,  temp2);					
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_RL + except_LB) == 0){
					temp1 = D_g_RL[p];	
					temp2 = D_g_LB[u];
					temp_diff=phase_difference(temp1,  temp2);					
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_FR) == 0){
					temp1 = D_g_LF[q];
					temp2 = D_g_FR[r];					
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_FB) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_FB[s];
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_BR) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_BR[t];
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_LF + except_LB) == 0){
					temp1 = D_g_LF[q];	
					temp2 = D_g_LB[u];
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_FR) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_FR[r];	
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_BR) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_BR[t];	
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}

				if ((except_FB + except_LB) == 0){
					temp1 = D_g_FB[s];
					temp2 = D_g_LB[u];	
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}		

				if ((except_BR + except_LB) == 0){
					temp1 = D_g_BR[t];
					temp2 = D_g_LB[u];	
					temp_diff=phase_difference(temp1,  temp2);
					E[idx_pick] += temp_diff;
					e_counttemp++;
				}	
	
				e_count=e_counttemp;
			}
#ifdef PRINT_DEBUG_SSL				
			else {
				printf("idx_pick=%d\r\n");
			}			
#endif			


#ifdef PRINT_DEBUG_SSL	
			if (idx_pick>-1){
				printf("idx_pick=%d, E_idx=[%d, %d, %d, %d, %d, %d] ",idx_pick, E_idx64[idx_pick][0], E_idx64[idx_pick][1], E_idx64[idx_pick][2], E_idx64[idx_pick][3], E_idx64[idx_pick][4], E_idx64[idx_pick][5]);
				printf("E[idx_pick]=%f e_count=%d E[idx_pick]/e_count = %.2f > %.2f\r\n\n", E[idx_pick], e_count, (float)E[idx_pick]/(float)e_count, E_avr_max);

				p=E_idx64[idx_pick][0];
				q=E_idx64[idx_pick][1];
				r=E_idx64[idx_pick][2];
				s=E_idx64[idx_pick][3];
				t=E_idx64[idx_pick][4];
				u=E_idx64[idx_pick][5];

				printf("D_g_RL[%d]=%f", p, D_g_RL[p]);
				if (except_RL==1) printf(" except RL\r\n"); else printf("\r\n"); 
				printf("D_g_LF[%d]=%f", q,  D_g_LF[q]);
				if (except_LF==1) printf(" except LF\r\n"); else printf("\r\n"); 
				printf("D_g_FR[%d]=%f", r, D_g_FR[r]);
				if (except_FR==1) printf(" except FR\r\n"); else printf("\r\n"); 
				printf("D_g_FB[%d]=%f", s, D_g_FB[s]);
				if (except_FB==1) printf(" except FB\r\n"); else printf("\r\n"); 
				printf("D_g_BR[%d]=%f", s, D_g_BR[t]);
				if (except_BR==1) printf(" except BR\r\n"); else printf("\r\n"); 
				printf("D_g_LB[%d]=%f", s, D_g_LB[u]);
				if (except_LB==1) printf(" except LB\r\n"); else printf("\r\n\n"); 
			} else {
				printf("idx_pick=%d\r\n");
			}
#endif				

			int DoA_cnt=0;
			double DoA=0.0;

			if ((except_RL + except_LF + except_FR + except_FB + except_BR + except_LB >= 4)&&((float)E[idx_pick]/(float)e_count > E_avr_max)) idx_pick = -1;
#ifdef PRINT_DEBUG_SSL
			if (idx_pick == -1) printf("(sum of excepts >= 4)\r\n ");	
#endif			
			if (idx_pick>-1){
				if ((float)E[idx_pick]/(float)e_count > E_avr_max) idx_pick = -1;
			}
#ifdef PRINT_DEBUG_SSL
			if (idx_pick == -1) printf("((float)E[idx_pick]/(float)e_count > %.2f)\r\n ", E_avr_max);	
#endif					

			if (idx_pick>-1){

				double arr[6];			
		
				if (except_RL == 0){	
					if (fabs(D_l_RL)<max_DoA_local){
						arr[DoA_cnt] = D_g_RL[E_idx64[idx_pick][0]];
						DoA_cnt++;
					}
				}
				if (except_LF == 0){
					if (fabs(D_l_LF)<max_DoA_local){
						arr[DoA_cnt]=D_g_LF[E_idx64[idx_pick][1]];
						DoA_cnt++;										
					}
				}
				if (except_FR == 0){
					if (fabs(D_l_FR)<max_DoA_local){	
						arr[DoA_cnt]=D_g_FR[E_idx64[idx_pick][2]];
						DoA_cnt++;						
					}
				}
				if (except_FB == 0){
					if (fabs(D_l_FB)<max_DoA_local){
						arr[DoA_cnt]=D_g_FB[E_idx64[idx_pick][3]];
						DoA_cnt++;											
					}
				}
				if (except_BR == 0){
					if (fabs(D_l_BR)<max_DoA_local){
						arr[DoA_cnt]=D_g_BR[E_idx64[idx_pick][4]];
						DoA_cnt++;											
					}
				}
				if (except_LB == 0){
					if (fabs(D_l_LB)<max_DoA_local){
						arr[DoA_cnt]=D_g_LB[E_idx64[idx_pick][5]];
						DoA_cnt++;											
					}
				}								
				DoA_mean_temp = phase_average(arr, DoA_cnt);				
				
#ifdef PRINT_DEBUG_SSL									
				printf("DoA_mean_temp= %.2f\r\n", DoA_mean_temp);
#endif				
				if ((g_ssl_debug_on==1)){				
					debug_matlab_doa_send(7, DoA_mean_temp);
				}			
			}
        }

		ccv_idx++;
		if (ccv_idx>=ccv_avg_size) ccv_idx=0;
	
	} else { //if ((ssl_vad>0)){

		for (i=0; i<ccv_avg_size; i++){
			for (m=0; m<ITD_size2; m++){
				ccv_RL[i][m]=0.0;
				ccv_FB[i][m]=0.0;
			}

			for (m=0; m<ITD_size; m++){		
				ccv_LF[i][m]=0.0;
				ccv_FR[i][m]=0.0;				
				ccv_BR[i][m]=0.0;	
				ccv_LB[i][m]=0.0;	
			}			
		}
        ccv_idx=0;
        ccv_stored=0;
		peakmax_ccv_idx=0;
	
	}

	if ((prev_vad == 0) && (ssl_vad==1)){
		prev_vad = 1;
		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {
					cluster[i][0] = DoA_mean_temp;
					cluster_power[i][0] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
					if (ccv_idx==0) cluster_ccvidx[i][0] = ccv_avg_size - 1;
					else cluster_ccvidx[i][0] = ccv_idx-1;
					cluster_idx[i]++;
					break;
				} else {
					if (phase_difference(cluster[i][cluster_idx[i]-1], DoA_mean_temp) <= 15.0){
						cluster[i][cluster_idx[i]] = DoA_mean_temp;
						cluster_power[i][cluster_idx[i]] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
						if (ccv_idx==0) cluster_ccvidx[i][cluster_idx[i]] = ccv_avg_size - 1;
						else cluster_ccvidx[i][cluster_idx[i]] = ccv_idx-1;						
						cluster_idx[i]++;
						break;
					}
				}
			}
		}		
	} else if ((prev_vad == 1) && (ssl_vad==1)){ //if ((prev_vad == 0) && (ssl_vad==1)){
		prev_vad = 1;
		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {
					cluster[i][0] = DoA_mean_temp;
					cluster_power[i][0] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
					if (ccv_idx==0) cluster_ccvidx[i][0] = ccv_avg_size - 1;
					else cluster_ccvidx[i][0] = ccv_idx-1;					
					cluster_idx[i]++;
					break;
				} else {
					if (phase_difference(cluster[i][cluster_idx[i]-1], DoA_mean_temp) <= 15.0){
						cluster[i][cluster_idx[i]] = DoA_mean_temp;
						cluster_power[i][cluster_idx[i]] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
						if (ccv_idx==0) cluster_ccvidx[i][cluster_idx[i]] = ccv_avg_size - 1;
						else cluster_ccvidx[i][cluster_idx[i]] = ccv_idx-1;	
						cluster_idx[i]++;
						break;
					}
				}
			}
		}
	} else if ((prev_vad == 1) && (ssl_vad==0)){ // else if ((prev_vad == 1) && (ssl_vad==1)){
		prev_vad = 0;
		int total_cluster = 0;
		int max_cluster = 0;
		double max_cluster_power = 0.0;
		int max_cluster_idx = 0;
		int min_total = 1;	

		for (i=0; i<5 ; i++){
			total_cluster += cluster_idx[i];

			if (cluster_idx[i]>1){
				if (mode == mode_gun){
					double cluster_power_avr=0.0;

					for (j=0; j<cluster_idx[i]; j++){
						if (cluster_power[i][j]>cluster_power_avr) {
							cluster_power_avr=cluster_power[i][j];
						}
					}

					if (max_cluster_power <= cluster_power_avr){
						max_cluster_power=cluster_power_avr;
						max_cluster_idx = i;
						max_cluster = cluster_idx[i];
					};	
					min_total = 0;		
				} else if (mode == mode_normal){
					double cluster_power_avr=0.0;
					for (j=0; j<cluster_idx[i]; j++){
						cluster_power_avr+=cluster_power[i][j];
					}
					cluster_power_avr=cluster_power_avr/(double)(cluster_idx[i]);

					// printf("i=%d, cluster_idx[i]= %d, cluster_power_avr = %f\r\n",i, cluster_idx[i], cluster_power_avr);

					if (max_cluster_power <= cluster_power_avr){
						max_cluster_power=cluster_power_avr;
						max_cluster_idx = i;
						max_cluster = cluster_idx[i];
					};

					// if (max_cluster <= cluster_idx[i]){
					// 	max_cluster = cluster_idx[i];
					// 	max_cluster_idx = i;
					// }

				} else if (mode == mode_noavg){
					if (max_cluster <= cluster_idx[i]){
						max_cluster = cluster_idx[i];
						max_cluster_idx = i;
					}
				} else {
					if (max_cluster <= cluster_idx[i]){
						max_cluster = cluster_idx[i];
						max_cluster_idx = i;
					}
				}
			}


		}

		if ((max_cluster>0)&&(total_cluster>min_total)){
			double mean_Doa = 0.0;
			mean_Doa = phase_average(cluster[max_cluster_idx], cluster_idx[max_cluster_idx]);
#ifdef PRINT_DEBUG_SSL2	
			printf("(ssl_vad==0); max_cluster_idx =%d, cluster_idx[max_cluster_idx]=%d max_cluster_power = %f mean_Doa = %f\r\n", max_cluster_idx, cluster_idx[max_cluster_idx], max_cluster_power, mean_Doa);
#endif
			if (g_ssl_debug_on==1){			
				debug_matlab_doa_send(14, mean_Doa);				
			}

			if (mean_Doa<0) mean_Doa=mean_Doa+360.0;
			*DoA_mean_p = mean_Doa;			
#ifdef PRINT_DEBUG_SSL2	
			printf("\r\n");
			for (i=0 ; i<5 ; i++){
				for (j=0; j<cluster_idx[i]; j++){
					printf("final cluster[%d][%d]=%.2f, cluster_power[%d][%d]=%f cluster_ccvidx[%d][%d]=%d\r\n", i, j, cluster[i][j], i, j, cluster_power[i][j], i, j, cluster_ccvidx[i][j]);
				}
			}	
			printf("\r\n");
#endif			
		}

		for (i=0 ; i<5 ; i++){
			cluster_idx[i] = 0;	
		}	
		sslhold += 32;		// 0.5 sec hold	
	} //else if ((prev_vad == 1) && (ssl_vad==0)){

	int total_cluster = 0;
	int max_cluster = 0;
	double max_cluster_power = 0.0;
	int max_cluster_idx = 0;
	int min_total = 1;	

	for (i=0; i<5 ; i++){
		total_cluster += cluster_idx[i];

		if (cluster_idx[i]>1){
			if (mode == mode_gun){
				double cluster_power_avr=0.0;
				for (j=0; j<cluster_idx[i]; j++){
					if (cluster_power[i][j]>cluster_power_avr) {
						cluster_power_avr=cluster_power[i][j];
					}
				}

				if (max_cluster_power <= cluster_power_avr){
					max_cluster_power=cluster_power_avr;
					max_cluster_idx = i;
					max_cluster = cluster_idx[i];
				};	
				min_total = 0;		
			} else if (mode == mode_normal){
				double cluster_power_avr=0.0;
				for (j=0; j<cluster_idx[i]; j++){
					cluster_power_avr+=cluster_power[i][j];
				}
				cluster_power_avr=cluster_power_avr/(double)(cluster_idx[i]);

				// printf("i=%d, cluster_idx[i]= %d, cluster_power_avr = %f\r\n",i, cluster_idx[i], cluster_power_avr);

				if (max_cluster_power <= cluster_power_avr){
					max_cluster_power=cluster_power_avr;
					max_cluster_idx = i;
					max_cluster = cluster_idx[i];
				};

				// if (max_cluster <= cluster_idx[i]){
				// 	max_cluster = cluster_idx[i];
				// 	max_cluster_idx = i;
				// }

			} else if (mode == mode_noavg){
				if (max_cluster <= cluster_idx[i]){
					max_cluster = cluster_idx[i];
					max_cluster_idx = i;
				}
			} else {
				if (max_cluster <= cluster_idx[i]){
					max_cluster = cluster_idx[i];
					max_cluster_idx = i;
				}
			}
		}


	}

	if ((max_cluster>=(CLUSTER_LENG-1)) || (total_cluster>=(CLUSTER_LENG-1))) {
	// if ((max_cluster>6)) {	
		prev_vad = 0;

		double mean_Doa = 0.0;
		mean_Doa = phase_average(cluster[max_cluster_idx], cluster_idx[max_cluster_idx]);
#ifdef PRINT_DEBUG_SSL2	
		printf("max_cluster_idx =%d, cluster_idx[max_cluster_idx]=%d max_cluster_power = %f mean_Doa = %f\r\n", max_cluster_idx, cluster_idx[max_cluster_idx], max_cluster_power, mean_Doa);
#endif
		if (g_ssl_debug_on==1){
			debug_matlab_doa_send(14, mean_Doa);			
		}
		if (mean_Doa<0) mean_Doa=mean_Doa+360.0;
		*DoA_mean_p = mean_Doa;		

#ifdef PRINT_DEBUG_SSL2		
		printf("\r\n");		
		for (i=0 ; i<5 ; i++){
			for (j=0; j<cluster_idx[i]; j++){
				printf("final cluster[%d][%d]=%.2f, cluster_power[%d][%d]=%f cluster_ccvidx[%d][%d]=%d\r\n", i, j, cluster[i][j], i, j, cluster_power[i][j], i, j, cluster_ccvidx[i][j]);
			}
		}
		printf("\r\n");
#endif			
		for (i=0 ; i<5 ; i++){	
			cluster_idx[i] = 0;
		}
		sslhold += 32; // 0.5 sec hold	
	} //if (max_cluster>15) 

	if ((g_ssl_debug_on==1)){
		g_ssl_debug_snd_idx++;
		if (g_ssl_debug_snd_idx>=g_ssl_debug_snd_period) {
			g_ssl_debug_snd_idx = 0;
			g_ssl_ccf_sent = 0;
		}
	}
}

void ssl_core_process_linear(sslInst_t *sslInst, short *in_L, short *in_R, short *in_C, double *DoA_mean_p) {
	
	int i, j, n, m, p, q, r, s;
	int idx_pick = -1;
	double temp_real, temp_imaj, temp_abs, DoA_temp, DoA_mean_temp;
	double ccv_thre_LR_mean=0.0;
	double ccv_thre_LR_max=0.0;
	double ccv_thre_LC_mean=0.0;
	double ccv_thre_LC_max=0.0;
	double ccv_thre_CR_mean=0.0;
	double ccv_thre_CR_max=0.0;	

	double ccv_LR_mean[ITD_size2];
	double ccv_LC_mean[ITD_size3];
	double ccv_CR_mean[ITD_size3];
	
	short *x_L = in_L;
	short *x_R = in_R;
	short *x_C = in_C;

	int ccv_LR_max_idx=-1;
	int ccv_LC_max_idx=-1;
	int ccv_CR_max_idx=-1;

	double D_l_LR;
	double D_l_LC;
	double D_l_CR;

	double D_g_LR[2];
	double D_g_LC[2];
	double D_g_CR[2];	

	double E[8];
	double E_min_temp=1000.0;

	*DoA_mean_p = -1.0;

	short *xin = in_C;

	float thresold_dB_ssl_off = sslInst->gun_trig_thresold;
	float thresold_dB_ssl_on = sslInst->gun_trig_thresold2;

	int peak = 0;

	int16_t* buffers[] = { in_L, in_R, in_C};
	dBFSResult result = calculate_dBFS_peak_and_ratio_multibuf(buffers, ssl_blocksize, 3);
	float e_curr_peak_dB = result.dBFS;
	float e_curr_peak_ratio = result.ratio;

	result = calculate_dBFS_rms_and_ratio_multibuf(buffers, ssl_blocksize, 4);
	float  e_curr_rms_dB = result.dBFS;
	float  e_curr_rms_ratio = result.ratio;

	g_noise_floor = update_noise_floor(e_curr_rms_ratio, e_curr_peak_ratio, &g_noise_floor, thresold_dB_ssl_on);
	double noise_floor_dbfs = calculate_dbfs(g_noise_floor);

	double max_peak_dbfs = calculate_dbfs(g_max_peak);

	// 피크 레벨 계산 및 업데이트
	if (e_curr_peak_dB > noise_floor_dbfs + thresold_dB_ssl_on) {
		if ((max_peak_dbfs+1.0)<=e_curr_peak_dB) peakmax_ccv_idx=ccv_idx;
	}

	g_max_peak = update_peak_max(noise_floor_dbfs, e_curr_rms_ratio, e_curr_peak_ratio, &g_max_peak, thresold_dB_ssl_on);
	max_peak_dbfs = calculate_dbfs(g_max_peak);
	
	if (e_curr_peak_dB < (noise_floor_dbfs + thresold_dB_ssl_on/4.0)) {
		 max_peak_dbfs = -90;
	}
	
	if (g_e_curr_dB<e_curr_peak_dB){
		g_e_curr_dB=e_curr_peak_dB;
	}	
	if (g_max_peak_dB<max_peak_dbfs){
		g_max_peak_dB=max_peak_dbfs;
	}

// #ifdef PRINT_DEBUG_SSL	
// 	printf("g_max_peak=%.2f, e_curr_peak_dB=%.2f, e_curr_rms_dB=%.2f noise_floor_dbfs=%.2f\r\n", max_peak_dbfs, e_curr_peak_dB, e_curr_rms_dB, noise_floor_dbfs);
// #endif

	if ((g_ssl_debug_on==1)&&(g_ssl_debug_snd_idx==0)){

		FloatPacket packet;
		packet.val1 = (float)g_e_curr_dB;
		packet.val2 = (float)g_max_peak_dB;
		packet.val3 = (float)e_curr_rms_dB;
		packet.val4 = (float)noise_floor_dbfs;

		int result = debug_matlab_ssl_float_packet_send(8, packet);
		g_e_curr_dB = -100.0;
		g_max_peak_dB = -100.0;
	}

	int ssl_vad=0;
	sslhold--;
	if (sslhold<=0) sslhold=0;
	if (sslhold==0) {
		if (((e_curr_rms_dB > (noise_floor_dbfs+thresold_dB_ssl_on))&&(e_curr_peak_dB>max_peak_dbfs+thresold_dB_ssl_off))||((ccv_idx>0)&&(ccv_idx<sslInst->ccv_last_num)))
		{	
			ssl_vad = 1;		
			if (e_curr_peak_dB>-3.0) peak = 1;	
	#ifdef PRINT_DEBUG_SSL	
			printf("\n\n");
			printf("*************************************************************************\r\n");	
			printf("peakmax_ccv_idx=%d ", peakmax_ccv_idx);
			printf("ccv_idx = %d, ccv_stored= %d ", ccv_idx, ccv_stored);
			// printf("g_max_peak=%.2f, e_curr_peak_dB=%.2f, e_curr_rms_dB=%.2f", max_peak_dbfs, e_curr_peak_dB, e_curr_rms_dB);
			printf("g_max_peak=%.2f, e_curr_peak_dB=%.2f, e_curr_rms_dB=%.2f noise_floor_dbfs=%.2f\r\n", max_peak_dbfs, e_curr_peak_dB, e_curr_rms_dB, noise_floor_dbfs);
			printf("thresold_dB_ssl_on=%f, thresold_dB_ssl_off=%f \r\n",thresold_dB_ssl_on,thresold_dB_ssl_off  );
			if (peak == 1) printf(" input peak!!");		
			printf(" \n\n");		
	#endif				
		}
	} 

	if ((ssl_vad>0)){		

		/*-------------------- make FFTs --------------------*/

		for (i=0; i<ssl_blocksize; i++)
			X_L[i]=(int)x_L[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_R[i]=(int)x_R[i]*hanning_w[i];
		for (i=0; i<ssl_blocksize; i++)
			X_C[i]=(int)x_C[i]*hanning_w[i];			

		
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_L[i] = ((float)X_L[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_R[i] = ((float)X_R[i])*deno;
		for (i=0; i<ssl_blocksize; i++)
			sslFFTinput_C[i] = ((float)X_C[i])*deno;		

		pffft_transform_ordered(ssl_L_p, sslFFTinput_L, sslFFT_L, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_R_p, sslFFTinput_R, sslFFT_R, pffftsslwork, PFFFT_FORWARD);
		pffft_transform_ordered(ssl_C_p, sslFFTinput_C, sslFFT_C, pffftsslwork, PFFFT_FORWARD);

        ccv_stored++;
        if (ccv_stored>ccv_avg_size)
            ccv_stored=ccv_avg_size;

#ifdef PRINT_DEBUG_SSL	
		printf("ccv_idx = %d, ccv_stored= %d ", ccv_idx, ccv_stored);
		printf("g_max_peak=%.2f, e_curr_peak_dB=%.2f, e_curr_rms_dB=%.2f", g_max_peak, e_curr_peak_dB, e_curr_rms_dB);
		if (peak == 1) printf(" input peak!!");		
		printf(" \n\n");		
#endif				

		peaklevel[ccv_idx]=(double)(e_curr_rms_ratio);

		/*-------------------- make ccv_LC --------------------*/
		int Nbytes = 2 * fsup_ssl_blocksize * sizeof(float);
		memset(CCVF, 0, Nbytes);

	    calculate_CCVF(sslFFT_L, sslFFT_C, CCVF, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, -20);

		//memcpy(ccv_temp, CCVF, Nbytes);
		//calculate_CCVF(sslFFT_C, sslFFT_R, CCVF, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize);
		//for (i=0; i<2 * fsup_ssl_blocksize; i++){
		//	CCVF[i]=(CCVF[i]+ccv_temp[i])*0.5;
		//}

		pffft_transform_ordered(ssl_ccv_LC_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max3; i++){			
			// ccv_LC[ccv_idx][ITD_max3+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			// ccv_LC[ccv_idx][ITD_max3+i]=sqrt(ccv_LC[ccv_idx][ITD_max3+i])*deno2;
			ccv_LC[ccv_idx][ITD_max3+i]=(double)(ccv_temp[2*i]);
			ccv_LC[ccv_idx][ITD_max3+i]=(ccv_LC[ccv_idx][ITD_max3+i])*deno2;			
		}

		m=fsup_ssl_blocksize-ITD_max3;
		for (i=0; i<ITD_max3; i++){
			// ccv_LC[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			// ccv_LC[ccv_idx][i]=sqrt(ccv_LC[ccv_idx][i])*deno2;
			ccv_LC[ccv_idx][i]=(double)(ccv_temp[2*m]);
			ccv_LC[ccv_idx][i]=(ccv_LC[ccv_idx][i])*deno2;			
			m++;
		}

		/*-------------------- make ccv_CR --------------------*/
		memset(CCVF, 0, Nbytes);

		calculate_CCVF(sslFFT_C, sslFFT_R, CCVF, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, -20);

		pffft_transform_ordered(ssl_ccv_CR_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max3; i++){			
			// ccv_CR[ccv_idx][ITD_max3+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			// ccv_CR[ccv_idx][ITD_max3+i]=sqrt(ccv_CR[ccv_idx][ITD_max3+i])*deno2;
			ccv_CR[ccv_idx][ITD_max3+i]=(double)(ccv_temp[2*i]);
			ccv_CR[ccv_idx][ITD_max3+i]=(ccv_CR[ccv_idx][ITD_max3+i])*deno2;			
		}

		m=fsup_ssl_blocksize-ITD_max3;
		for (i=0; i<ITD_max3; i++){
			// ccv_CR[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			// ccv_CR[ccv_idx][i]=sqrt(ccv_CR[ccv_idx][i])*deno2;
			ccv_CR[ccv_idx][i]=(double)(ccv_temp[2*m]);
			ccv_CR[ccv_idx][i]=(ccv_CR[ccv_idx][i])*deno2;			
			m++;
		}


		/*-------------------- make ccv_LR --------------------*/
		memset(CCVF, 0, Nbytes);
		calculate_CCVF(sslFFT_L, sslFFT_R, CCVF, sslInst->fbin_min, sslInst->fbin_max, fsup_ssl_blocksize, -20);

		pffft_transform_ordered(ssl_ccv_LR_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (i=0; i<=ITD_max2; i++){
			// ccv_LR[ccv_idx][ITD_max2+i]=(double)(ccv_temp[2*i]*ccv_temp[2*i]+ccv_temp[2*i+1]*ccv_temp[2*i+1]);
			// ccv_LR[ccv_idx][ITD_max2+i]=sqrt(ccv_LR[ccv_idx][ITD_max2+i])*deno2;
			ccv_LR[ccv_idx][ITD_max2+i]=(double)(ccv_temp[2*i]);
			ccv_LR[ccv_idx][ITD_max2+i]=(ccv_LR[ccv_idx][ITD_max2+i])*deno2;

		}

		m=fsup_ssl_blocksize-ITD_max2;
		for (i=0; i<ITD_max2; i++){
			// ccv_LR[ccv_idx][i]=(double)(ccv_temp[2*m]*ccv_temp[2*m]+ccv_temp[2*m+1]*ccv_temp[2*m+1]);
			// ccv_LR[ccv_idx][i]=sqrt(ccv_LR[ccv_idx][i])*deno2;
			ccv_LR[ccv_idx][i]=(double)(ccv_temp[2*m]);
			ccv_LR[ccv_idx][i]=(ccv_LR[ccv_idx][i])*deno2;			
			m++;
		}	

        if (ccv_stored>=sslInst->ccv_stored_min){

			double peaktotal=0;


			/*-------------------- make ccv_LR_mean ccv_thre_LR_max--------------------*/

			for (n=0; n<ITD_size2; n++){ 
				ccv_LR_mean[n]=0.0;
				peaktotal=0;

				for (i=0; i<ccv_stored; i++){
					ccv_LR_mean[n]+=ccv_LR[i][n]*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-i)));
					// ccv_LR_mean[n]+=ccv_LR[ccv_idx][n];
					peaktotal += peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-i)));
				}

				peaktotal = 1.0/peaktotal;
				ccv_LR_mean[n]=ccv_LR_mean[n]*peaktotal;
			}

			for (i=0; i<ITD_size2; i++){
				ccv_thre_LR_mean+=ccv_LR_mean[i];	
			}
			ccv_thre_LR_mean=ccv_thre_LR_mean/(double)ITD_size2;

			for (i=0; i<ITD_size2; i++){				
				
				if (ccv_thre_LR_max<=ccv_LR_mean[i]){
					ccv_thre_LR_max=ccv_LR_mean[i];
					ccv_LR_max_idx=i;
				}
			}

			/*-------------------- make ccv_LC_mean ccv_thre_LC_max --------------------*/
			/*-------------------- make ccv_CR_mean ccv_thre_CR_max --------------------*/

			for (n=0; n<ITD_size3; n++){ 
				ccv_LC_mean[n]=0.0;
				ccv_CR_mean[n]=0.0;
				peaktotal=0;

				for (i=0; i<ccv_stored; i++){
					ccv_LC_mean[n]+=ccv_LC[i][n]*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-i)));
					ccv_CR_mean[n]+=ccv_CR[i][n]*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-i)));
					// ccv_LC_mean[n]=ccv_LC[ccv_idx][n];
					// ccv_CR_mean[n]=ccv_CR[ccv_idx][n];
					peaktotal += peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-i)));
				}

				peaktotal = 1.0/peaktotal;	
							
				ccv_LC_mean[n]=ccv_LC_mean[n]*peaktotal;					
				ccv_CR_mean[n]=ccv_CR_mean[n]*peaktotal;	
			}			

			for (i=0; i<ITD_size3; i++){
				ccv_thre_LC_mean+=ccv_LC_mean[i];	
				ccv_thre_CR_mean+=ccv_CR_mean[i];	
			}

			ccv_thre_LC_mean=ccv_thre_LC_mean/(double)ITD_size3;					
			ccv_thre_CR_mean=ccv_thre_CR_mean/(double)ITD_size3;					

			for (i=0; i<ITD_size3; i++){
				if (ccv_thre_LC_max<=ccv_LC_mean[i]){
					ccv_thre_LC_max=ccv_LC_mean[i];
					ccv_LC_max_idx=i;
				}

				if (ccv_thre_CR_max<=ccv_CR_mean[i]){
					ccv_thre_CR_max=ccv_CR_mean[i];
					ccv_CR_max_idx=i;
				}				
			}

#ifdef PRINT_DEBUG_SSL			
			printf("ccv_thre_LR_max=%.5f(%d), ccv_thre_LR_mean(%.5f)+Cth=%.5f\n", ccv_thre_LR_max, ccv_LR_max_idx, (ccv_thre_LR_mean),  (ccv_thre_LR_mean+sslInst->CCVth));
			printf("ccv_thre_LC_max=%.5f(%d), ccv_thre_LC_mean(%.5f)+Cth=%.5f\n", ccv_thre_LC_max, ccv_LC_max_idx, (ccv_thre_LC_mean),  (ccv_thre_LC_mean+sslInst->CCVth) );
			printf("ccv_thre_CR_max=%.5f(%d), ccv_thre_CR_mean(%.5f)+Cth=%.5f\n", ccv_thre_CR_max, ccv_CR_max_idx, (ccv_thre_CR_mean),  (ccv_thre_CR_mean+sslInst->CCVth) );
			printf("\n");
#endif			


			// if ((g_ssl_debug_on==1)&&(g_ssl_debug_snd_idx==0)){
#ifdef SEND_DEBUG_CCVF				
			if ((g_ssl_debug_on==1)){				
				// debug_matlab_ccv_double(ccv_LR_mean, 0, ITD_size2);
				// debug_matlab_ccv_double(ccv_LC_mean, 1, ITD_size3);

				debug_matlab_ccv_double2(ccv_LR[ccv_idx], peaklevel[ccv_idx]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx))), 0, ITD_size2);
				debug_matlab_ccv_double2(ccv_LC[ccv_idx], peaklevel[ccv_idx]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx))), 1, ITD_size3);
				debug_matlab_ccv_double2(ccv_CR[ccv_idx], peaklevel[ccv_idx]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx))), 2, ITD_size3);

				debug_matlab_ccv_send(0, ITD_size2);
				debug_matlab_ccv_send(1, ITD_size3);
				debug_matlab_ccv_send(2, ITD_size3);
			}
#endif			

			int except_LR = 0;
			int except_LC = 0;
			int except_CR = 0;

			if ((ccv_thre_LR_max>ccv_thre_LR_mean+sslInst->CCVth)){
				D_l_LR=DoA_range2[ccv_LR_max_idx];
				if (abs(D_l_LR)>=max_DoA_local_wall){
					except_LR = 1;
				}
			} else {
				D_l_LR=DoA_range2[ccv_LR_max_idx];
				except_LR = 1;
			}

			if ((ccv_thre_LC_max>ccv_thre_LC_mean+sslInst->CCVth)){
				D_l_LC=DoA_range3[ccv_LC_max_idx];
				if (abs(D_l_LC)>=max_DoA_local_wall){
					except_LC = 1;
				}				
			} else {
				D_l_LC=DoA_range3[ccv_LC_max_idx];
				except_LC = 1;
			}

			if ((ccv_thre_CR_max>ccv_thre_CR_mean+sslInst->CCVth)){
				D_l_CR=DoA_range3[ccv_CR_max_idx];
				if (abs(D_l_CR)>=max_DoA_local_wall){
					except_CR = 1;
				}				
			} else {
				D_l_CR=DoA_range3[ccv_CR_max_idx];
				except_CR = 1;
			}

#ifdef PRINT_DEBUG_SSL					
			printf("except = %d, %d, \n", except_LR, except_LC);		
#endif	

			D_g_LR[0]=D_l_LR;
			D_g_LR[1]=D_l_LR;

			D_g_LC[0]=D_l_LC;
			D_g_LC[1]=D_l_LC;

			D_g_CR[0]=D_l_CR;
			D_g_CR[1]=D_l_CR;


#ifdef PRINT_DEBUG_SSL	
				printf("D_l_LR=[%3.1f, %3.1f, %3.1f] \n", D_l_LR, D_g_LR[0], D_g_LR[1]);
				printf("D_l_LC=[%3.1f, %3.1f, %3.1f] \n", D_l_LC, D_g_LC[0], D_g_LC[1]);
				printf("D_l_CR=[%3.1f, %3.1f, %3.1f] \n", D_l_CR, D_g_CR[0], D_g_CR[1]);
				printf("\n");
#endif			

			idx_pick=-1;
			double temp_diff;
			double temp1;
			double temp2;

			for (i=0; i<8 ; i++){
				p=E_idx_wall[i][0];
				q=E_idx_wall[i][1];
				r=E_idx_wall[i][2];
				E[i] = -1;

				if ((except_LR + except_LC) == 0){

					temp_diff=fabs(D_g_LR[p]-D_g_LC[q]);
					E[i] += temp_diff;
				}

				if ((except_LC + except_CR) == 0){

					temp_diff=fabs(D_g_LC[q]-D_g_CR[r]);
					E[i] += temp_diff;
				}

				if ((except_CR + except_LR) == 0){

					temp_diff=fabs(D_g_CR[r]-D_g_LR[p]);
					E[i] += temp_diff;
				}

				if (E[i]==-1) continue;

				if (E[i]<E_min_temp){
					E_min_temp=E[i];
					idx_pick=i;
				}
			}



#ifdef PRINT_DEBUG_SSL	
			printf("E[idx_pick]=%f\r\n", E_idx_wall[idx_pick]);

			p=E_idx_wall[idx_pick][0];
			q=E_idx_wall[idx_pick][1];
			r=E_idx_wall[idx_pick][2];

			if ((except_LR + except_LC) == 0){
				temp_diff=fabs(D_g_LR[p]-D_g_LC[q]);
				printf("D_g_LR[p]-D_g_LC[q]=%f\r\n", temp_diff);
			}

			if ((except_LC + except_CR) == 0){

				temp_diff=fabs(D_g_LC[q]-D_g_CR[r]);
				printf("D_g_LC[q]-D_g_CR[r]=%f\r\n", temp_diff);
			}

			if ((except_CR + except_LR) == 0){

				temp_diff=fabs(D_g_CR[r]-D_g_LR[p]);
				printf("D_g_CR[r]-D_g_LR[p]=%f\r\n", temp_diff);
			}
#endif				

			int DoA_cnt=0;
			double DoA=0.0;

			if (idx_pick>-1){

				double arr[4];
				
				if (except_LR == 0){
					
					if (fabs(D_l_LR)<max_DoA_local_wall){
						DoA+=D_g_LR[E_idx_wall[idx_pick][0]];
						arr[DoA_cnt]=D_g_LR[E_idx_wall[idx_pick][0]];
						DoA_cnt++;
						// printf("D_g_LR[E_idx_wall[%d][0]]=%f, DoA_cnt=%d \r\n", idx_pick, D_g_LR[E_idx_wall[idx_pick][0]], DoA_cnt);
					}
				}
				
				if (except_LC == 0){
					
					if (fabs(D_l_LC)<max_DoA_local_wall){
						DoA+=D_g_LC[E_idx_wall[idx_pick][1]];
						arr[DoA_cnt]=D_g_LC[E_idx_wall[idx_pick][1]];
						DoA_cnt++;
						// printf("D_g_LC[E_idx_wall[%d][1]]=%f, DoA_cnt=%d \r\n", idx_pick, D_g_LC[E_idx_wall[idx_pick][1]], DoA_cnt);
					}
				}

				if (except_CR == 0){
					
					if (fabs(D_l_CR)<max_DoA_local_wall){
						DoA+=D_g_CR[E_idx_wall[idx_pick][1]];
						arr[DoA_cnt]=D_g_CR[E_idx_wall[idx_pick][1]];
						DoA_cnt++;
						// printf("D_g_CR[E_idx_wall[%d][1]]=%f, DoA_cnt=%d \r\n", idx_pick, D_g_CR[E_idx_wall[idx_pick][1]], DoA_cnt);
					}
				}								

				if (DoA_cnt==4){
					find_outlier_and_average(arr, DoA_cnt, &DoA_mean_temp);
				} else {
					if (DoA_cnt>0)
						DoA_mean_temp=DoA/(double)DoA_cnt;
				}
				
#ifdef PRINT_DEBUG_SSL									
				printf("idx_pick=%d, E_idx=[%d, %d, %d] \n",idx_pick, E_idx_wall[idx_pick][0], E_idx_wall[idx_pick][1], E_idx_wall[idx_pick][2]);
				printf("DoA_mean_temp= %.2f\r\n", DoA_mean_temp);
				// printf("peaklevel[ccv_idx]= %.2f\r\n", peaklevel[ccv_idx]);
#endif
				
				if ((g_ssl_debug_on==1)){				

					debug_matlab_doa_send(7, DoA_mean_temp);
// #ifdef SEND_DEBUG_CCVF
// 					if (peakmax_ccv_idx== ccv_idx) {
// 						debug_matlab_ssl_float_send(10, ccv_thre_LR_mean*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)))+sslInst->CCVth);
// 						debug_matlab_ssl_float_send(11, ccv_thre_LC_mean*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)))+sslInst->CCVth);
// 						debug_matlab_ssl_float_send(12, ccv_thre_CR_mean*peaklevel[i]*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)))+sslInst->CCVth);
// 					}
// #endif					
					// for (i=0; i<ITD_size; i++){
					// 	printf("ccv_LR_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_LR_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
					// }
					
				}

			}



        }


		ccv_idx++;
		if (ccv_idx>=ccv_avg_size)
			ccv_idx=0;
	
	} else {

		// if (g_ssl_debug_on==1){
		// 	debug_matlab_doa_send(7, -60.0);
		// }


		for (i=0; i<ccv_avg_size; i++){
			for (m=0; m<ITD_size2; m++){
				ccv_LR[i][m]=0.0;
			}

			for (m=0; m<ITD_size3; m++){		
				ccv_LC[i][m]=0.0;
				ccv_CR[i][m]=0.0;				
			}			
		}
        ccv_idx=0;
        ccv_stored=0;
		peakmax_ccv_idx=0;
	
	}

	if ((prev_vad == 0) && (ssl_vad==1)){
		prev_vad = 1;

		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}					
					cluster[i][0] = DoA_temp;
					cluster_power[i][0] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
					cluster_ccvidx[i][0] = ccv_idx-1;
					
					// printf("cluster[%d][%d]=%f cluster_power[%d][%d] = %f;\r\n", i, 0, cluster[i][0], i, 0, cluster_power[i][0]);

					cluster_idx[i]++;
					break;
				} else {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}
					
					if (fabs(cluster[i][cluster_idx[i]-1]-DoA_temp) <= 30.0){
						cluster[i][cluster_idx[i]] = DoA_temp;
						cluster_power[i][cluster_idx[i]] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
						cluster_ccvidx[i][cluster_idx[i]] = ccv_idx-1;
					
						// printf("cluster[%d][%d]=%f cluster_power[%d][%d] = %f;\r\n", i, cluster_idx[i], cluster[i][0], i, cluster_idx[i], cluster_power[i][0]);
						// printf("cluster[%d][%d]=%f\r\n", i, cluster_idx[i], cluster[i][cluster_idx[i]]);

						cluster_idx[i]++;
						break;
					}
				}

			}
		}		

	} else if ((prev_vad == 1) && (ssl_vad==1)){
		prev_vad = 1;
		if (idx_pick>-1){
			for (i=0 ; i<5 ; i++){
				if (cluster_idx[i]==0) {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}

					cluster[i][0] = DoA_temp;
					cluster_power[i][0] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
					cluster_ccvidx[i][0] = ccv_idx-1;
					
					// printf("cluster[%d][%d]=%f cluster_power[%d][%d] = %f;\r\n", i, 0, cluster[i][0], i, 0, cluster_power[i][0]);					
					// printf("cluster[%d][%d]=%f\r\n", i, 0, cluster[i][0]);
					
					cluster_idx[i]++;
					
					break;
				} else {

					if (DoA_mean_temp>350.0) {
						DoA_temp = DoA_mean_temp - 360.0;
					} else {
						DoA_temp = DoA_mean_temp;
					}

					if (fabs(cluster[i][cluster_idx[i]-1]-DoA_temp) <= 20.0){
				
						cluster[i][cluster_idx[i]] = DoA_temp;
						cluster_power[i][cluster_idx[i]] = e_curr_rms_ratio; //*pow(0.8, fabs((double)(peakmax_ccv_idx-ccv_idx)));
						cluster_ccvidx[i][cluster_idx[i]] = ccv_idx-1;
					
						// printf("cluster[%d][%d]=%f cluster_power[%d][%d] = %f;\r\n", i, cluster_idx[i], cluster[i][0], i, cluster_idx[i], cluster_power[i][0]);						
						// printf("cluster[%d][%d]=%f\r\n", i, cluster_idx[i], cluster[i][cluster_idx[i]]);

						cluster_idx[i]++;
						break;
					}
				}

			}
		}
	} else if ((prev_vad == 1) && (ssl_vad==0)){
		prev_vad = 0;

		int total_cluster = 0;
		int max_cluster = 0;
		double max_cluster_power = 0.0;
		int max_cluster_idx = 0;

		for (i=0; i<5 ; i++){
			if (max_cluster <= cluster_idx[i]){
				max_cluster = cluster_idx[i];
				max_cluster_idx = i;
				total_cluster += cluster_idx[i];
			}
		}



		for (i=0; i<5 ; i++){
			// if (max_cluster!=1){
			// 	if (cluster_idx[i]<=1) continue;
			// }
				
			double cluster_power_avr=0.0;
			for (j=0; j<cluster_idx[i]; j++){
				// cluster_power_avr+=cluster_power[i][j];
				if (cluster_power_avr<=cluster_power[i][j]) cluster_power_avr=cluster_power[i][j];
			}
			// cluster_power_avr=cluster_power_avr/(double)(j);

			if (max_cluster_power <= cluster_power_avr){
				max_cluster_power=cluster_power_avr;
				max_cluster_idx = i;
				max_cluster = cluster_idx[i];
			};

			// if (max_cluster <= cluster_idx[i]){
			// 	max_cluster = cluster_idx[i];
			// 	max_cluster_idx = i;
			// }
		}

		if ((max_cluster>0)&&(total_cluster>1)){
			double mean_Doa = 0.0;
			for (i=0; i<cluster_idx[max_cluster_idx]; i++){
				mean_Doa += cluster[max_cluster_idx][i];
			}
			mean_Doa /= cluster_idx[max_cluster_idx];

			// if (mean_Doa<0) mean_Doa=mean_Doa+360.0;

			*DoA_mean_p = mean_Doa;
			// printf("mean DoA = %.2f \r\n", mean_Doa);
			// int Beamno = ((int)(*DoA_mean_p + 22.5) / (int)45) % 8;
			// printf("DoA = %.2f degree, Beam no = %d \r\n", *DoA_mean_p, Beamno );

			if (g_ssl_debug_on==1){
				// debug_matlab_ccv_double(ccv_LR_mean, 0, ITD_size2);
				// debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
				// debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
				// debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);

				// debug_matlab_ccv_send(0, ITD_size2);
				// debug_matlab_ccv_send(1, ITD_size);
				// debug_matlab_ccv_send(2, ITD_size);	
				// debug_matlab_ccv_send(3, ITD_size2);	

				debug_matlab_doa_send(14, *DoA_mean_p);
				// for (i=0; i<ITD_size; i++){
				// 	printf("ccv_LR_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_LR_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
				// }
				
			}

			for (i=0 ; i<5 ; i++){
#ifdef PRINT_DEBUG_SSL	
				for (j=0; j<cluster_idx[i]; j++){
					printf("final cluster[%d][%d]=%.2f, cluster_power[%d][%d]=%f cluster_ccvidx[%d][%d]=%d\r\n", i, j, cluster[i][j], i, j, cluster_power[i][j], i, j, cluster_ccvidx[i][j]);
				}
#endif
			}	
			

		}

		for (i=0 ; i<5 ; i++){
			cluster_idx[i] = 0;	
		}	
		sslhold += 128;			
	}

	int max_cluster = 0;
	double max_cluster_power = 0.0;
	int max_cluster_idx = 0;
	// for (i=0; i<5 ; i++){
	// 	if (max_cluster <= cluster_idx[i]){
	// 		max_cluster = cluster_idx[i];
	// 		max_cluster_idx = i;
	// 	}
	// }	
	for (i=0; i<5 ; i++){
		if (max_cluster <= cluster_idx[i]){
			max_cluster = cluster_idx[i];
			max_cluster_idx = i;
		}
	}

	for (i=0; i<5 ; i++){
		// if (max_cluster!=1){
		// 	if (cluster_idx[i]<=1) continue;
		// }

		double cluster_power_avr=0.0;

		for (j=0; j<cluster_idx[i]; j++){
			// cluster_power_avr+=cluster_power[i][j];
			if (cluster_power_avr<=cluster_power[i][j]) cluster_power_avr=cluster_power[i][j];
		}
		// cluster_power_avr=cluster_power_avr/(double)(j);

		if (max_cluster_power <= cluster_power_avr){
			max_cluster_power=cluster_power_avr;
			max_cluster_idx = i;
		};
		// if (max_cluster <= cluster_idx[i]){
		// 	max_cluster = cluster_idx[i];
		// 	max_cluster_idx = i;
		// }
	}		

	if (max_cluster>15) {
		prev_vad = 0;


		double mean_Doa = 0.0;
		for (i=0; i<cluster_idx[max_cluster_idx]; i++){
			mean_Doa += cluster[max_cluster_idx][i];
		}
		mean_Doa /= cluster_idx[max_cluster_idx];

		// if (mean_Doa<0) mean_Doa=mean_Doa+360.0;
		
		*DoA_mean_p = mean_Doa;
		// printf("mean DoA = %.2f \r\n", mean_Doa);
		// int Beamno = ((int)(*DoA_mean_p + 22.5) / (int)45) % 8;
    	// printf("DoA = %.2f degree, Beam no = %d \r\n", *DoA_mean_p, Beamno );

		if (g_ssl_debug_on==1){
			// debug_matlab_ccv_double(ccv_LR_mean, 0, ITD_size2);
			// debug_matlab_ccv_double(ccv_LF_mean, 1, ITD_size);
			// debug_matlab_ccv_double(ccv_FR_mean, 2, ITD_size);
			// debug_matlab_ccv_double(ccv_FB_mean, 3, ITD_size2);

			// debug_matlab_ccv_send(0, ITD_size2);
			// debug_matlab_ccv_send(1, ITD_size);
			// debug_matlab_ccv_send(2, ITD_size);
			// debug_matlab_ccv_send(3, ITD_size2);

			debug_matlab_doa_send(14, *DoA_mean_p);
			// for (i=0; i<ITD_size; i++){
			// 	printf("ccv_LR_mean=%f, ccv_LF_mean=%f, ccv_FR_mean=%f\n", ccv_LR_mean[i], ccv_LF_mean[i], ccv_FR_mean[i]);
			// }
			
		}

		for (i=0 ; i<5 ; i++){
#ifdef PRINT_DEBUG_SSL				
			for (j=0; j<cluster_idx[i]; j++){
				printf("final cluster[%d][%d]=%.2f, cluster_power[%d][%d]=%f cluster_ccvidx[%d][%d]=%d\r\n", i, j, cluster[i][j], i, j, cluster_power[i][j], i, j, cluster_ccvidx[i][j]);
			}
#endif			
			cluster_idx[i] = 0;
		}
		sslhold += 128;

	}

	if ((g_ssl_debug_on==1)){
		g_ssl_debug_snd_idx++;

		if (g_ssl_debug_snd_idx>=g_ssl_debug_snd_period) {
			g_ssl_debug_snd_idx = 0;
		}

	}

}

void calculate_ccv(PFFFT_Setup *ssl_ccv_p, double * ccv,
					float* FFT1, float* FFT2, float* CCVF,
					int colsize, int fbin_min, int fbin_max, int fsup_ssl_blocksize,
					double gate_dB) {

		/*-------------------- make ccv_LF --------------------*/
		int Nbytes = 2 * fsup_ssl_blocksize * sizeof(float);
		memset(CCVF, 0, Nbytes);
	    calculate_CCVF(FFT1, FFT2, CCVF, fbin_min, fbin_max, fsup_ssl_blocksize, gate_dB);
		pffft_transform_ordered(ssl_ccv_p, CCVF, ccv_temp, pffftccvwork, PFFFT_BACKWARD);

		for (int i=0; i<=colsize; i++){			
			ccv[colsize+i]=(double)(ccv_temp[2*i]);
			ccv[colsize+i]=(ccv[colsize+i])*deno2;			
		}

		int m=fsup_ssl_blocksize-colsize;
		for (int i=0; i<colsize; i++){
			ccv[i]=(double)(ccv_temp[2*m]);
			ccv[i]=(ccv[i])*deno2;			
			m++;
		}
}

void calculate_CCVF(float* FFT1, float* FFT2, float* CCVF,
					int fbin_min, int fbin_max, int fsup_ssl_blocksize,
					double gate_dB) {

	float temp_real[ssl_blocksize*16];
	float temp_imaj[ssl_blocksize*16];
	float temp_abs[ssl_blocksize*16];

    int m = fsup_ssl_blocksize - fbin_min;

	float abs_avr = 0.0;
	float abs_max = 0.0;
    for (int i = fbin_min; i <= fbin_max; i++) {
        temp_real[m] = FFT1[2 * i] * FFT2[2 * i] + FFT1[2 * i + 1] * FFT2[2 * i + 1];
        temp_imaj[m] = FFT1[2 * i + 1] * FFT2[2 * i] - FFT1[2 * i] * FFT2[2 * i + 1];
        temp_abs[m] = sqrtf(temp_real[m] * temp_real[m] + temp_imaj[m] * temp_imaj[m]);

		abs_avr += temp_abs[m];
		if (abs_max<temp_abs[m]) abs_max = temp_abs[m];
        m--;
    }

	abs_avr /= (float)(fbin_max-fbin_min+1);

	

	double gate_ratio = calculate_ratio(gate_dB);

	// printf("abs_avr=%f,abs_max=%f abs_max*gate_ratio=%f\r\n", abs_avr, abs_max, (abs_max*gate_ratio));

    m = fsup_ssl_blocksize - fbin_min;
    for (int i = fbin_min; i <= fbin_max; i++) {

		if ((abs_max*gate_ratio)<=temp_abs[m]) {
			CCVF[2 * i] = temp_real[m] / temp_abs[m];
			CCVF[2 * i + 1] = temp_imaj[m] / temp_abs[m];

			CCVF[2 * m] = CCVF[2 * i];
			CCVF[2 * m + 1] = -1.0 * CCVF[2 * i + 1];
		} else {
			CCVF[2 * i] = 0; //temp_real[m] / abs_max;
			CCVF[2 * i + 1] = 0; //temp_imaj[m] / abs_max;
			// CCVF[2 * i] = temp_real[m]; // / abs_max;
			// CCVF[2 * i + 1] = temp_imaj[m]; // / abs_max;			

			CCVF[2 * m] = CCVF[2 * i]; //0; //CCVF[2 * i];
			CCVF[2 * m + 1] = -1.0 * CCVF[2 * i + 1]; //0; //-1.0 * CCVF[2 * i + 1];
		}       
        m--;
    }	
}

void calculate_peak_temp_total(double *peaklevel, double *peaktemp, double *peaktotal, double r, int ccv_stored, int peakmax_ccv_idx){
	*peaktotal=0;
	for (int i=0; i<ccv_stored; i++){
		peaktemp[i] = peaklevel[i]*pow(r, fabs((double)(peakmax_ccv_idx-i)));
		*peaktotal += peaktemp[i];
	}
	*peaktotal = 1.0/(*peaktotal);
}

void calculate_ccv_mean(int mode, 
						double *pp_ccv, double *ccv_mean, 
						double *peaktemp, double peaktotal, 
						double *ccv_thre_mean, double *ccv_thre_max, int *ccv_max_idx, 
						int ccv_stored, int peakmax_ccv_idx, int itd_size){

	double *p_ccv[ccv_avg_size];
	double *p_temp = &pp_ccv[0];
	*ccv_thre_mean = 0.0;

	for (int i=0; i<ccv_stored; i++){
		p_ccv[i] = &p_temp[i*itd_size];
	}

	for (int n=0; n<itd_size; n++){ 
		ccv_mean[n]=0.0;

		if (mode == mode_gun) {
			for (int i=0; i<ccv_stored; i++){
				ccv_mean[n]+=p_ccv[i][n]*peaktemp[i];
			}
			ccv_mean[n]=ccv_mean[n]*peaktotal;

		} else if (mode == mode_normal) {
			for (int i=0; i<ccv_stored; i++){
				ccv_mean[n]+=p_ccv[i][n];
			}
			ccv_mean[n]=ccv_mean[n]/ccv_stored;			

		} else if (mode == mode_noavg) {
			ccv_mean[n]=ccv_RL[ccv_stored-1][n];
		} else {
			printf("not support mode\r\n");
			for (int i=0; i<ccv_stored; i++){
				ccv_mean[n]+=p_ccv[i][n];
			}
			ccv_mean[n]=ccv_mean[n]/ccv_stored;				
		}

		*ccv_thre_mean+=ccv_mean[n];	
		if (*ccv_thre_max<=ccv_mean[n]){
			*ccv_thre_max=ccv_mean[n];
			*ccv_max_idx=n;										
		}			
	}

	*ccv_thre_mean=*ccv_thre_mean/(double)(itd_size);
}

#define MAX_INT16 32767.0f
#define NUM_SAMPLES 256

// dBFS Peak 계산 함수
dBFSResult calculate_dBFS_peak_and_ratio(int16_t samples[], size_t num_samples) {
    int16_t max_value = 0;

    // 샘플의 최대 절대값 찾기
    for (size_t i = 0; i < num_samples; ++i) {
        if (abs(samples[i]) > max_value) {
            max_value = abs(samples[i]);
        }
    }

    dBFSResult result;

    // 최대값이 0일 때 (무음)
    if (max_value == 0) {
        result.dBFS = -INFINITY; // 무한대 음수
        result.ratio = 0.0f;
    } else {
        // 비율 계산
        result.ratio = (float)max_value / MAX_INT16;
        // dBFS 계산
        result.dBFS = 20.0f * log10f(result.ratio);
    }

    return result;
}

dBFSResult calculate_dBFS_peak_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers) {
    int16_t max_value = 0;

    // Iterate over each buffer
    for (size_t buffer_idx = 0; buffer_idx < num_buffers; ++buffer_idx) {
        // Iterate over each sample in the current buffer
        for (size_t i = 0; i < num_samples; ++i) {
            if (abs(buffers[buffer_idx][i]) > max_value) {
                max_value = abs(buffers[buffer_idx][i]);
            }
        }
    }

    dBFSResult result;

    // If the maximum value is 0, it means silence
    if (max_value == 0) {
        result.dBFS = -INFINITY; // Negative infinity for silence
        result.ratio = 0.0f;
    } else {
        // Calculate the ratio of the peak value to the maximum possible value
        result.ratio =  (float)max_value / MAX_INT16;
        // Calculate the dBFS value
        result.dBFS = 20.0f * log10f(result.ratio);
    }

    return result;
}

// dBFS 계산 함수
float calculate_dBFS_peak(int16_t samples[], size_t num_samples) {
    int16_t max_value = 0;
    
    // 샘플의 최대 절대값 찾기
    for (size_t i = 0; i < num_samples; ++i) {
        if (abs(samples[i]) > max_value) {
            max_value = abs(samples[i]);
        }
    }

    // 최대값이 0일 때 (무음)
    if (max_value == 0) {
        return -INFINITY; // 무한대 음수
    }

    // dBFS 계산
    float dBFS = 20.0f * log10f((float)max_value / MAX_INT16);
    return dBFS;
}

// dBFS RMS 계산 함수
float calculate_dBFS_rms(int16_t samples[], size_t num_samples) {
    float sum_squares = 0.0f;

    // 샘플의 제곱의 합 계산
    for (size_t i = 0; i < num_samples; ++i) {
        sum_squares += (float)samples[i] * (float)samples[i];
    }

    // RMS 계산
    float rms = sqrtf(sum_squares / num_samples);

    // 최대값이 0일 때 (무음)
    if (rms == 0) {
        return -INFINITY; // 무한대 음수
    }

    // dBFS 계산
    float dBFS = 20.0f * log10f(rms / MAX_INT16);
    return dBFS;
}

// dBFS Peak 계산 함수
dBFSResult calculate_dBFS_rms_and_ratio(int16_t samples[], size_t num_samples) {
    float sum_squares = 0.0f;

    // 샘플의 제곱의 합 계산
    for (size_t i = 0; i < num_samples; ++i) {
        sum_squares += (float)samples[i] * (float)samples[i];
    }

	dBFSResult result;

    // RMS 계산
    float rms = sqrtf(sum_squares / num_samples);

    // 최대값이 0일 때 (무음)
    if (rms == 0) {
        result.dBFS = -INFINITY; // 무한대 음수
        result.ratio = 0.0f;
    } else {
        // 비율 계산
        result.ratio = (float)rms / MAX_INT16;
        // dBFS 계산
        result.dBFS = 20.0f * log10f(result.ratio);
    }

    return result;
}

dBFSResult calculate_dBFS_rms_and_ratio_multibuf(int16_t* buffers[], size_t num_samples, size_t num_buffers) {
    float sum_squares = 0.0f;
    size_t total_samples = num_samples * num_buffers; // 전체 샘플 수 계산

    // 모든 버퍼의 샘플 제곱의 합 계산
    for (size_t buffer_idx = 0; buffer_idx < num_buffers; ++buffer_idx) {
        for (size_t i = 0; i < num_samples; ++i) {
            sum_squares += (float)buffers[buffer_idx][i] * (float)buffers[buffer_idx][i];
        }
    }

    dBFSResult result;

    // RMS 계산
    float rms = sqrtf(sum_squares / total_samples);

    // 최대값이 0일 때 (무음)
    if (rms == 0) {
        result.dBFS = -INFINITY; // 무한대 음수
        result.ratio = 0.0f;
    } else {
        // 비율 계산
        result.ratio = rms / MAX_INT16;
        // dBFS 계산
        result.dBFS = 20.0f * log10f(result.ratio);
    }

    return result;
}

int countZeroCrossings(int16_t *buffer, int size) {
    int zeroCrossings = 0;

    // 버퍼의 처음부터 끝까지 루프
    for (int i = 1; i < size; ++i) {
        // 현재 샘플과 이전 샘플이 서로 다른 부호를 가지는지 확인
        if ((buffer[i-1] >= 0 && buffer[i] < 0) || (buffer[i-1] < 0 && buffer[i] >= 0)) {
            zeroCrossings++;
        }
    }

    return zeroCrossings;
}

void find_outlier_and_average(double arr[], int size, double *average) {

    if (size != 4) {
        printf("The size of the array must be 4.\n");
        return;
    }

    double sum = 0.0;

    // Calculate the sum of all values
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }

    // Calculate the average of all values
    double overall_average = sum / size;

    double distances[4];
    double max_distance = 0.0;
    int outlier_idx = 0;

    // Calculate distances from each value to the overall average
    for (int i = 0; i < size; i++) {
        distances[i] = fabs(arr[i] - overall_average);
        if (distances[i] > max_distance) {
            max_distance = distances[i];
            outlier_idx = i;
        }
    }

	// printf("outlier_idx = %d ,", outlier_idx);

    // Calculate the average of the three closest values
    sum = 0.0;
    for (int i = 0; i < size; i++) {
        if (i != outlier_idx) {
            sum += arr[i];
			// printf("arr[%d] = %f, ", i, arr[i]);
        }
    }

    *average = sum / 3;

	// printf("average = %f\r\n", *average);
}

int find_outlier(double arr[], int size) {

    if (size != 4) {
        printf("The size of the array must be 4.\n");
        return -1;
    }

    // Calculate the average of all values
    double overall_average = phase_average(arr, size);

    double distances[4];
    double max_distance = 0.0;
    int outlier_idx = 0;

    // Calculate distances from each value to the overall average
    for (int i = 0; i < size; i++) {
        distances[i] = abs(phase_difference(arr[i] , overall_average));
        if (distances[i] > max_distance) {
            max_distance = distances[i];
            outlier_idx = i;
        }
    }

	// printf("outlier_idx = %d degree = %.2f\r\n", outlier_idx, arr[outlier_idx]);
	return outlier_idx;
}

double find_two_outliers(double arr[], int size, int* outlier_idx1, int* outlier_idx2) {

    if (size < 4) {
        printf("The size of the array must be greater than 4.\n");
        return -1000;
    }

    // Calculate the average of all values
    double overall_average = phase_average(arr, size);

    double distances[6];
    int first_max_idx = 0, second_max_idx = 0;
    double first_max_distance = 0.0, second_max_distance = 0.0;

    // Calculate distances from each value to the overall average
    for (int i = 0; i < size; i++) {
        distances[i] = abs(phase_difference(arr[i], overall_average));

        if (distances[i] > first_max_distance) {
            second_max_distance = first_max_distance;
            second_max_idx = first_max_idx;

            first_max_distance = distances[i];
            first_max_idx = i;
        } else if (distances[i] > second_max_distance) {
            second_max_distance = distances[i];
            second_max_idx = i;
        }
    }

    *outlier_idx1 = first_max_idx;
    *outlier_idx2 = second_max_idx;

    // printf("outlier_idx1 = %d degree = %.2f\r\n", *outlier_idx1, arr[*outlier_idx1]);
    // printf("outlier_idx2 = %d degree = %.2f\r\n", *outlier_idx2, arr[*outlier_idx2]);

	return overall_average;
}

#define THRESHOLD_DB 20.0
#define ALPHA 0.05 // 평활화 계수
#define ALPHA2 0.001 // 평활화 계수
#define ALPHA3 0.000001 // 평활화 계수
#define ALPHA4 0.5 // 평활화 계수



// 노이즈 플로어를 업데이트하는 함수
double update_noise_floor(double new_rms, double new_peak, double *p_noise_floor, double threshold_dB) {

    double noise_floor_dbfs = calculate_dbfs(*p_noise_floor);
    double new_rms_dbfs 	= calculate_dbfs(new_rms);
	double new_peak_dbfs    = calculate_dbfs(new_peak);

#define ALPHA_fast 0.05 // 평활화 계수
#define ALPHA_slow 0.001 // 평활화 계수

    if (new_peak_dbfs > noise_floor_dbfs + threshold_dB) {
		// 
		// printf("new_peak_dbfs=%f,\t\tnew_rms=%f,\t\tnoise_floor_dbfs=%f,\t\t *p_noise_floor=%f state=", new_peak_dbfs, new_rms_dbfs, calculate_dbfs(*p_noise_floor), *p_noise_floor);
		// printf("0\r\n");
#ifdef PRINT_DEBUG_NOISEFLOOR	
		printf("state_n=0 ");
#endif		

		return *p_noise_floor;
		
    }

    if (new_rms_dbfs > noise_floor_dbfs) {
		// IIR 필터 적용
		*p_noise_floor = (double)ALPHA_slow * new_rms + (1.0 - (double)ALPHA_slow) * (*p_noise_floor);
		// printf("new_peak_dbfs=%f,\t\tnew_rms=%f,\t\tnoise_floor_dbfs=%f,\t\t *p_noise_floor=%f state=", new_peak_dbfs, new_rms_dbfs, calculate_dbfs(*p_noise_floor), *p_noise_floor);
		// printf("1\r\n");

		// printf("noise_floor_dbfs=%f, new_rms_dbfs=%f\r\n", calculate_dbfs(*p_noise_floor), new_rms_dbfs);
#ifdef PRINT_DEBUG_NOISEFLOOR			
		printf("state_n=1 ");
#endif		

		return *p_noise_floor;
    }

	

    // IIR 필터 적용
    *p_noise_floor = (double)ALPHA_fast * new_rms + (1.0 - (double)ALPHA_fast) * (*p_noise_floor);

	if (calculate_dbfs(*p_noise_floor)<-90) {
		*p_noise_floor = calculate_ratio(-90.0);
		
	}

	// printf("new_peak_dbfs=%f,\t\tnew_rms=%f,\t\tnoise_floor_dbfs=%f,\t\t *p_noise_floor=%f state=", new_peak_dbfs, new_rms_dbfs, calculate_dbfs(*p_noise_floor), *p_noise_floor);
	// printf("2\r\n");
#ifdef PRINT_DEBUG_NOISEFLOOR	
	printf("state_n=2 ");
#endif	

    return *p_noise_floor;
}

// 노이즈 플로어를 업데이트하는 함수
double update_peak_max(double noisefloor_dB, double new_rms, double new_peak, double *p_peak_max, double threshold_dB) {

    double peak_max_dbfs = calculate_dbfs(*p_peak_max);
    double new_rms_dbfs = calculate_dbfs(new_rms);
	double new_peak_dbfs = calculate_dbfs(new_peak);

	// printf("new_peak_dbfs=%.1f,\t\tnew_rms=%.1f,\t\tnoise_floor_dbfs=%.1f,\t\t new_peak_dbfs=%.1f threshold_dB=%.1f\r\n", new_peak_dbfs, new_rms_dbfs, noisefloor_dB, new_peak_dbfs, threshold_dB);

#define ALPHA_fast2 0.5 // 평활화 계수
#define ALPHA_slow2 0.00001 // 평활화 계수

	if(new_rms_dbfs > noisefloor_dB + threshold_dB) {
		if (new_peak_dbfs >= (peak_max_dbfs)) {
			*p_peak_max = new_peak;
#ifdef PRINT_DEBUG_PEAKMAX				
			printf("state=0 ");
#endif

			return *p_peak_max;
		} else {
			// IIR 필터 적용
			// *p_peak_max = (double)ALPHA_slow2 * new_peak + (1.0 - (double)ALPHA_slow2) * *p_peak_max;
#ifdef PRINT_DEBUG_PEAKMAX			
			printf("state=1 ");
#endif			
			return *p_peak_max;
		}
	} else if ((new_rms_dbfs > noisefloor_dB + (threshold_dB / 2))) {
		// IIR 필터 적용
		*p_peak_max = (double)ALPHA_slow2 * new_peak + (1.0 - (double)ALPHA_slow2) * (*p_peak_max);	
#ifdef PRINT_DEBUG_PEAKMAX		
		printf("state=2 ");	
#endif		
		return *p_peak_max;
	}
    	
    if ((new_rms_dbfs < noisefloor_dB + (threshold_dB / 4))) {
		// IIR 필터 적용
		*p_peak_max = calculate_ratio(noisefloor_dB) ; //(double)ALPHA_fast2 * new_peak + (1.0 - (double)ALPHA_fast2) * *p_peak_max;
		// *p_peak_max = ((double)ALPHA_slow2) * calculate_ratio(noisefloor_dB) + (1.0 - (double)ALPHA_slow2) * (*p_peak_max);
#ifdef PRINT_DEBUG_PEAKMAX		
		printf("state=3 ");
#endif		
		return *p_peak_max;
    } else {
		// IIR 필터 적용
		*p_peak_max = ((double)ALPHA_fast2) * new_peak + (1.0 - (double)ALPHA_fast2) * (*p_peak_max);
#ifdef PRINT_DEBUG_PEAKMAX		
		printf("state=4 ");
#endif		
		return *p_peak_max;
	}		
}

// dBFS 값을 계산하는 함수
double calculate_dbfsfromPower(double power) {

	if (power == 0.0) return -INFINITY;

    return 10.0 * log10(power); // 16비트 오디오 최대값을 32768로 가정
}

// dBFS 값을 계산하는 함수
double calculate_dbfs(double rms) {

	if (rms == 0.0) return -INFINITY;

    return 20.0 * log10(rms); // 16비트 오디오 최대값을 32768로 가정
}

// ratio 값을 계산하는 함수
double calculate_ratio(double dbfs) {
    return pow(10.0, dbfs / 20.0); // 16비트 오디오 최대값을 32768로 가정
}

// 위상 차이를 계산하고 -180도에서 180도 사이로 조정하는 함수
double phase_difference(double phase1, double phase2) {
    double diff = phase1 - phase2;

    // 차이를 -180도에서 180도 사이로 조정
    while (diff > 180.0) {
        diff -= 360.0;
    }
    while (diff <= -180.0) {
        diff += 360.0;
    }

    return fabs(diff);
}

double phase_average(double phases[], int n) {
    double sum_sin = 0.0;
    double sum_cos = 0.0;

    // 위상을 각도로 변환하여 sin과 cos 값을 합산
    for (int i = 0; i < n; i++) {
        sum_sin += sin(phases[i] * M_PI / 180.0);
        sum_cos += cos(phases[i] * M_PI / 180.0);
    }

    // 평균 벡터의 각도를 구함
    double avg_phase_rad = atan2(sum_sin, sum_cos);

    // 라디안 값을 도 단위로 변환
    double avg_phase_deg = avg_phase_rad * 180.0 / M_PI;

    // -180도에서 180도 사이로 값 조정
    if (avg_phase_deg > 180.0) {
        avg_phase_deg -= 360.0;
    } else if (avg_phase_deg < -180.0) {
        avg_phase_deg += 360.0;
    }

    return avg_phase_deg;
}