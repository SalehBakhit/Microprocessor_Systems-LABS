/*******************************************************************************
  * @file    main.c
  * @author  Saleh Bakhit && Omar Ellakany (G21)
	* @version V2.0
  * @date    17-Nov-2017
  * @brief   This file demonstrates LAB 4
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "tim.h"
#include "gpio.h"
#include "lis3dsh.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"

/* Variables ---------------------------------------------------------*/
LIS3DSH_InitTypeDef Acc_instance;
LIS3DSH_DRYInterruptConfigTypeDef IT_instance;

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
int pressed = 0;	//0->no key detected	1->key press detected
char key;	//last pressed key

//--- 7-SEGMENT CONTROL ---
int D[4];		//stores numbers to be displayed
int dataReady = 0;	//ACC data is ready
int display_flag = 2;	//0-roll	||	1-pitch	||	2-user_roll	||	3-user_pitch
int sleep_flag = 0;	//0-neutral state(ON)	||	1-sleep state	||	2-wake up state

//--- GAME CONTROL ---
char user_roll[4], user_pitch[4];	//user entered values
int game_flag = 1;	//0->operation mode		||	1->start/restart game

/* Function prototypes -----------------------------------------------*/
//--- CONFIURATION & INITILIZATION ---
void SystemClock_Config(void);
void MX_Init(void);
void initializeACC(void);
void initializeIT(void);

//--- HELPER FUNCTIONS ---
void sleep(void);
void wake(void);
char scan_col_1(uint16_t GPIO_Pin_Col);
char scan_col_2(uint16_t GPIO_Pin_Col);
char scan_col_3(uint16_t GPIO_Pin_Col);
void scan_press(void);
void calculate_angles(void);
void IIR_C(float *InputArray, float *OutputArray, float *coeff, int length);
void numControl(int num, int D);
void FloatToIntArr(float num);
void IntToIntArr(int num);
void pwm_update(void);

/* Thread Function Management -----------------------------------------------*/
//--- THREAD FUNCTIONS ---
void keypad_handler(void const * argument);
void display_handler(void const * argument);
void ACC_handler(void const * argument);
void flag_handler(void const * argument);
void start_game(void const * argument);

//--- THREAD IDs ---
osThreadDef(keypad_handler, osPriorityNormal, 128, 0);
osThreadDef(display_handler, osPriorityAboveNormal, 128, 0);
osThreadDef(ACC_handler, osPriorityNormal, 128, 0);
osThreadDef(flag_handler, osPriorityNormal, 1, 0);
osThreadDef(start_game, osPriorityNormal, 128, 0);

osThreadId startHandle;
osThreadId keypadHandle;
osThreadId displayHandle;
osThreadId ACCHandle;
osThreadId flagHandle;

//--- SEMAPHORES ---
osSemaphoreId calcSem;

osSemaphoreDef(calcSem);


/**
	These lines are mandatory to make CMSIS-RTOS RTX work with te new Cube HAL
*/
#ifdef RTE_CMSIS_RTOS_RTX
extern uint32_t os_time;

uint32_t HAL_GetTick(void) { 
  return os_time; 
}
#endif


int main(void) {
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	
	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_GPIO_DISPLAY_Init();
	MX_TIM4_Init();
	
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
	
	/* Call init function for threads */
	MX_Init();
	
	/* Start scheduler */
	if (osKernelStart() != osOK)  {        // check osStatus for other possible valid values
		printf("error starting\n");
	}

}

/* HELPER FUNCTIONS */
//--- THREADS ---

//initilize threads
void MX_Init(void) {
	//create flag thread
	flagHandle = osThreadCreate(osThread(flag_handler), NULL);
	
	//keypad thread --> created in start_game()
	
	//start_game thread --> created in flag_handler thread
	
	//create display thread
	displayHandle = osThreadCreate(osThread(display_handler), NULL);
	
	//create ACC thread
	ACCHandle = osThreadCreate(osThread(ACC_handler), NULL);
	
	//create calculation semaphore
	calcSem = osSemaphoreCreate(osSemaphore(calcSem), 1);
}

