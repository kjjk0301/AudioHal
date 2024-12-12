/*
 * echo_canceller.c
 *
 *  Created on: 2024. 11. 19.
 *      Author: SEONGPIL
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h> // clock() 함수를 사용하기 위한 헤더
//#include "sys.h"
#include "ssl_core.h"
#include "echo_canceller.h"
// #include "echo_canceller_w.h"
#include "echo_canceller_w2.h"

//#include "debug_file.h"

//#define DEBUG_AEC

#define FRAME_SIZE 1600 // 프레임 크기
#define FILTER_LENGTH 1500
#define LEAKY_LENGTH 300
#define STEP_SIZE 1.0f // 스텝 사이즈 (부동소수점)
#define MIN_POWER -40.0f // dBFS 기준 최소 파워
#define GLOBAL_DELAY 1050 // 글로벌 딜레이 값
#define SAMPLE_RATE 16000 // 샘플링 주파수

// 필터 차수
#define ORDER 10 // 필터 차수: 계수 배열 크기 - 1

#define CORR_BLOCK_SIZE (SAMPLE_RATE / 10) // 1초 동안 10번에 나눠서 처리
#define MAX_DELAY 2000 // 최대 딜레이 범위 (±2000 샘플)
#define BUFFER_SIZE (1600 + MAX_DELAY * 2) // 추가 버퍼 크기
#define LONG_LONG_MIN (-9223372036854775807LL - 1) // 최소 long long 값 정의

// 변수 초기화
aecInst_t Sys_aecInst;

int32_t uBuf_real[PolyM][PolyL+Echo_taps-1];
int32_t uBuf_imag[PolyM][PolyL+Echo_taps-1];

int32_t xref_pow[PolyM];
int32_t err_pow[PolyM];

// int w_LP[Echo_single_taps];
int32_t w_single[Echo_single_taps];
int16_t uBuf_single[framesize_max+Echo_single_taps-1];

// int xref_single_pow;
// int e_single_pow;
// int y_single_pow;

float xref_pow_f;
float e_pow_f;
float y_pow_f;

int32_t w_multi[MicN][Echo_multi_taps];
int16_t uBuf_multi[MicN][framesize_max+Echo_multi_taps-1];
int32_t xref_multi_pow[MicN];
int32_t e_multi_pow[MicN];
int32_t y_multi_pow[MicN];

float xref_multi_pow_f[MicN];
float e_multi_pow_f[MicN];
float y_multi_pow_f[MicN];

int16_t d_buf_g[MicN+RefN][Ref_delay_max+framesize_max];

double Q15inv;
double Q23inv;

double dev_delay=1.0;
double dev_delay_multi=1.0;

int g_debug_idx = 0;


// 변수 초기화
int16_t refSignal[FRAME_SIZE];
int16_t micSignal[FRAME_SIZE];
int16_t errorSignal[FRAME_SIZE];
float weights[FILTER_LENGTH] = {0};
float weightsbackup[FILTER_LENGTH] = {0};
int32_t x[FILTER_LENGTH] = {0}; // 32비트 정수 버퍼
float leaky[FILTER_LENGTH];
int16_t delayBuffer[GLOBAL_DELAY] = {0};
float prevAttenuation = 0.0f;
float refScale = 0.5; // 스케일링 팩터 (32비트 정수)
float micScale = 4.5; // 스케일링 팩터 (32비트 정수)
int32_t refScaleQ15 = (int32_t)(1.0 * 32768.0f);
int32_t micScaleQ15 = (int32_t)(10.0 * 32768.0f);
int filter_done=0;

// 메인 함수에서 상태 배열을 정의하고 초기화
double xState[ORDER+1] = {0}; // 필터 차수만큼 0으로 초기화
double yState[ORDER+1] = {0};
// 분자 계수 배열 (b)
// double bfilt[ORDER+1] = {
//     0.0264830289751868,	-0.181960551096999,	0.597587438146549,	-1.27874011812734,
//     2.03823712005653,	-2.61006153635327,	2.81690923682046,	-2.61006153635327,
//     2.03823712005652,	-1.27874011812733,	0.597587438146548,	-0.181960551096998,
//     0.0264830289751867
// };

double bfilt[ORDER+1] = {
    0.0282055494152777,	-0.139559270631190,	0.308114884914936,	-0.400881600573803,
    0.294627914868264,	4.38402305362614e-16,	-0.294627914868264,	0.400881600573803,
    -0.308114884914936,	0.139559270631189,	-0.0282055494152776}; // 100 - 2500

// double bfilt[ORDER+1] = {
// 0.0401601259162519,	-0.163937263486003,	0.290148204078975,	-0.324195516371553,
// 0.232894922184584,	8.56064572110112e-16,	-0.232894922184586,	0.324195516371554
// -0.290148204078976,	0.163937263486003,	-0.0401601259162520}; // 100 - 3000

// double bfilt[ORDER+1] = {0.926936868721118,	-5.56162121232670,	13.9040530308168,	-18.5387373744224,
// 	13.9040530308168,	-5.56162121232670,	0.926936868721118};

// 분모 계수 배열 (a)
// a[0]은 항상 1이어야 함 (일반적으로 필터 설계 도구에서 자동으로 설정됨)
// double afilt[ORDER+1] = {
//     1,	-9.49560285511011,	42.3878483876040,	-117.969768865137,
//     228.409680877814,	-324.434228443696,	346.774377162189,	-281.027161780291,
//     171.333846970449,	-76.6021273860552,	23.8236557256342,	-4.62355178484802,
//     0.423031993625886
// };

double afilt[ORDER+1] = {1.0,	-7.86169682972115,	28.6131058788227,	-63.7884084465497,
	        96.7415501715750,	-104.416196330225,	81.2339183882431,	-44.9604403876083,
            16.9272972017569,	-3.90798102450962,	0.418851435535095};// 100 - 2500

// double afilt[ORDER+1] = {
// 1.0,	-7.20496753092482,	24.2288531954410,	-50.6409971896431,
// 73.3463140166789,	-77.0724406767867,	59.4667441781680,	-33.2292717485347,
// 12.8491074578649,	-3.09219085080541,	0.348849281748808};// 100 - 3000

// double afilt[ORDER+1] = {1.0,	-5.84827463755939,	14.2528406664852,	-18.5280691624172,
// 	13.5499271756971,	-5.28563599739812,	0.859211958594510};

aecInst_t* sysAECCreate(int32_t frame_len)
{
	int i;
    AEC_Init(&Sys_aecInst, frame_len);
    AEC_delay_init(&Sys_aecInst, &d_buf_g[0][0], Ref_delay);

    return &Sys_aecInst;
}


aecInst_t* sysAEC_multi_Create(int32_t frame_len)
{
	int i;
    AEC_Init(&Sys_aecInst, frame_len);
    AEC_delay_init(&Sys_aecInst, &d_buf_g[0][0], Ref_delay);	

    return &Sys_aecInst;
}

void AEC_Init(aecInst_t *aecInst, int32_t frame_len){

    int k, m, muQ15;
    float mu;
    aecInst_t * inst = (aecInst_t *)aecInst;

    inst->blocksize=frame_len;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block
    inst->bits=Q15;

    inst->taps=Echo_taps;
    inst->bufsize=Echo_taps+PolyL-1;
    inst->delay=Echo_delay;

    inst->taps_single=Echo_single_taps;
    inst->bufsize_single=Echo_single_taps+framesize_max-1;
    inst->delay_single=Echo_delay;

    inst->taps_multi=Echo_multi_taps;
    inst->bufsize_multi=Echo_multi_taps+framesize_max-1;
    inst->delay_multi=Echo_delay;    

    mu   = 0.05;
    muQ15=(int)(mu*32767.0);

    dev_delay = (double)(inst->taps) / (double)(inst->delay_single);
    dev_delay_multi = (double)(inst->taps) / (double)(inst->delay_multi);    

    Q15inv = (double)(1/(32768.0));
    Q23inv = (double)(1/(8388608.0));


    inst->p_w_LP = &w_single[0];
    inst->p_uBuf_LP = &uBuf_single[0];

    for (k=0; k<Echo_delay; k++){
        inst->p_w_LP[k]=rand()>>24;
    }
    for (k=Echo_delay; k<Echo_single_taps; k++){
        inst->p_w_LP[k]=0;
    }    
    
    xref_pow_f = 0.00001;
    e_pow_f = 0.00001;
    y_pow_f = 0.00001;

    memset(inst->p_uBuf_LP, 0, sizeof(short)*(inst->bufsize_single));

    inst->mu_opt_d_slow = 0.5;

    for (m=0; m<MicN; m++){
        for (k=0; k<Echo_delay; k++){
            w_multi[m][k]=rand()>>24;
        }
        for (k=Echo_delay; k<Echo_multi_taps; k++){
            w_multi[m][k]=0;
        }   
        
        xref_multi_pow_f[m] = 0.00001;
        e_multi_pow_f[m] = 0.00001;
        y_multi_pow_f[m] = 0.00001;   

        memset(&uBuf_multi[m][0], 0, sizeof(short)*(inst->bufsize_multi));      

        inst->mu_opt_d_slow_multi[m]= 0.5;
    }

    inst->mu_tap_d = 1.0;
    inst->mu_opt_d = 0.7;

    inst->reg1_d = 1000;
    inst->reg2_d = 10.0000e-07;    

    inst->tau_p_r = 0.1;
    inst->tau_p_f = 0.1;

    inst->tau_e_r = 0.1;
    inst->tau_e_f= 0.1;       

    // leaky 계수 초기화
    for (int i = 0; i < LEAKY_LENGTH; i++) {
        leaky[i] = 1.0f;
    }

    // 감소 비율 계산
    float decrement = (1.0f - 0.9999f) / (FILTER_LENGTH - LEAKY_LENGTH - 1);

    // leaky 계수를 1.0f에서 점진적으로 감소시키기
    for (int i = LEAKY_LENGTH; i < FILTER_LENGTH; i++) {
        leaky[i] = 1.0f - decrement * (i - LEAKY_LENGTH);
    }  

    for (int i = 0; i < ORDER; i++) {
        xState[i] = 0.0; // 필터 차수만큼 0으로 초기화
        yState[i] = 0.0;
    }

}



void AEC_multi_Init(aecInst_t *aecInst, int32_t frame_len){

    int k, m, muQ15;
    float mu;
    aecInst_t * inst = (aecInst_t *)aecInst;

    inst->blocksize=polyblocksize;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block
    inst->bits=Q15;

    inst->taps=Echo_taps;
    inst->bufsize=Echo_taps+PolyL-1;
    inst->delay=Echo_delay;

    inst->taps_multi=Echo_multi_taps;
    inst->bufsize_multi=Echo_multi_taps+framesize_max-1;
    inst->delay_multi=Echo_delay;

    mu   = 0.05;
    muQ15=(int)(mu*32767.0);

    dev_delay = (double)(inst->taps) / (double)(inst->delay_multi);

    Q15inv = (double)(1/(32768.0));
    Q23inv = (double)(1/(8388608.0));

    for (m=0; m<MicN; m++){
        for (k=0; k<Echo_delay; k++){
            w_multi[m][k]=rand()>>24;
        }
        for (k=Echo_delay; k<Echo_multi_taps; k++){
            w_multi[m][k]=0;
        }   
        
        xref_multi_pow_f[m] = 0.00001;
        e_multi_pow_f[m] = 0.00001;
        y_multi_pow_f[m] = 0.00001;   

        memset(&uBuf_multi[m][0], 0, sizeof(short)*(inst->bufsize_multi));      

        inst->mu_opt_d_slow_multi[m]= 0.5;
    }

    inst->mu_tap_d = 1.0;
    inst->mu_opt_d = 0.7;

    inst->reg1_d = 600;
    inst->reg2_d = 10.0000e-07;    

    inst->tau_p_r = 0.1;
    inst->tau_p_f = 0.1;

    inst->tau_e_r = 0.1;
    inst->tau_e_f= 0.1;     

}


void AEC_Proc(aecInst_t *aecInst, int32_t *inBufRef, int32_t *inBufMic, int32_t *outBuf, int band)
{
    int i, k, m, n, curidx, temp_real, temp_imag;
    // int p_inst, pe_inst, temp, devQ30, mu_optQ15, PinvQ18;
    int p_inst, pe_inst, temp, devQ30, mu_optQ15, PinvQ18;
    int n_e_realQ18, n_e_imagQ18, n_e_mu_realQ23, n_e_mu_imagQ23, n_e_mu_realQ23_opt, n_e_mu_imagQ23_opt;

    int32_t y_real, y_imag, e_real, e_imag;
    unsigned char *tempPtr1, *tempPtr2;

    aecInst_t * inst = (aecInst_t *)aecInst;

    int32_t *xin  = (int32_t *)inBufMic;
    int32_t *xref = (int32_t *)inBufRef;
    int32_t *xout = (int32_t *)outBuf;

    int taps     = inst->taps;
    int blocksize= inst->L;
    int bufsize  = inst->bufsize;

    int *w_real   = (int *)inst->p_w_real[band];
    int *w_imag   = (int *)inst->p_w_imag[band];

    int32_t *buf_real = (int32_t *)inst->p_uBuf_real[band];
    int32_t *buf_imag = (int32_t *)inst->p_uBuf_imag[band];

    int *p_p        = (int *)inst->p_xref_pow[band];
    int *p_pe       = (int *)inst->p_err_pow[band];

    int delay     = inst->delay;
    int   mu_tap    = inst->mu_tapQ15;
    mu_optQ15 = inst->mu_optQ15;

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int32_t);
    for(i=0; i<taps-1;i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 4;
    }

    tempPtr1=(unsigned char *)&buf_imag[0];
    tempPtr2=(unsigned char *)(&buf_imag[0])+blocksize*sizeof(int32_t);
    for(i=0; i<taps-1;i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 4;
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(i=0; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 8;
    }

    tempPtr1=(unsigned char *)&buf_imag[taps-1];
    tempPtr2=(unsigned char *)(&xref[0])+sizeof(int32_t);
    for(i=0; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 8;
    }

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;
        // filtering
        temp_real=16384;
        temp_imag=16384;
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (int)(w_real[k]) * (int)(buf_real[m]);
            temp_real=temp_real + (int)(w_imag[k]) * (int)(buf_imag[m]);
            temp_imag=temp_imag - (int)(w_imag[k]) * (int)(buf_real[m]);
            temp_imag=temp_imag + (int)(w_real[k]) * (int)(buf_imag[m]);
            m--;
        }
        y_real=(int32_t)(temp_real>>15);
        y_imag=(int32_t)(temp_imag>>15);

        // error calc
        e_real=xin[2*n]-y_real;
        e_imag=xin[2*n+1]-y_imag;

        xout[2*n]  =e_real;
        xout[2*n+1]=e_imag;

        // xout[2*n]  =y_real;
        // xout[2*n+1]=y_imag;        

        // long term signal power
        p_inst=0 + (buf_real[curidx])*(buf_real[curidx]);
        p_inst=p_inst+(buf_imag[curidx])*(buf_imag[curidx]);

        pe_inst=0 + (e_real)*(e_real);
        pe_inst=pe_inst+(e_imag)*(e_imag);

        *p_p  = (int)((float)*p_p * 0.99) ; // - ((*p_p + 4)>>3);
        *p_p  = *p_p  + (int)((float)p_inst*0.01); //>>3);
        *p_pe  = (int)((float)*p_pe * 0.99); // - ((*p_p + 4)>>3);
        *p_pe  = *p_pe  + (int)((float)pe_inst*0.01); //>>3);        
        // *p_pe = *p_pe - ((*p_pe + 4)>>3);
        // *p_pe = *p_pe + (pe_inst>>3);

        // temp=(*p_pe)*(delay);
        // temp=((*p_p)<<8)/(temp+1);

        // devQ30=0;
        // unsigned int temp2;
        // for (k=0; k<delay; k++){
        //     temp2 = 8 + ((unsigned int)(w_real[k])*(unsigned int)(w_real[k])+(unsigned int)(w_imag[k])*(unsigned int)(w_imag[k]));
        //     devQ30 += (temp2)>>4;
        //     // temp2 = ((unsigned int)(w_real[k])*(unsigned int)(w_real[k])+(unsigned int)(w_imag[k])*(unsigned int)(w_imag[k]));
        //     // devQ30 += (temp2);
        // }

        // devQ30=(int)sqrt((double)devQ30);
        
        // mu_optQ15=temp*(devQ30)+256;
        // mu_optQ15=(mu_optQ15>>9);

        // // inverse long term signal power
        // PinvQ18 = ((int)(2147483647))/(*p_p+1);
        // PinvQ18 = ((PinvQ18+512)>>10); //Q18 -> Q21

        // // filter update
        // n_e_realQ18=(int)(e_real)*PinvQ18;
        // n_e_imagQ18=-1*(int)(e_imag)*PinvQ18;

        // n_e_mu_realQ23=mu_tap*n_e_realQ18+512;
        // n_e_mu_realQ23=(n_e_mu_realQ23>>10);
        // n_e_mu_imagQ23=mu_tap*n_e_imagQ18+512;
        // n_e_mu_imagQ23=(n_e_mu_imagQ23>>10);

        // n_e_mu_realQ23_opt=mu_optQ15*n_e_realQ18+512;
        // n_e_mu_realQ23_opt=(n_e_mu_realQ23_opt>>10);
        // n_e_mu_imagQ23_opt=mu_optQ15*n_e_imagQ18+512;
        // n_e_mu_imagQ23_opt=(n_e_mu_imagQ23_opt>>10);

        // if (mu_tap>mu_optQ15) {
        //     n_e_mu_realQ23=n_e_mu_realQ23_opt;
        //     n_e_mu_imagQ23=n_e_mu_imagQ23_opt;
        // }
        // m=curidx;
        // for (k=0; k<delay; k++) {
        //     temp_real=      512+n_e_mu_realQ23*(int)(buf_real[m]); //Q23
        //     temp_real=temp_real-n_e_mu_imagQ23*(int)(buf_imag[m]); //Q23

        //     temp_imag=      512+n_e_mu_realQ23*(int)(buf_imag[m]); //Q23
        //     temp_imag=temp_imag+n_e_mu_imagQ23*(int)(buf_real[m]); //Q23

        //     w_real[k] = w_real[k] + (temp_real>>10);
        //     w_imag[k] = w_imag[k] + (temp_imag>>10);
        //     m--;
        // }

        // for (k=delay; k<taps; k++) {
        //     temp_real=      512+n_e_mu_realQ23_opt*(int)(buf_real[m]); //Q23
        //     temp_real=temp_real-n_e_mu_imagQ23_opt*(int)(buf_imag[m]); //Q23

        //     temp_imag=      512+n_e_mu_realQ23_opt*(int)(buf_imag[m]); //Q23
        //     temp_imag=temp_imag+n_e_mu_imagQ23_opt*(int)(buf_real[m]); //Q23

        //     w_real[k] = w_real[k] + (temp_real>>10);
        //     w_imag[k] = w_imag[k] + (temp_imag>>10);
        //     m--;
        // }

        temp=(*p_pe)*(delay);
        temp=((*p_p)<<8)/(temp+1);

        devQ30=0;
        unsigned int temp2;
        for (k=0; k<delay; k++){
            temp2 = 8 + ((unsigned int)(w_real[k])*(unsigned int)(w_real[k])+(unsigned int)(w_imag[k])*(unsigned int)(w_imag[k]));
            devQ30 += (temp2)>>4;
            // temp2 = ((unsigned int)(w_real[k])*(unsigned int)(w_real[k])+(unsigned int)(w_imag[k])*(unsigned int)(w_imag[k]));
            // devQ30 += (temp2);
        }

        devQ30=(int)sqrt((double)devQ30);

        // mu_optQ15=temp*(devQ30>>15)+4;
        // mu_optQ15=(mu_optQ15>>3);

        mu_optQ15=temp*(devQ30);
        mu_optQ15=(mu_optQ15>>8);

        // inverse long term signal power
        // PinvQ18 = ((int)(2147483647))/(*p_p+1);
        // PinvQ18 = ((PinvQ18+4096)>>13);

        PinvQ18 = ((int)(2147483647))/(*p_p+50);
        PinvQ18 = ((PinvQ18)>>11); //Q18 -> Q21

        // filter update
        n_e_realQ18=(int)(e_real)*PinvQ18;
        n_e_imagQ18=-1*(int)(e_imag)*PinvQ18;

        n_e_mu_realQ23=mu_tap*n_e_realQ18;
        n_e_mu_realQ23=(n_e_mu_realQ23>>10);
        n_e_mu_imagQ23=mu_tap*n_e_imagQ18;
        n_e_mu_imagQ23=(n_e_mu_imagQ23>>10);

        n_e_mu_realQ23_opt=mu_optQ15*n_e_realQ18;
        n_e_mu_realQ23_opt=(n_e_mu_realQ23_opt>>10);
        n_e_mu_imagQ23_opt=mu_optQ15*n_e_imagQ18;
        n_e_mu_imagQ23_opt=(n_e_mu_imagQ23_opt>>10);

        if (mu_tap>mu_optQ15) {
            n_e_mu_realQ23=n_e_mu_realQ23_opt;
            n_e_mu_imagQ23=n_e_mu_imagQ23_opt;
        }
        m=curidx;
        for (k=0; k<delay; k++) {
            temp_real=      0+n_e_mu_realQ23*(int)(buf_real[m]); //Q23
            temp_real=temp_real-n_e_mu_imagQ23*(int)(buf_imag[m]); //Q23

            temp_imag=      0+n_e_mu_realQ23*(int)(buf_imag[m]); //Q23
            temp_imag=temp_imag+n_e_mu_imagQ23*(int)(buf_real[m]); //Q23

            w_real[k] = w_real[k] + (temp_real>>10);
            w_imag[k] = w_imag[k] + (temp_imag>>10);
            m--;
        }

        for (k=delay; k<taps; k++) {
            temp_real=      0+n_e_mu_realQ23_opt*(int)(buf_real[m]); //Q23
            temp_real=temp_real-n_e_mu_imagQ23_opt*(int)(buf_imag[m]); //Q23

            temp_imag=      0+n_e_mu_realQ23_opt*(int)(buf_imag[m]); //Q23
            temp_imag=temp_imag+n_e_mu_imagQ23_opt*(int)(buf_real[m]); //Q23

            w_real[k] = w_real[k] + (temp_real>>10);
            w_imag[k] = w_imag[k] + (temp_imag>>10);
            m--;
        }        
    }

    // if (band==8){
    //     printf("temp=%d, devQ30=%d, mu_optQ15=%d, mu_tap=%d *p_p=%d, PinvQ18=%d\t\t",temp, devQ30, mu_optQ15, mu_tap, *p_p, PinvQ18);

    //     // for (k=0; k<taps; k++){
    //     //     printf("%d\t", w_real[k]);
    //     // }

    //     // printf("\r\n");

    // }

    // if (band==50){
    //     printf("temp=%d, devQ30=%d, mu_optQ15=%d, mu_tap=%d *p_p=%d, PinvQ18=%d\r\n",temp, devQ30, mu_optQ15, mu_tap, *p_p, PinvQ18);

    //     // for (k=0; k<taps; k++){
    //     //     printf("%d\t", w_real[k]);
    //     // }

    //     // printf("\r\n");

    // }
}


void AEC_delay(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay)
{
    int i;
    unsigned char *tempPtr1, *tempPtr2;

    aecInst_t * inst = (aecInst_t *)aecInst;

    short *xin  = (short *)inBuf;
    short *xout = (short *)outBuf;
    short *delay_Buf = (short *)d_Buf;

    int blocksize= inst->blocksize;

    tempPtr1=(unsigned char *)&delay_Buf[delay];
    tempPtr2=(unsigned char *)&xin[0];
    for (i=0 ; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1=(unsigned char *)&xout[0];
    tempPtr2=(unsigned char *)&delay_Buf[0];
    for (i=0 ; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1=(unsigned char *)&delay_Buf[0];
    tempPtr2=(unsigned char *)&delay_Buf[blocksize];
    for (i=0 ; i<delay; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }
}

int AEC_delay_with_framelen(aecInst_t *aecInst, short *inBuf, short *outBuf, short *d_Buf, int delay, int framelen)
{
    int i;
    unsigned char *tempPtr1, *tempPtr2;

    aecInst_t * inst = (aecInst_t *)aecInst;

    short *xin  = (short *)inBuf;
    short *xout = (short *)outBuf;
    short *delay_Buf = (short *)d_Buf;

    int blocksize= framelen;

    if ((framelen+delay) > (Ref_delay_max+framesize_max)){
        printf("total delay buffer length too liong, not support\r\n");
        return -1;
    }

    tempPtr1=(unsigned char *)&delay_Buf[delay];
    tempPtr2=(unsigned char *)&xin[0];
    for (i=0 ; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1=(unsigned char *)&xout[0];
    tempPtr2=(unsigned char *)&delay_Buf[0];
    for (i=0 ; i<blocksize; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    tempPtr1=(unsigned char *)&delay_Buf[0];
    tempPtr2=(unsigned char *)&delay_Buf[blocksize];
    for (i=0 ; i<delay; i++){
        memcpy(tempPtr1, tempPtr2, sizeof(short));
        tempPtr1 += 2;
        tempPtr2 += 2;
    }

    return 0;
}

void AEC_delay_init(aecInst_t *aecInst, short *d_Buf, int delay){

    aecInst_t * inst = (aecInst_t *)aecInst;

    short *delay_Buf = (short *)d_Buf;

    memset(&delay_Buf[0], 0, sizeof(short)*(Ref_delay_max+framesize_max));
}


void AEC_single_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real, n_e_mu_realQ33, n_e_mu_realQ33_opt;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
    
    int32_t *w_real_Q23   = (int32_t *)&w_single[0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_single[0];
    
    int16_t e_buf[polyblocksize];
    int16_t y_buf[polyblocksize];

    int16_t delay     = inst->delay;
    int16_t taps     = Echo_single_taps;
    int16_t blocksize= polyblocksize;

    float *p_p_f        = &xref_pow_f; 
    float *p_pe_f       = &e_pow_f;

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    double mu_tap_d   = inst->mu_tap_d; //10 / (double)taps;
    
    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow;

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    float sum_squares = 0.0f;
    for (k=0; k<taps; k++){
        sum_squares += (float)(buf_real[k]) * (float)(buf_real[k]);
    }    

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }
        y_real=temp_real>>15;

        if (n>0){
            sum_squares -= (float)(buf_real[n-1]) * (float)(buf_real[n-1]);
            m=curidx;
            sum_squares += (float)(buf_real[m]) * (float)(buf_real[m]);
        }
        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);

        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real;

        e_buf[n] = e_real;
        y_buf[n] = y_real;

        dev_d = 0;
        for (k=0; k<delay; k++){
            temp_d = (double)(w_real_Q23[k]) * Q23inv;
            dev_d += temp_d*temp_d;
        }

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

        temp_d = (double)(e_real) * (double)Pinv_f;

        n_e_mu_real_d_opt = mu_slow_d * temp_d;
        n_e_mu_real_d = mu_tap_d * temp_d;

        n_e_mu_realQ33 = (int32_t)(n_e_mu_real_d * (8.589934592000000e+09));
        n_e_mu_realQ33_opt = (int32_t)(n_e_mu_real_d_opt * (8.589934592000000e+09));

        m=curidx;
        for (k=0; k<delay; k++) {
            temp_real=      n_e_mu_realQ33*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }      

        for (k=delay; k<taps; k++) {
            temp_real=      n_e_mu_realQ33_opt*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }               

    }

        dBFSResult result = calculate_dBFS_rms_and_ratio(xref, polyblocksize);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(xin, polyblocksize);
        float d_curr_rms_dB = result.dBFS;
        float d_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, polyblocksize);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(y_buf, polyblocksize);
        float y_curr_rms_dB = result.dBFS;
        float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0// DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)){

        FloatPacket packet;
        packet.val1 = (float)d_curr_rms_dB;
        packet.val2 = (float)x_curr_rms_dB;
        packet.val3 = (float)e_curr_rms_dB;
        packet.val4 = (float)y_curr_rms_dB;
        packet.val5 = (float)x_power_long_dB;

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &w_real_Q23[0], taps);
        debug_matlab_ssl_float_send(10, (float)mu_slow_d);
    }

	if ((g_aec_debug_on==1)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 
    inst->mu_opt_d_slow = mu_slow_d;
}

void AEC_single_Proc_with_len(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real, n_e_mu_realQ33, n_e_mu_realQ33_opt;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
    
    int32_t *w_real_Q23   = (int32_t *)&w_single[0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_single[0];
    
    int16_t e_buf[framesize_max];
    int16_t y_buf[framesize_max];

    int16_t delay     = inst->delay_single;
    int16_t taps     = inst->taps_single;
    int16_t blocksize = framelen;

    float *p_p_f        = &xref_pow_f; 
    float *p_pe_f       = &e_pow_f;

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    double mu_tap_d   = inst->mu_tap_d; //10 / (double)taps;
    
    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow;

    dBFSResult result = calculate_dBFS_rms_and_ratio(xin, framelen);
    float d_curr_rms_dB = result.dBFS;
    float d_curr_rms_ratio = result.ratio;    

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    float sum_squares = 0.0f;
    for (k=0; k<taps; k++){
        sum_squares += (float)((buf_real[k]) * (buf_real[k]));
    }    

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<delay; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }

        int y_mis = temp_real>>15;

        for (k=delay; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }

        y_real=temp_real>>15;

        if (n>0){
            sum_squares -= (float)((buf_real[n-1]) * (buf_real[n-1]));
            m=curidx;
            sum_squares += (float)((buf_real[m]) * (buf_real[m]));
        }
        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);
        // float Pinv_half = fabsf (Pinv_f);

        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real; //- (int16_t) y_mis;

        e_buf[n] = (int16_t) e_real;
        y_buf[n] = (int16_t) y_real;

        dev_d = 0;
        // for (k=0; k<delay; k++){
        //     temp_d = (double)(w_real_Q23[k]) * Q23inv;
        //     dev_d += temp_d*temp_d;
        // }
        for (k=0; k<delay; k++){
            dev_d +=(double)(w_real_Q23[k])*(double)(w_real_Q23[k]);
        }

        dev_d = dev_d * Q23inv * Q23inv ;        

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

        temp_d = (double)(e_real) * (double)Pinv_f * (8.589934592000000e+09);

        n_e_mu_real_d_opt = mu_slow_d * temp_d;
        n_e_mu_real_d = mu_tap_d * temp_d;

        n_e_mu_realQ33 = (int32_t)(n_e_mu_real_d);
        n_e_mu_realQ33_opt = (int32_t)(n_e_mu_real_d_opt);

        m=curidx;
        for (k=0; k<delay; k++) {
            // int temp_int = (int)(Pinv_half*(float)(buf_real[m]));
            temp_real=      n_e_mu_realQ33*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }      

        for (k=delay; k<taps; k++) {
            // int temp_int = (int)(Pinv_half*(float)(buf_real[m]));
            temp_real=      n_e_mu_realQ33_opt*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }               

    }

        result = calculate_dBFS_rms_and_ratio(xref, framelen);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, framelen);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(y_buf, framelen);
        float y_curr_rms_dB = result.dBFS;
        float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0 //def DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)){

        FloatPacket packet;
        packet.val1 = (float)d_curr_rms_dB;
        packet.val2 = (float)x_curr_rms_dB;
        packet.val3 = (float)e_curr_rms_dB;
        packet.val4 = (float)y_curr_rms_dB;
        packet.val5 = (float)x_power_long_dB;

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &w_real_Q23[0], Echo_single_taps);
        debug_matlab_ssl_float_send(10, (float)(d_curr_rms_dB-e_curr_rms_dB));
    }

	if ((g_aec_debug_on==1)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 
    // if (g_debug_idx == 0) {
    //     printf("AEC single :: d rms = %.2fdB ", d_curr_rms_dB);
    //     printf("x rms = %.2fdB ", x_curr_rms_dB);
    //     printf("y rms = %.2fdB ", y_curr_rms_dB);
    //     printf("e rms = %.2fdB ", e_curr_rms_dB);
    //     printf("NR rms = %.2fdB \n", (float)(d_curr_rms_dB-e_curr_rms_dB));
    // }


    // g_debug_idx++;
    // if (g_debug_idx>=20) {
    //     g_debug_idx = 0;
    // }

    inst->mu_opt_d_slow = mu_slow_d;
}

void AEC_single_Proc_double(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
    
    int32_t *w_real_Q23   = (int32_t *)&w_single[0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_single[0];
    
    int16_t e_buf[polyblocksize];
    int16_t y_buf[polyblocksize];

    int16_t delay     = inst->delay;
    int16_t taps     = Echo_single_taps;
    int16_t blocksize= polyblocksize;
    int16_t bufsize  = Echo_single_taps+polyblocksize-1;    

    float *p_p_f        = &xref_pow_f; 
    float *p_pe_f       = &e_pow_f;

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    // double mu_tap_d   = 1; //10 / (double)taps;
    double mu_tap_d   = inst->mu_tap_d; //10 / (double)taps;
    
    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow;

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }
        y_real=temp_real>>15;

        float sum_squares = 0.0f;

        // 샘플의 제곱의 합 계산
        m=curidx;
        for (k=0; k<taps; k++){
            sum_squares += (float)(buf_real[m]) * (float)(buf_real[m]);
            m--;
        }
        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);

        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real;

        e_buf[n] = e_real;
        y_buf[n] = y_real;

        dev_d = 0;
        for (k=0; k<delay; k++){
            temp_d = (double)(w_real_Q23[k]) * Q23inv;
            dev_d += temp_d*temp_d;
        }

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

        n_e_mu_real_d_opt = mu_slow_d * (double)(e_real) * (double)Pinv_f;
        n_e_mu_real_d = mu_tap_d * (double)(e_real) * (double)Pinv_f;

        m=curidx;
        for (k=0; k<delay; k++) {
            temp_d=      n_e_mu_real_d*(double)(buf_real[m]);
            w_real_Q23[k] = w_real_Q23[k] + (int32_t)(temp_d*8388608.0);
            m--;
        }      

        for (k=delay; k<taps; k++) {
            temp_d=      n_e_mu_real_d_opt*(double)(buf_real[m]);
            w_real_Q23[k] = w_real_Q23[k] + (int32_t)(temp_d*8388608.0);
            m--;
        }                    

    }

        dBFSResult result = calculate_dBFS_rms_and_ratio(xin, polyblocksize);
        float d_curr_rms_dB = result.dBFS;
        float d_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(xref, polyblocksize);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, polyblocksize);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(y_buf, polyblocksize);
        float y_curr_rms_dB = result.dBFS;
        float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0 //def DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)){

        FloatPacket packet;
        packet.val1 = (float)d_curr_rms_dB;
        packet.val2 = (float)x_curr_rms_dB;
        packet.val3 = (float)e_curr_rms_dB;
        packet.val4 = (float)y_curr_rms_dB;
        packet.val5 = (float)x_power_long_dB;

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &w_real_Q23[0], taps);
        debug_matlab_ssl_float_send(10, (float)mu_slow_d);
    }

	if ((g_aec_debug_on==1)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 
    inst->mu_opt_d_slow = mu_slow_d;
}


void AEC_multi_Proc(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real, n_e_mu_realQ33, n_e_mu_realQ33_opt;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
      
    int32_t *w_real_Q23   = (int32_t *)&w_multi[chan][0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_multi[chan][0];
    
    int16_t e_buf[polyblocksize];
    int16_t y_buf[polyblocksize];

    int16_t delay     = inst->delay;
    int16_t taps     = Echo_multi_taps;
    int16_t blocksize= polyblocksize;

    float *p_p_f        = &xref_multi_pow_f[chan];
    float *p_pe_f       = &e_multi_pow_f[chan];    

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    double mu_tap_d   = inst->mu_tap_d;  //10 / (double)taps;

    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow_multi[chan];

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    float sum_squares = 0.0f;
    for (k=0; k<taps; k++){
        sum_squares += (float)(buf_real[k]) * (float)(buf_real[k]);
    }

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }
        y_real=temp_real>>15;

        if (n>0){
            sum_squares -= (float)(buf_real[n-1]) * (float)(buf_real[n-1]);
            m=curidx;
            sum_squares += (float)(buf_real[m]) * (float)(buf_real[m]);
        }
        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);

        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real;
       
        e_buf[n] = e_real;
        y_buf[n] = y_real;

        dev_d = 0;
        // for (k=0; k<delay; k++){
        //     temp_d = (double)(w_real_Q23[k]) * Q23inv;
        //     dev_d += temp_d*temp_d;
        // }
        for (k=0; k<delay; k++){
            dev_d +=(double)(w_real_Q23[k])*(double)(w_real_Q23[k]);
        }

        dev_d = dev_d * Q23inv * Q23inv ;

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        //  if (mu_opt_d > 1.5) mu_opt_d = 1.5;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

        temp_d = (double)(e_real) * (double)Pinv_f;

        n_e_mu_real_d_opt = mu_slow_d * temp_d;
        // n_e_mu_real_d_opt = mu_opt_d * temp_d;
        n_e_mu_real_d = mu_tap_d * temp_d;

        n_e_mu_realQ33 = (int32_t)(n_e_mu_real_d * (8.589934592000000e+09));
        n_e_mu_realQ33_opt = (int32_t)(n_e_mu_real_d_opt * (8.589934592000000e+09));

        m=curidx;
        for (k=0; k<delay; k++) {
            temp_real=      n_e_mu_realQ33*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }      

        for (k=delay; k<taps; k++) {
            temp_real=      n_e_mu_realQ33_opt*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }               

    }

        // dBFSResult result = calculate_dBFS_rms_and_ratio(xin, polyblocksize);
        // float d_curr_rms_dB = result.dBFS;
        // float d_curr_rms_ratio = result.ratio;

        dBFSResult result = calculate_dBFS_rms_and_ratio(xref, polyblocksize);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, polyblocksize);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        // result = calculate_dBFS_rms_and_ratio(y_buf, polyblocksize);
        // float y_curr_rms_dB = result.dBFS;
        // float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        // double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        // double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0 //def DEBUG_AEC
	// if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(chan==0)){
       
    //     FloatPacket packet;
    //     packet.val1 = (float)d_curr_rms_dB;
    //     packet.val2 = (float)x_curr_rms_dB;
    //     packet.val3 = (float)e_curr_rms_dB;
    //     packet.val4 = (float)y_curr_rms_dB;
    //     packet.val5 = (float)x_power_long_dB;

    //     debug_matlab_ssl_float_packet_send(8, packet);
    //     debug_matlab_int_array_send(9, &w_real_Q23[0], taps);
    //     debug_matlab_ssl_float_send(10, (float) mu_slow_d );
    // }

	// if ((g_aec_debug_on==1)&&(chan==0)){
	// 	g_aec_debug_snd_idx++;
	// 	if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
	// 		g_aec_debug_snd_idx = 0;
	// 	}
	// }
#endif 
    inst->mu_opt_d_slow_multi[chan] = mu_slow_d;
}

void AEC_multi_Proc_with_len(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan, int framelen)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real, n_e_mu_realQ33, n_e_mu_realQ33_opt;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
      
    int32_t *w_real_Q23   = (int32_t *)&w_multi[chan][0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_multi[chan][0];
    
    int16_t e_buf[framesize_max];
    int16_t y_buf[framesize_max];

    int16_t delay     = inst->delay_multi;
    int16_t taps     = inst->taps_multi;
    int16_t blocksize= framelen;

    float *p_p_f        = &xref_multi_pow_f[chan];
    float *p_pe_f       = &e_multi_pow_f[chan];    

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    double mu_tap_d   = inst->mu_tap_d;  //10 / (double)taps;

    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow_multi[chan];

    dBFSResult result = calculate_dBFS_rms_and_ratio(xin, framelen);
    float d_curr_rms_dB = result.dBFS;
    float d_curr_rms_ratio = result.ratio;    

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    float sum_squares = 0.0f;
    for (k=0; k<taps; k++){
        sum_squares += (float)((buf_real[k]) * (buf_real[k]));
    }    

    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<delay; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }

        int y_mis = temp_real>>15;

        for (k=delay; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }
        y_real=temp_real>>15;

        if (n>0){
            sum_squares -= (float)((buf_real[n-1]) * (buf_real[n-1]));
            m=curidx;
            sum_squares += (float)((buf_real[m]) * (buf_real[m]));
        }
        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);

        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real; //- (int16_t) y_mis;
       
        e_buf[n] = e_real;
        y_buf[n] = (int16_t) y_real;

        dev_d = 0;
        // for (k=0; k<delay; k++){
        //     temp_d = (double)(w_real_Q23[k]) * Q23inv;
        //     dev_d += temp_d*temp_d;
        // }
        for (k=0; k<delay; k++){
            dev_d +=(double)(w_real_Q23[k])*(double)(w_real_Q23[k]);
        }

        dev_d = dev_d * Q23inv * Q23inv ;

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

        temp_d = (double)(e_real) * (double)Pinv_f * (8.589934592000000e+09);

        n_e_mu_real_d_opt = mu_slow_d * temp_d;
        n_e_mu_real_d = mu_tap_d * temp_d;

        n_e_mu_realQ33 = (int32_t)(n_e_mu_real_d);
        n_e_mu_realQ33_opt = (int32_t)(n_e_mu_real_d_opt);

        m=curidx;
        for (k=0; k<delay; k++) {
            temp_real=      n_e_mu_realQ33*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }      

        for (k=delay; k<taps; k++) {
            temp_real=      n_e_mu_realQ33_opt*(int)(buf_real[m]) + 512; //Q23
            w_real_Q23[k] = w_real_Q23[k] + (temp_real>>10);
            m--;
        }               

    }

        result = calculate_dBFS_rms_and_ratio(xref, framelen);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, framelen);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(y_buf, framelen);
        float y_curr_rms_dB = result.dBFS;
        float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0//def DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(chan==0)){
       
        FloatPacket packet;
        packet.val1 = (float)d_curr_rms_dB;
        packet.val2 = (float)x_curr_rms_dB;
        packet.val3 = (float)e_curr_rms_dB;
        packet.val4 = (float)y_curr_rms_dB;
        packet.val5 = (float)x_power_long_dB;

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &w_real_Q23[0], Echo_multi_taps);
        debug_matlab_ssl_float_send(10, (float)(d_curr_rms_dB-e_curr_rms_dB));
    }

	if ((g_aec_debug_on==1)&&(chan==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 
    inst->mu_opt_d_slow_multi[chan] = mu_slow_d;
}


void AEC_multi_Proc_double(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int chan)
{
    int32_t k, m, n, curidx;
    int32_t temp_real, y_real, e_real;
    float temp_f, p_instf, pe_instf, tau, Pinv_f;
    double temp_d;
    unsigned char *tempPtr1, *tempPtr2; 

    aecInst_t *inst = (aecInst_t *)aecInst;
      
    int32_t *w_real_Q23   = (int32_t *)&w_multi[chan][0];    

    int16_t *xin  = (int16_t *)inBufMic;
    int16_t *xref = (int16_t *)inBufRef;
    int16_t *xout = (int16_t *)outBuf;
    int16_t *buf_real = (int16_t *)&uBuf_multi[chan][0];
    
#if 1 //def DEBUG_AEC
    int16_t e_buf[polyblocksize];
    int16_t y_buf[polyblocksize];
#endif
    int16_t delay     = inst->delay;
    int16_t taps     = Echo_multi_taps;
    int16_t blocksize= polyblocksize;

    float *p_p_f        = &xref_multi_pow_f[chan];
    float *p_pe_f       = &e_multi_pow_f[chan];    

    double opt_temp = ((double)(*p_p_f)) / ((double)(*p_pe_f+inst->reg2_d));
    opt_temp = opt_temp * dev_delay;

    double mu_tap_d   = inst->mu_tap_d;  //10 / (double)taps;

    double dev_d, mu_opt_d;
    double n_e_mu_real_d, n_e_mu_real_d_opt;

    double mu_slow_d = inst->mu_opt_d_slow_multi[chan];

    tempPtr1=(unsigned char *)&buf_real[0];
    tempPtr2=(unsigned char *)(&buf_real[0])+blocksize*sizeof(int16_t);
    for(n=0; n<taps-1; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }

    tempPtr1=(unsigned char *)&buf_real[taps-1];
    tempPtr2=(unsigned char *)(&xref[0]);
    for(n=0; n<blocksize; n++){
        memcpy(tempPtr1, tempPtr2, sizeof(int16_t));
        tempPtr1 += sizeof(int16_t);
        tempPtr2 += sizeof(int16_t);
    }


    for (n=0; n<blocksize; n++){
        curidx=taps-1+n;

        // filtering
        temp_real=16384; // (1<<14)
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (((w_real_Q23[k]+128)>>8) * (int32_t)(buf_real[m]));
            m--;
        }
        y_real=temp_real>>15;

        float sum_squares = 0.0f;

        // 샘플의 제곱의 합 계산
        m=curidx;
        for (k=0; k<taps; k++){
            sum_squares += (float)(buf_real[m]) * (float)(buf_real[m]);
            m--;
        }

        Pinv_f = 1.0 / (sum_squares+inst->reg1_d);


        // error calc
        e_real  = (int32_t)(xin[n])-y_real;
        xout[n] = (int16_t) e_real;
       
        e_buf[n] = e_real;
        y_buf[n] = y_real;

        dev_d = 0;
        for (k=0; k<delay; k++){
            temp_d = (double)(w_real_Q23[k]) * Q23inv;
            dev_d += temp_d*temp_d;
        }

        mu_opt_d = dev_d * opt_temp * inst->mu_opt_d;
        mu_slow_d = mu_slow_d * 0.95 + mu_opt_d * 0.05;
        if (mu_slow_d > 1.5) mu_slow_d = 1.5;

            n_e_mu_real_d_opt = mu_slow_d * (double)(e_real) * (double)Pinv_f;
            // n_e_mu_real_d_opt = mu_opt_d * (double)(e_real) * (double)Pinv_f;
            n_e_mu_real_d = mu_tap_d * (double)(e_real) * (double)Pinv_f;

            // if (mu_tap_d>mu_opt_d) {
            //     n_e_mu_real_d=n_e_mu_real_d_opt;
            // }

            m=curidx;
            for (k=0; k<delay; k++) {
                temp_d=      n_e_mu_real_d*(double)(buf_real[m]);
                w_real_Q23[k] = w_real_Q23[k] + (int32_t)(temp_d*8388608.0);
                m--;
            }      

            for (k=delay; k<taps; k++) {
                temp_d=      n_e_mu_real_d_opt*(double)(buf_real[m]);
                w_real_Q23[k] = w_real_Q23[k] + (int32_t)(temp_d*8388608.0);
                m--;
            }                    

    }

        dBFSResult result = calculate_dBFS_rms_and_ratio(xin, polyblocksize);
        float d_curr_rms_dB = result.dBFS;
        float d_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(xref, polyblocksize);
        float x_curr_rms_dB = result.dBFS;
        float x_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(e_buf, polyblocksize);
        float e_curr_rms_dB = result.dBFS;
        float e_curr_rms_ratio = result.ratio;

        result = calculate_dBFS_rms_and_ratio(y_buf, polyblocksize);
        float y_curr_rms_dB = result.dBFS;
        float y_curr_rms_ratio = result.ratio;

        // long term signal power      
        p_instf=x_curr_rms_ratio*x_curr_rms_ratio;
        pe_instf=e_curr_rms_ratio*e_curr_rms_ratio;

        if (*p_p_f<p_instf){
            tau = inst->tau_p_r;
        } else {
            tau = inst->tau_p_f;
        }
        *p_p_f = (*p_p_f) * tau + p_instf*(1.0-tau);

        if (*p_pe_f<pe_instf){
            tau = inst->tau_e_r;
        } else {
            tau = inst->tau_e_f;
        }
        *p_pe_f = (*p_pe_f) * tau + pe_instf*(1.0-tau);

        double x_power_long_dB = calculate_dbfsfromPower((double)*p_p_f);
        double e_power_long_dB = calculate_dbfsfromPower((double)*p_pe_f);

#if 0//def DEBUG_AEC
	if ((g_aec_debug_on==1)&&(g_aec_debug_snd_idx==0)&&(chan==0)){
       
        FloatPacket packet;
        packet.val1 = (float)d_curr_rms_dB;
        packet.val2 = (float)x_curr_rms_dB;
        packet.val3 = (float)e_curr_rms_dB;
        packet.val4 = (float)y_curr_rms_dB;
        packet.val5 = (float)x_power_long_dB;

        debug_matlab_ssl_float_packet_send(8, packet);
        debug_matlab_int_array_send(9, &w_real_Q23[0], taps);
        debug_matlab_ssl_float_send(10, (float) mu_slow_d );
    }

	if ((g_aec_debug_on==1)&&(chan==0)){
		g_aec_debug_snd_idx++;
		if (g_aec_debug_snd_idx>=g_aec_debug_snd_period) {
			g_aec_debug_snd_idx = 0;
		}
	}
#endif 
    inst->mu_opt_d_slow_multi[chan] = mu_slow_d;
}



void AEC_Init_new(aecInst_t *aecInst){
    // leaky 계수 초기화
    for (int i = 0; i < LEAKY_LENGTH; i++) {
        leaky[i] = 1.0f;
    }

    for (int i = LEAKY_LENGTH; i < FILTER_LENGTH; i++) {
        leaky[i] = 1.0f - (0.0001f * (i - LEAKY_LENGTH));
    }
}

// RMS 계산 함수 (32비트 정수)
float calculate_rms_int32(int16_t *signal, int length) {
    long long sum = 0;
    for (int i = 0; i < length; i++) {
        sum += (int32_t)signal[i] * (int32_t)signal[i];
    }
    return sqrtf((float)sum / (float)length);
}

// 신호를 부드럽게 노멀라이즈하는 함수 (32비트 정수)
void smooth_normalize_signal_rms_int32(int16_t *signal, float rmsVal, int length, float *currentScale, float targetRMSdB) {
    
    float targetRMS = (pow(10, targetRMSdB / 20.0) * 32768.0f); //1842.68f; //3276.8f; //(pow(10, targetRMSdB / 20.0) * (1 << 15)); // -20dB에 해당하는 RMS 값

    if (rmsVal > 0) {
        float newScale = targetRMS / rmsVal;
        *currentScale = (0.95f * (*currentScale) + 0.05f * newScale);
        int32_t scaleQ15 = (*currentScale)*32768.0f;

        // printf("rmsVal=%f, targetRMS=%f, newScale=%f, currentScale=%f,", rmsVal, targetRMS,  newScale, *currentScale);

        for (int i = 0; i < length; i++) {
            signal[i] = (int16_t)(((int32_t)signal[i] * (int32_t)(scaleQ15)) >> 15);
        }
    }
}

// 글로벌 딜레이 적용 함수 (16비트 정수)
void apply_global_delay_int16(int16_t *signal, int16_t *delayBuffer, int delay, int length) {
    for (int i = 0; i < delay; i++) {
        int16_t temp = delayBuffer[i];
        delayBuffer[i] = signal[length - delay + i];
        signal[i] = temp;
    }
}

// NLMS 에코 캔슬러 함수 (필터 가중치 업데이트는 플로팅 연산)
void nlms_echo_canceller(int16_t *refSignal, int16_t *micSignal, float *weights, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length, int updateon) {
    const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

    int32_t weightsQ15[FILTER_LENGTH];

    for (int n = 0; n < length; n++) {
        // x 배열을 쉬프트하고 새로운 샘플 추가
        for (int i = FILTER_LENGTH - 1; i > 0; i--) {
            x[i] = x[i - 1];
        }
        x[0] = refSignal[n];

        // if (*prevAttenuation < 10.0f && refPower > -40.0f && filter_done == 1) {
        //     for (int i = 0; i < FILTER_LENGTH; i++) {
        //         weightsQ15[i] = (int32_t)(weightsbackup[i] * 32768.0f);
        //     }
        // } else {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
        }            
        // }

        int32_t y = 16384;
        for (int i = 0; i < FILTER_LENGTH; i++) {
            y += (weightsQ15[i] * x[i]);
        }

        y = y>>15;

        // 잔차 신호 계산
        float e = (float)micSignal[n] -  (float)y;
        errorSignal[n] = (int16_t)e;

        if (updateon==1) {
        // if (updateon==1) {
            // 노멀라이제이션 팩터 계산
            float normFactor = 0.002f;
            for (int i = 0; i < FILTER_LENGTH; i++) {
                normFactor += (float)x[i] * (float)x[i];
            }

            // 스텝 크기 조정
            float mu = (*prevAttenuation > 15.0f || refPower < -40.0f) ? 0.15f * STEP_SIZE / normFactor : STEP_SIZE / normFactor;

            // 필터 가중치 업데이트 (플로팅 연산)
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i] = weights[i] * leaky[i] + mu * e * (float)x[i];
            }
        } else {
            for (int i = 0; i < FILTER_LENGTH; i++) {
                weights[i] = weights[i] * leaky[i];
            }            
        }
    }

    // 필터 계수 발산 방지
    int resetWeights = 0; // 초기값: 0 (거짓)
    for (int i = 0; i < FILTER_LENGTH; i++) {
        if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
            resetWeights = 1; // 참
            break;
        }
    }
    if (resetWeights) {
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weights[i] = 0.0f; // 필터 계수를 0으로 초기화
        }
        printf("Weights reset to prevent divergence.\n");
    }

    // 감쇄량 계산
    float micPower = calculate_rms_int32(micSignal, length);
    float errorPower = calculate_rms_int32((int16_t *)errorSignal, length);
    *prevAttenuation = 20.0f * log10f(micPower / (errorPower + 1));
    // printf("micPower= %.2f, errorPower=%.2f, Attenuation = %.2f updateon=%d\n",20.0f * log10f(micPower)-90.3f, 20.0f * log10f(errorPower)-90.3f, *prevAttenuation, updateon);

    if (*prevAttenuation>15.0f){
        for (int i = 0; i < FILTER_LENGTH; i++) {
            weightsbackup[i] = weights[i]; // 필터 계수를 0으로 초기화
        }
        // filter_done = 1;
    }
}

// #include <time.h> // clock() 함수를 사용하기 위한 헤더

// // #define ENABLE_PROFILING2 // 프로파일링 활성화

// void nlms_echo_canceller(int16_t *refSignal, int16_t *micSignal, float *weights, float *leaky, int16_t *errorSignal, int32_t *x, float refPower, float *prevAttenuation, int length) {
//     const float WEIGHT_THRESHOLD = 10.0f; // 필터 가중치의 상한 값

//     int32_t weightsQ15[FILTER_LENGTH];

// #ifdef ENABLE_PROFILING2
//     clock_t start, end;
//     double stepTime;
//     double totalTimes[5] = {0.0}; // 단계별 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     start = clock();
// #endif
//     for (int n = 0; n < length; n++) {
//         // x 배열을 쉬프트하고 새로운 샘플 추가
//         for (int i = FILTER_LENGTH - 1; i > 0; i--) {
//             x[i] = x[i - 1];
//         }
//         x[0] = refSignal[n];
//     }
// #ifdef ENABLE_PROFILING2
//     end = clock();
//     stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
//     totalTimes[0] += stepTime; // x 배열 업데이트 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     start = clock();
// #endif

//     for (int i = 0; i < FILTER_LENGTH; i++) {
//         weightsQ15[i] = (int32_t)(weights[i] * 32768.0f);
//     }

//     for (int n = 0; n < length; n++) {
//         // 필터 출력 계산 (플로팅 연산)
//         int32_t y = 0;
//         for (int i = 0; i < FILTER_LENGTH; i++) {
//             y += (weightsQ15[i] * x[i]) >> 15;
//         }

//         // 잔차 신호 계산
//         float e = (float)micSignal[n] -  (float)y;
//         errorSignal[n] = (int16_t)e;
//     }
// #ifdef ENABLE_PROFILING2
//     end = clock();
//     stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
//     totalTimes[1] += stepTime; // 필터 출력 및 잔차 신호 계산 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     start = clock();
// #endif
//     // 노멀라이제이션 팩터 계산
//     float normFactor = 0.0f;
//     for (int i = 0; i < FILTER_LENGTH; i++) {
//         normFactor += (float)x[i] * (float)x[i];
//     }
//     normFactor += 0.002f; // 0으로 나누는 것 방지
// #ifdef ENABLE_PROFILING2
//     end = clock();
//     stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
//     totalTimes[2] += stepTime; // 노멀라이제이션 팩터 계산 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     start = clock();
// #endif
//     // 스텝 크기 조정 및 필터 가중치 업데이트 (플로팅 연산)
//     float mu = (refPower > 25.0f || refPower < -40.0f) ? 0.15f * STEP_SIZE / normFactor : STEP_SIZE / normFactor;
//     for (int i = 0; i < FILTER_LENGTH; i++) {
//         weights[i] = weights[i] * leaky[i] + mu * (float)errorSignal[i] * (float)x[i];
//     }
// #ifdef ENABLE_PROFILING2
//     end = clock();
//     stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
//     totalTimes[3] += stepTime; // 필터 가중치 업데이트 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     start = clock();
// #endif
//     // 필터 계수 발산 방지
//     int resetWeights = 0; // 초기값: 0 (거짓)
//     for (int i = 0; i < FILTER_LENGTH; i++) {
//         if (fabs(weights[i]) > WEIGHT_THRESHOLD || isnan(weights[i])) {
//             resetWeights = 1; // 참
//             break;
//         }
//     }
//     if (resetWeights) {
//         for (int i = 0; i < FILTER_LENGTH; i++) {
//             weights[i] = 0.0f; // 필터 계수를 0으로 초기화
//         }
//         printf("Weights reset to prevent divergence.\n");
//     }
// #ifdef ENABLE_PROFILING2
//     end = clock();
//     stepTime = ((double)(end - start)) / CLOCKS_PER_SEC;
//     totalTimes[4] += stepTime; // 필터 계수 발산 방지 시간 누적
// #endif

// #ifdef ENABLE_PROFILING2
//     // 한 프레임 처리 후 평균 시간 출력
//     printf("Execution time for NLMS echo canceller (per frame):\n");
//     printf("1. x Array Update: %f seconds\n", totalTimes[0]);
//     printf("2. Filter Output & Error Signal: %f seconds\n", totalTimes[1]);
//     printf("3. Normalization Factor: %f seconds\n", totalTimes[2]);
//     printf("4. Weight Update: %f seconds\n", totalTimes[3]);
//     printf("5. Weight Reset Check: %f seconds\n", totalTimes[4]);
// #endif
// }



// 밴드패스 필터 함수 (플로팅 연산, 상태 유지)
void bandpass_filter_float(float *signal, int length, double *b, double *a, int order, double *xState, double *yState) {
    for (int n = 0; n < length; n++) {
        double y = b[0] * (double)signal[n]; // b[0] * x[n]

        // IIR 필터링 (Recursive part)
        for (int i = 1; i <= order; i++) {
            y += b[i] * (double)xState[i - 1]; // 분자 계수 (b)와 xState 곱하기 (양수)
            // printf("y=%f ", y);
            y -= a[i] * (double)yState[i - 1]; // 분모 계수 (a)와 yState 곱하기 (음수)
            // printf("y=%f ", y);
        }

        // printf("y=%.2f ", y);

        // `a[0]`으로 출력 정규화
        // y /= a[0];

        // 상태 업데이트 (Shift states)
        for (int i = order - 1; i > 0; i--) {
            xState[i] = xState[i - 1];
            yState[i] = yState[i - 1];
            // printf("yState[%d]=%.2f ", i, yState[i]);
        }
        xState[0] = signal[n]; // 현재 입력 샘플을 xState에 저장
        yState[0] = (float)y;         // 현재 출력을 yState에 저장
        // printf("yState[0]=%.2f \r\n", yState[0]);

        signal[n] = (float)y; // 필터링된 신호 저장
    }
}

int16_t refBuffer[BUFFER_SIZE] = {0}; // 레퍼런스 신호 버퍼
int16_t micBuffer[BUFFER_SIZE] = {0}; // 마이크 신호 버퍼
int bufferIndex = 0; // 버퍼 인덱스

// 크로스 코릴레이션을 사용하여 글로벌 딜레이 계산 (정수 연산)
long long crossCorr[MAX_DELAY * 2 + 1] = {0}; // 누적 크로스 코릴레이션 결과
int frameCount = 0; // 처리한 프레임 수
int estimatedDelay = 0; // 추정된 글로벌 딜레이    
int delayupdated = 0;
int delayinit = 0;
int corr_hold = 0;

// #define ENABLE_PROFILING // 연산 시간 측정 활성화

#define NUM_STEPS 8 // 총 연산 단계 수
#define PRINT_INTERVAL 1.0 // 평균을 출력할 간격 (초)

int32_t mem_r;
int32_t mem_d;

int32_t mem_r_d;
int32_t mem_d_d;

float gate_aec = 1.0;

void AEC_single_Proc_matlab(aecInst_t *aecInst, int16_t *inBufRef, int16_t *inBufMic, int16_t *outBuf, int framelen, int delay, int delay_auto, int filton, float MicscaledB) {
    aecInst_t *inst = (aecInst_t *)aecInst;
    float refPower, refRMS;
    float micPower, micRMS;

    int updateon=1;

    float micscale = powf(10.0f, MicscaledB/20.0);

    int32_t pre_emph_q15 = (int32_t)(0.7f * 32768.0f);

    if (inBufRef != NULL) {

        // if ((delay_auto == 1)) {
        //     // 버퍼에 현재 프레임을 추가 (순환 버퍼처럼 사용)
        //     memmove(&refBuffer[0], &refBuffer[framelen], (BUFFER_SIZE - framelen) * sizeof(int16_t));
        //     memmove(&micBuffer[0], &micBuffer[framelen], (BUFFER_SIZE - framelen) * sizeof(int16_t));
        //     memcpy(&refBuffer[BUFFER_SIZE - framelen], inBufRef, framelen * sizeof(int16_t));
        //     memcpy(&micBuffer[BUFFER_SIZE - framelen], inBufMic, framelen * sizeof(int16_t));

        //     refRMS = calculate_rms_int32(inBufRef, framelen);
        //     refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        //     if (refPower > MIN_POWER) {
        //         corr_hold = 2;
        //     }

        //     corr_hold--;
        //     if (corr_hold <= 0) corr_hold = 0;

        //     if (corr_hold > 0) {
        //         calculate_global_delay(refBuffer, micBuffer, framelen, crossCorr, &frameCount, &estimatedDelay, &delayinit, &delayupdated);
        //     }

        //     if (delayupdated == 1) {
        //         AEC_delay_init(inst, &d_buf_g[0][0], Ref_delay);
        //         delayupdated = 0;
        //     }
        // }

        if (delayinit == 0) {
            estimatedDelay = delay;
        }



        pre_emphasis(&inBufRef[0], pre_emph_q15, &mem_r, framelen);
        pre_emphasis(&inBufMic[0], pre_emph_q15, &mem_d, framelen);

        // de_emphasis(&inBufRef[0], pre_emph_q15, &mem_r_d, framelen);
        // de_emphasis(&inBufMic[0], pre_emph_q15, &mem_d_d, framelen);

        // 글로벌 딜레이 적용
        AEC_delay_with_framelen(inst, &inBufRef[0], &inBufRef[0], &d_buf_g[0][0], estimatedDelay, framelen);        

        // 참조 신호의 RMS 크기 계산
        refRMS = calculate_rms_int32(inBufRef, framelen);
        refPower = 20.0f * log10f(refRMS + 1e-10f) - 90.3090f; // dBFS로 변환
        // micRMS = calculate_rms_int32(inBufMic, framelen);
        // micPower = 20.0f * log10f(micRMS + 1e-10f) - 90.3090f; // dBFS로 변환

        // printf("refPower = %.2fdB, micPower=%.2f \r\n",refPower, micPower);     

        // 참조 신호와 마이크 신호를 부드럽게 노멀라이즈
        if (refPower > MIN_POWER) {
            smooth_normalize_signal_rms_int32(inBufRef, refRMS, framelen, &refScale, -25.0);

            // micScaleQ15 = (int32_t)(micScale * refScale * 32768.0f);
            micScaleQ15 = (int32_t)(micscale * refScale * 32768.0f);

            for (int i = 0; i < framelen; i++) {
                inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
            }

            updateon = 1;

        } else {
            refScaleQ15 = (int32_t)(refScale * 32768.0f);
            // micScaleQ15 = (int32_t)(micScale * refScale * 32768.0f);
            micScaleQ15 = (int32_t)(micscale * refScale * 32768.0f);

            for (int i = 0; i < framelen; i++) {
                inBufRef[i] = (int16_t)(((int32_t)(inBufRef[i]) * refScaleQ15) >> 15);
                inBufMic[i] = (int16_t)(((int32_t)(inBufMic[i]) * micScaleQ15) >> 15);
            }

            updateon = 0;
        }

        // printf("refScale = %f, micScale=%f \r\n",refScale, micScale);
    }

    // if (filton == 1) {
    //     // 마이크 신호에 밴드패스 필터 적용 (플로팅 연산)
    //     float micSignalFloat[2000];
    //     for (int i = 0; i < framelen; i++) {
    //         micSignalFloat[i] = (float)(inBufMic[i]) / 32768.0f; // 정수를 부동소수점으로 변환
    //     }
    //     bandpass_filter_float(micSignalFloat, framelen, bfilt, afilt, ORDER, xState, yState);

    //     for (int i = 0; i < framelen; i++) {
    //         inBufMic[i] = (int16_t)(micSignalFloat[i] * 32768.0f);
    //     }
    // }

    if (inBufRef != NULL) {

        // printf("refPower= %.2f, ",refPower);
        // NLMS 에코 캔슬러 실행
        nlms_echo_canceller(inBufRef, inBufMic, weights, leaky, errorSignal, x, refPower, &prevAttenuation, framelen, updateon);


        int32_t inverseMicScale = (int32_t)(32768.0f / (refScale));
        for (int i = 0; i < framelen; i++) {
            // inBufRef[i] = inBufMic[i];
            inBufRef[i] = (int16_t)((inverseMicScale * (int32_t)inBufMic[i])>>15); // 잔차 신호를 역으로 스케일링
        }

        // 역 스케일링: 잔차 신호를 원래 크기로 보정
        // inverseMicScale = (int32_t)(32768.0f / (micScale * refScale));
        float gate_dB = 0.0f;
        if (refPower > MIN_POWER) {
            gate_dB = -20.0f;
        } 
        
        float gate_level = powf(10.0f, (gate_dB/20.0f));
        gate_aec = gate_aec * 0.8 + gate_level * 0.2;
        inverseMicScale = (int32_t)((32768.0f / (micscale * refScale)) * gate_aec);

        for (int n = 0; n < framelen; n++) {
            errorSignal[n] = (int16_t)((inverseMicScale * (int32_t)errorSignal[n])>>15); // 잔차 신호를 역으로 스케일링
        }

        for (int i = 0; i < framelen; i++) {
            inBufMic[i] = errorSignal[i];
        }
    }

    de_emphasis(&inBufRef[0], pre_emph_q15, &mem_r_d, framelen);
    de_emphasis(&inBufMic[0], pre_emph_q15, &mem_d_d, framelen);
    
}

// INT32_MIN이 정의되지 않은 경우 직접 정의
#ifndef INT32_MIN
#define INT32_MIN (-2147483647 - 1)
#endif

// 크로스 코릴레이션을 사용하여 글로벌 딜레이를 계산하는 함수 (정수 연산)
void calculate_global_delay(int16_t *refBuffer, int16_t *micBuffer, int framelen, long long *crossCorr, int *frameCount, int *estimatedDelay, int *delayinit, int *delayupdated) {
    // 각 프레임마다 크로스 코릴레이션 계산 (정수 연산)
    for (int delay = -MAX_DELAY; delay <= MAX_DELAY; delay++) {
        long long sum = 0;
        for (int i = 0; i < framelen; i++) {
            int refIndex = i;
            int micIndex = i + delay + MAX_DELAY; // micBuffer에서의 인덱스

            // 범위를 벗어나지 않는 경우에만 계산
            if (micIndex >= 0 && micIndex < BUFFER_SIZE) {
                sum += (long long)refBuffer[refIndex] * (long long)micBuffer[micIndex];
            }
        }
        crossCorr[delay + MAX_DELAY] += sum; // 누적 결과에 합산
    }

    (*frameCount)++;

    // 5초 동안 모든 프레임을 처리한 경우 최종 결과 계산
    if (*frameCount >= (SAMPLE_RATE * 5 / framelen)) { // 5초 동안 모든 프레임 처리 완료
        long long maxCorr = LONG_LONG_MIN;
        int delay_result = 0;
        for (int delay = -MAX_DELAY; delay <= MAX_DELAY; delay++) {
            if ((crossCorr[delay + MAX_DELAY]) > maxCorr) {
                maxCorr = (crossCorr[delay + MAX_DELAY]);

                delay_result = delay+MAX_DELAY-100;
            }
        }

        if (*delayinit == 0){
            *estimatedDelay = delay_result;
            *delayinit = 1;
            *delayupdated = 1;
        }

        if (abs(delay_result-*estimatedDelay)>100) {
            *estimatedDelay = delay_result;        
            *delayupdated = 1;
            *delayinit = 1;
        }

        printf("delay_result = %d, Estimated Global Delay: %d\n", delay_result, *estimatedDelay);        
            

        // // 추정된 딜레이 출력
        // printf("Estimated Global Delay: %d\n", *estimatedDelay);

        // 누적 결과 초기화
        for (int i = 0; i < (MAX_DELAY * 2 + 1); i++) {
            crossCorr[i] = 0;
        }
        *frameCount = 0; // 프레임 카운트 초기화
    }
}


int pre_emphasis(int16_t *signal, int32_t pre_emph_q15, int32_t *mem, size_t frame_size) {

    int saturated = 0;

    for (size_t i = 0; i < frame_size; i++) {

        // Pre-emphasis 계산 (Q15 형식)
        int32_t tmp = (int32_t)signal[i] - ((pre_emph_q15 * (*mem))>>15);

         if (tmp > 32767)
         {
            tmp = 32767;
            saturated = 1;
         }
         if (tmp < -32767)
         {
            tmp = -32767;
            saturated = 1;
         }

        // 메모리를 업데이트 (이전 샘플 저장)
        *mem = (int32_t)signal[i];

        // 결과를 다시 int16_t로 변환
        signal[i] = (int16_t)(tmp);
    }

    return saturated;

}

int de_emphasis(int16_t *signal, int32_t de_emph_q15, int32_t *mem, size_t frame_size) {

    int saturated = 0;

    for (size_t i = 0; i < frame_size; i++) {
        // De-emphasis 계산 (Q15 형식)
        int32_t tmp = (int32_t)signal[i] + ((de_emph_q15 * (*mem)) >> 15);

         if (tmp > 32767)
         {
            tmp = 32767;
            saturated = 1;
         }
         if (tmp < -32767)
         {
            tmp = -32767;
            saturated = 1;
         }

        // 메모리를 업데이트 (이전 샘플 저장)
        *mem = (int32_t)signal[i];

        // 결과를 다시 int16_t로 변환
        signal[i] = (int16_t)(tmp);
    }

    return saturated;
}