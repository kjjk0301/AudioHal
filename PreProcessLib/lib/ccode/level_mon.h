#ifndef LEVEL_MON_H_
#define LEVEL_MON_H_

#define sNWz 49

typedef struct MonInst_s {
    short       blocksize;

    short beta_p_r_num;
    short beta_p_f_num;

    short beta_p_r;
    short beta_p_f;

    short round_bit_p_r;
	short round_bit_p_f;

    float tau_rise;
    float tau_fall;

    float noise_level;
    float target_level;

	float noise_dB;
    float target_dB;

    float noise_after_level;
    float target_after_level;

	float noise_after_dB;
    float target_after_dB;    

    float noise_bpf_level;
    float target_bpf_level;

	float noise_bpf_dB;
    float target_bpf_dB;

    float noise_bpf_after_level;
    float target_bpf_after_level;

	float noise_bpf_after_dB;
    float target_bpf_after_dB;    

	int noise_calc_percent;
    int target_calc_percent;

} MonInst_t;

MonInst_t * sysMonCreate();
void Mon_Init(MonInst_t *MonInst);
void Mon_DeInit();
// void Mon_process(MonInst_t *MonInst, short *in, short state, short vad_short, short vad_long);
int get_noise_level(MonInst_t *MonInst);
int get_target_level(MonInst_t *MonInst);
int get_noise_after_level(MonInst_t *MonInst);
int get_target_after_level(MonInst_t *MonInst);
int get_SDNR_level(MonInst_t *MonInst);
// void Mon_process2(MonInst_t *MonInst, short *in, short vad_short, short vad_long);
// void Mon_process3(MonInst_t *MonInst, short *in, short *in_after, short vad_short, short vad_long);
// void reset_levels();
void reset_noise_levels(MonInst_t *MonInst);
void reset_target_levels(MonInst_t *MonInst);

int get_noise_calc_percent(MonInst_t *MonInst);
int get_target_calc_percent(MonInst_t *MonInst);

void Mon_process_LEQ(MonInst_t *MonInst, short *in, short *in_after);
void Mon_process_BPF(MonInst_t *MonInst, short *in, short *in_after);
int Mon_process_noise(MonInst_t *MonInst, short vad_short, short vad_long, int cnt_done);
int Mon_process_target(MonInst_t *MonInst, short vad_short, short vad_long, int cnt_done);

extern int speech_BPF_Buf[2][sNWz+NUM_FRAMES-1];

#endif