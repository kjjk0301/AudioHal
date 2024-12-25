
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

#define SAMPLING_FREQ       16000
#define Q15                 32768

extern int phase;

uint16_t num_of_sine[31] = {2400,	1920,	1600,	1200,	960,
		800,	600,	480,	384,	300,
		240,	192,	160,	120,	96,
		384,	60,	48,	192,	30,
		24,	96,	384,	12,	48,
		192,	6,	24,	96,	3,
		12 };

uint16_t idx_of_sine[32] = {0,	2400,	4320,	5920,	7120,
		8080,	8880,	9480,	9960,	10344,
		10644,	10884,	11076,	11236,	11356,
		11452,	11836,	11896,	11944,	12136,
		12166,	12190,	12286,	12670,	12682,
		12730,	12922,	12928,	12952,	13048,
		13051, 13063};

double freq_of_sine[32] = {20,	25,	30,	40,	50,
		60,	80,	100,	125,	160,
		200,	250,	300,	400,	500,
		625,	800,	1000,	1250,	1600,
		2000,	2500,	3125,	4000,	5000,
		6250,	8000,	10000,	12500,	16000,
		20000, 25000};

int16_t sine_table[13063];

int16_t impulse_table[16000];

void make_sine_table() {

	int i, j;
	double phase = 0;

    static double max_phase = 2. * M_PI;
    double step;

	for (i=0; i<31; i++){

		phase = 0;

		step = max_phase * freq_of_sine[i] / (double)SAMPLING_FREQ;

		for (j = idx_of_sine[i]; j<idx_of_sine[i+1]; j++){

			sine_table[j]=(int16_t)(sin(phase)*(double)(Q15-1)*0.5);

	        phase += step;
	        if (phase >= max_phase)
	            phase -= max_phase;
		}

	}

}

void make_impulse_table() {

	int i, j;

	int impulse_len = 1;

	for (i=0; i<16000; i++){

		if (i<impulse_len) {
			impulse_table[i] = (int16_t)(1.0*(double)(Q15-1)*0.5);
		} else {
			impulse_table[i] = (int16_t)(0);
		}
	}

}

void generate_sine(int16_t *input, int count, int *_phase, double freq, float max_dB)
{

	int phase_idx = (int)*_phase;

	uint8_t freq_idx=0;

	for (int i=0; i<32; i++){

		if (freq_of_sine[i]>freq) {
			freq_idx = i-1;
			break;
		}
	}

	if (phase_idx<idx_of_sine[freq_idx] || phase_idx>=idx_of_sine[freq_idx+1]){
		phase_idx = idx_of_sine[freq_idx];
	}

	int j=phase_idx;
	for (int i=0; i<count; i++){
		input[i*2] = sine_table[j]>>2;
		input[i*2+1] = 0; //sine_table[j]>>2;
		j++;
		if (j==idx_of_sine[freq_idx+1]){
			j=idx_of_sine[freq_idx];
		}
	}

	*_phase = (int)j;

}

void generate_impulse(int16_t *input, int count, int *p_idx)
{

	int idx = (int)*p_idx;

	int j=idx;

	for (int i=0; i<count; i++){
		input[i*2] = impulse_table[j]>>1;
		input[i*2+1] = 0; //impulse_table[j]>>1;
		j++;
		if (j==16000){
			j=0;
		}
	}

	*p_idx = (int)j;

}