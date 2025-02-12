/**
  ******************************************************************************
  * @file    aspl_nr.h
  * @author  ASPL inc. 
  * @brief   This file contains all the functions prototypes for the
  *          libasplnr.a
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 ASPL inc.
  * All rights reserved.</center></h2>
  ******************************************************************************
  *
  * version : 0.1.0
  * release date : 2024.11.19
  * release info : MarkT aec first release
  *
  ******************************************************************************
  */ 

#ifndef _ASPL_NR_H
#define _ASPL_NR_H

#define aspl_RET_SUCCESS 0
#define aspl_RET_FAIL    1
#define aspl_RET_FAIL_MALLOC    2


typedef enum
{
    aspl_NR_CMD_VERSION = 0,               // GET
    aspl_NR_CMD_RESULT,                    // GET
    aspl_NR_CMD_LOG,                        // SET
    aspl_NR_CMD_RESET,                  // SET
    aspl_NR_CMD_DEBUG,                  // SET/GET
    aspl_NR_CMD_ENABLE,                 // SET/GET

  /* aspl_NR */
    aspl_NR_CMD_SET_NR,
    aspl_NR_CMD_SET_EXPERT_PARAM,
} aspl_NR_CMD_E;

typedef struct aspl_NR_CONFIG_
{
  int enable;
  int sensitivity; // 0 ~ 3 : "Off", "Low", "Middle", "High"
  char tuning_file_path[256];

  int noise_level;
  int target_level;
  int SNR_current;

  void * Total_Inst_p;
  void * aecInst_p[4];

  int aec_Mic_N;
  int offset_Q15_L;
  int offset_Q15_R;
  int offset_Q15_ref;

  float AEC_filter_save[2][2000];
  int AEC_filter_len[2];
  int AEC_globaldelay[2]; // for 1024 buffer
  int AEC_filter_updated;
  int AEC_filter_loaded;  

} aspl_NR_CONFIG;

typedef struct aspl_NR_analysis_
{
  int noise_level;
  int target_level;
  int noise_after_level;
  int target_after_level;  
  int SNR_current;
  int SNR_after_current;
  int SDNR;
  float sound_quality;
} aspl_NR_analysis;


typedef struct aspl_nr_params_s {

  int ns_voice_start_bin; //0 | noise reduction | Mid band(음성band) 시작 주파수 bin
  int ns_voice_end_bin;   //1 | noise reduction | Mid band(음성band) 끝 주파수 bin
  int ns_Low_solo;        //2 | noise reduction | Low band solo monitoring enable
  int ns_Mid_solo;        //3 | noise reduction | Mid band solo monitoring enable
  int ns_Hi_solo;         //4 | noise reduction | Hi band solo monitoring enable
  int ns_beta_e_num_0;    //5 | noise reduction | Low band instance spectrum time constant 1
  int ns_beta_e_num_1;    //6 | noise reduction | Low band instance spectrum time constant 2
  int ns_beta_e_num_2;    //7 | noise reduction | Mid band instance spectrum time constant 1
  int ns_beta_e_num_3;    //8 | noise reduction | Mid band instance spectrum time constant 2
  int ns_beta_e_num_4;    //9 | noise reduction | Hi band instance spectrum time constant 1
  int ns_beta_e_num_5;    //10 | noise reduction | Hi band instance spectrum time constant 2
  int ns_beta_r_num_0;    //11 | noise reduction | Low band noise spectrum time constant 1
  int ns_beta_r_num_1;    //12 | noise reduction | Low band noise spectrum time constant 2
  int ns_beta_r_num_2;    //13 | noise reduction | Mid band noise spectrum time constant 1
  int ns_beta_r_num_3;    //14 | noise reduction | Mid band noise spectrum time constant 2
  int ns_beta_r_num_4;    //15 | noise reduction | Hi band noise spectrum time constant 1
  int ns_beta_r_num_5;    //16 | noise reduction | Hi band noise spectrum time constant 2
	int	ns_betaQ15;         //17 | noise reduction | noise reduction 강도를 결정하는 beta값
  int ns_max_att;         //18 | noise reduction | Low band noise reduction 한도
  int ns_min_att;         //19 | noise reduction | Mid band noise reduction 한도
  int ns_high_att;        //20 | noise reduction | Hi band noise reduction 한도
  int ns_slope;           //21 | noise reduction | 각 밴드 사이의 noise reduction 양의 경사도

  int vad_d_SNR;          //22 | target sound activity detection | 소음 변화량 인식 threshold
  int vad_d_SNR_vad;      //23 | target sound activity detection | 음성 활동 인식 threshold
  int vad_n_floor_min;    //24 | target sound activity detection | noise floor 설정값

	int agc_max_gain_dB;      //25 | auto gain control | agc 최대 증폭 게인
	int agc_min_gain_dB;      //26 | auto gain control | agc 최소 증폭(감쇄) 게인
	int agc_target_SPL;       //27 | auto gain control | agc 타겟 레벨
	int agc_gate_dB;          //28 | auto gain control | agc gating 최대 레벨 (현재 사용안함)
	int agc_att_num;          //29 | auto gain control | agc 반응 속도(attack time) time-constant
	int agc_rel_num;          //30 | auto gain control | agc 반응 속도(release time) time-constant

	int vad_hold_max;         //31 | target sound activity detection | 음성 인식시 최대 유지 시간 (hold-time)

  int NS_enable;            //32 | noise reduction | noise reduction 알고리즘 전체 enable
  int AGC_enable;           //33 | auto gain control | auto gain control 알고리즘 전체 enable

  int EQ_Low;               //34 | noise reduction | Low band Equalizer 
  int EQ_Hi;                //35 | noise reduction | Hi band Equalizer 

  /* paramters for NR level 1 */
  int betaQ15_NR1;          //36 | noise reduction | noise reduction 강도를 결정하는 beta값
  int max_att_NR1;          //37 | noise reduction | Low band noise reduction 한도
  int min_att_NR1;          //38 | noise reduction | Mid band noise reduction 한도
  int slope_NR1;            //39 | noise reduction | 각 밴드 사이의 noise reduction 양의 경사도
  int high_att_NR1;         //40 | noise reduction | Hi band noise reduction 한도
  int target_SPL_NR1;       //41 | auto gain control | agc 타겟 레벨

  /* paramters for NR level 2 */
  int betaQ15_NR2;          //42 | noise reduction | noise reduction 강도를 결정하는 beta값
  int max_att_NR2;          //43 | noise reduction | Low band noise reduction 한도
  int min_att_NR2;          //44 | noise reduction | Mid band noise reduction 한도
  int slope_NR2;            //45 | noise reduction | 각 밴드 사이의 noise reduction 양의 경사도
  int high_att_NR2;         //46 | noise reduction | Hi band noise reduction 한도
  int target_SPL_NR2;       //47 | auto gain control | agc 타겟 레벨

  /* paramters for NR level 3 */
  int betaQ15_NR3;          //48 | noise reduction | noise reduction 강도를 결정하는 beta값
  int max_att_NR3;          //49 | noise reduction | Low band noise reduction 한도
  int min_att_NR3;          //50 | noise reduction | Mid band noise reduction 한도
  int slope_NR3;            //51 | noise reduction | 각 밴드 사이의 noise reduction 양의 경사도
  int high_att_NR3;         //52 | noise reduction | Hi band noise reduction 한도
  int target_SPL_NR3;       //53 | auto gain control | agc 타겟 레벨

  int vad_hold_min;         //54 | target sound activity detection | 음성 인식시 최소 유지 시간 (hold-time)

  int ns_Ma_size;           //55 | noise reduction | instance spectrum average time

  int DC_reject_beta;       //56 | DC rejection | DC rejection cut-off주파수 결정 factor

  int ns_beta_low_ratio;    //57 | noise reduction | low-band beta weighting factor
  int ns_beta_high_ratio;   //58 | noise reduction | hi-band beta weighting factor

  int vad_tau_s_r;          //59 | target sound activity detection | noise level estimation time-constant 1
  int vad_tau_s_f;          //60 | target sound activity detection | noise level estimation time-constant 2


  int global_gain;          //61 | global gain | global gain in dB

  int reserve30;            //62
  int reserve31;            //63    

} aspl_nr_params_t;

