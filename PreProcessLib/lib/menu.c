
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
// #include <alsa/asoundlib.h>
#include <assert.h>

#include "./ccode/sys.h"
#include "./ccode/beamforming.h"
#include "./ccode/polyphase.h"
#include "./ccode/noise_supression.h"
#include "./ccode/echo_canceller.h"
#include "./ccode/autogaincontrol.h"
#include "./ccode/ssl_vad.h"
#include "./ccode/ssl_core.h"
//#include "./ccode/decimate_filt.h"
#include "./ccode/time_constant.h"
#include "./ccode/dB_scale.h"
#include "./ccode/audio_utils.h"
#include "./ccode/debug_file.h"
#include "../include/aspl_nr.h"
#include "../exec/keyboard.h"
// #include "menu.h"

// void NR_param_set(nsInst_t* nsInstp);
void NR_instance_power_param_set(nsInst_t* nsInstp);
void NR_noise_spectrum_estimation_set(nsInst_t* nsInstp);
void NR_Hmin_set(nsInst_t* nsInstp);
void NR_Target_freq_band_set(nsInst_t* nsInstp);
void Show_print_current_default_parameters(nsInst_t* nsInstp);
void Show_print_current_parameters(nsInst_t* nsInstp);
void reset_Hmin(nsInst_t* nsInstp);
void NR_EQ_set(nsInst_t* nsInstp);

void Show_mainmenu();
void Show_NR_default_detail_menu(int level);
void Show_NR_default_menu();
void Show_NR_menu();
void Show_NR_instance_power_menu();
void Show_NR_noise_spectrum_estimation_menu();
void Show_Hmin_set_menu(nsInst_t* nsInstp);
void Show_EQ_set_menu(nsInst_t* nsInstp);
void Show_Target_freq_band_Setup_menu(nsInst_t* nsInstp);

// void VAD_param_set(vadInst_t* vadInstp);
void Show_VAD_menu(vadInst_t* vadInstp);

// void AGC_param_set(agcInst_t* agcInstp);
void Show_AGC_menu();
void NR_default_param_set_detail(nsInst_t* nsInstp, int level);
void NR_default_param_set(nsInst_t* nsInstp);
void NR_param_set(nsInst_t* nsInstp);
void Show_mainmenu();
void VAD_param_set(vadInst_t* vadInstp);
void AGC_param_set(agcInst_t* agcInstp);

void BF_param_set(bfInst_t* bfInstp);
void Show_BF_menu(bfInst_t* bfInstp);

void ssl_param_set(sslInst_t* sslInstp);
void Show_ssl_menu(sslInst_t* sslInstp);

void aec_param_set(aecInst_t* aecInstp);
void Show_aec_menu(aecInst_t* aecInstp);

extern Total_Inst_t Sys_Total_Inst;

void param_set(){

    Total_Inst_t* Total_Instp = &Sys_Total_Inst;
    nsInst_t* nsInstp = Sys_Total_Inst.NSinst_p;
    agcInst_t* agcInstp = Sys_Total_Inst.agcInst_p;
    vadInst_t* vadInstp = Sys_Total_Inst.vadInst_p;
    bfInst_t* bfInstp = Sys_Total_Inst.bfInst_p;
    sslInst_t* sslInst_p = Sys_Total_Inst.sslInst_p;
    aecInst_t* aecInst_p = Sys_Total_Inst.aecInst_p;

    Show_mainmenu();


        while (1) {

        if (_kbhit())
        {
            int ch = _getch();
            _putch(ch);

            if(ch =='q') {
                printf("\n\r");
                // sem_post(&log_sem);
                break;
            } else if (ch =='a') {

                NR_param_set(nsInstp);

            } else if (ch =='b') {

                VAD_param_set(vadInstp);

            } else if (ch =='c') {

                AGC_param_set(agcInstp);

            } else if (ch =='d') {
                NR_default_param_set(nsInstp);
                // BF_param_set(bfInstp);

            } else if (ch =='t') {
                // NR_default_param_set(nsInstp);
                // BF_param_set(bfInstp);
                ssl_param_set(sslInst_p);
                // aec_param_set(aecInst_p);

            }             
            else if (ch =='e') {
                if (Total_Instp->NS_enable==1){
                    Total_Instp->NS_enable=0;
                    printf("NS disabled \r\n");
                } else if (Total_Instp->NS_enable==0){
                    Total_Instp->NS_enable=1;
                    printf("NS enabled \r\n");
                }
            }
            else if (ch =='f') {
                if (Total_Instp->AGC_enable==1){
                    Total_Instp->AGC_enable=0;
                    printf("AGC disabled \r\n");
                } else if (Total_Instp->AGC_enable==0){
                    Total_Instp->AGC_enable=1;
                    printf("AGC enabled \r\n");
                }
            }
            else if (ch =='u') {
                if (Total_Instp->bf_enable==1){
                    Total_Instp->bf_enable=0;
                    printf("BF disabled \r\n");
                } else if (Total_Instp->bf_enable==0){
                    Total_Instp->bf_enable=1;
                    printf("BF enabled \r\n");
                }
            }            
            else if (ch =='g') {
                if (Total_Instp->DC_rej_enable==1){
                    Total_Instp->DC_rej_enable=0;
                    printf("DC_rej disabled \r\n");
                } else if (Total_Instp->DC_rej_enable==0){
                    Total_Instp->DC_rej_enable=1;
                    printf("DC_rej enabled \r\n");
                }
            }

            else if (ch =='h') {
                Total_Instp->DC_beta += 1;
                if (Total_Instp->DC_beta >= 14) Total_Instp->DC_beta = 14;
                printf("DC_rej beta = %d \r\n", Total_Instp->DC_beta);
            }    

            else if (ch =='i') {
                Total_Instp->DC_beta -= 1;
                if (Total_Instp->DC_beta <= 0) Total_Instp->DC_beta = 0;
                printf("DC_rej beta = %d \r\n", Total_Instp->DC_beta);
            }     

            else if (ch =='j') {
                if (g_vad_debug_on==1){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;                       
                    printf("matlab vad_debug disabled \r\n");
                } else if (g_vad_debug_on==0){
                    g_vad_debug_on=1;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;                        
                    printf("matlab vad_debug enabled \r\n");
                }
            }

            else if (ch =='k') {
                if (g_agc_debug_on==1){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;                       
                    printf("matlab agc_debug disabled \r\n");
                } else if (g_agc_debug_on==0){
                    g_vad_debug_on=0;
                    g_agc_debug_on=1;
                    g_ns_debug_on=0;                        
                    printf("matlab agc_debug enabled \r\n");
                }
            }

            else if (ch =='l') {
                if (g_ns_debug_on==1){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;                       
                    printf("matlab ns_debug disabled \r\n");
                } else if (g_ns_debug_on==0){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=1;                        
                    printf("matlab ns_debug enabled \r\n");
                }
            }             


            else if (ch =='m') {
                // if (g_ssl_debug_on==1){
                //     g_vad_debug_on=0;
                //     g_agc_debug_on=0;
                //     g_ns_debug_on=0;    
                //     g_ssl_debug_on=0;
                //     printf("matlab ssl_debug disabled \r\n");
                // } else if (g_ssl_debug_on==0){
                //     g_vad_debug_on=0;
                //     g_agc_debug_on=0;
                //     g_ns_debug_on=0;   
                //     g_ssl_debug_on=1;                      
                //     printf("matlab ssl_debug enabled \r\n");
                // }
                if (g_aec_debug_on==1){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;    
                    g_ssl_debug_on=0;
                    g_aec_debug_on=0;
                    printf("matlab aec_debug disabled \r\n");
                } else if (g_aec_debug_on==0){
                    g_vad_debug_on=0;
                    g_agc_debug_on=0;
                    g_ns_debug_on=0;   
                    g_ssl_debug_on=0;                      
                    g_aec_debug_on=1;                                          
                    printf("matlab aec_debug enabled \r\n");
                }                
            }    

            else if (ch =='n') {
                Total_Instp->poly_scale += 1;
                if (Total_Instp->poly_scale >= 15) Total_Instp->poly_scale = 15;
                printf("poly_scale = %d \r\n", Total_Instp->poly_scale);
            }    

            else if (ch =='o') {
                Total_Instp->poly_scale -= 1;
                if (Total_Instp->poly_scale <= 0) Total_Instp->poly_scale = 0;
                printf("poly_scale = %d \r\n", Total_Instp->poly_scale);
            }   

            else if (ch =='p') {
                if (Total_Instp->bf_enable ==1){
                    Total_Instp->bf_enable=0;
                    printf("bf disabled \r\n");
                } else if (Total_Instp->bf_enable==0){
                    Total_Instp->bf_enable=1;
                    printf("bf enabled \r\n");
                }
            }
            else if (ch =='r') {
                
                double temp_dB = (20*log10((double)(Total_Instp->g_gainQ15)/32767.0));
                temp_dB = temp_dB + 1.0;

                if (temp_dB >= 35.0) temp_dB = 35.0;

                double temp_gain = pow(10, (double)temp_dB/20.0);
                Total_Instp->g_gainQ15 = (int)(temp_gain * 32767.0);

                printf("g_gain = %2.2f (%d) \r\n", temp_dB, Total_Instp->g_gainQ15);
            }    

            else if (ch =='s') {
                double temp_dB = (20*log10((double)(Total_Instp->g_gainQ15)/32767.0));
                temp_dB = temp_dB - 1.0;

                if (temp_dB < -30.0) temp_dB = -30.0;

                double temp_gain = pow(10, (double)temp_dB/20.0);
                Total_Instp->g_gainQ15 = (int)(temp_gain * 32767.0);

                printf("g_gain = %2.2f (%d) \r\n", temp_dB, Total_Instp->g_gainQ15);
            } 
            else if (ch =='v') {
                Total_Instp->tuning_delay += 10;
                if (Total_Instp->tuning_delay >= Ref_delay) Total_Instp->tuning_delay = Ref_delay;

                printf("tuning_delay = %d (%f msec) \r\n", Total_Instp->tuning_delay, (double)(Total_Instp->tuning_delay)/16.0);
            }    

            else if (ch =='w') {
                Total_Instp->tuning_delay -= 10;
                if (Total_Instp->tuning_delay <= 0) Total_Instp->tuning_delay = 0;

                printf("tuning_delay = %d (%f msec) \r\n", Total_Instp->tuning_delay, (double)(Total_Instp->tuning_delay)/16.0);
            }                              

            else {
                Show_mainmenu();
            }

            Show_mainmenu();
        }
        usleep(100);
    }

    printf("param set end\n");
}

