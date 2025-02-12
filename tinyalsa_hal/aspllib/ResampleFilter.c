
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include "Resample.h"


double pLowPassFilter[LP_PASS_TAPS] = { -0.0131602469827335, 0.0543226456344256, -0.124378805415714, 0.195316119533469, 0.774358143658792, 0.195316119533469, -0.124378805415714, 0.0543226456344256, -0.0131602469827335 };
double pLowPassFilter3[LP_PASS_TAPS3] = { 0.00485388605797101,	0.0142668778964594,	0.000979102332543584, - 0.0539697665172142, - 0.0766382179224417,	0.0539234344426129,	0.300858879523048,	0.431527212114720,	0.300858879523048,	0.0539234344426129, - 0.0766382179224417, - 0.0539697665172142,	0.000979102332543584,	0.0142668778964594,	0.00485388605797101 };
double pLowPassFilter6[LP_PASS_TAPS6] = { 0.000619955265981611,	0.00200323648535191,	0.00402311731128261,	0.00547185405430258,	0.00417731077462859, -0.00209404107585827, -0.0139364365919728, -0.0286005239545985, -0.0394769851483141, -0.0376853759719786, -0.0156742472331338,	0.0285468625293750,	0.0886989075119932,	0.150863309501073,	0.197780159979498,	0.215247152306662,	0.197780159979498,	0.150863309501073,	0.0886989075119932,	0.0285468625293750, -0.0156742472331338,-0.0376853759719786, -0.0394769851483141, -0.0286005239545985, -0.0139364365919728, -0.00209404107585827,	0.00417731077462859,	0.00547185405430258,	0.00402311731128261,	0.00200323648535191,	0.000619955265981611 };

int CreateResampler(UpdamplerContext* pContext, unsigned short MaxBufferSz, unsigned short FilterTaps, double* pFIlter)
{
	int i;

	pContext->MaxBufferSz = MaxBufferSz;
    pContext->FilterOrder = FilterTaps;

	for(i=0;i<FilterTaps;i++)
		pContext->FilterWindow[i]=0.0;

	
	for(i=0;i<FilterTaps;i++)
			pContext->FilterCoef[i]=pFIlter[i];
	
    return 1;
}

void DoUpsample(UpdamplerContext* pContext, short* pIn, short* pOut,unsigned short Sz,unsigned int Scale,double MultVal)
{
    int i, j;
    double FilterSum;
    double FilterMult;

    for (i = 0; i < (Sz *(int) Scale); i++)
    {
        // Get i'th Input
        if ((i % Scale) == 0)
            pContext->FilterWindow[pContext->FilterOrder - 1] = (double)(pIn[i / Scale]) / 32768.0;
        else
            pContext->FilterWindow[pContext->FilterOrder - 1] = 0.0;

        // Filter
        FilterSum = 0.0;
        for (j = 0; j < pContext->FilterOrder; j++)
        {
            FilterMult = pContext->FilterWindow[j] * pContext->FilterCoef[j];
            FilterSum += FilterMult;
        }
        // Set Filtered Output        
        pOut[i] = (short)(FilterSum * 32768.0 * MultVal);

        // Slide
        for(j=0;j<pContext->FilterOrder-1;j++)
			pContext->FilterWindow[j]=pContext->FilterWindow[j+1];
//        memcpy(&pContext->FilterWindow[0], &pContext->FilterWindow[1], (pContext->FilterOrder - 1) * sizeof(double));
    }
}

void DoDnsample(UpdamplerContext* pContext, short* pIn, short* pOut, unsigned short Sz,unsigned int Scale, double MultVal)
{
    int i, j, k;
    double FilterSum;
    double FilterMult;

    k = 0;

//		printf(" ASPL DN sample order %d  %f %f",pContext->FilterOrder,pContext->FilterCoef[0],pContext->FilterCoef[pContext->FilterOrder-1]);
//		printf(" ASPL DN sample order %d  %f %f",pContext->FilterOrder,pContext->FilterWindow[0],pContext->FilterWindow[pContext->FilterOrder-1]);

	
    for (i = 0; i < Sz; i++)
    {
        // Get i'th Input
        pContext->FilterWindow[pContext->FilterOrder - 1] = (double)(pIn[i])/32768.;
        // Filter
        FilterSum = 0.0;
        for (j = 0; j < pContext->FilterOrder; j++)
        {
            FilterMult = pContext->FilterWindow[j] * pContext->FilterCoef[j];
            FilterSum += FilterMult;
        }

        // Set Filtered Output        
        if ((i % Scale) == 0)
            pOut[k++] = (short)(FilterSum* 32768. * MultVal);

        // Slide
		for(j=0;j<pContext->FilterOrder-1;j++)
			pContext->FilterWindow[j]=pContext->FilterWindow[j+1];
		
    }
}

void DestroyResampler(UpdamplerContext* pContext)
{
//    if (pContext->FilterCoef != NULL) pContext->FilterCoef = NULL;
    pContext->CurrentBufferSz = 0;
    pContext->FilterOrder = 0;
    pContext->MaxBufferSz = 0;
}