extern aspl_nr_params_t g_aspl_nr_params;

int aspl_NR_create(void* data); // 1mic용 NR 초기화 함수
int aspl_NR_process(short* data, int len); // 1mic용 NR 수행 함수
int aspl_NR_process_enables(short* data, int len, int NS_enable, int AGC_enable, int globalgain_dB); // 1mic용 NR 수행 함수 with NS, AGC enable control and glabal gain control(dB) 
int aspl_NR_destroy(void); // NR 제거 함수
int aspl_NR_set(aspl_NR_CMD_E cmd, void* data); // NR단계 적용 함수

void aspl_NR_expert_param_read(aspl_nr_params_t* tmp_p);
void aspl_NR_expert_param_write(aspl_nr_params_t* tmp_p);
int aspl_NR_param_set(int param_num, int value); // expert mode, NR 파라메터 변경
int aspl_NR_param_get(int param_num); // expert mode, NR 파라메터 읽기
int aspl_NR_total_param_set_from_file(const char* file_path); // expert mode, 튜닝 파라메터 바이너리 파일로 부터 파라메터 전체 업데이트
int aspl_NR_total_param_write_to_file(const char* file_path); // expert mode, 전체 튜닝 파라메터를 바이너리 파일로 기록 

int aspl_NR_create_2mic(void* data); // 2mic NR 초기화 함수
// int aspl_NR_process_2mic(short* data, int len, int Beam1, int Beam_auto, double * pDoA); // 2mic NR 수행 함수, with Beamforming auto mode : 1 auto, 0 manual
int aspl_NR_process_2mic(short* data, int len, int Beam1, int Beam_auto, double * pDoA, aspl_NR_CONFIG* config);

int aspl_AEC_create(aspl_NR_CONFIG* config);
int aspl_AEC_process_2ch(int16_t* data, int16_t* ref, int len, int aec_delay, float micscaledB, aspl_NR_CONFIG* config);
int aspl_AEC_process_single(int16_t* data, int16_t* ref, int len, int aec_delay, float micscaledB, aspl_NR_CONFIG* config);
int aspl_AEC_process_filtersave(int16_t* data, int16_t* ref, int len, int aec_delay, int delayauto, float micscaledB, aspl_NR_CONFIG* config);
int aspl_AEC_filterload(aspl_NR_CONFIG* config);

void aspl_print_ver();
const char* aspl_getVersionInfo();

#endif