void NR_default_param_set_detail(nsInst_t* nsInstp, int level){

    Show_NR_default_detail_menu(level);

        int * tmp_p_betaQ15;
        int * tmp_p_max_att;
        int * tmp_p_min_att;
        int * tmp_p_slope;
        int * tmp_p_high_att;
        int * tmp_p_target_SPL;

    if (level == 1){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR1;//(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR1;
        tmp_p_max_att= &Sys_Total_Inst.max_att_NR1;//(int32_t)-15; //Sys_Total_Inst.max_att_NR1;
        tmp_p_min_att= &Sys_Total_Inst.min_att_NR1;  //(int32_t)-1; //Sys_Total_Inst.min_att_NR1; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR1;  //(int32_t)15; //Sys_Total_Inst.slope_NR1; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR1; //(int32_t)-10; //Sys_Total_Inst.high_att_NR1;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR1; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR1;
    } else if (level == 2){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR2; //(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR2;
        tmp_p_max_att = &Sys_Total_Inst.max_att_NR2; //(int32_t)-20; //Sys_Total_Inst.max_att_NR2;
        tmp_p_min_att = &Sys_Total_Inst.min_att_NR2;  //(int32_t)-4; //Sys_Total_Inst.min_att_NR2; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR2; //(int32_t)18; //Sys_Total_Inst.slope_NR2; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR2; //(int32_t)-12; //Sys_Total_Inst.high_att_NR2;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR2; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR2;
    } else if (level == 3){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR3; //(int32_t)(0.4*32767.0); // Sys_Total_Inst.betaQ15_NR3;
        tmp_p_max_att = & Sys_Total_Inst.max_att_NR3; //(int32_t)-25; //Sys_Total_Inst.max_att_NR3;
        tmp_p_min_att = &Sys_Total_Inst.min_att_NR3; //(int32_t)-8; //Sys_Total_Inst.min_att_NR3; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR3;  //(int32_t)20; //Sys_Total_Inst.slope_NR3; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR3; //(int32_t)-15; //Sys_Total_Inst.high_att_NR3;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR3;  //(int32_t)73; //Sys_Total_Inst.target_SPL_NR3; 
    } else {
        printf("wrong levlel : %d", level);
        return;
    }

    aspl_NR_CONFIG config;

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. High band Hmin up [dB]
                *tmp_p_max_att+=1.0;
                if (*tmp_p_max_att>0) *tmp_p_max_att=0;

                printf("*tmp_p_max_att=%02f\n", (float)*tmp_p_max_att);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  

                printf("\r\n");
            } else if (ch =='b') { //b. High band Hmin down [dB]
                 *tmp_p_max_att-=1.0;

                 printf("*tmp_p_max_att=%02f\n", (float)*tmp_p_max_att);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='c') { //c. Mid band Hmin up [dB]
                *tmp_p_min_att+=1.0;
                if (*tmp_p_min_att>0) *tmp_p_min_att=0;

                printf("*tmp_p_min_att=%02f\n", (float)*tmp_p_min_att);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='d') { //d. Mid band Hmin down [dB]
                *tmp_p_min_att-=1.0;

                printf("*tmp_p_min_att=%02f\n", (float)*tmp_p_min_att);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='e') { //e. Low band Hmin up [dB]
                *tmp_p_slope+=1.0;

                printf("*tmp_p_slope=%02f\n", (float)(*tmp_p_slope)/10.0);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='f') { //f. Low band Hmin down [dB]
                *tmp_p_slope-=1.0;

                printf("*tmp_p_slope=%02f\n", (float)(*tmp_p_slope)/10.0);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='g') { //c. Mid band Hmin up [dB]
                *tmp_p_high_att+=1.0;
                if (*tmp_p_high_att>0) *tmp_p_high_att=0;

                printf("*tmp_p_high_att=%02f\n", (float)*tmp_p_high_att);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            } else if (ch =='h') { //d. Mid band Hmin down [dB]
                *tmp_p_high_att-=1.0;

                printf("*tmp_p_high_att=%02f\n", (float)*tmp_p_high_att);


                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");
            }
            else if (ch =='i') { //i. betaQ15 [dB]
                *tmp_p_betaQ15+=(int32_t)(0.1*32768.0);
                if (*tmp_p_betaQ15>=62259){
                    *tmp_p_betaQ15=62259;
                }
                printf("*tmp_p_betaQ15=%d, (%02f)\n", *tmp_p_betaQ15, (float)(*tmp_p_betaQ15)/32768.0);


                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");

            }
            else if (ch =='j') { //j. betaQ15
                *tmp_p_betaQ15-=(int32_t)(0.1*32768.0);
                if (*tmp_p_betaQ15<=0){
                    *tmp_p_betaQ15=0;
                }
                printf("*tmp_p_betaQ15=%d, (%02f)\n", *tmp_p_betaQ15, (float)(*tmp_p_betaQ15)/32768.0);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");

            }     
            else if (ch =='k') { //i. betaQ15 [dB]
                *tmp_p_target_SPL+=1.0;
                printf("*tmp_p_target_SPL=%d\n", *tmp_p_target_SPL);


                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");

            }
            else if (ch =='l') { //j. betaQ15
                *tmp_p_target_SPL-=1.0;
                printf("*tmp_p_target_SPL=%d\n", *tmp_p_target_SPL);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");

            }    

            else if (ch =='m') { //m. set as current
                *tmp_p_betaQ15 = nsInstp->betaQ15;
                *tmp_p_max_att= (int32_t)nsInstp->max_att;
                *tmp_p_min_att= (int32_t)nsInstp->min_att;
                *tmp_p_slope = (int32_t)(nsInstp->slope*10.0);
                *tmp_p_high_att = (int32_t)nsInstp->high_att;

                printf("*tmp_p_max_att=%02f\n", (float)*tmp_p_max_att);
                printf("*tmp_p_min_att=%02f\n", (float)*tmp_p_min_att);
                printf("*tmp_p_slope=%02f\n", (float)(*tmp_p_slope)/10.0);
                printf("*tmp_p_high_att=%02f\n", (float)*tmp_p_high_att);
                printf("*tmp_p_betaQ15=%d, (%02f)\n", *tmp_p_betaQ15, (float)(*tmp_p_betaQ15)/32768.0);
                printf("*tmp_p_target_SPL=%d\n", *tmp_p_target_SPL);

                config.enable = 1;
                config.sensitivity = level;
                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&config);  
                printf("\r\n");

            }                
            
                 
            else {
                Show_NR_default_detail_menu(level);
            }
        }
    }
}

void NR_default_param_set(nsInst_t* nsInstp){

    Show_NR_default_menu();

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. NR1 parameter
                NR_default_param_set_detail(nsInstp, 1);

            } else if (ch =='b') { //b. NR2 parameter
                NR_default_param_set_detail(nsInstp, 2);

            } else if (ch =='c') { //c. NR3 parameter
                NR_default_param_set_detail(nsInstp, 3);

            }            
            else {
                Show_NR_default_menu();
            }
        }
    }
}


void NR_param_set(nsInst_t* nsInstp){

    Show_NR_menu();

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. Instance power Estimation
                NR_instance_power_param_set(nsInstp);

            } else if (ch =='b') { //b. Noise spectrum Estimation
                NR_noise_spectrum_estimation_set(nsInstp);

            } else if (ch =='c') { //c. Reduction Amount Setup
                NR_Hmin_set(nsInstp);

            } else if (ch =='d') { //d. Target freq band Setup
                NR_Target_freq_band_set(nsInstp);

            } else if (ch =='e') { //d. Target freq band Setup
                NR_EQ_set(nsInstp);

            } else if (ch =='f') { //d. Target freq band Setup
                Show_print_current_parameters(nsInstp);
            }            
            else {
                Show_NR_menu();
            }
        }
    }
}

