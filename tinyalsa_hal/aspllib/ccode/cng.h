/*
 * cng.h
 *
 *  Created on: 2024. 12. 12.
 *      Author: SEONGPIL
 */

#ifndef CNG_H_
#define CNG_H_

#include "sys.h"

typedef struct cngInst_s {

    // 글로벌 필터 상태 변수 정의
    float state_low[3];
    float state_peak[3];
    float state_lp[3];

} cngInst_t;

cngInst_t* sysCNGCreate();
void CNG_Init(cngInst_t *cngInst);
void CNG_DeInit(cngInst_t *cngInst);

void CNG_process(cngInst_t *cngInst, int16_t *signal, int length, float noiseGainDbfs);


#endif /* CNG_H_ */