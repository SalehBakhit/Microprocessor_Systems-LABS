/*
 * ECSE 426 - uP
 * G21 - Saleh Bakhit(260632353) + Omar Ellakany(260603639)
 * LAB 1 - biquad IIR filter (Assembly, C, and CMSIS)
 * func: y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
 * All three functions implement a second order filter as mentioned in discution board
*/

#include <stdio.h>
#include "arm_math.h"

void IIR_C(float *InputArray, float *OutputArray, float *coeff, int length);
void IIR_CMSIS(float *InputArray, float *OutputArray, float *pCoeffs, int length);
void IIR_Cstate(float *pIn, float *pOut, float *pCoeffs, int length);

extern void IIR_asm(float *pIn, float *pOut, float *pCoeffs, int length);

/*This method represens a test workbench
 *It calls all three functions and generate output array(init it to 0 every time)
*/
int main() {
	float pIn_1[] = {0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 16, 4, 18, 6, 10,
							10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 19, 12, 10, 10, 10, 10, 10, 0, 0};
	float pIn_2[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	int length_1 = sizeof(pIn_1)/sizeof(pIn_1[0]);
	int length_2 = sizeof(pIn_2)/sizeof(pIn_2[0]);
	float pOut_1[length_1];
	float pOut_2[length_2];
	float pCoeffs[] = {0.2, 0.4, 0.2, 0.3, -0.2};	//{b0, b1, b2, a1, a2}
	
	//pIn_1
	printf("X1\n");
	//Assembly implementation
	printf("\nIIR_asm\n");
	IIR_asm(pIn_1, pOut_1, pCoeffs, length_1);
	for(int i = 0; i < length_1; i++) {	//print output array
		printf("input %d \t%f\toutput %f\n", i, *(pIn_1+i), *(pOut_1+i));
	}
	for(int i = 0; i < length_1; i++) {	//init output array to 0 (for testing purposes)
		*(pOut_1+i) = 0;
	}
	
	//CMSIS implementation
	printf("\nIIR_CMSIS\n");
	IIR_CMSIS(pIn_1, pOut_1, pCoeffs, length_1);
	for(int i = 0; i < length_1; i++) {
		printf("input %d \t%f\toutput %f\n", i, *(pIn_1+i), *(pOut_1+i));
	}
	for(int i = 0; i < length_1; i++) {	//init output array to 0 (for testing purposes)
		*(pOut_1+i) = 0;
	}
	
	//C implementation(2 implementations)
	printf("\nIIR_C\n");
	IIR_C(pIn_1, pOut_1, pCoeffs, length_1);
	
	//IIR_Cstate(pIn_1, pOut_1, pCoeffs, length_1);	//with state implementaion
	
	for(int i = 0; i < length_1; i++) {
		printf("input %d \t%f\toutput %f\n", i, *(pIn_1+i), *(pOut_1+i));
	}
	
	//pIn_2
	printf("\nX2\n");
	//Assembly implementation
	printf("\nIIR_asm\n");
	IIR_asm(pIn_2, pOut_2, pCoeffs, length_2);
	for(int i = 0; i < length_2; i++) {	//print output array
		printf("input %d \t%f\toutput %f\n", i, *(pIn_2+i), *(pOut_2+i));
	}
	for(int i = 0; i < length_2; i++) {	//init output array to 0 (for testing purposes)
		*(pOut_2+i) = 0;
	}
	
	//CMSIS implementation
	printf("\nIIR_CMSIS\n");
	IIR_CMSIS(pIn_2, pOut_2, pCoeffs, length_2);
	for(int i = 0; i < length_2; i++) {
		printf("input %d \t%f\toutput %f\n", i, *(pIn_2+i), *(pOut_2+i));
	}
	for(int i = 0; i < length_2; i++) {	//init output array to 0 (for testing purposes)
		*(pOut_2+i) = 0;
	}
	
	//C implementation(2 implementations)
	printf("\nIIR_C\n");
	IIR_C(pIn_2, pOut_2, pCoeffs, length_2);
	
	//IIR_Cstate(pIn_2, pOut_2, pCoeffs, length_2);	//with state implementaion
	
	for(int i = 0; i < length_2; i++) {
		printf("input %d \t%f\toutput %f\n", i, *(pIn_2+i), *(pOut_2+i));
	}

	return 0;
}

//fills the array of outputs corresonding to order 2 filter
void IIR_C(float *InputArray, float *OutputArray, float *coeff, int length) {
	for(int n = 0; n < length; n++) {
		//y[0] = b0*x[0]
		if(n == 0) {
			*OutputArray = (*coeff)*(*InputArray);
		}
		//y[1] = b0*x[1] + b1x[0] + a1y[0]
		else if(n == 1) {
			*(OutputArray+n) = (*coeff)* *(InputArray+n) + *(coeff+1)* *(InputArray+n-1) + *(coeff+3)* *(OutputArray+n-1);
		}
		else {
			*(OutputArray+n) = (*coeff)* *(InputArray+n) + *(coeff+1)* *(InputArray+n-1) + *(coeff+2)* *(InputArray+n-2)
												+ *(coeff+3)* *(OutputArray+n-1) + *(coeff+4)* *(OutputArray+n-2);
		}
	}
}

void IIR_Cstate(float *pIn, float *pOut, float *pCoeffs, int length) {	//same as before but makes use of states
	//initilize pState[] = {x[n-1], x[n-2], y[n-1], y[n-2]}
	float pState[] = {0, 0, 0, 0};
	
	for(int n = 0; n < length; n++) {
		*(pOut+n) = *(pIn+n) * *(pCoeffs) + *(pState) * *(pCoeffs+1) + *(pState+1) * *(pCoeffs+2)
							+ *(pState+2) * *(pCoeffs+3) + *(pState+3) * *(pCoeffs+4);
		
		//update states
		pState[1] = pState[0];
		pState[3] = pState[2];
		pState[0] = *(pIn+n);
		pState[2] = *(pOut+n);
	}
}

void IIR_CMSIS(float *InputArray, float *OutputArray, float *pCoeffs, int length) {
	float pState[] = {0, 0, 0, 0};	//init states to 0

	arm_biquad_casd_df1_inst_f32 S1 = {1, pState, pCoeffs};	//create inst S1 of the filter with 1 stage(2nd order)
	arm_biquad_cascade_df1_f32(&S1, InputArray, OutputArray, length);	//filter output
	
}
