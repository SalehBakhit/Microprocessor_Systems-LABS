/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "lis3dsh.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"


LIS3DSH_InitTypeDef Acc_instance;
LIS3DSH_DRYInterruptConfigTypeDef IT_instance;
/* Private variables ---------------------------------------------------------*/
//--- BUFFER CONTROL ---
#define N 5	//size of filter buffer
float pCoeffs[] = {0.25, 0.5, 0.25, -0.2, 0.175};	//{b0, b1, b2, a1, a2}
int cnt = 0;	//take care of first N-1 readings
float dataX[N], dataY[N], dataZ[N];	//unfiltered(but calibrated) acc readings
float resX[N], resY[N], resZ[N];	//filted(and calibrated) acc readings

//--- CALCULATION CONTROL ---
#define M_PI acos(-1.0)
#define TO_DEG (180/M_PI)
float x, y, z;	//calibrated x, y and z acc values
float pitch, roll;	//actual roll and pitch values
float Buffer[3];	//uncalibrated readings of ACCx, ACCy, ACCz

//--- CALIBRATION CONSTANTS ---
#define ACC11 1.00496598e-03
#define ACC12 -1.76592184e-05 
#define ACC13 1.08907334e-05 
#define ACC10 -8.32411647e-03

#define ACC21 -1.35474347e-05
#define ACC22 9.70302615e-04
#define ACC23 -1.15849207e-05
#define ACC20 -8.22144747e-03

#define ACC31 -4.15446229e-05
#define ACC32 3.76624230e-05
#define ACC33 9.44707775e-04
#define ACC30 -3.76561880e-02

//--- KEYPAD CONTROL ---
const __IO uint32_t DELAY = 100;	//delay to keypad press to deal with debouncing
int count = 0;	//keeps track of long presses(how many consecutive presses)
int pressed = 0;	//a key is being held
int wait = 0;	//a key has been pressed 0->no key detected	1->key press detected
char key, prevKey;	//current key and previous key

//--- 7-SEGMENT CONTROL ---
int D[4];		//stores numbers to be displayed
int dataReady = 0;	//ACC data is ready
int display_falg = 0;	//0-display roll	1-display pitch
int sleep_flag = 0;	//0- display on	1- display off 

//--- GAME CONTROL ---
char user_roll[4], user_pitch[4];	//user entered values


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void initializeACC(void);
void initializeIT(void);
int sleep(void);
char scan_col_1(uint16_t GPIO_Pin_Col);
char scan_col_2(uint16_t GPIO_Pin_Col);
char scan_col_3(uint16_t GPIO_Pin_Col);
void scan_press(void);
void calculate_angles(void);
void IIR_C(float *InputArray, float *OutputArray, float *coeff, int length);
void numControl(int num, int D);
void FloatToIntArr(float num);
void start_game(void);
void pwm_update(void);


