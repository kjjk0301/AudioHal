
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "cng.h"

#define Q15_MAXf 32767.0f // Define max amplitude for Q15 format

// // 글로벌 필터 상태 변수 정의
float state_low[2] = {0};
float state_peak[2] = {0};
float state_lp[2] = {0};

cngInst_t* sysCNGCreate()
{
    cngInst_t * cngInst_p = malloc(sizeof(cngInst_t));
    CNG_Init(cngInst_p);

    return cngInst_p;
}

void CNG_Init(cngInst_t *cngInst){

    cngInst_t *inst = (cngInst_t *)cngInst;

    for (int i=0; i<2; i++){
        inst->state_low[i]=3.0;
        inst->state_peak[i]=2.0;
        inst->state_lp[i]=1.0;
    }
}

void CNG_DeInit(cngInst_t *cngInst){

    cngInst_t *inst = (cngInst_t *)cngInst;

	if(inst)
	{
		free(inst);
		inst = NULL;
	}
}

// dBFS 값을 리니어 게인 값으로 변환
float dbfs_to_linear(float dbfs) {
    return powf(10.0f, dbfs / 20.0f);
}

// IIR 필터 적용 함수
void apply_iir_filter(float *input, float *output, int length, float *b, float *a, float *state, int order) {
    for (int i = 0; i < length; i++) {
        float result = b[0] * input[i] + state[0];
        for (int j = 1; j <= order; j++) {
            state[j - 1] = b[j] * input[i] - a[j] * result + state[j];
        }
        output[i] = result;
    }
}

// 화이트 노이즈 생성 함수
void generate_white_noise(float *noiseBuffer, int length) {
    float scale = Q15_MAXf / (float)RAND_MAX;
    for (int i = 0; i < length; i++) {
        noiseBuffer[i] = ((float)rand() * scale) * 2.0f - Q15_MAXf; // -1.0 ~ 1.0 사이 화이트 노이즈
    }
}


// 컴포트 노이즈 생성 함수
void generate_comfort_noise(cngInst_t *cngInst, float *noiseBuffer, int length) {

    cngInst_t *inst = (cngInst_t *)cngInst;

    // printf("cngInst->state_low[0]=%f\r\n", inst->state_low[0]);
    // printf("cngInst->state_low[1]=%f\r\n", inst->state_low[1]);
    // printf("cngInst->state_peak[0]=%f\r\n", inst->state_peak[0]);
    // printf("cngInst->state_peak[1]=%f\r\n", inst->state_peak[1]);
    // printf("cngInst->state_lp[0]=%f\r\n", inst->state_lp[0]);
    // printf("cngInst->state_lp[1]=%f\r\n", inst->state_lp[1]);

    // 필터 계수 정의
    float b_low[3] = {1.0342, -1.8315, 0.8153};
    float a_low[3] = {1.0000, -1.8360, 0.8450};

    float b_peak[3] = {0.7361,   -0.8683,    0.4919};
    float a_peak[3] = {1.0000,   -0.8683,    0.2280};

    float b_lp[3] = {0.5690, 1.1381, 0.5690};
    float a_lp[3] = {1.0000, 0.9428, 0.3333};

    // 임시 버퍼 생성
    float tempBuffer[length];

    // 1. 화이트 노이즈 생성
    generate_white_noise(noiseBuffer, length);

    // 2. 저주파 강조 필터 적용
    apply_iir_filter(noiseBuffer, tempBuffer, length, b_low, a_low, &inst->state_low[0], 2);

    // 3. 중주파 감쇠 필터 적용
    apply_iir_filter(tempBuffer, noiseBuffer, length, b_peak, a_peak, &inst->state_peak[0], 2);

    // 4. 고주파 롤오프 필터 적용
    apply_iir_filter(noiseBuffer, tempBuffer, length, b_lp, a_lp, &inst->state_lp[0], 2);

    // 최종 결과 저장
    memcpy(noiseBuffer, tempBuffer, sizeof(float) * length);
}


// // 핑크 노이즈 생성 함수
// void generate_pink_noise(float *noiseBuffer, int length, float noiseLevel) {
//     float b[7] = {0.02109238, 0.07113478, 0.68873558, 0.12020611, 0.12509026, 0.13112453, 0.00051465};
//     float state[6] = {0};

//     float scale = Q15_MAXf / (float)RAND_MAX;

//     for (int i = 0; i < length; i++) {
//         float white = ((float)rand() * scale) * 2.0f - Q15_MAXf; // -1.0 ~ 1.0 사이의 화이트 노이즈
//         float pink = white * b[0] + state[0] * b[1] + state[1] * b[2] + state[2] * b[3] +
//                      state[3] * b[4] + state[4] * b[5] + state[5] * b[6];

//         // 상태 업데이트
//         for (int j = 5; j > 0; j--) {
//             state[j] = state[j - 1];
//         }
//         state[0] = white;

//         // 노이즈 크기 조정
//         noiseBuffer[i] = pink * noiseLevel;

//         // printf("pink= %f white =%f, noiseBuffer[i] = %f noiseLevel = %f\n", pink, white, noiseBuffer[i], noiseLevel);
//     }
// }

// 컴포트 노이즈 추가 함수
void add_comfort_noise(cngInst_t *cngInst, int16_t *signal, float *noise, int length, float noiseGainDbfs) {

    float noiseGain = dbfs_to_linear(noiseGainDbfs);

    for (int i = 0; i < length; i++) {
        float mixed = (float)signal[i] + noise[i] * noiseGain;
        if (mixed > 32767.0f) {
            mixed = 32767.0f; // 클리핑 방지
        } else if (mixed < -32768.0f) {
            mixed = -32768.0f; // 클리핑 방지
        }
        signal[i] = (int16_t)mixed;

        // printf("noise[i] = %f signal[i] = %d, mixed=%f\n", noise[i], signal[i], mixed);
    }
}

// 컴포트 노이즈 처리 함수
void CNG_process(cngInst_t *cngInst, int16_t *signal, int length, float noiseGainDbfs) {

    float noise[length];
    // 1. 컴포트 노이즈 생성
    generate_comfort_noise(cngInst, noise, length);

    // 2. 컴포트 노이즈를 신호에 추가
    add_comfort_noise(cngInst, signal, noise, length, noiseGainDbfs);
}