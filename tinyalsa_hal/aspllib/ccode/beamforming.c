/*
 * beamforming.c
 *
 *  Created on: 2018. 9. 2.
 *      Author: Seong Pil Moon
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "sys.h"
#include "beamforming.h"
#include "beamforming_filt_2ch_250212.h"


bfInst_t Sys_bfInst;

int32_t bf_d_buf_g[BeamN][PolyM][(adaptive_BF_delay+PolyL+5)*2];
int32_t uBuf_bf_real[BeamN][PolyM][PolyL+adaptive_BF_taps-1];
int32_t uBuf_bf_imag[BeamN][PolyM][PolyL+adaptive_BF_taps-1];

int xref_bf_pow[BeamN][PolyM];
int err_bf_pow[BeamN][PolyM];


bfInst_t* sysBFCreate()
{
	int i, j;
    BF_Init(&Sys_bfInst, &fft_in_mat[0][0], &fft_bf_mat[0][0], &fft_bf_rear_mat[0][0]);
	for (j=0; j<BeamN; j++){
		for (i=0; i<PolyM; i++){	
		    BF_delay_init(&Sys_bfInst, &bf_d_buf_g[j][i][0], adaptive_BF_delay+1);	
		}
	}
	BF_adaptive_Init(&Sys_bfInst);

    return &Sys_bfInst;
}

void BF_Init(bfInst_t *bfInst, void *fft_xin_mat, void *fft_xout_mat, void *fft_xout_rear_mat)
{

	int m,n;
    bfInst_t *inst = (bfInst_t *)bfInst;
    unsigned char * tempPtr;

    inst->blocksize=polyblocksize;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block

//	    inst->bits=Q15;

    inst->MN=MicN; // number of microphones 
    inst->J=BeamN;  //  number of beam channels : BeamN

//		tempPtr = (unsigned char *)fft_xin_mat;
	for (m=0 ; m<MicN ; m++){
//			tempPtr=tempPtr + m*PolyM*PolyL*4;
//			inst->fft_bfin_mat[m]= (int32_t *)tempPtr;
		inst->fft_bfin_mat[m]=&fft_in_mat[m][0];
	}

//		tempPtr = (unsigned char *)fft_xout_mat;
	for (m=0 ; m<BeamN ; m++){
		inst->fft_bfout_mat[m]=&fft_bf_mat[m][0];

	}

	for (m=0 ; m<BeamN ; m++){
		for (n=0 ; n<(PolyM*PolyL*2) ; n++){
            fft_bf_mat[m][n];
            fft_bf_rear_mat[m][n];
        }
	}
    

//		tempPtr = (unsigned char *)fft_xout_rear_mat;
	for (m=0 ; m<BeamN ; m++){
		inst->fft_bfout_rear_mat[m]=&fft_bf_rear_mat[m][0];

	}

	
}

void BF_process(bfInst_t *bfInst){

    int k, m, n, j, i;
	int temp_real, temp_imag, temp_real2, temp_imag2, x_real, x_imag;

    bfInst_t *inst = (bfInst_t *)bfInst;

	int blocksize= inst->L;
	
	for (j = 0; j <BeamN ; j++)
	{
		i = 0;
		for (k = 0; k < PolyM/2+1 ; k++)
		{
			for (n = 0 ; n < blocksize ; n++)
			{
				temp_real=16384;
				temp_imag=16384;

				// temp_real2=16384;
				// temp_imag2=16384;
				
				for (m = 0 ; m <MicN; m++)
				{
					x_real=(int)(inst->fft_bfin_mat[m][i]);
					x_imag=(int)(inst->fft_bfin_mat[m][i+1]);
	 
					temp_real=temp_real+WbfQ15_real[j][k][m]*x_real;
					temp_real=temp_real-WbfQ15_imag[j][k][m]*x_imag;
					
					temp_imag=temp_imag+WbfQ15_real[j][k][m]*x_imag;
					temp_imag=temp_imag+WbfQ15_imag[j][k][m]*x_real;

					// temp_real2=temp_real2+WbfQ15_rear_real[j][k][m]*x_real;
					// temp_real2=temp_real2-WbfQ15_rear_imag[j][k][m]*x_imag;
					
					// temp_imag2=temp_imag2+WbfQ15_rear_real[j][k][m]*x_imag;
					// temp_imag2=temp_imag2+WbfQ15_rear_imag[j][k][m]*x_real;
				}
				inst->fft_bfout_mat[j][i]=(int32_t)(temp_real>>15);
				inst->fft_bfout_mat[j][i+1]=(int32_t)(temp_imag>>15);
				// inst->fft_bfout_rear_mat[j][i]=(int32_t)(temp_real2>>15);
				// inst->fft_bfout_rear_mat[j][i+1]=(int32_t)(temp_imag2>>15);				
				i+=2;
			}
		}
	}
}

void BF_process_rear(bfInst_t *bfInst){

    int k, m, n, j, i;
	int temp_real, temp_imag, temp_real2, temp_imag2, x_real, x_imag;

    bfInst_t *inst = (bfInst_t *)bfInst;

	int blocksize= inst->L;
	
	for (j = 0; j <BeamN ; j++)
	{
		i = 0;
		for (k = 0; k < 27 ; k++)
        // for (k = 0; k < 128 ; k++)
		{
			for (n = 0 ; n < blocksize ; n++)
			{
				temp_real2=16384;
				temp_imag2=16384;
				
				for (m = 0 ; m <MicN; m++)
				{
					x_real=(int)(inst->fft_bfin_mat[m][i]);
					x_imag=(int)(inst->fft_bfin_mat[m][i+1]);
	 
					temp_real2=temp_real2+WbfQ15_rear_real[j][k][m]*x_real;
					temp_real2=temp_real2-WbfQ15_rear_imag[j][k][m]*x_imag;
					
					temp_imag2=temp_imag2+WbfQ15_rear_real[j][k][m]*x_imag;
					temp_imag2=temp_imag2+WbfQ15_rear_imag[j][k][m]*x_real;
				}
				inst->fft_bfout_rear_mat[j][i]=(int32_t)(temp_real2>>15);
				inst->fft_bfout_rear_mat[j][i+1]=(int32_t)(temp_imag2>>15);				
				i+=2;
			}
		}
	}
}

void BF_delay(bfInst_t *bfInst, int32_t *inBuf, int32_t *outBuf, int32_t *d_Buf, int delay)
{
    int i;
    unsigned char *tempPtr1, *tempPtr2;

    bfInst_t * inst = (bfInst_t *)bfInst;

    int32_t *xin  = (int32_t *)inBuf;
    int32_t *xout = (int32_t *)outBuf;
    int32_t *delay_Buf = (int32_t *)d_Buf;

    int blocksize= inst->L;

    tempPtr1=(unsigned char *)&delay_Buf[0];
    tempPtr2=(unsigned char *)&delay_Buf[blocksize*2];
    for (i=0 ; i<(delay*2); i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 4;
    }

    tempPtr1=(unsigned char *)&delay_Buf[delay*2];
    tempPtr2=(unsigned char *)&xin[0];
    for (i=0 ; i<(blocksize*2); i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 4;
    }

    tempPtr1=(unsigned char *)&xout[0];
    tempPtr2=(unsigned char *)&delay_Buf[0];
    for (i=0 ; i<(blocksize*2); i++){
        memcpy(tempPtr1, tempPtr2, sizeof(int32_t));
        tempPtr1 += 4;
        tempPtr2 += 4;
    }
}

void BF_delay_init(bfInst_t *bfInst, int32_t *d_Buf, int delay){

    bfInst_t * inst = (bfInst_t *)bfInst;

    int32_t *delay_Buf = (int32_t *)d_Buf;

    int blocksize= inst->L;

    memset(&delay_Buf[0], 0, sizeof(int32_t)*(blocksize+delay)*2);
}

void BF_adaptive_Init(bfInst_t *bfInst){

    int j, k, muQ15;
    // float mu;
    bfInst_t * inst = (bfInst_t *)bfInst;

    inst->taps=adaptive_BF_taps;
    inst->bufsize=adaptive_BF_taps+PolyL-1;
    inst->delay=adaptive_BF_delay;

    inst->mu   = 0.003; // for adaptive BF 	
    muQ15=(int)(inst->mu*32767.0);

    inst->mu_tapQ15=muQ15/(1+inst->taps);

	for (j=0; j<inst->J; j++){
	    for (k=0 ; k<inst->M; k++){
	        inst->p_w_real[j][k]=&w_bf_real[j][k][0];
	        inst->p_w_imag[j][k]=&w_bf_imag[j][k][0];

	        inst->p_uBuf_real[j][k]=&uBuf_bf_real[j][k][0];
	        inst->p_uBuf_imag[j][k]=&uBuf_bf_imag[j][k][0];

	        inst->p_xref_pow[j][k]=&xref_bf_pow[j][k];
	        inst->p_err_pow[j][k]=&err_bf_pow[j][k];

	        memset(&inst->p_uBuf_real[j][k][0], 0, sizeof(int32_t)*(inst->bufsize));
	        memset(&inst->p_uBuf_imag[j][k][0], 0, sizeof(int32_t)*(inst->bufsize));
	        *(inst->p_xref_pow[j][k])=33;
	        *(inst->p_err_pow[j][k])=33;
	    }
	}
}


void BF_adaptive_Proc(bfInst_t *bfInst, int32_t *inBufRef, int32_t *inBufMic, int32_t *outBuf, int band, int Beam)
{
    int i, k, m, n, curidx, temp_real, temp_imag;
    // int p_inst, pe_inst, temp, devQ30, mu_optQ15, PinvQ18;
    unsigned int p_inst, pe_inst, temp, devQ30, mu_optQ15, PinvQ18;
    int n_e_realQ18, n_e_imagQ18, n_e_mu_realQ23, n_e_mu_imagQ23, n_e_mu_realQ23_opt, n_e_mu_imagQ23_opt;

    int32_t y_real, y_imag, e_real, e_imag;
    unsigned char *tempPtr1, *tempPtr2;

    bfInst_t * inst = (bfInst_t *)bfInst;

    int32_t *xin  = (int32_t *)inBufMic;
    int32_t *xref = (int32_t *)inBufRef;
    int32_t *xout = (int32_t *)outBuf;

    int taps     = inst->taps;
    int blocksize= inst->L;
    int bufsize  = inst->bufsize;

    int *w_real   = (int *)inst->p_w_real[Beam][band];
    int *w_imag   = (int *)inst->p_w_imag[Beam][band];


    int32_t *buf_real = (int32_t *)inst->p_uBuf_real[Beam][band];
    int32_t *buf_imag = (int32_t *)inst->p_uBuf_imag[Beam][band];

    int *p_p        = (int *)inst->p_xref_pow[Beam][band];
    int *p_pe       = (int *)inst->p_err_pow[Beam][band];

    int delay     = inst->delay;
    int   mu_tap    = inst->mu_tapQ15;

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
        temp_real=65536;
        temp_imag=65536;
        m=curidx;
        for (k=0; k<taps; k++){
            temp_real=temp_real + (int)(w_real[k]) * (int)(buf_real[m]);
            temp_real=temp_real + (int)(w_imag[k]) * (int)(buf_imag[m]);
            temp_imag=temp_imag - (int)(w_imag[k]) * (int)(buf_real[m]);
            temp_imag=temp_imag + (int)(w_real[k]) * (int)(buf_imag[m]);
            m--;
        }
        // y_real=(int32_t)(temp_real>>17);
        // y_imag=(int32_t)(temp_imag>>17);
        y_real=(int32_t)(temp_real>>19);
        y_imag=(int32_t)(temp_imag>>19);        

        // error calc
        e_real=xin[2*n]-y_real;
        e_imag=xin[2*n+1]-y_imag;

        xout[2*n]  =e_real;
        xout[2*n+1]=e_imag;

        // long term signal power
        p_inst=4     +(int)(buf_real[curidx])*(int)(buf_real[curidx]);
        p_inst=p_inst+(int)(buf_imag[curidx])*(int)(buf_imag[curidx]);

        pe_inst=4      +(int)(e_real)*(int)(e_real);
        pe_inst=pe_inst+(int)(e_imag)*(int)(e_imag);

        *p_p  = *p_p  - ((*p_p+4)>>3);
        *p_p  = *p_p  + (p_inst>>3);
        *p_pe = *p_pe - ((*p_pe+4)>>3);
        *p_pe = *p_pe + (pe_inst>>3);

        temp=(*p_pe)*(delay);
        temp=((*p_p)<<8)/(temp+1);

        devQ30=0;
        unsigned int temp2;
        for (k=0; k<delay; k++){
            temp2 = 8 + ((unsigned int)(w_real[k])*(unsigned int)(w_real[k])+(unsigned int)(w_imag[k])*(unsigned int)(w_imag[k]));
            devQ30 += (temp2)>>4;
        }

        devQ30=(int)sqrt((double)devQ30);
        
        mu_optQ15=temp*(devQ30)+2048;
        mu_optQ15=(mu_optQ15>>12);

        

        // inverse long term signal power
        PinvQ18 = ((int)(2147483647))/(*p_p+1);
        PinvQ18 = ((PinvQ18+512)>>10); //Q18 -> Q21

        // filter update
        n_e_realQ18=(int)(e_real)*PinvQ18;
        n_e_imagQ18=-1*(int)(e_imag)*PinvQ18;

        n_e_mu_realQ23=mu_tap*n_e_realQ18+512;
        n_e_mu_realQ23=(n_e_mu_realQ23>>10);
        n_e_mu_imagQ23=mu_tap*n_e_imagQ18+512;
        n_e_mu_imagQ23=(n_e_mu_imagQ23>>10);

        n_e_mu_realQ23_opt=mu_optQ15*n_e_realQ18+512;
        n_e_mu_realQ23_opt=(n_e_mu_realQ23_opt>>10);
        n_e_mu_imagQ23_opt=mu_optQ15*n_e_imagQ18+512;
        n_e_mu_imagQ23_opt=(n_e_mu_imagQ23_opt>>10);

        if (mu_tap>mu_optQ15) {
            n_e_mu_realQ23=n_e_mu_realQ23_opt;
            n_e_mu_imagQ23=n_e_mu_imagQ23_opt;
        }
        m=curidx;
        for (k=0; k<delay; k++) {
            temp_real=      1024+n_e_mu_realQ23*(int)(buf_real[m]); //Q23
            temp_real=temp_real-n_e_mu_imagQ23*(int)(buf_imag[m]); //Q23

            temp_imag=      1024+n_e_mu_realQ23*(int)(buf_imag[m]); //Q23
            temp_imag=temp_imag+n_e_mu_imagQ23*(int)(buf_real[m]); //Q23

            w_real[k] = w_real[k] + (temp_real>>11);
            w_imag[k] = w_imag[k] + (temp_imag>>11);
            m--;
        }

        for (k=delay; k<taps; k++) {
            temp_real=      1024+n_e_mu_realQ23_opt*(int)(buf_real[m]); //Q23
            temp_real=temp_real-n_e_mu_imagQ23_opt*(int)(buf_imag[m]); //Q23

            temp_imag=      1024+n_e_mu_realQ23_opt*(int)(buf_imag[m]); //Q23
            temp_imag=temp_imag+n_e_mu_imagQ23_opt*(int)(buf_real[m]); //Q23

            w_real[k] = w_real[k] + (temp_real>>11);
            w_imag[k] = w_imag[k] + (temp_imag>>11);
            m--;
        }
    }
    // if ((band==8) && (Beam==0)){
    //     printf("temp=%d, devQ30=%d, mu_optQ15=%d, mu_tap=%d PinvQ18=%d \r\n",temp, devQ30, mu_optQ15, mu_tap, PinvQ18);
    // }
  
    // if ((band==8) && (Beam==4)){
    //     printf("temp=%d, devQ30=%d, mu_optQ15=%d, mu_tap=%d PinvQ18=%d\r\n",temp, devQ30, mu_optQ15, mu_tap, PinvQ18);
    // }
}