int main(void) {
  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
	
  /* USER CODE INIT BEGIN */
	initializeACC();	//initialize accelerometer
	initializeIT();	//initialize data ready interrupt
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 1);
	
	//start TIM4 and PWM for all four channels(LEDs)
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);	//green LED
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);	//orange LED
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);	//red LED
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);	//blue LED
  /* USER CODE INIT END */
	
	//start game and print target angles
	start_game();	//waits untill user inputs desired roll and pitch
	printf("desired roll: %d\n", atoi(user_roll));
	printf("desired pitch: %d\n", atoi(user_pitch));
	
	prevKey = key;	//keep track of last pressed key vs. current
	
  while (1) {
		//--- update angles and brightness of LEDs whenever dataReady flag is set
		if(dataReady) {
			dataReady = 0;
			calculate_angles();
			pwm_update();
		}
		
		//--- OPERATION MODE
		/*two parts: 1- which angle to dispaly
								 2- if a key is pressed for > 2s, trigger appropriate response */
		//PART I
		scan_press();
		if(prevKey != key) {	//if a dirrent key(command) is pressed
			prevKey = key;
			if(key == '1') {	//disply roll
				display_falg = 0;
			}
			else if(key == '2') {	//display pitch
				display_falg = 1;
			}
			count = 0;	//reset since not same key
		}
		
		//PART 2
		if(!pressed) {	//if a key that was in hold mode is let go
			if(count > 30) {	//30 consecutive pressed = around 4 seconds
				if(key == '*') {	//sleep mode
					sleep_flag = sleep();
				} else if(key == '#') {	//wake up
					sleep_flag = 0;
				}
			}
			else if(count > 15) {	//15 consecutive pressed = around 2 seconds
				if(key == '*') {	//restart game
					sleep_flag = sleep();
					start_game();
				}
			}
			count = 0;	//reset since no key is held anymore
		}
		
		//--- 7-SEGMENT DISPLAY
		//
		if(display_falg) {	//1->pitch
			FloatToIntArr(pitch);
		} else {	//0->roll
			FloatToIntArr(roll);
		}
		if(!sleep_flag) {	//if falg not set
			for(int i = 0; i < 4; i++) {	//Display roll or pitch
				numControl(D[i], i);
				HAL_Delay(1);
			}
		}
  }

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == GPIO_PIN_0) {
		dataReady = 1;	//set dataReady flag
	}
}

/* HELPER FUNCTIONS */
//To start/restart the game (takes care of user part)
void start_game(void) {
	//clear arrays
	for(int i = 0; i < 3; i++) {
		user_roll[i] = NULL;
		user_pitch[i] = NULL;
	}
	int index = 0;
	
	//step 1: Enter desired roll angle
	printf("Enter desired roll angle\n");
	while(key != '#') {	//wait for enter key
		while(!wait) {	//wait for a key to be pressed
			scan_press();
		}
		if(wait) {
			user_roll[index] = key;
			index++;
			if(key == '*') {	//delete (re-write at same position next time)
				index = index-2;
			}
			wait = 0;
		}
	}
	key = NULL;
	index = 0;
	
	//step 2: Enter desired pitch angle
	printf("Enter desired pitch angle\n");
	while(key != '#') {	//wait for enter key
		while(!wait) {	//wait for a key to be pressed
			scan_press();
		}
		if(wait) {
			user_pitch[index] = key;
			index++;
			if(key == '*') {	//delete (re-write at same position next time)
				index = index-2;
			}
			wait = 0;
		}
	}
	key = NULL;
	
	//operation mode in main
	sleep_flag = 0;	//wake up display
}

//turn off 7-segment display
int sleep(void) {
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
	return 1;
}

//IIR 2nd order
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

//--- KEYPAD ---
/*
---PIN CONFIGURATION---
GPIO_PIN_13 (PB13) <- C1
GPIO_PIN_14 (PB14) <- C2
GPIO_PIN_15 (PB15) <- C3

GPIO_PIN_0 (PD0) <- R1
GPIO_PIN_1 (PD1) <- R2
GPIO_PIN_2 (PD2) <- R3
GPIO_PIN_3 (PD3) <- R4
*/
void scan_press() {
	//reset rows
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);

	if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == 0) {	//C1 pressed
		HAL_Delay(DELAY);	//debouncing delay
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == 0) {	//if still detected, proceed
			key = scan_col_1(GPIO_PIN_13);	//scan for the corresponding row
			pressed = 1;
			wait = 1;
		}
	}
	
	else if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == 0) {	//C2 pressed
		HAL_Delay(DELAY);
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == 0) {	//if still detected, proceed
			key = scan_col_2(GPIO_PIN_14);
			pressed = 1;
			wait = 1;
		}
	}

	else if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == 0) {	//C3 pressed
		HAL_Delay(DELAY);
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == 0) {	//if still detected, proceed
			key = scan_col_3(GPIO_PIN_15);
			pressed = 1;
			wait = 1;
		}
	}
	else {
		pressed = 0;
	}
}