//handles keypad in operation mode
void keypad_handler(void const * argument) {
	while(1) {
		scan_press();
		//--- OPERATION MODE
		/*two parts: 1- which value to dispaly
								 2- if a key is pressed for > 2s, trigger appropriate response */
		
		if(pressed) {	//a key has been detected
			//PART I
			if(key == '1') {	//disply roll
				display_flag = 0;
			}
			else if(key == '2') {	//display pitch
				display_flag = 1;
			}
			else if(key == '#') {	//display next
				if(display_flag == 3) {	//was displaying user_pitch
					display_flag = 0;	//display roll next
				}
				else {
					display_flag++;
				}
			}
			
			pressed = 0;	//reset flag
		}
		else {	//no key pressed
			//PART 2
			if(count > 30) {	//30 consecutive presses = around 4 seconds
				if(key == '*') {	//sleep mode
					sleep_flag = 1;
				} else if(key == '#') {	//wake up
					sleep_flag = 2;
				}
				count = 0;
			}
			else if(count > 15) {	//15 consecutive presses = around 2 seconds
				if(key == '*') {	//restart game
					game_flag = 1;
				}
				count = 0;
			}
			count = 0;
		}
		
	}
	
}

//handles display at all times
void display_handler(void const * argument) {
	while(1) {
		//capture calcSem
		osSemaphoreWait(calcSem, 1);
		
		if(display_flag == 0) {	//display roll
			FloatToIntArr(roll);
		}
		else if(display_flag == 1) {	//display pitch
			FloatToIntArr(pitch);
		}
		else if(display_flag == 2) {	//display user_roll
			IntToIntArr(atoi(user_roll));
		}
		else if(display_flag == 3) {	//display user_pitch
			IntToIntArr(atoi(user_pitch));
		}
		
		//release
		osSemaphoreRelease(calcSem);
		
		for(int i = 0; i < 4; i++) {	//Display desired value
			numControl(D[i], i);
			osDelay(1);
		}
	}
	
}

//handles ACC && PWM
void ACC_handler(void const * argument) {
	while(1) {
		if(dataReady) {	//update angles and duty cycles
			//capture calcSem
			osSemaphoreWait(calcSem, osWaitForever);
			
			
			calculate_angles();
			pwm_update();
			
			dataReady = 0;	//reset flag
			
			//release
			osSemaphoreRelease(calcSem);
		}
	}
	
}

//check game/sleep flags
void flag_handler(void const * argument) {
	while(1) {
		if(game_flag) {
			printf("start game\n");
			osThreadTerminate(keypadHandle);
			startHandle = osThreadCreate(osThread(start_game), NULL);
			
			game_flag = 0;	//reset flag
		}
		
		//kill display if flag is set
		if(sleep_flag) {
			//turn off GPIO pins and disable clk
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
			
			__GPIOC_CLK_DISABLE();
			__GPIOE_CLK_DISABLE();
	
			//de-init TIM4 interrupt and pwm GPIO pins and turn off
			HAL_TIM_PWM_MspDeInit(&htim4);
			
			//terminate display_handler && ACC_handler
			osThreadTerminate(displayHandle);
			osThreadTerminate(ACCHandle);
			
			sleep_flag = 0;	//reset flag to neutral state
		}
		else if(sleep_flag == 2) {
			wake();
			//start display_handler && ACC_handler
			displayHandle = osThreadCreate(osThread(display_handler), NULL);
			ACCHandle = osThreadCreate(osThread(ACC_handler), NULL);
			
			sleep_flag = 0;	//reset flag to neutral state
		}
	}
	
}

//wake up the display/LEDs
void wake(void) {
	//init 7-segment GPIO pins
	MX_GPIO_DISPLAY_Init();
			
	//init TIM4 interrupt and pwm GPIO pins and turn off
	HAL_TIM_PWM_MspInit(&htim4);
}