void NR_instance_power_param_set(nsInst_t* nsInstp){

    Show_NR_instance_power_menu();

    while(1){     
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. High band rise time up [msec]                 
                nsInstp->beta_e_num[0]++;
                if (nsInstp->beta_e_num[0]>=23) nsInstp->beta_e_num[0]=22;
                nsInstp->beta_e[0]=beta[nsInstp->beta_e_num[0]];
                nsInstp->round_bit_e[0]=round_bit[nsInstp->beta_e_num[0]];
                nsInstp->gamma_inv_e[0]=gamma_inv[nsInstp->beta_e_num[0]];
                printf("High band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
            } else if (ch =='b') { //b. High band rise time down [msec]
                nsInstp->beta_e_num[0]--;
                if (nsInstp->beta_e_num[0]<0) nsInstp->beta_e_num[0]=0;
                nsInstp->beta_e[0]=beta[nsInstp->beta_e_num[0]];
                nsInstp->round_bit_e[0]=round_bit[nsInstp->beta_e_num[0]];
                nsInstp->gamma_inv_e[0]=gamma_inv[nsInstp->beta_e_num[0]];
                printf("High band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
            } else if (ch =='c') { //b. High band fall time up [msec]
                nsInstp->beta_e_num[1]++;
                if (nsInstp->beta_e_num[1]>=23) nsInstp->beta_e_num[1]=22;
                nsInstp->beta_e[1]=beta[nsInstp->beta_e_num[1]];
                nsInstp->round_bit_e[1]=round_bit[nsInstp->beta_e_num[1]];
                nsInstp->gamma_inv_e[1]=gamma_inv[nsInstp->beta_e_num[1]];
                printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);      
            } else if (ch =='d') { //b. High band fall time down [msec]
                nsInstp->beta_e_num[1]--;
                if (nsInstp->beta_e_num[1]<0) nsInstp->beta_e_num[1]=0;
                nsInstp->beta_e[1]=beta[nsInstp->beta_e_num[1]];
                nsInstp->round_bit_e[1]=round_bit[nsInstp->beta_e_num[1]];
                nsInstp->gamma_inv_e[1]=gamma_inv[nsInstp->beta_e_num[1]];
                printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);            
            } else if (ch =='e') { //c. Mid  band rise time up [msec]
                nsInstp->beta_e_num[2]++;
                if (nsInstp->beta_e_num[2]>=23) nsInstp->beta_e_num[2]=22;
                nsInstp->beta_e[2]=beta[nsInstp->beta_e_num[2]];
                nsInstp->round_bit_e[2]=round_bit[nsInstp->beta_e_num[2]];
                nsInstp->gamma_inv_e[2]=gamma_inv[nsInstp->beta_e_num[2]];
                printf("Mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
            } else if (ch =='f') { //c. Mid  band rise time down [msec]
                nsInstp->beta_e_num[2]--;
                if (nsInstp->beta_e_num[2]<0) nsInstp->beta_e_num[2]=0;
                nsInstp->beta_e[2]=beta[nsInstp->beta_e_num[2]];
                nsInstp->round_bit_e[2]=round_bit[nsInstp->beta_e_num[2]];
                nsInstp->gamma_inv_e[2]=gamma_inv[nsInstp->beta_e_num[2]];
                printf("Mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);            
            } else if (ch =='g') { //d. Mid  band fall time up [msec] 
                nsInstp->beta_e_num[3]++;
                if (nsInstp->beta_e_num[3]>=23) nsInstp->beta_e_num[3]=22;
                nsInstp->beta_e[3]=beta[nsInstp->beta_e_num[3]];
                nsInstp->round_bit_e[3]=round_bit[nsInstp->beta_e_num[3]];
                nsInstp->gamma_inv_e[3]=gamma_inv[nsInstp->beta_e_num[3]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);      
            } else if (ch =='h') { //d. Mid  band fall time down [msec]
                nsInstp->beta_e_num[3]--;
                if (nsInstp->beta_e_num[3]<0) nsInstp->beta_e_num[3]=0;
                nsInstp->beta_e[3]=beta[nsInstp->beta_e_num[3]];
                nsInstp->round_bit_e[3]=round_bit[nsInstp->beta_e_num[3]];
                nsInstp->gamma_inv_e[3]=gamma_inv[nsInstp->beta_e_num[3]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);            
            } else if (ch =='i') { //e. Low  band rise time up [msec] 
                nsInstp->beta_e_num[4]++;
                if (nsInstp->beta_e_num[4]>=23) nsInstp->beta_e_num[4]=22;
                nsInstp->beta_e[4]=beta[nsInstp->beta_e_num[4]];
                nsInstp->round_bit_e[4]=round_bit[nsInstp->beta_e_num[4]];
                nsInstp->gamma_inv_e[4]=gamma_inv[nsInstp->beta_e_num[4]];
                printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
            } else if (ch =='j') { //e. Low  band rise time down [msec] 
                nsInstp->beta_e_num[4]--;
                if (nsInstp->beta_e_num[4]<0) nsInstp->beta_e_num[4]=0;
                nsInstp->beta_e[4]=beta[nsInstp->beta_e_num[4]];
                nsInstp->round_bit_e[4]=round_bit[nsInstp->beta_e_num[4]];
                nsInstp->gamma_inv_e[4]=gamma_inv[nsInstp->beta_e_num[4]];
                printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);            
            } else if (ch =='k') { //e. Low  band fall time up [msec] 
                nsInstp->beta_e_num[5]++;
                if (nsInstp->beta_e_num[5]>=23) nsInstp->beta_e_num[5]=22;
                nsInstp->beta_e[5]=beta[nsInstp->beta_e_num[5]];
                nsInstp->round_bit_e[5]=round_bit[nsInstp->beta_e_num[5]];
                nsInstp->gamma_inv_e[5]=gamma_inv[nsInstp->beta_e_num[5]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);      
            } else if (ch =='l') { //f. Low  band fall time down [msec] 
                nsInstp->beta_e_num[5]--;
                if (nsInstp->beta_e_num[5]<0) nsInstp->beta_e_num[5]=0;
                nsInstp->beta_e[5]=beta[nsInstp->beta_e_num[5]];
                nsInstp->round_bit_e[5]=round_bit[nsInstp->beta_e_num[5]];
                nsInstp->gamma_inv_e[5]=gamma_inv[nsInstp->beta_e_num[5]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);    
            } else if (ch =='m') { //e. Low  band fall time up [msec] 
                nsInstp->Ma_size++;
                if (nsInstp->Ma_size>=Ma_size_max)  nsInstp->Ma_size=Ma_size_max;

                printf("nsInstp->Ma_size = %d \r\n", nsInstp->Ma_size);      
            } else if (ch =='n') { //f. Low  band fall time down [msec] 
                nsInstp->Ma_size--;
                if (nsInstp->Ma_size<=1)  nsInstp->Ma_size=1;

                printf("nsInstp->Ma_size = %d \r\n", nsInstp->Ma_size);    
            } 
            else if (ch =='o') { //e. Low  band rise time up [msec] 
                nsInstp->beta_e_num[0]++;
                nsInstp->beta_e_num[2]++;
                nsInstp->beta_e_num[4]++;
                if (nsInstp->beta_e_num[0]>=23) nsInstp->beta_e_num[0]=22;
                if (nsInstp->beta_e_num[2]>=23) nsInstp->beta_e_num[2]=22;
                if (nsInstp->beta_e_num[4]>=23) nsInstp->beta_e_num[4]=22;

                nsInstp->beta_e[0]=beta[nsInstp->beta_e_num[0]];
                nsInstp->round_bit_e[0]=round_bit[nsInstp->beta_e_num[0]];
                nsInstp->gamma_inv_e[0]=gamma_inv[nsInstp->beta_e_num[0]];

                nsInstp->beta_e[2]=beta[nsInstp->beta_e_num[2]];
                nsInstp->round_bit_e[2]=round_bit[nsInstp->beta_e_num[2]];
                nsInstp->gamma_inv_e[2]=gamma_inv[nsInstp->beta_e_num[2]];

                nsInstp->beta_e[4]=beta[nsInstp->beta_e_num[4]];
                nsInstp->round_bit_e[4]=round_bit[nsInstp->beta_e_num[4]];
                nsInstp->gamma_inv_e[4]=gamma_inv[nsInstp->beta_e_num[4]];
                printf("high  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);      
                printf("mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
                printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
            } else if (ch =='p') { //e. Low  band rise time down [msec] 
                nsInstp->beta_e_num[0]--;
                nsInstp->beta_e_num[2]--;
                nsInstp->beta_e_num[4]--;
                if (nsInstp->beta_e_num[0]<0) nsInstp->beta_e_num[0]=0;
                if (nsInstp->beta_e_num[2]<0) nsInstp->beta_e_num[2]=0;
                if (nsInstp->beta_e_num[4]<0) nsInstp->beta_e_num[4]=0;

                nsInstp->beta_e[0]=beta[nsInstp->beta_e_num[0]];
                nsInstp->round_bit_e[0]=round_bit[nsInstp->beta_e_num[0]];
                nsInstp->gamma_inv_e[0]=gamma_inv[nsInstp->beta_e_num[0]];

                nsInstp->beta_e[2]=beta[nsInstp->beta_e_num[2]];
                nsInstp->round_bit_e[2]=round_bit[nsInstp->beta_e_num[2]];
                nsInstp->gamma_inv_e[2]=gamma_inv[nsInstp->beta_e_num[2]];

                nsInstp->beta_e[4]=beta[nsInstp->beta_e_num[4]];
                nsInstp->round_bit_e[4]=round_bit[nsInstp->beta_e_num[4]];
                nsInstp->gamma_inv_e[4]=gamma_inv[nsInstp->beta_e_num[4]];

                printf("high  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);      
                printf("mid  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
                printf("Low  band rise time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
 
            } else if (ch =='r') { //e. Low  band fall time up [msec] 
                nsInstp->beta_e_num[1]++;
                nsInstp->beta_e_num[3]++;
                nsInstp->beta_e_num[5]++;

                if (nsInstp->beta_e_num[1]>=23) nsInstp->beta_e_num[1]=22;
                if (nsInstp->beta_e_num[3]>=23) nsInstp->beta_e_num[3]=22;
                if (nsInstp->beta_e_num[5]>=23) nsInstp->beta_e_num[5]=22;

                nsInstp->beta_e[1]=beta[nsInstp->beta_e_num[1]];
                nsInstp->round_bit_e[1]=round_bit[nsInstp->beta_e_num[1]];
                nsInstp->gamma_inv_e[1]=gamma_inv[nsInstp->beta_e_num[1]];

                nsInstp->beta_e[3]=beta[nsInstp->beta_e_num[3]];
                nsInstp->round_bit_e[3]=round_bit[nsInstp->beta_e_num[3]];
                nsInstp->gamma_inv_e[3]=gamma_inv[nsInstp->beta_e_num[3]];

                nsInstp->beta_e[5]=beta[nsInstp->beta_e_num[5]];
                nsInstp->round_bit_e[5]=round_bit[nsInstp->beta_e_num[5]];
                nsInstp->gamma_inv_e[5]=gamma_inv[nsInstp->beta_e_num[5]];
                printf("High  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);    
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);      
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);        
            } else if (ch =='s') { //f. Low  band fall time down [msec] 
                nsInstp->beta_e_num[1]--;
                nsInstp->beta_e_num[3]--;
                nsInstp->beta_e_num[5]--;

                if (nsInstp->beta_e_num[1]<0) nsInstp->beta_e_num[1]=0;
                if (nsInstp->beta_e_num[3]<0) nsInstp->beta_e_num[3]=0;
                if (nsInstp->beta_e_num[5]<0) nsInstp->beta_e_num[5]=0;

                nsInstp->beta_e[1]=beta[nsInstp->beta_e_num[1]];
                nsInstp->round_bit_e[1]=round_bit[nsInstp->beta_e_num[1]];
                nsInstp->gamma_inv_e[1]=gamma_inv[nsInstp->beta_e_num[1]];

                nsInstp->beta_e[3]=beta[nsInstp->beta_e_num[3]];
                nsInstp->round_bit_e[3]=round_bit[nsInstp->beta_e_num[3]];
                nsInstp->gamma_inv_e[3]=gamma_inv[nsInstp->beta_e_num[3]];

                nsInstp->beta_e[5]=beta[nsInstp->beta_e_num[5]];
                nsInstp->round_bit_e[5]=round_bit[nsInstp->beta_e_num[5]];
                nsInstp->gamma_inv_e[5]=gamma_inv[nsInstp->beta_e_num[5]];

                printf("High  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);    
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);      
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);        
            }




            else {
                Show_NR_instance_power_menu();
            }
        }
    }
}

void NR_noise_spectrum_estimation_set(nsInst_t* nsInstp){

    Show_NR_noise_spectrum_estimation_menu();

    while(1){     
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. High band rise time up [msec]                 
                nsInstp->beta_r_num[0]++;
                if (nsInstp->beta_r_num[0]>=23) nsInstp->beta_r_num[0]=22;
                nsInstp->beta_r[0]=beta[nsInstp->beta_r_num[0]];
                nsInstp->round_bit_r[0]=round_bit[nsInstp->beta_r_num[0]];
                nsInstp->gamma_inv_r[0]=gamma_inv[nsInstp->beta_r_num[0]];
                printf("High band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);
            } else if (ch =='b') { //b. High band rise time down [msec]
                nsInstp->beta_r_num[0]--;
                if (nsInstp->beta_r_num[0]<0) nsInstp->beta_r_num[0]=0;
                nsInstp->beta_r[0]=beta[nsInstp->beta_r_num[0]];
                nsInstp->round_bit_r[0]=round_bit[nsInstp->beta_r_num[0]];
                nsInstp->gamma_inv_r[0]=gamma_inv[nsInstp->beta_r_num[0]];
                printf("High band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);
            } else if (ch =='c') { //c. Mid  band rise time up [msec]
                nsInstp->beta_r_num[2]++;
                if (nsInstp->beta_r_num[2]>=23) nsInstp->beta_r_num[2]=22;
                nsInstp->beta_r[2]=beta[nsInstp->beta_r_num[2]];
                nsInstp->round_bit_r[2]=round_bit[nsInstp->beta_r_num[2]];
                nsInstp->gamma_inv_r[2]=gamma_inv[nsInstp->beta_r_num[2]];
                printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);      
            } else if (ch =='d') { //c. Mid  band rise time down [msec]
                nsInstp->beta_r_num[2]--;
                if (nsInstp->beta_r_num[2]<0) nsInstp->beta_r_num[2]=0;
                nsInstp->beta_r[2]=beta[nsInstp->beta_r_num[2]];
                nsInstp->round_bit_r[2]=round_bit[nsInstp->beta_r_num[2]];
                nsInstp->gamma_inv_r[2]=gamma_inv[nsInstp->beta_r_num[2]];
                printf("High band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);            
            } else if (ch =='e') { //d. Mid  band fall time up [msec] 
                nsInstp->beta_r_num[3]++;
                if (nsInstp->beta_r_num[3]>=23) nsInstp->beta_r_num[3]=22;
                nsInstp->beta_r[3]=beta[nsInstp->beta_r_num[3]];
                nsInstp->round_bit_r[3]=round_bit[nsInstp->beta_r_num[3]];
                nsInstp->gamma_inv_r[3]=gamma_inv[nsInstp->beta_r_num[3]];
                printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);      
            } else if (ch =='f') { //d. Mid  band fall time down [msec]
                nsInstp->beta_r_num[3]--;
                if (nsInstp->beta_r_num[3]<0) nsInstp->beta_r_num[3]=0;
                nsInstp->beta_r[3]=beta[nsInstp->beta_r_num[3]];
                nsInstp->round_bit_r[3]=round_bit[nsInstp->beta_r_num[3]];
                nsInstp->gamma_inv_r[3]=gamma_inv[nsInstp->beta_r_num[3]];
                printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);            
            } else if (ch =='g') { //e. Low  band fall time up [msec] 
                nsInstp->beta_r_num[5]++;
                if (nsInstp->beta_r_num[5]>=23) nsInstp->beta_r_num[5]=22;
                nsInstp->beta_r[5]=beta[nsInstp->beta_r_num[5]];
                nsInstp->round_bit_r[5]=round_bit[nsInstp->beta_r_num[5]];
                nsInstp->gamma_inv_r[5]=gamma_inv[nsInstp->beta_r_num[5]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);      
            } else if (ch =='h') { //f. Low  band fall time down [msec] 
                nsInstp->beta_r_num[5]--;
                if (nsInstp->beta_r_num[5]<0) nsInstp->beta_r_num[5]=0;
                nsInstp->beta_r[5]=beta[nsInstp->beta_r_num[5]];
                nsInstp->round_bit_r[5]=round_bit[nsInstp->beta_r_num[5]];
                nsInstp->gamma_inv_r[5]=gamma_inv[nsInstp->beta_r_num[5]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);    
            }  else if (ch =='i') { //e. Low  band fall time up [msec] 
                nsInstp->beta_r_num[6]++;
                if (nsInstp->beta_r_num[6]>=23) nsInstp->beta_r_num[6]=22;
                nsInstp->beta_r[6]=beta[nsInstp->beta_r_num[6]];
                nsInstp->round_bit_r[6]=round_bit[nsInstp->beta_r_num[6]];
                nsInstp->gamma_inv_r[6]=gamma_inv[nsInstp->beta_r_num[6]];
                printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
            } else if (ch =='j') { //f. Low  band fall time down [msec] 
                nsInstp->beta_r_num[6]--;
                if (nsInstp->beta_r_num[6]<0) nsInstp->beta_r_num[6]=0;
                nsInstp->beta_r[6]=beta[nsInstp->beta_r_num[6]];
                nsInstp->round_bit_r[6]=round_bit[nsInstp->beta_r_num[6]];
                nsInstp->gamma_inv_r[6]=gamma_inv[nsInstp->beta_r_num[6]];
                printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);    
            } else if (ch =='k') { //e. Low  band fall time up [msec] 
                nsInstp->beta_r_num[8]++;
                if (nsInstp->beta_r_num[8]>=23) nsInstp->beta_r_num[8]=22;
                nsInstp->beta_r[8]=beta[nsInstp->beta_r_num[8]];
                nsInstp->round_bit_r[8]=round_bit[nsInstp->beta_r_num[8]];
                nsInstp->gamma_inv_r[8]=gamma_inv[nsInstp->beta_r_num[8]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
            } else if (ch =='l') { //f. Low  band fall time down [msec] 
                nsInstp->beta_r_num[8]--;
                if (nsInstp->beta_r_num[8]<0) nsInstp->beta_r_num[8]=0;
                nsInstp->beta_r[8]=beta[nsInstp->beta_r_num[8]];
                nsInstp->round_bit_r[8]=round_bit[nsInstp->beta_r_num[8]];
                nsInstp->gamma_inv_r[8]=gamma_inv[nsInstp->beta_r_num[8]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);    
            }  else if (ch =='m') { //e. Low  band fall time up [msec] 
                nsInstp->beta_r_num[0]++;
                nsInstp->beta_r_num[3]++;
                nsInstp->beta_r_num[6]++;

                if (nsInstp->beta_r_num[0]>=23) nsInstp->beta_r_num[0]=22;
                nsInstp->beta_r[0]=beta[nsInstp->beta_r_num[0]];
                nsInstp->round_bit_r[0]=round_bit[nsInstp->beta_r_num[0]];
                nsInstp->gamma_inv_r[0]=gamma_inv[nsInstp->beta_r_num[0]];
                printf("High  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);  

                if (nsInstp->beta_r_num[3]>=23) nsInstp->beta_r_num[3]=22;
                nsInstp->beta_r[3]=beta[nsInstp->beta_r_num[3]];
                nsInstp->round_bit_r[3]=round_bit[nsInstp->beta_r_num[3]];
                nsInstp->gamma_inv_r[3]=gamma_inv[nsInstp->beta_r_num[3]];
                printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);  

                if (nsInstp->beta_r_num[6]>=23) nsInstp->beta_r_num[6]=22;
                nsInstp->beta_r[6]=beta[nsInstp->beta_r_num[6]];
                nsInstp->round_bit_r[6]=round_bit[nsInstp->beta_r_num[6]];
                nsInstp->gamma_inv_r[6]=gamma_inv[nsInstp->beta_r_num[6]];
                printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
            } else if (ch =='n') { //f. Low  band fall time down [msec] 
                nsInstp->beta_r_num[0]--;
                nsInstp->beta_r_num[3]--;
                nsInstp->beta_r_num[6]--;

                if (nsInstp->beta_r_num[0]<0) nsInstp->beta_r_num[0]=0;
                nsInstp->beta_r[0]=beta[nsInstp->beta_r_num[0]];
                nsInstp->round_bit_r[0]=round_bit[nsInstp->beta_r_num[0]];
                nsInstp->gamma_inv_r[0]=gamma_inv[nsInstp->beta_r_num[0]];
                printf("High  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]);  

                if (nsInstp->beta_r_num[3]<0) nsInstp->beta_r_num[3]=0;
                nsInstp->beta_r[3]=beta[nsInstp->beta_r_num[3]];
                nsInstp->round_bit_r[3]=round_bit[nsInstp->beta_r_num[3]];
                nsInstp->gamma_inv_r[3]=gamma_inv[nsInstp->beta_r_num[3]];
                printf("Mid  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);                  

                if (nsInstp->beta_r_num[6]<0) nsInstp->beta_r_num[6]=0;
                nsInstp->beta_r[6]=beta[nsInstp->beta_r_num[6]];
                nsInstp->round_bit_r[6]=round_bit[nsInstp->beta_r_num[6]];
                nsInstp->gamma_inv_r[6]=gamma_inv[nsInstp->beta_r_num[6]];
                printf("Low  band rise fast time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);    
            } else if (ch =='o') { //e. Low  band fall time up [msec] 
                nsInstp->beta_r_num[2]++;
                nsInstp->beta_r_num[5]++;
                nsInstp->beta_r_num[8]++;

                if (nsInstp->beta_r_num[2]>=23) nsInstp->beta_r_num[2]=22;
                nsInstp->beta_r[2]=beta[nsInstp->beta_r_num[2]];
                nsInstp->round_bit_r[2]=round_bit[nsInstp->beta_r_num[2]];
                nsInstp->gamma_inv_r[2]=gamma_inv[nsInstp->beta_r_num[2]];
                printf("High  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);     

                if (nsInstp->beta_r_num[5]>=23) nsInstp->beta_r_num[5]=22;
                nsInstp->beta_r[5]=beta[nsInstp->beta_r_num[5]];
                nsInstp->round_bit_r[5]=round_bit[nsInstp->beta_r_num[5]];
                nsInstp->gamma_inv_r[5]=gamma_inv[nsInstp->beta_r_num[5]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);     

                if (nsInstp->beta_r_num[8]>=23) nsInstp->beta_r_num[8]=22;
                nsInstp->beta_r[8]=beta[nsInstp->beta_r_num[8]];
                nsInstp->round_bit_r[8]=round_bit[nsInstp->beta_r_num[8]];
                nsInstp->gamma_inv_r[8]=gamma_inv[nsInstp->beta_r_num[8]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
            } else if (ch =='p') { //f. Low  band fall time down [msec] 
                nsInstp->beta_r_num[2]--;
                nsInstp->beta_r_num[5]--;
                nsInstp->beta_r_num[8]--;

                if (nsInstp->beta_r_num[2]<0) nsInstp->beta_r_num[2]=0;
                nsInstp->beta_r[2]=beta[nsInstp->beta_r_num[2]];
                nsInstp->round_bit_r[2]=round_bit[nsInstp->beta_r_num[2]];
                nsInstp->gamma_inv_r[2]=gamma_inv[nsInstp->beta_r_num[2]];
                printf("High  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);   

                if (nsInstp->beta_r_num[5]<0) nsInstp->beta_r_num[5]=0;
                nsInstp->beta_r[5]=beta[nsInstp->beta_r_num[5]];
                nsInstp->round_bit_r[5]=round_bit[nsInstp->beta_r_num[5]];
                nsInstp->gamma_inv_r[5]=gamma_inv[nsInstp->beta_r_num[5]];
                printf("Mid  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);   

                if (nsInstp->beta_r_num[8]<0) nsInstp->beta_r_num[8]=0;
                nsInstp->beta_r[8]=beta[nsInstp->beta_r_num[8]];
                nsInstp->round_bit_r[8]=round_bit[nsInstp->beta_r_num[8]];
                nsInstp->gamma_inv_r[8]=gamma_inv[nsInstp->beta_r_num[8]];
                printf("Low  band fall time = %f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);    
            }                                               
            else {
                Show_NR_noise_spectrum_estimation_menu();
            }
        }
    }
}

void NR_Hmin_set(nsInst_t* nsInstp){
    int k;
    Show_Hmin_set_menu(nsInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. High band Hmin up [dB]
                nsInstp->max_att+=1.0;
                if (nsInstp->max_att>0) nsInstp->max_att=0;

                printf("nsInstp->max_att=%02f\n", nsInstp->max_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='b') { //b. High band Hmin down [dB]
                 nsInstp->max_att-=1.0;

                 printf("nsInstp->max_att=%02f\n", nsInstp->max_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='c') { //c. Mid band Hmin up [dB]
                nsInstp->min_att+=1.0;
                if (nsInstp->min_att>0) nsInstp->min_att=0;

                printf("nsInstp->min_att=%02f\n", nsInstp->min_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='d') { //d. Mid band Hmin down [dB]
                nsInstp->min_att-=1.0;

                printf("nsInstp->min_att=%02f\n", nsInstp->min_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='e') { //e. Low band Hmin up [dB]
                nsInstp->slope+=0.1;

                printf("nsInstp->slope=%02f\n", nsInstp->slope);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='f') { //f. Low band Hmin down [dB]
                nsInstp->slope-=0.1;

                printf("nsInstp->slope=%02f\n", nsInstp->slope);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='g') { //c. Mid band Hmin up [dB]
                nsInstp->high_att+=1.0;
                if (nsInstp->high_att>0) nsInstp->high_att=0;

                printf("nsInstp->high_att=%02f\n", nsInstp->high_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } else if (ch =='h') { //d. Mid band Hmin down [dB]
                nsInstp->high_att-=1.0;

                printf("nsInstp->high_att=%02f\n", nsInstp->high_att);

                reset_Hmin(nsInstp);
                printf("\r\n");
            } 
            else if (ch =='i') { //i. betaQ15 [dB]
                nsInstp->betaQ15+=(int32_t)(0.1*32768.0);
                if (nsInstp->betaQ15>=62259){
                    nsInstp->betaQ15=62259;
                }

                printf("nsInstp->betaQ15=%d, (%02f)\n", nsInstp->betaQ15, (float)nsInstp->betaQ15/32768.0);
            } else if (ch =='j') { //j. betaQ15
                nsInstp->betaQ15-=(int32_t)(0.1*32768.0);
                if (nsInstp->betaQ15<=0){
                    nsInstp->betaQ15=0;
                }
                printf("nsInstp->betaQ15=%d, (%02f)\n", nsInstp->betaQ15, (float)nsInstp->betaQ15/32768.0);
            }
            else if (ch =='k') { //i. betaQ15 [dB]
                nsInstp->beta_low_ratio+=0.1;
                if (nsInstp->beta_low_ratio>=2.0){
                    nsInstp->beta_low_ratio=2.0;
                }
                printf("nsInstp->beta_low_ratio=%02f\n", nsInstp->beta_low_ratio);
            } else if (ch =='l') { //j. betaQ15
                nsInstp->beta_low_ratio-=0.1;
                if (nsInstp->beta_low_ratio<=0.0){
                    nsInstp->beta_low_ratio=0.0;
                }
                printf("nsInstp->beta_low_ratio=%02f\n", nsInstp->beta_low_ratio);
            }
            else if (ch =='m') { //i. betaQ15 [dB]
                nsInstp->beta_high_ratio+=0.1;
                if (nsInstp->beta_high_ratio>=2.0){
                    nsInstp->beta_high_ratio=2.0;
                }
                printf("nsInstp->beta_high_ratio=%02f\n", nsInstp->beta_high_ratio);
            } else if (ch =='n') { //j. betaQ15
                nsInstp->beta_high_ratio-=0.1;
                if (nsInstp->beta_high_ratio<=0.0){
                    nsInstp->beta_high_ratio=0.0;
                }
                printf("nsInstp->beta_high_ratio=%02f\n", nsInstp->beta_high_ratio);
            }
            else {
                Show_Hmin_set_menu(nsInstp);
            }
        }
    }
}

void NR_EQ_set(nsInst_t* nsInstp){
    int k;
    Show_EQ_set_menu(nsInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. High band Hmin up [dB]

                nsInstp->EQ_Low+=1;
                if (nsInstp->EQ_Low>3) nsInstp->EQ_Low=3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

            } else if (ch =='b') { //b. High band Hmin down [dB]
                nsInstp->EQ_Low-=1;
                if (nsInstp->EQ_Low<-3) nsInstp->EQ_Low=-3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
            
            } else if (ch =='c') { //c. Mid band Hmin up [dB]
                nsInstp->EQ_Mid+=1;
                if (nsInstp->EQ_Mid>3) nsInstp->EQ_Mid=3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

            } else if (ch =='d') { //d. Mid band Hmin down [dB]
                nsInstp->EQ_Mid-=1;
                if (nsInstp->EQ_Mid<-3) nsInstp->EQ_Mid=-3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

            } else if (ch =='e') { //e. Low band Hmin up [dB]
                nsInstp->EQ_Hi+=1;
                if (nsInstp->EQ_Hi>3) nsInstp->EQ_Hi=3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

            } else if (ch =='f') { //f. Low band Hmin down [dB]
                nsInstp->EQ_Hi-=1;
                if (nsInstp->EQ_Hi<-3) nsInstp->EQ_Hi=-3;

                printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
                printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
                printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

            } 
            else {
                Show_EQ_set_menu(nsInstp);
            }
        }
    }
}

void reset_Hmin(nsInst_t* nsInstp){
    int k;

    float rtemp = 1.0 / (float) (nsInstp->slope);

    for (k=0; k<nsInstp->voice_start_bin; k++) {
        float temp= nsInstp->min_att + (nsInstp->max_att/((float)nsInstp->voice_start_bin*(float)nsInstp->voice_start_bin)) * (float)((k-nsInstp->voice_start_bin)*(k-nsInstp->voice_start_bin));
        (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);


        if (k%8==0) printf("\n");
        printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);    
    }

    printf("\n");
        
    for (k=nsInstp->voice_start_bin ; k<PolyM/2+1; k++){
        float temp= nsInstp->min_att + nsInstp->high_att + (-1 * nsInstp->high_att)*powf(rtemp,((float)(k-nsInstp->voice_start_bin)/(float)(nsInstp->voice_end_bin-nsInstp->voice_start_bin)));
        
        // if (k>nsInstp->voice_end_bin){
        //     temp= temp + (float)(k-nsInstp->voice_end_bin)/(float)(PolyM/2-nsInstp->voice_end_bin)*3.0;
        // }
        
        (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10, (temp/20))*(float)Q15_val);

        if (k%8==0) printf("\n");
        printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);        
    }

    // for (k=0 ; k<=nsInstp->voice_end_bin; k++){
    //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)k+nsInstp->slope)+nsInstp->min_att)/20))*(float)Q15_val);
    //         if (k%8==0) printf("\n");
    //         printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
    // }

    // for (k=nsInstp->voice_end_bin+1 ; k<PolyM/2+1; k++){
    //     (nsInstp->HminQ15_p)[k] = (int32_t)(powf(10,((((nsInstp->max_att-nsInstp->min_att)*(nsInstp->slope))/((float)(k)+nsInstp->slope)+nsInstp->min_att+((nsInstp->high_att-nsInstp->min_att)*(nsInstp->slope))/((float)(PolyM/2+1-k) *0.5+nsInstp->slope*0.5))/20))*(float)Q15_val);
    //     if (k%8==0) printf("\n");
    //         printf("%2.1fdB, %d, ", 20.0*log10((float)(nsInstp->HminQ15_p)[k]/(float)Q15_val), (nsInstp->HminQ15_p)[k]);
    // }
}



void NR_Target_freq_band_set(nsInst_t* nsInstp){

    Show_Target_freq_band_Setup_menu(nsInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { //a. Low/Mid Band up [Hz] 
                nsInstp->voice_start_bin++;
                if (nsInstp->voice_start_bin>=nsInstp->voice_end_bin) nsInstp->voice_start_bin=nsInstp->voice_end_bin-1;
                printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
                reset_Hmin(nsInstp);
            } else if (ch =='b') { //b. Mid/High band down [Hz]
                nsInstp->voice_start_bin--;
                if (nsInstp->voice_start_bin<1) nsInstp->voice_start_bin=1;
                printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
                reset_Hmin(nsInstp);
            } else if (ch =='c') { //c. Mid/High band up [Hz] 
                nsInstp->voice_end_bin++;
                if (nsInstp->voice_end_bin>=PolyM/2-2) nsInstp->voice_end_bin=PolyM/2-2;
                printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
                reset_Hmin(nsInstp);
            } else if (ch =='d') { //d. Mid/High band down [Hz] 
                nsInstp->voice_end_bin--;
                if (nsInstp->voice_end_bin<=nsInstp->voice_start_bin) nsInstp->voice_end_bin=nsInstp->voice_start_bin+1;
                printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
                reset_Hmin(nsInstp);
            } else if (ch =='e') { //e. Low  band Solo On/Off
                if (nsInstp->Low_solo==0){
                    nsInstp->Low_solo=1;
                    nsInstp->Mid_solo=0;
                    nsInstp->Hi_solo=0;
                    nsInstp->Low_bypass = 0;
                    nsInstp->Mid_bypass = 1;
                    nsInstp->Hi_bypass =1;
                } else if (nsInstp->Low_solo==1){
                    nsInstp->Low_solo=0;
                    nsInstp->Mid_solo=0;
                    nsInstp->Hi_solo=0;                    
                    nsInstp->Low_bypass = 0;
                    nsInstp->Mid_bypass = 0;
                    nsInstp->Hi_bypass =0;
                }

            } else if (ch =='f') { //f. Mid  band Solo On/Off
                if (nsInstp->Mid_solo==0){
                    nsInstp->Low_solo=0;
                    nsInstp->Mid_solo=1;
                    nsInstp->Hi_solo=0;  
                    nsInstp->Low_bypass = 1;
                    nsInstp->Mid_bypass = 0;
                    nsInstp->Hi_bypass =1;
                } else if (nsInstp->Mid_solo==1){
                    nsInstp->Low_solo=0;
                    nsInstp->Mid_solo=0;
                    nsInstp->Hi_solo=0;  
                    nsInstp->Low_bypass = 0;
                    nsInstp->Mid_bypass = 0;
                    nsInstp->Hi_bypass =0;
                }
            } else if (ch =='g') { //f. High band Solo On/Off 
                if (nsInstp->Hi_solo==0){
                    nsInstp->Low_solo=0;
                    nsInstp->Mid_solo=0;
                    nsInstp->Hi_solo=1;  
                    nsInstp->Low_bypass = 1;
                    nsInstp->Mid_bypass = 1;
                    nsInstp->Hi_bypass =0;
                } else if (nsInstp->Hi_solo==1){
                    nsInstp->Low_solo=0;
                    nsInstp->Mid_solo=0;
                    nsInstp->Hi_solo=0;  
                    nsInstp->Low_bypass = 0;
                    nsInstp->Mid_bypass = 0;
                    nsInstp->Hi_bypass =0;
                }
            } 
            else {
                Show_Target_freq_band_Setup_menu(nsInstp);
            }
        }
    }
}

void Show_mainmenu(){
        printf( "\r\n================== Expert tuning Menu ==========================\r\n");
        printf( "  a. NR paramter Setting                                   \r\n");
        printf( "  b. VAD paramter Setting                                  \r\n");
        printf( "  c. AGC paramter Setting                                  \r\n");
        printf( "  d. NR default param setting                                \r\n");
        printf( "  t. ssl param setting                                \r\n");        
        printf( "  e. NR       enable toggle                                \r\n");
        printf( "  f. AGC      enable toggle                                \r\n");
        printf( "  u. BF      enable toggle                                \r\n");
        printf( "  g. DC rejection enable toggle                            \r\n");
        printf( "  h. DC rejection beta up                                      \r\n");
        printf( "  i. DC rejection beta down                                      \r\n");
        printf( "  j. matlab vad_debug  toggle                                \r\n");
        printf( "  k. matlab agc_debug  toggle                                \r\n");
        printf( "  l. matlab ns_debug  toggle                                \r\n");
        printf( "  m. matlab ssl_debug  toggle                                \r\n");     
        printf( "  n. poly scale up                                      \r\n");
        printf( "  o. poly scale down                                      \r\n"); 
        printf( "  p. bf enable toggle                            \r\n");          
        printf( "  r. global gain up                                      \r\n");
        printf( "  s. global gain down                                      \r\n");       
        printf( "  v. tuning delay up                                      \r\n");
        printf( "  w. tuning delay down                                      \r\n");                              
        printf( "  q. Exit expert tuning param                              \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_NR_default_detail_menu(int level){

    int * tmp_p_betaQ15;
    int * tmp_p_max_att;
    int * tmp_p_min_att;
    int * tmp_p_slope;
    int * tmp_p_high_att;
    int * tmp_p_target_SPL;

    if (level == 1){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR1;//(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR1;
        tmp_p_max_att= &Sys_Total_Inst.max_att_NR1;//(int32_t)-15; //Sys_Total_Inst.max_att_NR1;
        tmp_p_min_att= &Sys_Total_Inst.min_att_NR1;  //(int32_t)-1; //Sys_Total_Inst.min_att_NR1; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR1;  //(int32_t)15; //Sys_Total_Inst.slope_NR1; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR1; //(int32_t)-10; //Sys_Total_Inst.high_att_NR1;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR1; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR1;
    } else if (level == 2){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR2; //(int32_t)(0.4*32767.0); //Sys_Total_Inst.betaQ15_NR2;
        tmp_p_max_att = &Sys_Total_Inst.max_att_NR2; //(int32_t)-20; //Sys_Total_Inst.max_att_NR2;
        tmp_p_min_att = &Sys_Total_Inst.min_att_NR2;  //(int32_t)-4; //Sys_Total_Inst.min_att_NR2; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR2; //(int32_t)18; //Sys_Total_Inst.slope_NR2; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR2; //(int32_t)-12; //Sys_Total_Inst.high_att_NR2;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR2; //(int32_t)73; //Sys_Total_Inst.target_SPL_NR2;
    } else if (level == 3){
        tmp_p_betaQ15 = &Sys_Total_Inst.betaQ15_NR3; //(int32_t)(0.4*32767.0); // Sys_Total_Inst.betaQ15_NR3;
        tmp_p_max_att = & Sys_Total_Inst.max_att_NR3; //(int32_t)-25; //Sys_Total_Inst.max_att_NR3;
        tmp_p_min_att = &Sys_Total_Inst.min_att_NR3; //(int32_t)-8; //Sys_Total_Inst.min_att_NR3; 
        tmp_p_slope = &Sys_Total_Inst.slope_NR3;  //(int32_t)20; //Sys_Total_Inst.slope_NR3; 
        tmp_p_high_att = &Sys_Total_Inst.high_att_NR3; //(int32_t)-15; //Sys_Total_Inst.high_att_NR3;
        tmp_p_target_SPL = &Sys_Total_Inst.target_SPL_NR3;  //(int32_t)73; //Sys_Total_Inst.target_SPL_NR3; 
    } else {
        printf("wrong levlel : %d", level);
        return;
    }

    printf( "\r\n============== NR %d default paramter Setting ====================\r\n", level);
    printf( "  a / b. max_att of Hmin up/down [dB] = %2.2f dB\r\n", (float)*tmp_p_max_att);
    printf( "  c / d. min_att of Hmin up/down [dB] = %2.2f dB\r\n", (float)*tmp_p_min_att);
    printf( "  e / f. slope of Hmin up/down = %2.2f dB\r\n", (float)*tmp_p_slope/10.0);
    printf( "  g / h. high band att of Hmin up/down [dB] = %2.2f dB\r\n", (float)*tmp_p_high_att);
    printf( "  i / j. betaQ15 = %d ,(%2.2f) \r\n", *tmp_p_betaQ15, (float)(*tmp_p_betaQ15)/32768.0);
    printf( "  k / l. target_SPL = %d \r\n", *tmp_p_target_SPL);
    printf( "  m. Set as current NS param                                             \r\n");
    printf( "  q. Exit Menu                                             \r\n");
    printf( "  To print this memu, press any other key                  \r\n");
    printf(   "=======================================================\r\n\n");
}

void Show_NR_default_menu(){
        printf( "\r\n============== NR default paramter Setting ====================\r\n");
        printf( "  a. NR1 parameter setting                             \r\n");	
        printf( "  b. NR2 parameter setting                              \r\n");	
        printf( "  c. NR3 parameter setting                                 \r\n");	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_NR_menu(){
        printf( "\r\n============== NR paramter Setting ====================\r\n");
        printf( "  a. Instance power Estimation                             \r\n");	
        printf( "  b. Noise spectrum Estimation                             \r\n");	
        printf( "  c. Reduction Amount Setup                                \r\n");	
        printf( "  d. Target freq band Setup                                \r\n");	
        printf( "  e. 3 band EQ Setup                                       \r\n");	    
        printf( "  f. print current parameters                              \r\n");	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_NR_instance_power_menu(){
        printf( "\r\n============ NR Instance power Estimation =============\r\n");
        printf( "  a / b. High band rise time up/down [msec]                            \r\n");	
        printf( "  c / d. High band fall time up/down [msec]                            \r\n");	
        printf( "  e / f. Mid  band rise time up/down [msec]                            \r\n");	
        printf( "  g / h. Mid  band fall time up/down [msec]                            \r\n");
        printf( "  i / j. Low  band rise time up/down [msec]                            \r\n");	
        printf( "  k / l. Low  band fall time up/down [msec]                            \r\n");	
        printf( "  m / n. MA avg size up/down                             \r\n");	
        printf( "  o / p. all band rise time up/down [msec]                            \r\n");	
        printf( "  r / s. all band fall time up/down [msec]                            \r\n");	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_NR_noise_spectrum_estimation_menu(){
        printf( "\r\n============ NR noise_spectrum_estimation =============\r\n");
        printf( "  a / b. High band rise fast time up/down [msec]                            \r\n");	      
        printf( "  c / d. High band fall time up/down [msec]                            \r\n");	
        printf( "  e / f. Mid  band rise fast time up/down [msec]                            \r\n");	       
        printf( "  g / h. Mid  band fall time up/down [msec]                            \r\n");
        printf( "  i / j. Low  band rise fast time up/down [msec]                            \r\n");	       
        printf( "  k / l. Low  band fall time up/down [msec]                            \r\n");	
        printf( "  m / n. all band rise time up/down [msec]                            \r\n");	
        printf( "  o / p. all band fall time up/down [msec]                            \r\n");	        
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_Hmin_set_menu(nsInst_t* nsInstp){
        printf( "\r\n============= NR Hmin_set ============================\r\n");
        printf( "  a / b. max_att of Hmin up/down [dB] = %2.2f dB\r\n", nsInstp->max_att);
        printf( "  c / d. min_att of Hmin up/down [dB] = %2.2f dB\r\n", nsInstp->min_att);
        printf( "  e / f. slope of Hmin up/down = %2.2f dB\r\n", nsInstp->slope);
        printf( "  g / h. high band att of Hmin up/down [dB] = %2.2f dB\r\n", nsInstp->high_att);
        printf( "  i / j. betaQ15 = %d ,(%2.2f) \r\n", nsInstp->betaQ15, (float)nsInstp->betaQ15/32768.0);
        printf( "  k / l. beta_low_ratio = %2.2f \r\n", nsInstp->beta_low_ratio);
        printf( "  m / n. beta_high_ratio = %2.2f \r\n", nsInstp->beta_high_ratio);
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_EQ_set_menu(nsInst_t* nsInstp){
        printf( "\r\n============= NR Hmin_set ============================\r\n");
        printf( "  a / b. Low band EQ up/down [dB] = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
        printf( "  c / d. mid band EQ up/down [dB] = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
        printf( "  e / f. Hi  band EQ up/down [dB] = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_Target_freq_band_Setup_menu(nsInst_t* nsInstp){
        printf( "\r\n============= NR Hmin_set ===========================\r\n");
        printf( "  a / b. Low/Mid Band up/down [Hz]= %4.1f Hz, voice_start_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
        printf( "  c / d. Mid/High band up/down [Hz]= %4.1f Hz, voice_end_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);    
        printf( "  e. Low  band Solo On/Off                          \r\n");	
        printf( "  f. Mid  band Solo On/Off                          \r\n");	
        printf( "  g. High band Solo On/Off                          \r\n");	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void Show_print_current_default_parameters(nsInst_t* nsInstp){
        printf(   "=======================================================\r\n");
        printf("High band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
        printf("High band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);      
        printf("Mid  band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
        printf("Mid  band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);              
        printf("Low  band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
        printf("Low  band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);              
        printf(   "======================================================\r\n");
        printf("High band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]); 
        printf("High band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);      
        printf("Mid band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);     
        printf("Mid band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);      
        printf("Low band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
        printf("Low band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
        printf(   "======================================================\r\n");
        printf("max_att Hmin = %2.2f dB\r\n", nsInstp->max_att);
        printf("min_att Hmin = %2.2f dB\r\n", nsInstp->min_att);
        printf("slope Hmin = %2.2f \r\n", nsInstp->slope);
        printf("beta = %2.2f (Q15 = %d)\r\n", (float)nsInstp->betaQ15/32768.0 , nsInstp->betaQ15);
        printf(   "======================================================\r\n");
        printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
        printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
        printf(   "=======================================================\r\n");
        printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
        printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
        printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

        int i;

        printf("nsInstp->H_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%2.1f ", 20*log10(((double)nsInstp->H_p[i])/32768.0));
        }
        printf("\r\n=======================================================\r\n\n");

        printf("nsInstp->HminQ15_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%2.1f ", 20*log10(((double)nsInstp->HminQ15_p[i])/32768.0));
        }
        printf("\r\n=======================================================\r\n\n");

        printf("nsInstp->See_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%d ", nsInstp->See_p[i]);
        }

        printf("\r\n=======================================================\r\n\n");
        printf("nsInstp->Sbb_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%d ", nsInstp->Sbb_p[i]);
        }
        printf("\r\n=======================================================\r\n\n");

}

void Show_print_current_parameters(nsInst_t* nsInstp){
        printf(   "=======================================================\r\n");
        printf("High band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[0]]);
        printf("High band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[1]]);      
        printf("Mid  band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[2]]);      
        printf("Mid  band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[3]]);              
        printf("Low  band rise time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[4]]);      
        printf("Low  band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_e_num[5]]);              
        printf(   "======================================================\r\n");
        printf("High band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[0]]); 
        printf("High band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[2]]);      
        printf("Mid band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[3]]);     
        printf("Mid band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[5]]);      
        printf("Low band rise fast time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[6]]);      
        printf("Low band fall time = %4.3f [msec]\r\n", tau_msec[nsInstp->beta_r_num[8]]);      
        printf(   "======================================================\r\n");
        printf("max_att Hmin = %2.2f dB\r\n", nsInstp->max_att);
        printf("min_att Hmin = %2.2f dB\r\n", nsInstp->min_att);
        printf("slope Hmin = %2.2f \r\n", nsInstp->slope);
        printf("beta = %2.2f (Q15 = %d)\r\n", (float)nsInstp->betaQ15/32768.0 , nsInstp->betaQ15);
        printf(   "======================================================\r\n");
        printf("voice band start freq = %4.1f Hz, voice_start_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_start_bin)/(float)(PolyM), nsInstp->voice_start_bin);
        printf("voice band end freq = %4.1f Hz, voice_end_bin = %d\r\n", (float)SAMPLING_FREQ*(float)(nsInstp->voice_end_bin)/(float)(PolyM), nsInstp->voice_end_bin);            
        printf(   "=======================================================\r\n");
        printf( "  Low band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Low)), nsInstp->EQ_Low);
        printf( "  mid band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Mid)), nsInstp->EQ_Mid);
        printf( "  Hi  band EQ = %2.2f dB (%d)\r\n", 20.0*log10(powf(2,(float)nsInstp->EQ_Hi)), nsInstp->EQ_Hi);

        int i;

        printf("nsInstp->H_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%2.1f ", 20*log10(((double)nsInstp->H_p[i])/32768.0));
        }
        printf("\r\n=======================================================\r\n\n");

        printf("nsInstp->HminQ15_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%2.1f ", 20*log10(((double)nsInstp->HminQ15_p[i])/32768.0));
        }
        printf("\r\n=======================================================\r\n\n");

        printf("nsInstp->See_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%d ", nsInstp->See_p[i]);
        }

        printf("\r\n=======================================================\r\n\n");
        printf("nsInstp->Sbb_p[i] ==============================================\r\n");
        for (i=0; i<PolyM/2; i++){
            if ((i%16)==0) printf("\n");
            printf("%d ", nsInstp->Sbb_p[i]);
        }
        printf("\r\n=======================================================\r\n\n");

}


void VAD_param_set(vadInst_t* vadInstp){

    Show_VAD_menu(vadInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { // a. d_SNR       up [dB] 
                vadInstp->d_SNR+=0.5;
                vadInstp->d_SNR_ratio = pow(10, vadInstp->d_SNR/10.0)-1.0;

                printf("vadInstp->d_SNR=%02f, vadInstp->d_SNR_ratio=%02.3f\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);            

            } else if (ch =='b') { //b. d_SNR       down [dB] 
                vadInstp->d_SNR-=0.5;
                if (vadInstp->d_SNR<=0) vadInstp->d_SNR=0;
                vadInstp->d_SNR_ratio = pow(10, vadInstp->d_SNR/10.0)-1.0;

                printf("vadInstp->d_SNR=%02f, vadInstp->d_SNR_ratio=%02.3f\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);            
            
            } else if (ch =='c') { //c. d_SNR_vad   up [dB]
                vadInstp->d_SNR_vad+=0.5;
                vadInstp->d_SNR_vad_ratio = pow(10, vadInstp->d_SNR_vad/10.0)-1.0;

                printf("vadInstp->d_SNR_vad=%02f, vadInstp->d_SNR_vad_ratio=%02.3f\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);            
            
            } else if (ch =='d') { //d. d_SNR_vad   down [dB]
                vadInstp->d_SNR_vad-=0.5;
                if (vadInstp->d_SNR_vad<=0) vadInstp->d_SNR_vad=0;
                vadInstp->d_SNR_vad_ratio = pow(10, vadInstp->d_SNR_vad/10.0)-1.0;

                printf("vadInstp->d_SNR_vad=%02f, vadInstp->d_SNR_vad_ratio=%02.3f\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);            
            } else if (ch =='e') { //e. n_floor_min up [dB] 
                vadInstp->n_floor_min+=1;
                vadInstp->t_chg_min=pow(10,(((double)vadInstp->n_floor_min)/10));

                printf("vadInstp->n_floor_min=%d, vadInstp->t_chg_min=%.2f\n", vadInstp->n_floor_min, vadInstp->t_chg_min);            
            
            } else if (ch =='f') { //f. n_floor_min down [dB] 
                vadInstp->n_floor_min-=1;
                vadInstp->t_chg_min=pow(10,(((double)vadInstp->n_floor_min)/10));

                printf("vadInstp->n_floor_min=%d, vadInstp->t_chg_min=%.2f\n", vadInstp->n_floor_min, vadInstp->t_chg_min);            
           } 
           else if (ch =='h') { //f. vad_hold_max up [frames] 
                vadInstp->vad_hold_max+=1;
                
                printf("vadInstp->vad_hold_max=%d, %.2f msec\n", vadInstp->vad_hold_max, (float)(vadInstp->vad_hold_max)*256.0/16.0);            
           }
           else if (ch =='i') { //f. vad_hold_max down [frames] 
                vadInstp->vad_hold_max-=1;
                
                printf("vadInstp->vad_hold_max=%d, %.2f msec\n", vadInstp->vad_hold_max, (float)(vadInstp->vad_hold_max)*256.0/16.0);                      
           }
           else if (ch =='j') { //f. vad_hold_max up [frames] 
                vadInstp->vad_hold_min+=1;
                
                printf("vadInstp->vad_hold_min=%d, %.2f msec\n", vadInstp->vad_hold_min, (float)(vadInstp->vad_hold_min)*256.0/16.0);            
           }
           else if (ch =='k') { //f. vad_hold_max down [frames] 
                vadInstp->vad_hold_min-=1;
                
                printf("vadInstp->vad_hold_min=%d, %.2f msec\n", vadInstp->vad_hold_min, (float)(vadInstp->vad_hold_min)*256.0/16.0);                      
           }      
           else if (ch =='l') { //l. 
                vadInstp->tau_s_r += 0.0001;
                
                printf("vadInstp->tau_s_r=%f\n", vadInstp->tau_s_r);            
           }
           else if (ch =='m') { //m. 
                vadInstp->tau_s_r -= 0.0001;
                
                printf("vadInstp->tau_s_r=%f\n", vadInstp->tau_s_r);  
            }    
           else if (ch =='n') { //l. 
                vadInstp->tau_s_f += 0.01;
                
                printf("vadInstp->tau_s_f=%f\n", vadInstp->tau_s_f);            
           }
           else if (ch =='o') { //m. 
                vadInstp->tau_s_f -= 0.01;
                
                printf("vadInstp->tau_s_f=%f\n", vadInstp->tau_s_f);  
            }                                 
                    
           else if (ch =='g') { //g. show all current VAD params 
                printf("vadInstp->d_SNR=%02f, vadInstp->d_SNR_ratio=%02.3f\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);    
                printf("vadInstp->d_SNR_vad=%02f, vadInstp->d_SNR_vad_ratio=%02.3f\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);  
                printf("vadInstp->n_floor_min=%d, vadInstp->t_chg_min=%.2f\n", vadInstp->n_floor_min, vadInstp->t_chg_min); 
                printf("vadInstp->vad_hold_max=%d, %.2f msec\n", vadInstp->vad_hold_max, (float)(vadInstp->vad_hold_max)*256.0/16.0);      
                printf("vadInstp->vad_hold_min=%d, %.2f msec\n", vadInstp->vad_hold_min, (float)(vadInstp->vad_hold_min)*256.0/16.0);                                                        
                printf("vadInstp->tau_s_r=%f\n", vadInstp->tau_s_r); 
                printf("vadInstp->tau_s_f=%f\n", vadInstp->tau_s_f); 
           }           
            else {
                Show_VAD_menu(vadInstp);
            }
        }
    }
}

void Show_VAD_menu(vadInst_t* vadInstp){
        printf( "\r\n============== VAD paramter Setting ====================\r\n");
        printf( "  a / b. d_SNR       up/down d_SNR=%02f, d_SNR_ratio=%02.3f\r\n", vadInstp->d_SNR, vadInstp->d_SNR_ratio);	
        printf( "  c / d. d_SNR_vad   up/down d_SNR_vad=%02f, d_SNR_vad_ratio=%02.3f\r\n", vadInstp->d_SNR_vad, vadInstp->d_SNR_vad_ratio);  	
        printf( "  e / f. n_floor_min up/down n_floor_min=%d, t_chg_min=%.2f\r\n", vadInstp->n_floor_min, vadInstp->t_chg_min); 	
        printf( "  h / i. hold max up/down    vad_hold_max=%d, %.2f msec\n", vadInstp->vad_hold_max, (float)(vadInstp->vad_hold_max)*256.0/16.0); 
        printf( "  j / k. hold min up/down    vad_hold_min=%d, %.2f msec\n", vadInstp->vad_hold_min, (float)(vadInstp->vad_hold_min)*256.0/16.0);     
        printf("   l / m. tau_s_r up/down    vadInstp->tau_s_r=%f\n", vadInstp->tau_s_r);  
        printf("   n / o. tau_s_f up/down    vadInstp->tau_s_f=%f\n", vadInstp->tau_s_f);  
        printf( "  g.  show all current VAD params                           \r\n");	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void AGC_param_set(agcInst_t* agcInstp){

    Show_AGC_menu(agcInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { // a. target_SPL       up [dB] 
                agcInstp->target_SPL += 1.0;
                agcInstp->Kp_dB = agcInstp->target_SPL - FS_SPL + Q15_dB *2;
                agcInstp->Kp=powf(10.0,(agcInstp->Kp_dB*0.05)); 

                printf("target_SPL = %f, Kp_dB=%f\r\n", agcInstp->target_SPL, agcInstp->Kp_dB);

            } else if (ch =='b') { //b. target_SPL       down [dB] 
                agcInstp->target_SPL -= 1.0;
                agcInstp->Kp_dB = agcInstp->target_SPL - FS_SPL + Q15_dB *2;
                agcInstp->Kp=powf(10.0,(agcInstp->Kp_dB*0.05)); 

                printf("target_SPL = %f, Kp_dB=%f\r\n", agcInstp->target_SPL, agcInstp->Kp_dB);
            
            } else if (ch =='c') { //c. max_gain_dB       up
                agcInstp->max_gain_dB += 1.0;
                agcInstp->max_gain= powf (10, agcInstp->max_gain_dB*0.05);

                printf("max_gain_dB = %f, max_gain=%f\r\n", agcInstp->max_gain_dB, agcInstp->max_gain);
           
            
            } else if (ch =='d') { //d. max_gain_dB       down
                agcInstp->max_gain_dB -= 1.0;
                agcInstp->max_gain= powf (10, agcInstp->max_gain_dB*0.05);

                printf("max_gain_dB = %f, max_gain=%f\r\n", agcInstp->max_gain_dB, agcInstp->max_gain);       
            } else if (ch =='e') { //e. min_gain_dB       up [dB] 
                agcInstp->min_gain_dB += 1.0;
                agcInstp->min_gain= powf (10, agcInstp->min_gain_dB*0.05);

                printf("min_gain_dB = %f, min_gain=%f\r\n", agcInstp->min_gain_dB, agcInstp->min_gain);
       
            
            } else if (ch =='f') { //f. min_gain_dB       down [dB] 
                agcInstp->min_gain_dB -= 1.0;
                agcInstp->min_gain= powf (10, agcInstp->min_gain_dB*0.05);

                printf("min_gain_dB = %f, min_gain=%f\r\n", agcInstp->min_gain_dB, agcInstp->min_gain);         
           } else if (ch =='g') { //g. AGC attack time up [msec] 
                agcInstp->att_num++;
                if (agcInstp->att_num>=14) agcInstp->att_num=14;
                agcInstp->tau_att=tau16k_90_msec[agcInstp->att_num];	
                agcInstp->r_att=gamma16k[agcInstp->att_num];
                printf("AGC attack time = %f [msec]\r\n", tau16k_90_msec[agcInstp->att_num]);
           }else if (ch =='h') { //h. AGC attack time down [msec] 
                agcInstp->att_num--;
                if (agcInstp->att_num<=0) agcInstp->att_num=0;
                agcInstp->tau_att=tau16k_90_msec[agcInstp->att_num];	
                agcInstp->r_att=gamma16k[agcInstp->att_num];
                printf("AGC attack time = %f [msec]\r\n", tau16k_90_msec[agcInstp->att_num]);     
           } else if (ch =='i') { //i. AGC release time up [msec]
                agcInstp->rel_num++;
                if (agcInstp->rel_num>=14) agcInstp->rel_num=14;
                agcInstp->tau_rel=tau16k_90_msec[agcInstp->rel_num];
                agcInstp->r_rel=gamma16k[agcInstp->rel_num];      
                printf("AGC release time = %f [msec]\r\n", tau16k_90_msec[agcInstp->rel_num]);
           } else if (ch =='j') { //j. AGC release time down [msec]
                agcInstp->rel_num--;
                if (agcInstp->rel_num<=0) agcInstp->rel_num=0;
                agcInstp->tau_rel=tau16k_90_msec[agcInstp->rel_num];
                agcInstp->r_rel=gamma16k[agcInstp->rel_num];      
                printf("AGC release time = %f [msec]\r\n", tau16k_90_msec[agcInstp->rel_num]);
            } else if (ch =='m') { //i. AGC x fast time constant(rise) up [msec]
                agcInstp->beta_f_r_num++;
                if (agcInstp->beta_f_r_num>=14) agcInstp->beta_f_r_num=14;
                agcInstp->beta_f_r=beta16k[agcInstp->beta_f_r_num];
                agcInstp->round_bit_f_r=round_bit16k[agcInstp->beta_f_r_num];     
                printf("AGC x fast time constant(rise) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_f_r_num]);
           } else if (ch =='n') { //j. AGC x fast time constant(rise) down [msec]
                agcInstp->beta_f_r_num--;
                if (agcInstp->beta_f_r_num<=0) agcInstp->beta_f_r_num=0;
                agcInstp->beta_f_r=beta16k[agcInstp->beta_f_r_num];
                agcInstp->round_bit_f_r=round_bit16k[agcInstp->beta_f_r_num];     
                printf("AGC x fast time constant(rise) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_f_r_num]);
            } else if (ch =='o') { //i. AGC x fast time constant(fall) up [msec]
                agcInstp->beta_f_f_num++;
                if (agcInstp->beta_f_f_num>=14) agcInstp->beta_f_f_num=14;
                agcInstp->beta_f_f=beta16k[agcInstp->beta_f_f_num];
                agcInstp->round_bit_f_f=round_bit16k[agcInstp->beta_f_f_num];     
                printf("AGC x fast time constant(fall) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_f_f_num]);
           } else if (ch =='p') { //j. AGC x fast time constant(fall) down [msec]
                agcInstp->beta_f_f_num--;
                if (agcInstp->beta_f_f_num<=0) agcInstp->beta_f_f_num=0;
                agcInstp->beta_f_f=beta16k[agcInstp->beta_f_f_num];
                agcInstp->round_bit_f_f=round_bit16k[agcInstp->beta_f_f_num];     
                printf("AGC x fast time constant(fall) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_f_f_num]);
            } else if (ch =='r') { //i. AGC x peak time constant(rise) up [msec]
                agcInstp->beta_p_r_num++;
                if (agcInstp->beta_p_r_num>=14) agcInstp->beta_p_r_num=14;
                agcInstp->beta_p_r=beta16k[agcInstp->beta_p_r_num];
                agcInstp->round_bit_p_r=round_bit16k[agcInstp->beta_p_r_num];     
                printf("AGC x peak time constant(rise) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_p_r_num]);
           } else if (ch =='s') { //j. AGC x peak time constant(rise) down [msec]
                agcInstp->beta_p_r_num--;
                if (agcInstp->beta_p_r_num<=0) agcInstp->beta_p_r_num=0;
                agcInstp->beta_p_r=beta16k[agcInstp->beta_p_r_num];
                agcInstp->round_bit_p_r=round_bit16k[agcInstp->beta_p_r_num];     
                printf("AGC x peak time constant(rise) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_p_r_num]);
            } else if (ch =='t') { //i. AGC x peak time constant(fall) up [msec]
                agcInstp->beta_p_f_num++;
                if (agcInstp->beta_p_f_num>=14) agcInstp->beta_p_f_num=14;
                agcInstp->beta_p_f=beta16k[agcInstp->beta_p_f_num];
                agcInstp->round_bit_p_f=round_bit16k[agcInstp->beta_p_f_num];     
                printf("AGC x peak time constant(fall) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_p_f_num]);
           } else if (ch =='u') { //j. AGC x peak time constant(fall) down [msec]
                agcInstp->beta_p_f_num--;
                if (agcInstp->beta_p_f_num<=0) agcInstp->beta_p_f_num=0;
                agcInstp->beta_p_f=beta16k[agcInstp->beta_p_f_num];
                agcInstp->round_bit_p_f=round_bit16k[agcInstp->beta_p_f_num];     
                printf("AGC x peak time constant(fall) = %f [msec]\r\n", tau16k_msec[agcInstp->beta_p_f_num]);
            } else if (ch =='v') { //i. AGC min vcnt up [msec]
                agcInstp->min_vcnt+=10;
                if (agcInstp->min_vcnt>=5000) agcInstp->min_vcnt=5000;  
                printf("AGC min vcnt up/down [msec] = %4.2f msec\r\n", (double)(agcInstp->min_vcnt)/16.0);
           } else if (ch =='w') { //j. AGC min vcnt down [msec]
                agcInstp->min_vcnt-=10;
                if (agcInstp->min_vcnt<=0) agcInstp->min_vcnt=0;   
                printf("AGC min vcnt up/down [msec] = %4.2f msec\r\n", (double)(agcInstp->min_vcnt)/16.0);
            } else if (ch =='1') { //1. globalMakeupGain_dB up
                agcInstp->globalMakeupGain_dB+=1.0;
                agcInstp->threshold_dBFS-=1.0;
                if (agcInstp->threshold_dBFS>=(agcInstp->globalMakeupGain_dB)*-1.0) agcInstp->threshold_dBFS=(agcInstp->globalMakeupGain_dB)*-1.0;  
                printf("input limiter globalMakeupGain_dB = %4.2f dB\r\n", (agcInstp->globalMakeupGain_dB));
                printf("input limiter threshold_dBFS = %4.2f dB\r\n", (agcInstp->threshold_dBFS));
           } else if (ch =='2') { //2. globalMakeupGain_dB down
                agcInstp->globalMakeupGain_dB-=1.0;
                agcInstp->threshold_dBFS+=1.0;
                if (agcInstp->globalMakeupGain_dB<=0.0) agcInstp->globalMakeupGain_dB=0.0;
                if (agcInstp->threshold_dBFS>=(agcInstp->globalMakeupGain_dB)*-1.0) agcInstp->threshold_dBFS=(agcInstp->globalMakeupGain_dB)*-1.0;                                    
                printf("input limiter globalMakeupGain_dB = %4.2f dB\r\n", (agcInstp->globalMakeupGain_dB));
                printf("input limiter threshold_dBFS = %4.2f dB\r\n", (agcInstp->threshold_dBFS));
            } else if (ch =='3') { //3. threshold_dBFS up
                agcInstp->threshold_dBFS+=1.0;
                if (agcInstp->threshold_dBFS>=(agcInstp->globalMakeupGain_dB)*-1.0) agcInstp->threshold_dBFS=(agcInstp->globalMakeupGain_dB)*-1.0;  
                printf("input limiter globalMakeupGain_dB = %4.2f dB\r\n", (agcInstp->globalMakeupGain_dB));
                printf("input limiter threshold_dBFS = %4.2f dB\r\n", (agcInstp->threshold_dBFS));
           } else if (ch =='4') { //4. threshold_dBFS down
                agcInstp->threshold_dBFS-=1.0;
                if (agcInstp->threshold_dBFS>=(agcInstp->globalMakeupGain_dB)*-1.0) agcInstp->threshold_dBFS=(agcInstp->globalMakeupGain_dB)*-1.0;  
                printf("input limiter globalMakeupGain_dB = %4.2f dB\r\n", (agcInstp->globalMakeupGain_dB));                
                printf("input limiter threshold_dBFS = %4.2f dB\r\n", (agcInstp->threshold_dBFS));


            } else {
                Show_AGC_menu(agcInstp);
            }
        }
    }
}

void Show_AGC_menu(agcInst_t* agcInstp){
        printf( "\r\n============== AGC paramter Setting ====================\r\n");
        printf( "  a / b. target_SPL       up/down [dB] = %2.0f dB\r\n", agcInstp->target_SPL);
        printf( "  c / d. max_gain_dB       up/down [dB] = %2.0f dB\r\n", agcInstp->max_gain_dB);
        printf( "  e / f. min_gain_dB       up/down [dB] = %2.0f dB\r\n", agcInstp->min_gain_dB);
        printf( "  g / h. AGC attack time up/down [msec] = %4.2f msec\r\n", agcInstp->tau_att);
        printf( "  i / j. AGC release time up/down [msec] = %4.2f msec\r\n", agcInstp->tau_rel);
        // printf( "  k / l. gate reduction  up/down [dB] = %2.0f dB\r\n", agcInstp->gate_dB);
        printf( "  m / n. AGC x fast time constant(rise) up/down [msec] = %4.2f msec\r\n", tau16k_msec[agcInstp->beta_f_r_num]);
        printf( "  o / p. AGC x fast time constant(fall) up/down [msec] = %4.2f msec\r\n", tau16k_msec[agcInstp->beta_f_f_num]);
        printf( "  r / s. AGC x peak time constant(rise) up/down [msec] = %4.2f msec\r\n", tau16k_msec[agcInstp->beta_p_r_num]);
        printf( "  t / u. AGC x peak time constant(fall) up/down [msec] = %4.2f msec\r\n", tau16k_msec[agcInstp->beta_p_f_num]);
        printf( "  v / w. AGC min vcnt up/down [msec] = %4.2f msec\r\n", (double)(agcInstp->min_vcnt)/16.0);
        printf( "  1 / 2. input limiter gain up / down  = %4.2f dB\r\n", (agcInstp->globalMakeupGain_dB));
        printf( "  3 / 4. input limiter threshold up / down = %4.2f dB\r\n", (agcInstp->threshold_dBFS));
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}


void BF_param_set(bfInst_t* bfInstp){

    Show_BF_menu(bfInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { // a. mu       up 
                bfInstp->mu_tapQ15+=1;
                printf( "mu_tapQ15=%d\r\n", bfInstp->mu_tapQ15);	
            } else if (ch =='b') { //b. mu       down
                bfInstp->mu_tapQ15-=1;
                if (bfInstp->mu_tapQ15<=0)
                    bfInstp->mu_tapQ15=0;
                printf( "mu_tapQ15=%d\r\n", bfInstp->mu_tapQ15);	
            }        
            else {
                Show_BF_menu(bfInstp);
            }
        }
    }
}

void Show_BF_menu(bfInst_t* bfInstp){
        printf( "\r\n============== BF paramter Setting ====================\r\n");
        printf( "  a / b. mu       up/down , mu_tapQ15=%d\r\n", bfInstp->mu_tapQ15);	
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}

void ssl_param_set(sslInst_t* sslInstp){

    Show_ssl_menu(sslInstp);

    while(1){
        if (_kbhit())
        {    
            int ch = _getch();
            _putch(ch);
            if(ch =='q') {         //q. Exit Menu
                printf("\n\r");
                break;
            } else if (ch =='a') { // a. fmin up
                double fmin = ((double)(sslInstp->fbin_min)/(double)ssl_blocksize*(double)16000.0);
                // double fmin = ((double)(sslInstp->fbin_min)/(double)ssl_blocksize512*(double)16000.0);
                fmin+=100;
                if (fmin>=7900.0) fmin=7900.0;
                sslInstp->fbin_min=(int)((double)ssl_blocksize*fmin/16000.0);
                // sslInstp->fbin_min=(int)((double)ssl_blocksize512*fmin/16000.0);
                printf( "fmin=%f, bin=%d\r\n", fmin,sslInstp->fbin_min);	
            } else if (ch =='b') { //b. fmin down
                double fmin = ((double)(sslInstp->fbin_min)/(double)ssl_blocksize*(double)16000.0);
                // double fmin = ((double)(sslInstp->fbin_min)/(double)ssl_blocksize512*(double)16000.0);
                fmin-=50;
                if (fmin<=50.0) fmin=50.0;
                sslInstp->fbin_min=(int)((double)ssl_blocksize*fmin/16000.0);
                // sslInstp->fbin_min=(int)((double)ssl_blocksize512*fmin/16000.0);
                printf( "fmin=%f, bin=%d\r\n", fmin,sslInstp->fbin_min);
            }        
            else if (ch =='c') { // c. fmax up
                double fmax = ((double)(sslInstp->fbin_max)/(double)ssl_blocksize*(double)16000.0);
                // double fmax = ((double)(sslInstp->fbin_max)/(double)ssl_blocksize512*(double)16000.0);
                fmax+=100;
                if (fmax>=7900.0) fmax=7900.0;
                sslInstp->fbin_max=(int)((double)ssl_blocksize*fmax/16000.0);
                // sslInstp->fbin_max=(int)((double)ssl_blocksize512*fmax/16000.0);
                printf( "fmax=%f, bin=%d\r\n", fmax,sslInstp->fbin_max);	
            } else if (ch =='d') { //d. fmax down
                double fmax = ((double)(sslInstp->fbin_max)/(double)ssl_blocksize*(double)16000.0);
                // double fmax = ((double)(sslInstp->fbin_max)/(double)ssl_blocksize512*(double)16000.0);
                fmax-=100;
                if (fmax<=100.0) fmax=50.0;
                sslInstp->fbin_max=(int)((double)ssl_blocksize*fmax/16000.0);
                // sslInstp->fbin_max=(int)((double)ssl_blocksize512*fmax/16000.0);
                printf( "fmax=%f, bin=%d\r\n", fmax, sslInstp->fbin_max);
            }  
            else if (ch =='e') { // e.gun threshold up
                // float thresold_dB = 10.0*log10f((float)(sslInstp->gun_trig_thresold)/(float)Q31_MAX);
                float thresold_dB = (float)(sslInstp->gun_trig_thresold);
                thresold_dB += 1.0;
                if (thresold_dB>=0.0) thresold_dB = 0.0;
                // float thresold_f = powf(10.0f, thresold_dB / 10.0f);
                // sslInstp->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
                sslInstp->gun_trig_thresold=(int)thresold_dB;
                printf( "thresold_dB=%f, thresoldQ15=%d\r\n", thresold_dB, sslInstp->gun_trig_thresold);	
            } else if (ch =='f') { //f.gun threshold down
                // float thresold_dB = 10.0*log10f((float)(sslInstp->gun_trig_thresold)/(float)Q31_MAX);
                float thresold_dB = (float)(sslInstp->gun_trig_thresold);
                thresold_dB -= 1.0;
                // float thresold_f = powf(10.0f, thresold_dB / 10.0f);
                // sslInstp->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
                sslInstp->gun_trig_thresold=(int)thresold_dB;
                printf( "thresold_dB=%f, thresoldQ15=%d\r\n", thresold_dB, sslInstp->gun_trig_thresold);
            }  
           else if (ch =='m') { // e.gun threshold up
                // float thresold_dB = 10.0*log10f((float)(sslInstp->gun_trig_thresold)/(float)Q31_MAX);
                float thresold_dB2 = (float)(sslInstp->gun_trig_thresold2);
                thresold_dB2 += 1.0;
                // if (thresold_dB2>=0.0) thresold_dB2 = 0.0;
                // float thresold_f = powf(10.0f, thresold_dB / 10.0f);
                // sslInstp->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
                sslInstp->gun_trig_thresold2=(int)thresold_dB2;
                printf( "thresold_dB2=%f, thresoldQ15=%d\r\n", thresold_dB2, sslInstp->gun_trig_thresold2);	
            } else if (ch =='n') { //f.gun threshold down
                // float thresold_dB = 10.0*log10f((float)(sslInstp->gun_trig_thresold)/(float)Q31_MAX);
                float thresold_dB2 = (float)(sslInstp->gun_trig_thresold2);
                thresold_dB2 -= 1.0;
                // float thresold_f = powf(10.0f, thresold_dB / 10.0f);
                // sslInstp->gun_trig_thresold=(int)(thresold_f*(float)Q31_MAX);
                sslInstp->gun_trig_thresold2=(int)thresold_dB2;
                printf( "thresold_dB2=%f, thresoldQ15=%d\r\n", thresold_dB2, sslInstp->gun_trig_thresold2);
            }              
            else if (ch =='g') { // g.CCV threshold up
                sslInstp->CCVth += 0.00001;
                printf( "Cth =%f\r\n",sslInstp->CCVth);
            } else if (ch =='h') { //h.CCV threshold down
                sslInstp->CCVth -= 0.00001;
                printf( "Cth =%f\r\n",sslInstp->CCVth);
            }    
            else if (ch =='i') { // i. ccv_stored_min up
                sslInstp->ccv_stored_min += 1;
                printf( "ccv_stored_min =%d\r\n",sslInstp->ccv_stored_min);
            } else if (ch =='j') { //j. ccv_stored_min down
                sslInstp->ccv_stored_min -= 1;
                printf( "ccv_stored_min =%d\r\n",sslInstp->ccv_stored_min);
            }  
            else if (ch =='k') { // k. ccv_last_num up
                sslInstp->ccv_last_num += 1;
                printf( "ccv_last_num =%d\r\n",sslInstp->ccv_last_num);
            } else if (ch =='l') { // l. ccv_last_num up
                sslInstp->ccv_last_num -= 1;
                printf( "ccv_last_num =%d\r\n",sslInstp->ccv_last_num);
            }   
            else if (ch =='o') { // o.gate_dB threshold up
                sslInstp->gate_dB += 10.0;
                printf( "gate_dB =%f\r\n",sslInstp->gate_dB);
            } else if (ch =='p') { //p.gate_dB threshold down
                sslInstp->gate_dB -= 10.0;
                printf( "gate_dB =%f\r\n",sslInstp->gate_dB);
            }                                                                   
            else {
                Show_ssl_menu(sslInstp);
            }
        }
    }
}

void Show_ssl_menu(sslInst_t* sslInstp){
        printf( "\r\n============== SSL paramter Setting ====================\r\n");
        printf( "  a / b. fmin up/down , fmin=%f, %d\r\n", ((double)(sslInstp->fbin_min)/(double)ssl_blocksize*(double)16000.0), sslInstp->fbin_min);	
        printf( "  c / d. fmax up/down , fmin=%f, %d\r\n", ((double)(sslInstp->fbin_max)/(double)ssl_blocksize*(double)16000.0), sslInstp->fbin_max);	
        // printf( "  a / b. fmin up/down , fmin=%f, %d\r\n", ((double)(sslInstp->fbin_min)/(double)ssl_blocksize512*(double)16000.0), sslInstp->fbin_min);	
        // printf( "  c / d. fmax up/down , fmin=%f, %d\r\n", ((double)(sslInstp->fbin_max)/(double)ssl_blocksize512*(double)16000.0), sslInstp->fbin_max);	
        // printf( "  e / f. gun threshold up/down , thresold_dB=%f, thresoldQ15=%d\r\n", 10.0*log10f((float)(sslInstp->gun_trig_thresold)/(float)Q31_MAX), sslInstp->gun_trig_thresold);
        printf( "  e / f. gun threshold up/down , thresoldQ15=%d\r\n", sslInstp->gun_trig_thresold);
        printf( "  m / n. gun threshold 2 up/down , thresoldQ15=%d\r\n", sslInstp->gun_trig_thresold2);
        printf( "  g / h. CCV threshold up/down , Cth =%f\r\n",sslInstp->CCVth);        
        printf( "  i / j. ccv_stored_min up/down , ccv_stored_min =%d\r\n",sslInstp->ccv_stored_min);  
        printf( "  k / l. ccv_last_num up/down , Cth =%d\r\n",sslInstp->ccv_last_num);  
        printf( "  o / p. gate_dB up/down , gate_dB =%f\r\n",sslInstp->gate_dB);  
        printf( "  q. Exit Menu                                             \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}



// void aec_param_set(aecInst_t* aecInstp){

//     Total_Inst_t* Total_Instp = &Sys_Total_Inst;

//     Show_aec_menu(aecInstp);

//     while(1){
//         if (_kbhit())
//         {    
//             int ch = _getch();
//             _putch(ch);
//             if(ch =='q') {         //q. Exit Menu
//                 printf("\n\r");
//                 break;
//             } else if (ch =='a') { // a. mu_opt_d up
//                 double mu = (double)(aecInstp->mu_opt_d);
//                 mu+=0.01;
//                 if (mu>=50.0) mu=50.0;
//                 aecInstp->mu_opt_d=mu;
//                 printf( "mu_opt_d=%f\r\n", aecInstp->mu_opt_d);
//             } else if (ch =='b') { //b. mu_opt_d down
//                 double mu = (double)(aecInstp->mu_opt_d);
//                 mu-=0.01;
//                 if (mu<=0.0) mu=0.0;
//                 aecInstp->mu_opt_d=mu;
//                 printf( "mu_opt_d=%f\r\n", aecInstp->mu_opt_d);     
//             } else if (ch =='c') { // c. mu_tap_d up
//                 double mu = (double)(aecInstp->mu_tap_d);
//                 mu+=0.01;
//                 if (mu>=2.0) mu=2.0;
//                 aecInstp->mu_tap_d=mu;
//                 printf( "mu_tap_d=%f\r\n", aecInstp->mu_tap_d);
//             } else if (ch =='d') { // d. mu_tap_d down
//                 double mu = (double)(aecInstp->mu_tap_d);
//                 mu-=0.01;
//                 if (mu<=0.0) mu=0.0;
//                 aecInstp->mu_tap_d=mu;
//                 printf( "mu_tap_d=%f\r\n", aecInstp->mu_tap_d);
//             } else if (ch =='e') { // e. reg1_d up
//                 double reg = (double)(aecInstp->reg1_d);

//                 double tempd = reg / (double)(1.073741824000000e+09);
//                 double tempdB_d = 10*log10(tempd);
//                 tempdB_d+=1.0;
//                 tempd = pow(10,tempdB_d/(10.0));
//                 reg = tempd * (double)(1.073741824000000e+09);
//                 aecInstp->reg1_d=reg;
//                 printf( "reg1_d=%f, %fdB\r\n", aecInstp->reg1_d, tempdB_d);

//             } else if (ch =='f') { // f. reg1_d down
//                 double reg = (double)(aecInstp->reg1_d);

//                 double tempd = reg / (double)(1.073741824000000e+09);
//                 double tempdB_d = 10*log10(tempd);
//                 tempdB_d-=1.0;
                
//                 if (tempdB_d<=-90.0) tempdB_d=-90.0;

//                 tempd = pow(10,tempdB_d/(10.0));
//                 reg = tempd * (double)(1.073741824000000e+09);

//                 if (reg<=0.0) reg=0.0;
                
//                 aecInstp->reg1_d=reg;
//                 printf( "reg1_d=%f, %fdB\r\n", aecInstp->reg1_d, tempdB_d);   
//             } else if (ch =='g') { // g. reg2_d up
//                 double reg = (double)(aecInstp->reg2_d);

//                 double tempdB_d = 10*log10(reg);
//                 tempdB_d+=1.0;
//                 reg = pow(10,tempdB_d/(10.0));

//                 aecInstp->reg2_d=reg;
//                 printf( "reg2_d=%f, %fdB\r\n", (aecInstp->reg2_d)*1.0000e+08, tempdB_d);
//             } else if (ch =='h') { // h. mu_tap_d down
//                 double reg = (double)(aecInstp->reg2_d);

//                 double tempdB_d = 10*log10(reg);
//                 tempdB_d-=1.0;
//                 if (tempdB_d<=-90.0) tempdB_d=-90.0;

//                 reg = pow(10,tempdB_d/(10.0));

//                 if (reg<=0.0) reg=0.0;
//                 aecInstp->reg2_d=reg;
//                 printf( "reg2_d=%f, %fdB\r\n", (aecInstp->reg2_d)*1.0000e+08, tempdB_d);  
//             }
//             else if (ch =='i') {
//                 aecInstp->delay_single += 1;
//                 if (aecInstp->delay_single >= (Echo_single_taps-100)) aecInstp->delay_single = Echo_single_taps - 100;

//                 printf("misalign cal delay = %d \r\n", aecInstp->delay_single);
//             }    
//             else if (ch =='j') {
//                 aecInstp->delay_single -= 1;
//                 if (aecInstp->delay_single <= 0) aecInstp->delay_single = 0;

//                 printf("misalign cal delay = %d \r\n", aecInstp->delay_single);
//             } 
//            else if (ch =='k') { //l. 
//                 aecInstp->tau_p_r += 0.1;
                
//                 printf("aecInstp->tau_p_r=%f\n", aecInstp->tau_p_r);            
//            }
//            else if (ch =='l') { //m. 
//                 aecInstp->tau_p_r -= 0.1;
                
//                 printf("aecInstp->tau_p_r=%f\n", aecInstp->tau_p_r);  
//             }
//            else if (ch =='m') { //l. 
//                 aecInstp->tau_p_f += 0.1;
                
//                 printf("aecInstp->tau_p_f=%f\n", aecInstp->tau_p_f);            
//            }
//            else if (ch =='n') { //m. 
//                 aecInstp->tau_p_f -= 0.1;
                
//                 printf("aecInstp->tau_p_f=%f\n", aecInstp->tau_p_f);  
//             }
//            else if (ch =='o') { //l. 
//                 aecInstp->tau_e_r += 0.1;
                
//                 printf("aecInstp->tau_e_r=%f\n", aecInstp->tau_e_r);            
//            }
//            else if (ch =='p') { //m. 
//                 aecInstp->tau_e_r -= 0.1;
                
//                 printf("aecInstp->tau_e_r=%f\n", aecInstp->tau_e_r);  
//             }
//            else if (ch =='r') { //l. 
//                 aecInstp->tau_e_f += 0.1;
                
//                 printf("aecInstp->tau_e_f=%f\n", aecInstp->tau_e_f);            
//            }
//            else if (ch =='s') { //m. 
//                 aecInstp->tau_e_f -= 0.1;
                
//                 printf("aecInstp->tau_e_f=%f\n", aecInstp->tau_e_f);  
//             }

//             else if (ch =='t') {
//                 aecInstp->taps_single += 100;
//                 if (aecInstp->taps_single >= Echo_single_taps) aecInstp->taps_single = Echo_single_taps;

//                 printf("taps_single = %d \r\n", aecInstp->taps_single);
//             }    
//             else if (ch =='u') {
//                 aecInstp->taps_single -= 100;
//                 if (aecInstp->taps_single <= aecInstp->delay) aecInstp->taps_single = aecInstp->delay;

//                 printf("taps_single = %d \r\n", aecInstp->taps_single);
//             } 

//             else if (ch =='v') {
//                 Total_Instp->tuning_delay += 10;
//                 if (Total_Instp->tuning_delay >= Ref_delay) Total_Instp->tuning_delay = Ref_delay;

//                 printf("tuning_delay = %d (%f msec) \r\n", Total_Instp->tuning_delay, (double)(Total_Instp->tuning_delay)/16.0);
//             }    
//             else if (ch =='w') {
//                 Total_Instp->tuning_delay -= 10;
//                 if (Total_Instp->tuning_delay <= 0) Total_Instp->tuning_delay = 0;

//                 printf("tuning_delay = %d (%f msec) \r\n", Total_Instp->tuning_delay, (double)(Total_Instp->tuning_delay)/16.0);
//             }                                                                     
//             else {
//                 Show_aec_menu(aecInstp);
//             }
//         }
//     }
// }

// void Show_aec_menu(aecInst_t* aecInstp){
//     Total_Inst_t* Total_Instp = &Sys_Total_Inst;
//         printf( "\r\n============== AEC paramter Setting ====================\r\n");
//         printf( "  a / b. mu_opt_d up/down , mu_opt_d=%f\r\n", aecInstp->mu_opt_d);	
//         printf( "  c / d. mu_tap_d up/down , mu_tap_d=%f\r\n", aecInstp->mu_tap_d);	
//         printf( "  e / f. reg1_d up/down   , reg1_d=%f\r\n", aecInstp->reg1_d);	
//         printf( "  g / h. reg2_d up/down   , reg2_d=%f, (%f)\r\n", aecInstp->reg2_d, (aecInstp->reg2_d)*1.0000e+08);	
//         printf( "  i / j. misalign cal delay up / down, delay=%d         \r\n", aecInstp->delay);  
//         printf("   k / l. aecInstp->tau_p_r up/down    aecInstp->tau_p_r=%f\n", aecInstp->tau_p_r);                            
//         printf("   m / n. aecInstp->tau_p_f up/down    aecInstp->tau_p_f=%f\n", aecInstp->tau_p_f);        
//         printf("   o / p. aecInstp->tau_e_r up/down    aecInstp->tau_e_r=%f\n", aecInstp->tau_e_r);                                    
//         printf("   r / s. aecInstp->tau_e_f up/down    aecInstp->tau_e_f=%f\n", aecInstp->tau_e_f);                                            
//         printf("   t / u. filter tap up/down    taps_single=%d         \r\n", aecInstp->taps_single);             
//         printf( "  v / w. tuning delay up / down, tuning delay=%d         \r\n", Total_Instp->tuning_delay);               
//         printf( "  q. Exit Menu                                             \r\n");
//         printf( "  To print this memu, press any other key                  \r\n");
//         printf(   "=======================================================\r\n\n");
// }