char scan_col_1(uint16_t GPIO_Pin_Col) {
	for(int i = 0; i < 4; i++) {
		if(i == 0) {	//set R1 and check Column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R1&C1
				printf("1\n");
				count++;
				return '1';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
		}
		else if(i == 1) {	//set R2 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R2&C1
				printf("4\n");
				count++;
				return '4';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
		}
		else if(i == 2) {	//set R3 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R3&C1
				printf("7\n");
				count++;
				return '7';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
		}
		else if(i == 3) {	//set R4 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R4&C1
				printf("*\n");
				count++;
				return '*';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
		}
	}
	return '\n';
}

char scan_col_2(uint16_t GPIO_Pin_Col) {
	for(int i = 0; i < 4; i++) {
		if(i == 0) {	//set R1 and check Column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R1&C2
				printf("2\n");
				count++;
				return '2';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
		}
		else if(i == 1) {	//set R2 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R2&C2
				printf("5\n");
				count++;
				return '5';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
		}
		else if(i == 2) {	//set R3 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R3&C2
				printf("8\n");
				count++;
				return '8';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
		}
		else if(i == 3) {	//set R4 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R4&C2
				printf("0\n");
				count++;
				return '0';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
		}
	}
	return '\n';
}

char scan_col_3(uint16_t GPIO_Pin_Col) {
	for(int i = 0; i < 4; i++) {
		if(i == 0) {	//set R1 and check Column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R1&C3
				printf("3\n");
				count++;
				return '3';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
		}
		else if(i == 1) {	//set R2 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R2&C3
				printf("6\n");
				count++;
				return '6';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
		}
		else if(i == 2) {	//set R3 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R3&C3
				printf("9\n");
				count++;
				break;
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
		}
		else if(i == 3) {	//set R4 and check column
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
			if(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin_Col) == 1) {	//R4&C3
				printf("#\n");
				count++;
				return '#';
			}
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
		}
	}
	return '\n';
}

	//--- 7-SEGMENT DISPLAY ---
/*
---PIN CONFIGURATION---
GPIO_PIN_11 (PD11) <- D4 SET
GPIO_PIN_10 (PD10) <- D3 SET
GPIO_PIN_9 (PD9) <- D2  SET
GPIO_PIN_8 (PD8) <- D1 SET

GPIO_PIN_0 (PD0) <- A (PE14)
GPIO_PIN_1 (PD1) <- B (PE13)
GPIO_PIN_2 (PD2) <- C (PE12)
GPIO_PIN_3 (PD3) <- D (PE11)
GPIO_PIN_4 (PD4) <- E (PE10)
GPIO_PIN_5 (PD5) <- F (PE9)
GPIO_PIN_6 (PD6) <- G (PE8)
GPIO_PIN_7 (PD7) <- DP (PE7)
*/
/* Sets a digit (of the 7-segment display),
	 and the corresponding segments */
void numControl(int num, int D) {
	/* Truth table of numbers 0-9
			NUM		A	B	C	D	E	F	G

				0		1	1	1	1	1	1	0

				1		0	1	1	0	0	0	0

				2		1	1	0	1	1	0	1

				3		1	1	1	1	0	0	1

				4		0	1	1	0	0	1	1

				5		1	0	1	1	0	1	1

				6		1	0	1	1	1	1	1

				7		1	1	1	0	0	0	0

				8		1	1	1	1	1	1	1

				9		1	1	1	1	0	1	1
	*/
	
	/*takes care of setting/resetting Ds
	  decimal point is set with D2 */
	if(D == 0) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
	}
	else if(D == 1) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
	}
	else if(D == 2) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
	}
	else if(D == 3) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
	}
	
	//takes care of displaying the required number (according to the truth table)
	if(num == 0) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
	} else if(num == 1) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
	} else if(num == 2) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 3) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 4) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 5) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 6) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 7) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
	} else if(num == 8) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	} else if(num == 9) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
	}
	
}

/* converts from float to int[]
	 XXX.Y
	 D[0] <- num in D1
	 D[1] <- num in D2
	 D[2] <- num in D3
	 D[3] <- num in D4*/