//To start/restart the game (takes care of user part)
void start_game(void const * argument) {
	//clear arrays
	for(int i = 0; i < 3; i++) {
		user_roll[i] = NULL;
		user_pitch[i] = NULL;
	}
	int index = 0;
	key = NULL;
	display_flag = 2;	//display user_roll
	
	//step 1: Enter desired roll angle
	printf("Enter desired roll angle\n");
	while(key != '#') {	//wait for "enter" key
		while(!pressed) {
			scan_press();
		}
		if(pressed && key != '#') {	//a key has been pressed
			if(key == '*') {	//delete (re-write at same position next time)
				if(index != 0) {
					index--;
					user_roll[index] = NULL;
				}
			}
			else {
				if(index < 3) {
					user_roll[index] = key;
					index++;
				}
			}
			pressed = 0;	//reset pressed flag
		}
	}
	display_flag++;	//display user_pitch
	printf("user_roll is %s\n", user_roll);
	key = NULL;
	index = 0;
	pressed = 0;
	
	//step 2: Enter desired pitch angle
	printf("Enter desired pitch angle\n");
	while(key != '#') {	//wait for enter key
		while(!pressed) {
			scan_press();
		}
		if(pressed && key != '#') {	//a key has been pressed
			if(key == '*') {	//delete (re-write at same position next time)
				if(index!=0) {
					index--;
					user_pitch[index] = NULL;
				}
			}
			else {
				if(index<3) {
					user_pitch[index] = key;
					index++;
				}
			}
			pressed = 0;	//reset pressed flag
		}
	}
	printf("user_pitch is %s\n", user_pitch);
	
	//reset flags/key
	key = NULL;
	pressed = 0;
	display_flag = 0;
	
	//start keypad_handelr
	keypadHandle = osThreadCreate(osThread(keypad_handler), NULL);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == GPIO_PIN_0) {
		dataReady = 1;	//set dataReady flag
	}
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
		}
	}
	
	else if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == 0) {	//C2 pressed
		HAL_Delay(DELAY);
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == 0) {	//if still detected, proceed
			key = scan_col_2(GPIO_PIN_14);
			pressed = 1;
		}
	}

	else if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == 0) {	//C3 pressed
		HAL_Delay(DELAY);
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == 0) {	//if still detected, proceed
			key = scan_col_3(GPIO_PIN_15);
			pressed = 1;
		}
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
				return '9';
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
GPIO_PIN_11 (PD11)	OLD <- D4 SET (PC9)	CURRENT
GPIO_PIN_10 (PD10)	OLD <- D3 SET (PC8)	CURRENT
GPIO_PIN_9  (PD9) 	OLD <- D2 SET	(PC6)	CURRENT
GPIO_PIN_8  (PD8)		OLD <- D1 SET (PC5)	CURRENT

GPIO_PIN_0 (PD0)	OLD <- A  (PE14)	CURRENT
GPIO_PIN_1 (PD1)	OLD <- B  (PE13)	CURRENT
GPIO_PIN_2 (PD2)	OLD <- C  (PE12)	CURRENT
GPIO_PIN_3 (PD3)	OLD <- D  (PE11)	CURRENT
GPIO_PIN_4 (PD4)	OLD <- E  (PE10)	CURRENT
GPIO_PIN_5 (PD5)	OLD <- F  (PE9)		CURRENT
GPIO_PIN_6 (PD6)	OLD <- G  (PE8)		CURRENT
GPIO_PIN_7 (PD7)	OLD <- DP (PE7)		CURRENT
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
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
	}
	else if(D == 1) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
	}
	else if(D == 2) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
	}
	else if(D == 3) {
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
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

/* converts from float to int[]
	 XXX
	 D[0] <- num in D1
	 D[1] <- num in D2
	 D[2] <- num in D3
	 D[3] <- num in D4*/
void IntToIntArr(int num) {
	D[0] = num/100;
	D[1] = num/10- D[0]*10;
	D[2] = num - D[1]*10 - D[0]*100;
	D[3] = 0;
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

/* HELPER FUNCTIONS END */

/**
  * System Clock Configuration
  */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the
     device is clocked below the maximum system frequency (see datasheet). */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
