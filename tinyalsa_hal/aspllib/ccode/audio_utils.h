
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

int DC_rejection(int * offset_Q15_p, int xin, int beta);
void audioEQ_allocate_bufs(uint32_t bufsize);
void audioEQ_free_bufs();
void audioEQ_process_samples_float(void *tx_buf, void *rx_buf, uint32_t filt_len, uint32_t filt_num, uint32_t chan);

#ifdef CALC_SPL
void squarer_float(void *tx_buf, void *rx_buf, uint32_t filt_len);
float calc_timeweight(float *L, void *rx_buf, uint32_t input_len, float beta, float P0_dB);
float calc_Leq(float *Leq, void *calc_buf, void *rx_buf, uint32_t buf_len, uint32_t input_len, float P0_dB);
float calc_SPL_dB(float L, float P0_dB);
#endif