void FloatToIntArr(float num) {
	D[0] = (int)(num/100);
	D[1] = (int)(num/10) - D[0]*10;
	D[2] = (int)(num) - D[1]*10 - D[0]*100;
	D[3] = (int)(num*10) - D[2]*10 - D[1]*100 - D[0]*1000;
}

void calculate_angles(void) {
	LIS3DSH_ReadACC(&Buffer[0]);

	//Calibrated x, y, z
	x = ACC11*Buffer[1] + ACC12*Buffer[0] + ACC13*Buffer[2] + ACC10;
	y = ACC21*Buffer[1] + ACC22*Buffer[0] + ACC23*Buffer[2] + ACC20;
	z = ACC31*Buffer[1] + ACC32*Buffer[0] + ACC33*Buffer[2] + ACC30;
	
	//initializing buffers for with first n samples
	if(cnt< N) {
		dataX[cnt] = x;
		dataY[cnt] = y;
		dataZ[cnt] = z;
	}
	//updating buffers everythime
	else {
		for(int i=0;i<N-1;i++) {
			dataX[i] = dataX[i+1];
			dataY[i] = dataY[i+1];
			dataZ[i] = dataZ[i+1];						
		}
		dataX[N-1] = x;
		dataY[N-1] = y;
		dataZ[N-1] = z;
		//filtering and passing output to buffers resx, resy and resz
		IIR_C(dataX, resX, pCoeffs, N);
		IIR_C(dataY, resY, pCoeffs, N);
		IIR_C(dataZ, resZ, pCoeffs, N);
	}
	
	pitch = atan(resX[N-1]/sqrt(resY[N-1]*resY[N-1] + resZ[N-1]*resZ[N-1]))*TO_DEG;
	roll = atan(resY[N-1]/sqrt(resX[N-1]*resX[N-1] + resZ[N-1]*resZ[N-1]))*TO_DEG;
	if(pitch != 0) {
		pitch += 90;
	}
	if(roll != 0) {
		roll += 90;
	}
	
	cnt++;
}

/* HELPER FUNCTIONS END */

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

void pwm_update(void) {
	//getting user roll and pitch
	int userRoll = atoi(user_roll);
	int userPitch = atoi(user_pitch);
	//difference between desired and actual angles
	int duty_roll = (abs((int)roll-userRoll));
	int duty_pitch = (abs((int)pitch-userPitch));

	//turn off led's whenever difference is less than 5
	if((abs((int)pitch-userPitch)) <= 5) duty_pitch = 0;
	
	if((abs((int)roll-userRoll)) <= 5) duty_roll = 0;
	
	//update duty cycle
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_1,duty_roll);
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_2,duty_roll);
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3,duty_pitch);
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_4,duty_pitch);
}

void initializeACC(void){
	
	Acc_instance.Axes_Enable = LIS3DSH_XYZ_ENABLE;
	Acc_instance.AA_Filter_BW = LIS3DSH_AA_BW_200;//at least double data rate
	Acc_instance.Full_Scale = LIS3DSH_FULLSCALE_2;
	Acc_instance.Power_Mode_Output_DataRate = LIS3DSH_DATARATE_50;
	Acc_instance.Self_Test = LIS3DSH_SELFTEST_NORMAL;
	Acc_instance.Continous_Update = LIS3DSH_ContinousUpdate_Enabled;
	
	LIS3DSH_Init(&Acc_instance);
}

void initializeIT(void) {
	IT_instance.Dataready_Interrupt = LIS3DSH_DATA_READY_INTERRUPT_ENABLED;
	IT_instance.Interrupt_signal = LIS3DSH_ACTIVE_HIGH_INTERRUPT_SIGNAL;
	IT_instance.Interrupt_type = LIS3DSH_INTERRUPT_REQUEST_PULSED;
	
	LIS3DSH_DataReadyInterruptConfig(&IT_instance);
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
