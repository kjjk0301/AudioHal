/*
 * noise_supression.c
 *
 *  Created on: 2018. 8. 28.
 *      Author: Seong Pil Moon
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

#include "sys.h"
#include "noise_supression.h"
#include "time_constant.h"
#include "dB_scale.h"

#include "debug_file.h"

#pragma DATA_ALIGN(SEE, 8)
int32_t SEE[PolyM];
#pragma DATA_ALIGN(SBB, 8)
int32_t SBB[PolyM];

// #pragma DATA_ALIGN(HPRE2, 8)
// int HPRE2[PolyM];
#pragma DATA_ALIGN(H_MAIN, 8)
int32_t H_MAIN[PolyM];
int32_t Hmin[PolyM/2+1];
nsInst_t Sys_nsInst;

nsInst_t* sysNSCreate()
{
    NS_Init(&Sys_nsInst);

    return &Sys_nsInst;
}

void NS_Init(void *nsInst){

    nsInst_t *inst = (nsInst_t *)nsInst;
	int k=0;

    inst->blocksize=polyblocksize;
    inst->N=PolyN; // polyphase filter length
    inst->M=PolyM; // filterbank channel number
    inst->R=PolyR; // decimation factor
    inst->L=PolyL; // blocksize/R : channel samples within a block

    inst->Ma_size = 2;

    inst->voice_start_bin = 3;
    inst->voice_end_bin = 32;

    inst->Low_bypass=0;
    inst->Mid_bypass=0;
    inst->Hi_bypass=0;

    inst->Low_solo=0;
    inst->Mid_solo=0;
    inst->Hi_solo=0;

    inst->EQ_Low=0;
    inst->EQ_Mid=0;
    inst->EQ_Hi=0;

    inst->beta_e_num[0]=17; //11; // high rising
    inst->beta_e_num[1]=17; //11; // high falling
    inst->beta_e_num[2]=17; //14; // mid(voice) rising
    inst->beta_e_num[3]=17; //14; // mid(voice) falling
    inst->beta_e_num[4]=17; //16; // low rising
    inst->beta_e_num[5]=17; //17; // low falling

    for (k=0; k<6; k++){
        inst->beta_e[k]=beta[inst->beta_e_num[k]];
        inst->round_bit_e[k]=round_bit[inst->beta_e_num[k]];
        inst->gamma_inv_e[k]=gamma_inv[inst->beta_e_num[k]];
        // printf("    time_constant=%.2f msec, beta_e[k]=%d, round_bit_e[k]=%d, gamma_inv_e[k]=%d \r\n",tau_msec[inst->beta_e_num[k]], inst->beta_e[k], inst->round_bit_e[k], inst->gamma_inv_e[k]);
    }

    inst->beta_r_num[0]=6;
    inst->beta_r_num[2]=15;
    inst->beta_r_num[3]=6;
    inst->beta_r_num[5]=15;
    inst->beta_r_num[6]=6;
    inst->beta_r_num[8]=15;            

    for (k=0; k<9; k++){
        inst->beta_r[k]=beta[inst->beta_r_num[k]];
        inst->round_bit_r[k]=round_bit[inst->beta_r_num[k]];
        inst->gamma_inv_r[k]=gamma_inv[inst->beta_r_num[k]];
        // printf("    time_constant=%.2f msec, beta_r[k]=%d, round_bit_r[k]=%d, gamma_inv_r[k]=%d \r\n",tau_msec[inst->beta_r_num[k]], inst->beta_r[k], inst->round_bit_r[k], inst->gamma_inv_r[k]);
    }    

    inst->See_p=SEE;
    inst->Sbb_p=SBB;

    inst->H_p = H_MAIN;
	inst->HminQ15_p = Hmin;
	inst->betaQ15=(int32_t)(0.4*32767.0);

    inst->beta_low_ratio = 2.0;
    inst->beta_high_ratio = 2.0;

	for (k=0 ; k<PolyM ; k++){
		(inst->H_p)[k]=32767;
		(inst->See_p)[k]=0;
		(inst->Sbb_p)[k]=0;
	}

    inst->max_att=-25; //-35;
    inst->min_att=-4; //-6;
    inst->high_att=-8; //-10;
    inst->slope = 2;

    float rtemp = 1.0 / (float) (inst->slope);

    for (k=0; k<inst->voice_start_bin; k++) {
        float temp= inst->min_att + (inst->max_att/((float)inst->voice_start_bin*(float)inst->voice_start_bin)) * (float)((k-inst->voice_start_bin)*(k-inst->voice_start_bin));
        (inst->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);

        // if (k%8==0) printf("\n");
        // printf("%2.1fdB, %d, ", 20.0*log10((float)(inst->HminQ15_p)[k]/(float)Q15_val), (inst->HminQ15_p)[k]);    
    }

        // printf("\n");
        
    for (k=inst->voice_start_bin ; k<PolyM/2+1; k++){
        float temp= inst->min_att + inst->high_att + (-1 * inst->high_att)*powf(rtemp,((float)(k-inst->voice_start_bin)/(float)(inst->voice_end_bin-inst->voice_start_bin)));
        
        // if (k>inst->voice_end_bin){
        //     temp= temp + (float)(k-inst->voice_end_bin)/(float)(PolyM/2-inst->voice_end_bin)*3.0;
        // }
        
        (inst->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);
        // if (k%8==0) printf("\n");
        // printf("%2.1fdB, %d, ", 20.0*log10((float)(inst->HminQ15_p)[k]/(float)Q15_val), (inst->HminQ15_p)[k]);  
    }


    // for (k=0 ; k<=inst->voice_end_bin; k++){
    //     (inst->HminQ15_p)[k] = (int32_t)(powf(10,((((inst->max_att-inst->min_att)*(inst->slope))/((float)k+inst->slope)+inst->min_att)/20))*(float)Q15_val);
    //     printf("    Hmin[%d] = %f dB, HminQ15 = %d\r\n", k, 20.0*log10((float)(inst->HminQ15_p)[k]/(float)Q15_val), (inst->HminQ15_p)[k]);
    // }

    // for (k=inst->voice_end_bin+1 ; k<PolyM/2+1; k++){
    //     (inst->HminQ15_p)[k] = (int32_t)(powf(10,((((inst->max_att-inst->min_att)*(inst->slope))/((float)(k)+inst->slope)+inst->min_att+((inst->high_att-inst->min_att)*(inst->slope))/((float)(PolyM/2+1- k) *0.5+inst->slope*0.5))/20))*(float)Q15_val);
    //     // (inst->HminQ15_p)[k] = (int32_t)(powf(10,((((inst->max_att-inst->min_att)*(inst->slope))/((float)k+inst->slope)+inst->min_att+inst->high_att)/20))*(float)Q15_val);
    //     printf("    Hmin[%d] = %f dB, HminQ15 = %d\r\n", k, 20.0*log10((float)(inst->HminQ15_p)[k]/(float)Q15_val), (inst->HminQ15_p)[k]);
    // }

    memset(&fft_ns_buf_mat[0], 0, sizeof(int32_t)*(PolyM*(PolyL+Ma_size_max-1))); 	

// #ifdef DEBUG_NS_MATLAB
// 	debug_matlab_open();
// #endif	

}

void NS_DeInit(){

// #ifdef DEBUG_NS_MATLAB
// 	debug_matlab_close();
// #endif	
}

void NS_process(void *nsInst, void *fft_in_mat, void *fft_buf_mat, void *fft_out_mat, int vad){

	int32_t pow_inst[PolyM/2+1];
    int k, m, n, kk, Hpre_Q15, Hmin_Q15, temp;
    int32_t bb[PolyM]={ 0, };
	int32_t Htemp_Q15[PolyM]={ 0, };
	short beta_r, round_bit_r;

    float pow_inst_f=0;
    float tempf = 0;

    nsInst_t *inst = (nsInst_t *)nsInst;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
    int32_t *fft_xbuf_mat = (int32_t *)fft_buf_mat;

    int32_t *See = (int32_t *)inst->See_p;
    int32_t *Sbb = (int32_t *)inst->Sbb_p;

    int32_t *H_Q15 = inst->H_p;

    // float r_a = 0.995;
	// float r_r = 0.995;
    // float r_a = 0.001;
    // float r_r = 0.005;

    int buf_len = PolyL + inst->Ma_size+2 -1;
    float deno = 1.0/((float)inst->Ma_size);
    float deno2 = 1.0/((float)(inst->Ma_size)+2.0);

    float beta_low = (float)(inst->betaQ15)*inst->beta_low_ratio;
    float beta_high = (float)(inst->betaQ15)*inst->beta_high_ratio;

    for (n=0, kk=0; n<PolyM/2 ; n++) {

        for (k=0; k<buf_len-PolyL; k++) {
            fft_xbuf_mat[kk+k] = fft_xbuf_mat[kk+k+PolyL];
        }
        kk+=(PolyL+Ma_size_max-1);
    }


    for (m=0 ; m<PolyL ; m++){
        // Power estimation
        for (n=0, k=m*2, kk=(inst->Ma_size)-1+m; n<PolyM/2+1 ; n++) {
            pow_inst_f=(float)fft_xin_mat[k]*(float)fft_xin_mat[k]+(float)fft_xin_mat[k+1]*(float)fft_xin_mat[k+1];
            pow_inst[n]=((int)(sqrtf(pow_inst_f)*1024));
            fft_xbuf_mat[kk] = pow_inst[n];

            k+=PolyL*2;
            kk+=(PolyL+Ma_size_max-1);
        }

        if ((vad>0) || (vad<0) ){
            for (n = 0, kk=m; n<PolyM/2; n++){
                tempf = 0.0;

                if (n<inst->voice_start_bin){
                    for (k=0; k<inst->Ma_size+2; k++){
                        tempf+=(float)fft_xbuf_mat[kk+k];
                    }
                    tempf=tempf * deno2;
                    See[n] = (int32_t) tempf;
                }

                else if (n<inst->voice_end_bin){
                    for (k=0; k<inst->Ma_size; k++){
                        tempf+=(float)fft_xbuf_mat[kk+k];
                    }
                    tempf=tempf * deno;
                    See[n] = (int32_t) tempf;
                }
                
                else {
                    for (k=0; k<inst->Ma_size+2; k++){
                        tempf+=(float)fft_xbuf_mat[kk+k];
                    }
                    tempf=tempf * deno2;
                    See[n] = (int32_t) tempf;
                } 

                
                kk+=(PolyL+Ma_size_max-1);
            }
        }
        else 
        {
            for (n=0; n<inst->voice_start_bin ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[4]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[5]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                    } 
                }
            }

            for (n=inst->voice_start_bin; n<=inst->voice_end_bin ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[2]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[3]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                    }
                }
            }

            for (n=inst->voice_end_bin+1; n<PolyM/2+1 ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[0]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[1]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                    }
                }
            }
        }






        // estimate Sbb
        if (vad<=0){

            // frequency axis smoothing
            bb[0]=See[0];

            for (k=1; k<=inst->voice_end_bin; k++){
                // bb[k]=See[k]-((See[k]+(inst->round_bit_freq))>>(inst->beta_freq));
                // bb[k]=bb[k]+((bb[k-1]+(inst->round_bit_freq))>>(inst->beta_freq));
                bb[k] = (int32_t)(0.5 * (double)(bb[k-1]) + (1-0.5) * (double)(See[k]));  
            }

            for (k=inst->voice_end_bin+1; k<PolyM/2+1; k++){
                // bb[k]=See[k]-((See[k]+(inst->round_bit_freq_high))>>(inst->beta_freq_high));
                // bb[k]=bb[k]+((bb[k-1]+(inst->round_bit_freq_high))>>(inst->beta_freq_high));
                bb[k] = (int32_t)(0.5 * (double)(bb[k-1]) + (1-0.5) * (double)(See[k]));  
            }

            for (k = PolyM/2-1; k>inst->voice_end_bin ; k--){
                // bb[k]=bb[k]-((bb[k]+(inst->round_bit_freq_high))>>(inst->beta_freq_high));
                // bb[k]=bb[k]+((bb[k+1]+(inst->round_bit_freq_high))>>(inst->beta_freq_high));
                bb[k] = (int32_t)(0.5* (double)(bb[k+1]) + (1-0.5) * (double)(bb[k]));  
            }

            for (k = inst->voice_end_bin; k>=0 ; k--){
                // bb[k]=bb[k]-((bb[k]+(inst->round_bit_freq))>>(inst->beta_freq));
                // bb[k]=bb[k]+((bb[k+1]+(inst->round_bit_freq))>>(inst->beta_freq));
                bb[k] = (int32_t)(0.5 * (double)(bb[k+1]) + (1-0.5) * (double)(bb[k])); 
            }



            for (k = 0 ; k< inst->voice_start_bin ; k++) {
                
                if (bb[k]>Sbb[k]) {
				// if (vad==0){
                    // Sbb[k] = (int32_t)(0.0183 * (double)(Sbb[k]) + (1-0.0183) * (double)(bb[k]));  
                    if (inst->gamma_inv_r[6]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[6]))>>(inst->beta_r[6]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[6]))>>(inst->beta_r[6]));
                    } else {
                        beta_r=inst->beta_r[6];
                        round_bit_r=(inst->round_bit_r[6]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
				// } 
                }
                else {
                // if (vad==0){
                    // Sbb[k] = (int32_t)(0.9608 * (double)(Sbb[k]) + (1-0.9608) * (double)(bb[k])); 
                    if (inst->gamma_inv_r[8]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[8]))>>(inst->beta_r[8]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[8]))>>(inst->beta_r[8]));
                    } else {
                        beta_r=inst->beta_r[8];
                        round_bit_r=(inst->round_bit_r[8]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
                // }
                }
            }
        
            for (k = inst->voice_start_bin ; k<= inst->voice_end_bin ; k++) {
                if (bb[k]>Sbb[k]) {
				// if (vad==0){
                    if (inst->gamma_inv_r[3]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[3]))>>(inst->beta_r[3]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[3]))>>(inst->beta_r[3]));
                    } else {
                        beta_r=inst->beta_r[3];
                        round_bit_r=(inst->round_bit_r[3]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
				// }
                }
                else {
                // if (vad==0){
                    if (inst->gamma_inv_r[5]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[5]))>>(inst->beta_r[5]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[5]))>>(inst->beta_r[5]));
                    } else {
                        beta_r=inst->beta_r[5];
                        round_bit_r=(inst->round_bit_r[5]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
                // }
                }
            }
        
            for (k = inst->voice_end_bin+1 ; k< PolyM/2 ; k++) {
                if (bb[k]>Sbb[k]) {
				// if (vad==0){
                    if (inst->gamma_inv_r[0]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[0]))>>(inst->beta_r[0]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[0]))>>(inst->beta_r[0]));
                    } else {
                        beta_r=inst->beta_r[0];
                        round_bit_r=(inst->round_bit_r[0]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
				// }
                }
                else {
                // if (vad==0){
                    if (inst->gamma_inv_r[2]==0) {
                        temp=bb[k]-((bb[k]+(inst->round_bit_r[2]))>>(inst->beta_r[2]));
                        Sbb[k]=temp+((Sbb[k]+(inst->round_bit_r[2]))>>(inst->beta_r[2]));
                    } else {
                        beta_r=inst->beta_r[2];
                        round_bit_r=(inst->round_bit_r[2]);
                        Sbb[k]=Sbb[k]-((Sbb[k]+round_bit_r)>>beta_r);
                        Sbb[k]=Sbb[k]+((bb[k]+round_bit_r)>>beta_r);
                    }
                // }
                }
            }
        }



		H_Q15[0]=0;
		H_Q15[PolyM/2]=0;

        for (k = 1 ; k<inst->voice_start_bin ; k++){
            Htemp_Q15[k]=(int32_t)(((float)Sbb[k]/((float)See[k]+0.1))*32768.0);
            Htemp_Q15[k]=32767-(int32_t)(((float)Htemp_Q15[k]*beta_low)/((float)H_Q15[k]+0.1));
            H_Q15[k]=MAX(Htemp_Q15[k], inst->HminQ15_p[k]);
        }     

        for (k = inst->voice_start_bin ; k<inst->voice_end_bin ; k++){
            Htemp_Q15[k]=(int32_t)(((float)Sbb[k]/((float)See[k]+0.1))*32768.0);
            Htemp_Q15[k]=32767-(int32_t)(((float)Htemp_Q15[k]*(float)(inst->betaQ15))/((float)H_Q15[k]+0.1));
            H_Q15[k]=MAX(Htemp_Q15[k], inst->HminQ15_p[k]);
        }       

        for (k = inst->voice_end_bin ; k<PolyM/2 ; k++){
            Htemp_Q15[k]=(int32_t)(((float)Sbb[k]/((float)See[k]+0.1))*32768.0);
            Htemp_Q15[k]=32767-(int32_t)(((float)Htemp_Q15[k]*beta_high)/((float)H_Q15[k]+0.1));
            H_Q15[k]=MAX(Htemp_Q15[k], inst->HminQ15_p[k]);
        }                 

        for (k=m*2, n=0; n<inst->voice_start_bin; n++){

            if (inst->Low_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Low))+4096)>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Low))+4096)>>13);
            }
            k+=PolyL*2;
        }

        for (n=inst->voice_start_bin; n<=inst->voice_end_bin; n++){

            if (inst->Mid_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Mid))+4096)>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Mid))+4096)>>13);
            }
            k+=PolyL*2;
        }
        for (n=inst->voice_end_bin+1; n<PolyM/2+1; n++){

            if (inst->Hi_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Hi)))>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Hi)))>>13);
            }
            k+=PolyL*2;
        }                  
    }


#ifdef DEBUG_NS_MATLAB
if (g_ns_debug_on==1){
    debug_matlab_spectrum_oct_int(See, 0, 128);
    debug_matlab_spectrum_oct_int(Sbb, 1, 128);
    debug_matlab_spectrum_oct_int(H_Q15, 2, 128);
    debug_matlab_spectrum_oct_send(3);
}    
#endif    
}

    int32_t See_oct[22];
    int32_t H_Q15_oct[22];

void NS_oct_process(void *nsInst, void *fft_in_mat, void *fft_buf_mat, void *fft_out_mat, int vad){

	int32_t pow_inst[PolyM/2+1];
    int k, m, n, kk, Hpre_Q15, Hmin_Q15, temp;
    int32_t bb[PolyM]={ 0, };
	int32_t Htemp_Q15[PolyM]={ 0, };
	short beta_r, round_bit_r;

    float pow_inst_f=0;
    float tempf = 0;

    nsInst_t *inst = (nsInst_t *)nsInst;

    int32_t *fft_xin_mat = (int32_t *)fft_in_mat;
    int32_t *fft_xout_mat = (int32_t *)fft_out_mat;
    int32_t *fft_xbuf_mat = (int32_t *)fft_buf_mat;

    int32_t *See = (int32_t *)inst->See_p;
    int32_t *Sbb = (int32_t *)inst->Sbb_p;

    int32_t *H_Q15 = inst->H_p;

    const float third_octave_freqs[] = {31, 93, 156, 218, 281, 343, 406, 468, 531, 593, 656, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 7500};
    size_t freq_count = sizeof(third_octave_freqs) / sizeof(float);

    int buf_len = PolyL + inst->Ma_size+2 -1;
    float deno = 1.0/((float)inst->Ma_size);
    float deno2 = 1.0/((float)(inst->Ma_size)+2.0);

    float beta_low = (float)(inst->betaQ15)*inst->beta_low_ratio;
    float beta_high = (float)(inst->betaQ15)*inst->beta_high_ratio;

    for (n=0, kk=0; n<PolyM/2 ; n++) {

        for (k=0; k<buf_len-PolyL; k++) {
            fft_xbuf_mat[kk+k] = fft_xbuf_mat[kk+k+PolyL];
        }
        kk+=(PolyL+Ma_size_max-1);
    }


    for (m=0 ; m<PolyL ; m++){
        // Power estimation
        for (n=0, k=m*2, kk=(inst->Ma_size)-1+m; n<PolyM/2+1 ; n++) {
            pow_inst_f=(float)fft_xin_mat[k]*(float)fft_xin_mat[k]+(float)fft_xin_mat[k+1]*(float)fft_xin_mat[k+1];
            pow_inst[n]=((int)(sqrtf(pow_inst_f)*1024));
            fft_xbuf_mat[kk] = pow_inst[n];

            k+=PolyL*2;
            kk+=(PolyL+Ma_size_max-1);
        }

        if ((vad>0) || (vad<0) ){
            for (n = 0, kk=m; n<PolyM/2; n++){
                tempf = 0.0;

                if (n<inst->voice_end_bin){
                    for (k=0; k<inst->Ma_size; k++){
                        tempf+=(float)fft_xbuf_mat[kk+k];
                    }
                    tempf=tempf * deno;
                    See[n] = (int32_t) tempf;
                }
                
                else {
                    for (k=0; k<inst->Ma_size+2; k++){
                        tempf+=(float)fft_xbuf_mat[kk+k];
                    }
                    tempf=tempf * deno2;
                    See[n] = (int32_t) tempf;
                } 

                
                kk+=(PolyL+Ma_size_max-1);
            }
        }
        else 
        {
            for (n=0; n<inst->voice_start_bin ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[4]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[4]))>>(inst->beta_e[4]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[5]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[5]))>>(inst->beta_e[5]));
                    } 
                }
            }

            for (n=inst->voice_start_bin; n<=inst->voice_end_bin ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[2]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[2]))>>(inst->beta_e[2]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[3]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[3]))>>(inst->beta_e[3]));
                    }
                }
            }

            for (n=inst->voice_end_bin+1; n<PolyM/2+1 ; n++) {
                if (pow_inst[n]>See[n]) {
                    if (inst->gamma_inv_e[0]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[0]))>>(inst->beta_e[0]));
                    }
                }
                else {
                    if (inst->gamma_inv_e[1]==0) {
                        temp=pow_inst[n]-((pow_inst[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                        See[n]=temp+((See[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                    } else {
                        See[n]=See[n]-((See[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                        See[n]=See[n]+((pow_inst[n]+(inst->round_bit_e[1]))>>(inst->beta_e[1]));
                    }
                }
            }
        }

        See_oct[0] = 0;
        calculate_third_octave_spectrum(&See[0], &See_oct[1]);

        // estimate Sbb
        // if (vad<=0){

            // frequency axis smoothing
            // bb[0]=See_oct[0];

            // for (k=1; k<freq_count-1; k++){
            //     // bb[k]=See[k]-((See[k]+(inst->round_bit_freq))>>(inst->beta_freq));
            //     // bb[k]=bb[k]+((bb[k-1]+(inst->round_bit_freq))>>(inst->beta_freq));
            //     bb[k] = (int32_t)(0.5 * (double)(bb[k-1]) + (1-0.5) * (double)(See_oct[k]));  
            // }


            // for (k = freq_count-2; k>=0 ; k--){
            //     // bb[k]=bb[k]-((bb[k]+(inst->round_bit_freq))>>(inst->beta_freq));
            //     // bb[k]=bb[k]+((bb[k+1]+(inst->round_bit_freq))>>(inst->beta_freq));
            //     bb[k] = (int32_t)(0.5 * (double)(bb[k+1]) + (1-0.5) * (double)(See_oct[k])); 
            // }
            for(size_t i = 1; i < freq_count; i++) {
                float start_freq = third_octave_freqs[i-1];
                float end_freq = third_octave_freqs[i];
                int start_idx = (start_freq / (16000 / 2)) * (256 / 2);
                int end_idx = (end_freq / (16000 / 2)) * (256 / 2);

                int32_t sum = 0;
                float beta = 0;
                if (end_idx<inst->voice_start_bin){
                    if (See_oct[i]>Sbb[i]) {
                    // if (vad==0){
                        // Sbb[k] = (int32_t)(0.0183 * (double)(Sbb[k]) + (1-0.0183) * (double)(bb[k]));  
                        if (inst->gamma_inv_r[6]==0) {
                            temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[6]))>>(inst->beta_r[6]));
                            Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[6]))>>(inst->beta_r[6]));
                        } else {
                            beta_r=inst->beta_r[6];
                            round_bit_r=(inst->round_bit_r[6]);
                            Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                            Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                        }
                    // } 
                    }
                    else {
                    // if (vad==0){
                        // Sbb[i] = (int32_t)(0.9608 * (double)(Sbb[i]) + (1-0.9608) * (double)(bb[i])); 
                        if (inst->gamma_inv_r[8]==0) {
                            temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[8]))>>(inst->beta_r[8]));
                            Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[8]))>>(inst->beta_r[8]));
                        } else {
                            beta_r=inst->beta_r[8];
                            round_bit_r=(inst->round_bit_r[8]);
                            Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                            Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                        }
                    // }
                    }
                } else if (end_idx<=inst->voice_end_bin){
                    if (vad<=0){
                        if (See_oct[i]>Sbb[i]) {
                        // if (vad==0){
                            if (inst->gamma_inv_r[3]==0) {
                                temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[3]))>>(inst->beta_r[3]));
                                Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[3]))>>(inst->beta_r[3]));
                            } else {
                                beta_r=inst->beta_r[3];
                                round_bit_r=(inst->round_bit_r[3]);
                                Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                                Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                            }
                        // }
                        }
                        else {
                        // if (vad==0){
                            if (inst->gamma_inv_r[5]==0) {
                                temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[5]))>>(inst->beta_r[5]));
                                Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[5]))>>(inst->beta_r[5]));
                            } else {
                                beta_r=inst->beta_r[5];
                                round_bit_r=(inst->round_bit_r[5]);
                                Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                                Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                            }
                        // }
                        }
                    }
                } else {
                    if (vad<=0){
                        if (See_oct[i]>Sbb[i]) {
                        // if (vad==0){
                            if (inst->gamma_inv_r[0]==0) {
                                temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[0]))>>(inst->beta_r[0]));
                                Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[0]))>>(inst->beta_r[0]));
                            } else {
                                beta_r=inst->beta_r[0];
                                round_bit_r=(inst->round_bit_r[0]);
                                Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                                Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                            }
                        // }
                        }
                        else {
                        // if (vad==0){
                            if (inst->gamma_inv_r[2]==0) {
                                temp=See_oct[i]-((See_oct[i]+(inst->round_bit_r[2]))>>(inst->beta_r[2]));
                                Sbb[i]=temp+((Sbb[i]+(inst->round_bit_r[2]))>>(inst->beta_r[2]));
                            } else {
                                beta_r=inst->beta_r[2];
                                round_bit_r=(inst->round_bit_r[2]);
                                Sbb[i]=Sbb[i]-((Sbb[i]+round_bit_r)>>beta_r);
                                Sbb[i]=Sbb[i]+((See_oct[i]+round_bit_r)>>beta_r);
                            }
                        // }
                        }
                    }
                }
            }         

		H_Q15_oct[0]=0;
		H_Q15_oct[freq_count-2]=0;

		H_Q15[0]=0;
		H_Q15[PolyM/2]=0;


        for(size_t i = 1; i < freq_count; i++) {
            float start_freq = third_octave_freqs[i-1];
            float end_freq = third_octave_freqs[i];
            int start_idx = (start_freq / (16000 / 2)) * (256 / 2);
            int end_idx = (end_freq / (16000 / 2)) * (256 / 2);

            int32_t sum = 0;
            float beta = 0;
            if (end_idx<inst->voice_start_bin){
                beta=beta_low;
            } else if (end_idx<=inst->voice_end_bin){
                beta=(float)(inst->betaQ15);
            } else {
                beta=beta_high;
            }
    
            Htemp_Q15[i]=(int32_t)(((float)Sbb[i]/((float)See_oct[i]+0.1))*32768.0);
            Htemp_Q15[i]=32767-(int32_t)((float)Htemp_Q15[i]*beta/((float)H_Q15[start_idx]+0.1));

            for(int j = start_idx; j < end_idx; j++) {
                H_Q15[j]=MAX(Htemp_Q15[i], inst->HminQ15_p[j]);
            }
        }             

        for (k=m*2, n=0; n<inst->voice_start_bin; n++){

            if (inst->Low_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Low))+4096)>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Low))+4096)>>13);
            }
            k+=PolyL*2;
        }

        for (n=inst->voice_start_bin; n<=inst->voice_end_bin; n++){

            if (inst->Mid_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Mid))+4096)>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Mid))+4096)>>13);
            }
            k+=PolyL*2;
        }
        for (n=inst->voice_end_bin+1; n<PolyM/2+1; n++){

            if (inst->Hi_bypass==1){
                fft_xout_mat[k] = 0;
                fft_xout_mat[k+1] = 0;
            } else {
                fft_xout_mat[k]=((fft_xin_mat[k]*(H_Q15[n]>>(2-inst->EQ_Hi))+4096)>>13);
                fft_xout_mat[k+1]=((fft_xin_mat[k+1]*(H_Q15[n]>>(2-inst->EQ_Hi))+4096)>>13);
            }
            k+=PolyL*2;
        }                  
    }


#ifdef DEBUG_NS_MATLAB
	if ((g_ns_debug_on==1)&&(g_ns_debug_snd_idx==0)){           

        // debug_matlab_spectrum_oct_int(See, 0, 128);
        debug_matlab_spectrum_int(See_oct, 0, 21);
        // debug_matlab_spectrum_oct_int(Sbb, 1, 128);
        // debug_matlab_spectrum_int(Sbb, 1, 21);

        int32_t temp_out[22];

        temp_out[0]=0;

        for(size_t i = 1; i < freq_count; i++) {
            float start_freq = third_octave_freqs[i-1];
            float end_freq = third_octave_freqs[i];
            int start_idx = (start_freq / (16000 / 2)) * (256 / 2);
            int end_idx = (end_freq / (16000 / 2)) * (256 / 2);


            temp_out[i] = ((See_oct[i]>>7)*(H_Q15[start_idx]>>8));
        }        

        debug_matlab_spectrum_int(temp_out, 1, 21);
        debug_matlab_spectrum_oct_int(H_Q15, 2, 128);
        debug_matlab_spectrum_oct_send(3);
    }    

	if (g_ns_debug_on==1){
		g_ns_debug_snd_idx++;
		if (g_ns_debug_snd_idx>=g_ns_debug_snd_period) {
			g_ns_debug_snd_idx = 0;
		}
	}

#endif    
}
