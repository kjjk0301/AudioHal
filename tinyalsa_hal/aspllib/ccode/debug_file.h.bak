/*
 * debug_file.h
 *
 *  Created on: 2019. 7. 1.
 *      Author: Seong Pil Moon
 */

#ifndef DEBUG_FILE_H_
#define DEBUG_FILE_H_

// #define DEBUG_SSL_VAD

#define DEBUG_SSL_VAD_MATLAB
#define DEBUG_AGC_MATLAB
#define DEBUG_NS_MATLAB

#define DEBUG_NUM_VAD 0
#define DEBUG_NUM_AGC 1
#define DEBUG_NUM_NS  2

#define FRAME_TYPE_FIELD 0
#define FRAME_LENG_FIELD 1
#define FRAME_DATA_FIELD 2

#define DEBUGFILENUM 10

#ifdef DEBUG_SSL_VAD

extern FILE *fp1[DEBUGFILENUM];

void debug_file_open();
void debug_file_close();
int debug_file_write_int(int value, int filenum);
int debug_file_write_double(double value, int filenum);
int debug_file_write_return(int filenum);

#endif

extern int g_vad_debug_on;
extern int g_agc_debug_on;
extern int g_ns_debug_on;
extern int g_ssl_debug_on;
extern int g_aec_debug_on;

extern int g_vad_debug_snd_idx;
extern int g_agc_debug_snd_idx;
extern int g_ns_debug_snd_idx;
extern int g_ssl_debug_snd_idx;
extern int g_aec_debug_snd_idx;

extern int g_vad_debug_snd_period;
extern int g_agc_debug_snd_period;
extern int g_ns_debug_snd_period;
extern int g_ssl_debug_snd_period;
extern int g_aec_debug_snd_period;

int debug_matlab_open();
void debug_matlab_close();
int debug_matlab_int(int debugnum, int value, int num);
int debug_matlab_float(int debugnum, float value, int num);
int debug_matlab_send(int debugnum);
int debug_matlab_spectrum_int(int* p_spec, int spec_num, int spec_len);
int debug_matlab_spectrum_send(int spec_num);
int debug_matlab_spectrum_oct_int(int* p_spec, int spec_num, int spec_len);
int debug_matlab_spectrum_oct_send(int spec_num);
void calculate_third_octave_spectrum(const int32_t *spectrum, int32_t *result);
int debug_matlab_ccv_double(double* p_ccv, int ccv_num, int ccv_len);
int debug_matlab_ccv_double2(double* p_ccv, double peak, int ccv_num, int ccv_len);
int debug_matlab_ccv_send(int ccv_num, int ccv_leng);
int debug_matlab_doa_send(int num, double DoA);
int debug_matlab_ssl_vad_send(int32_t vad);
int debug_matlab_ssl_float_send(int num, float val);
int debug_matlab_audio (short* audiobuffer, short len);
int debug_matlab_float_array_send(int type, float* val, int num);
int debug_matlab_int_array_send(int type, int32_t* val, int num);

// 데이터 구조체 정의
typedef struct {
    float val1;
    float val2;
    float val3;
    float val4;
    float val5;
} FloatPacket;

int debug_matlab_ssl_float_packet_send(int num, FloatPacket packet);

#endif /* DEBUG_FILE_H_ */


