/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include "usbd_cdc_if.h"
#include<stdio.h>
#include<stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc5;

DAC_HandleTypeDef hdac1;
DAC_HandleTypeDef hdac2;
DAC_HandleTypeDef hdac3;

OPAMP_HandleTypeDef hopamp1;
OPAMP_HandleTypeDef hopamp4;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
//1 -> int between 1 and 8191
uint16_t xRes = 200;
//2 -> int between 1 and 8191
uint16_t yRes = 200;
//3 -> int between 1 and 8191
uint16_t skipSteps = 1;

//4 -> int between 0 and 8191
uint16_t xOffset = 4095;
//5 -> int between 0 and 8191
uint16_t yOffset = 4095;
//8 -> int between 1 and 20
uint8_t adcAvg = 8;

//7 -> int between 0 and 2500
uint16_t laserPower = 2200;

//image data buffer -> set a little under the USB CDC 512 limit
static const uint16_t lenBuffer = 500;
static unsigned char imageData[500]; //apparently can't use lenBuffer

int pgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;

int scanOffsetX = 0;
int scanOffsetY = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC5_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_OPAMP4_Init(void);
static void MX_TIM1_Init(void);
static void MX_DAC1_Init(void);
static void MX_DAC3_Init(void);
static void MX_DAC2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//microsecond delay
void delay(uint32_t us) {
	//from AN4776 page 10, 1.3.1
	if (us > 65535) { //a check to see if the scans asks for bad delays
		printf("A delay of %d is too long!", us);
		us = 65535; //set to max
	}
	TIM1->ARR = us;
	TIM1->CNT = 0;
	TIM1->CR1 |= TIM_CR1_CEN; //enable timer
	while (TIM1->CNT < us)
		; //wait
	TIM1->CR1 &= ~TIM_CR1_CEN; //disable timer
}

//USB cdc transmit
//https://github.com/alexeykosinov/Redirect-printf-to-USB-VCP-on-STM32H7-MCU
int _write(int file, char *ptr, int len) {
	static uint8_t rc = USBD_OK;

	do {
		rc = CDC_Transmit_FS((unsigned char*) ptr, len);
	} while (USBD_BUSY == rc);

	if (USBD_FAIL == rc) {
		/// NOTE: Should never reach here.
		/// TODO: Handle this error.
		return 0;
	}
	return len;
}

//USB cdc receive
uint8_t buffer[64];
bool newReceived;

//ADC
uint32_t analogRead(uint8_t channel) {
	uint32_t ADCValue = 0;
	if (channel == 1) {
		HAL_ADC_Start(&hadc1);
		if (HAL_ADC_PollForConversion(&hadc1, 1000000) == HAL_OK) {
			ADCValue = HAL_ADC_GetValue(&hadc1);
		}
		HAL_ADC_Stop(&hadc1);
	} else if (channel == 5) {
		HAL_ADC_Start(&hadc5);
		if (HAL_ADC_PollForConversion(&hadc5, 1000000) == HAL_OK) {
			ADCValue = HAL_ADC_GetValue(&hadc5);
		}
		HAL_ADC_Stop(&hadc5);
	}

	return ADCValue/8; //divide by 8 for 8x oversampling
}

//parse usb serial input
uint16_t getNum() {
	uint16_t num = 0;
	for (int i = 1; i <= 4; i++) {
		if (isdigit(buffer[i])) { //only works if buffer isn't initialized with all zeros - shouldn't be
			num *= 10;
			num += buffer[i] - '0';
		}
	}
	return num;
}

//fine focus
//return true for successful focus, false for not
void fineFocus() {
	int32_t currentAdc;
	int16_t topAdc = -10000;
	int16_t bottomAdc = 10000;
	int16_t topDac = 0;
	int16_t bottomDac = 0;
	int16_t middleDac;
	int8_t precision = 2; //normally 7 for 13-bit

	//set offsets
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	htim2.Instance->CCR4 = xOffset;
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);
	htim3.Instance->CCR4 = yOffset;

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);
	//htim16.Instance->CCR1 = laserPower;

	//reset fine focus
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);

	//delay to settle
	HAL_Delay(100);

	for (int i = 0; i < 4096; i += precision) { //can do +=5?
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i);
		//delay_us(1000);
		HAL_Delay(8);
		currentAdc = 0;
		for (int a = 1; a <= adcAvg; a++) { //adc averaging
			currentAdc += analogRead(5) - analogRead(1);
			delay(50);
		}
		currentAdc = (currentAdc / adcAvg);
		if (currentAdc > topAdc) {
			topAdc = currentAdc;
			topDac = i;
			HAL_Delay(40);
		} else if (currentAdc < bottomAdc) {
			bottomAdc = currentAdc;
			bottomDac = i;
			HAL_Delay(40);
		}
		delay(6000);
		printf("%d,%d\r\n", i, currentAdc); //print to see curve
	}
	printf("Done\r\n");
	middleDac = ((topDac - bottomDac) / 2) + bottomDac;

	//temp offset
	middleDac += 40;

	for (int i = 0; i <= middleDac; i += precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i);
		if (i <= 2) { //don't need large timer if not resetting axis
			HAL_Delay(200);
		} else {
			delay(2000);
		}
		currentAdc = 0;
		for (int a = 1; a <= adcAvg; a++) {
			currentAdc += analogRead(5) - analogRead(1);
			delay(50);
		}
		currentAdc = (currentAdc / adcAvg);
		printf("%d,%d\r\n", i, currentAdc);
		delay(1000);
	}



	HAL_Delay(300);
	printf("Done\r\n");
}

//coarse focus
void coarseFocus() {
	uint32_t sum = 0;
	uint16_t minDac = 0;
	uint8_t precision = 1;
	uint16_t minSum = 10000;

	//set offsets
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	htim2.Instance->CCR4 = xOffset;
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);
	htim3.Instance->CCR4 = yOffset;

	//set fine focus to close-to-center to avoid center instability and coarse to start
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2800);

	//reset coarse focus
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0);

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);
	//htim16.Instance->CCR1 = laserPower;

	//delay to settle
	HAL_Delay(100);

	for (int i = 0; i < 4096; i += precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, i);
		delay(1000);
		sum = 0;
		for (int a = 1; a <= adcAvg; a++) {
			sum += analogRead(5) + analogRead(1);
			delay(50);
		}
		sum = (sum / adcAvg);
		if (sum < minSum) { //values get smaller as light increases (inverted)
			minSum = sum;
			minDac = i;
		}
		delay(1000);
		printf("%d,%d\r\n", i, sum); //print to see curve
	}
	printf("Done\r\n");

	// wind back to avoid dislodging sample
	for (int i = 4095; i >= 0; i -= precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, i);
		delay(200);
	}

	delay(2000);
	for (int i = 0; i <= minDac; i += precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, i);
		delay(1000);
		sum = 0;
		for (int a = 1; a <= adcAvg; a++) {
			sum += analogRead(5) + analogRead(1);
			delay(50);
		}
		sum = (sum / adcAvg);
		printf("%d,%d\r\n", i, sum);
		delay(1000);
	}

	HAL_Delay(300);
	printf("Done\r\n");
}

//main scan
void scan() {
	//TODO: swap UI readline for read until char (look at docs for pyserial) and only append a \r\n when sending a row
	int raw;

	uint16_t usedCounter = 0; //keep track of how much of the array has been filled

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);
	//htim16.Instance->CCR1 = laserPower;

	//fix artifacts at top of image by resetting properly

//	//wind back Y scan to avoid shaking sample (might error out if register is not set?)
//	int currentY = htim3.Instance->CCR4;
//	if (scanOffsetY < currentY) {
//		for (int y = currentY; y >= scanOffsetY; y--) {
//			htim3.Instance->CCR4 = y;
//			delay(200);
//		}
//	} else if (scanOffsetY > currentY) {
//		for (int y = currentY; y <= scanOffsetY; y++) {
//					htim3.Instance->CCR4 = y;
//					delay(200);
//		}
//	}
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);

	HAL_Delay(100);

	//printf("starting scan loop\r\n"); //disabling so I can straight copy values
	for (int y = 0; y < yRes; y++) {
		HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, scanOffsetY + (y * skipSteps));
		if (y <= 1) {
			HAL_Delay(yRes / 4); // normally yRes * skipSteps / 4
		}
		HAL_Delay(yRes / 15);
		for (int x = 0; x < xRes; x++) {
			//will send leftover data on new row if there is any
			if (usedCounter >= lenBuffer - 20) { //give 20 buffer to avoid exceeding the length of the array.
			//imageData[usedCounter+1] = '\r';
			//imageData[usedCounter+2] = '\n';
				static uint8_t rc = USBD_OK;
				do {
					rc = CDC_Transmit_FS(imageData, usedCounter);
				} while (USBD_BUSY == rc);

				if (USBD_FAIL == rc) {
					printf("Failed to send image\r\n");
					return;
				}
				HAL_Delay(lenBuffer / 10);
				memset(imageData, 0, lenBuffer);
				usedCounter = 0;
			}
			raw = 0;
			HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, scanOffsetX + (x * skipSteps));
			if (x <= 2) { //don't need large timer if not resetting axis
				HAL_Delay(xRes / 4); //is too slow as skipSteps gets higher -> should be more weighted for resolution, as that's where stability is needed?
			} else {
				delay(xRes * 10);
			}
			for (int i = 1; i <= adcAvg; i++) {
				raw += analogRead(5) + analogRead(1);
				delay(50);
			}
			raw = 8191 - (raw / adcAvg);

			if (x + 1 == xRes) {
				usedCounter += sprintf(imageData + usedCounter, "%d\r\n", raw);
			} else {
				usedCounter += sprintf(imageData + usedCounter, "%d,", raw);
			}
		}
		delay(10000); //change to a delay based on resolution?0000000000000000000
	}
	//do one last send of leftovers here
	if (usedCounter > 0) {
		//imageData[usedCounter+1] = '\r'; //might not need these since not using readline
		//imageData[usedCounter+2] = '\n';
		static uint8_t rc = USBD_OK;
		do {
			rc = CDC_Transmit_FS(imageData, usedCounter);
		} while (USBD_BUSY == rc);

		if (USBD_FAIL == rc) {
			printf("Failed to send image\r\n");
			return;
		}
		HAL_Delay(lenBuffer / 10);
		memset(imageData, 0, lenBuffer);
		usedCounter = 0;
	}
}

void fastScan() {

	int raw;

	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);
	//htim16.Instance->CCR1 = laserPower;

	//set offsets
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	htim2.Instance->CCR4 = xOffset;
	//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);
	htim3.Instance->CCR4 = yOffset;


	for (int y = 0; y < yRes; y++) {

		if (y == 0) {
			//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (scanOffsetY + (yRes*skipSteps)/2.55));
			htim3.Instance->CCR4 = (scanOffsetY + (yRes*skipSteps)/2.55);
			delay(8197); //half of 61hz, resonance frequency of SF-HD65
		}

		//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (scanOffsetY + (y * skipSteps)));
		htim3.Instance->CCR4 = (scanOffsetY + (y * skipSteps));

		for (int x = 0; x < xRes; x++) {
			raw = 0;

			//ringing compensation
			if (x == 0) {
				//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (scanOffsetX + (xRes*skipSteps)/2.55));
				htim2.Instance->CCR4 = (scanOffsetX + (xRes*skipSteps)/2.1);
				delay(8197); //half of 61hz, resonance frequency of SF-HD65
			}

			//HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (scanOffsetX + (x * skipSteps)));
			htim2.Instance->CCR4 = (scanOffsetX + (x * skipSteps));

			for (int i = 1; i <= adcAvg; i++) {
				raw += analogRead(5) + analogRead(1); //takes a lot of time
			}
			raw = 8191 - (raw / adcAvg);

			if (x + 1 == xRes) {
				printf("%d\r\n", raw);
			} else {
				printf("%d,", raw);
			}
		}

		//add delay later
	}
	printf("%d\r\n", 0);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_ADC5_Init();
  MX_OPAMP1_Init();
  MX_OPAMP4_Init();
  MX_USB_Device_Init();
  MX_TIM1_Init();
  MX_DAC1_Init();
  MX_DAC3_Init();
  MX_DAC2_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
	//DAC setup
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
	HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
	//HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
	//HAL_DAC_Start(&hdac3, DAC_CHANNEL_2);

	//ADC setup
	//HAL_ADC_Start(&hadc1);
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	//HAL_ADC_Start(&hadc5);
	HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED);

	//opamp setup for ADC inputs
	HAL_OPAMP_Start(&hopamp1);
	HAL_OPAMP_Start(&hopamp4);

	//opamp start for DAC outputs
	//HAL_OPAMP_Start(&hopamp3);
	//HAL_OPAMP_Start(&hopamp6);

	//timer setup
	HAL_TIM_Base_Start(&htim1);

//	//laser pwm setup
//	HAL_TIM_Base_Start(&htim16);
//	HAL_TIM_PWM_Init(&htim16);
//	HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);

	//PWM start
	HAL_TIM_Base_Start(&htim2);
	HAL_TIM_PWM_Init(&htim2);

	HAL_TIM_Base_Start(&htim3);
	HAL_TIM_PWM_Init(&htim3);

	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4); //x
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4); //y


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		//turn off between commands
		HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
		//htim16.Instance->CCR1 = 0;

		//TODO: Have interrupt to kill program if USB is disconnected?
		//TODO: change ADC opamps to "fast mode"?
		//DAC
		//out2-ch1 = laser

		//protocol:
		//first char -> 0-9:
		//0=start scan
		//1=change x resolution
		//2=change y resolution
		//3=change number of steps to skip (aka zoom -> steps to skip * resolution = scan area out of 8192)
		//4=change X offset
		//5=change Y offset
		//6=change opamp gain
		//7=change laser power
		//8=change number of ADC samples to average - use DMA later
		//9=fine or coarse focus
		//*10=change whether to output FES or SUM or both (aka just the two ADC values raw) - not needed yet

		//After leading number, rest is dependent on choice
		//setup with default values
		//0 -> no input after

		//missing vars are above

		//6 -> int between 0 and 5 (0=2x, 1=4x, 2=8x, 3=16x, 4=32x, 5=64x)
		uint8_t adcGain = 1;

		//buffer[0]
		uint8_t start;

		//value of following data after buffer[0]
		uint16_t num = 0;

		//missing is above
		//9 -> (1=fine focus, 2=coarse focus).

		//when starting a scan, do checks to make sure you aren't exceeding 8192 for any VCM output, and not exceeding 2500 for laser input.
		//avoid floats?
		if (newReceived) {
			//printf("Received\r\n");
			//hangs on line below -> when I send loads of 0s, it only shows one in the buffer.
			if (buffer[0] == '0') {
				start = 0;
			} else {
				start = buffer[0] - '0';
			}
			num = getNum();
			switch (start) {
			case 0:
				//printf("Scanning\r\n");
				scanOffsetX = xOffset - (xRes * skipSteps / 2); // set focus to center of scanned image (make sure > 0)
				scanOffsetY = yOffset - (yRes * skipSteps / 2);

				if ((scanOffsetX + (xRes * skipSteps)) < 4096 && (scanOffsetX + (xRes * skipSteps)) >= 0) {
					if ((scanOffsetY + (yRes * skipSteps)) < 4096 && (scanOffsetY + (yRes * skipSteps)) >= 0) {
						if (num == 1) { //disable flipping for now
							//scan();
							fastScan();
							HAL_Delay(1);
						}
					} else {
						printf("X offset, res or skipSteps too high!\r\n");
					}
				} else {
					printf("Y offset, res or skipSteps too high!\r\n");
				}
				break;
			case 1:
				if (num < 4096 && num > 0) {
					xRes = num;
				}
				//printf("%d\r\n", xRes);
				break;
			case 2:
				if (num < 4096 && num > 0) {
					yRes = num;
				}
				//printf("%d\r\n", yRes);
				break;
			case 3:
				if (num < 4096 && num > 0) {
					skipSteps = num;
				}
				//printf("%d\r\n", skipSteps);
				break;
			case 4:
				//apply offsets here so that focus is on the correct scan area, or apply in focus functions
				if (num < 4096) {
					xOffset = num;
				}
				//printf("%d\r\n", xOffset);
				break;
			case 5:
				if (num < 4096) {
					yOffset = num;
				}
				//printf("%d\r\n", yOffset);
				break;
			case 6:
				if (num <= 5) {
					adcGain = num;
					HAL_OPAMP_Stop(&hopamp1);
					HAL_OPAMP_Stop(&hopamp4);
					switch (adcGain) {
					case 0:
						pgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
						break;
					case 1:
						pgaGain = OPAMP_PGA_GAIN_4_OR_MINUS_3;
						break;
					case 2:
						pgaGain = OPAMP_PGA_GAIN_8_OR_MINUS_7;
						break;
					case 3:
						pgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
						break;
					case 4:
						pgaGain = OPAMP_PGA_GAIN_32_OR_MINUS_31;
						break;
					case 5:
						pgaGain = OPAMP_PGA_GAIN_64_OR_MINUS_63;
						break;
					}
					HAL_Delay(2); //just to give it time to shut down?
					MX_OPAMP1_Init();
					MX_OPAMP4_Init();
					HAL_Delay(2); //just to give it time to start up?
					HAL_OPAMP_Start(&hopamp1);
					HAL_OPAMP_Start(&hopamp4);
				}
				//printf("%d\r\n", adcGain);
				break;
			case 7:
				if (num <= 2500) {
					laserPower = num;
				}
				//printf("%d\r\n", laserPower);
				break;
			case 8:
				if (num <= 20 && num != 0) {
					adcAvg = num;
				}
				//printf("%d\r\n", adcAvg);
				break;
			case 9:
				if (num == 1) {
					fineFocus();
				} else if (num == 2) {
					coarseFocus();
				} else {
					printf("Bad input\r\n");
				}
				break;
			}

			newReceived = false;
		}
		HAL_Delay(10); //10ms polling delay
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 23;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV6;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_8;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_NONE;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC5_Init(void)
{

  /* USER CODE BEGIN ADC5_Init 0 */

  /* USER CODE END ADC5_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC5_Init 1 */

  /* USER CODE END ADC5_Init 1 */

  /** Common config
  */
  hadc5.Instance = ADC5;
  hadc5.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc5.Init.Resolution = ADC_RESOLUTION_12B;
  hadc5.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc5.Init.GainCompensation = 0;
  hadc5.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc5.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc5.Init.LowPowerAutoWait = DISABLE;
  hadc5.Init.ContinuousConvMode = DISABLE;
  hadc5.Init.NbrOfConversion = 1;
  hadc5.Init.DiscontinuousConvMode = DISABLE;
  hadc5.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc5.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc5.Init.DMAContinuousRequests = DISABLE;
  hadc5.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc5.Init.OversamplingMode = ENABLE;
  hadc5.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_8;
  hadc5.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_NONE;
  hadc5.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc5.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC5_Init 2 */

  /* USER CODE END ADC5_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT2 config
  */
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief DAC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC2_Init(void)
{

  /* USER CODE BEGIN DAC2_Init 0 */

  /* USER CODE END DAC2_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC2_Init 1 */

  /* USER CODE END DAC2_Init 1 */

  /** DAC Initialization
  */
  hdac2.Instance = DAC2;
  if (HAL_DAC_Init(&hdac2) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac2, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC2_Init 2 */

  /* USER CODE END DAC2_Init 2 */

}

/**
  * @brief DAC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC3_Init(void)
{

  /* USER CODE BEGIN DAC3_Init 0 */

  /* USER CODE END DAC3_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC3_Init 1 */

  /* USER CODE END DAC3_Init 1 */

  /** DAC Initialization
  */
  hdac3.Instance = DAC3;
  if (HAL_DAC_Init(&hdac3) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_DISABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_INTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac3, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT2 config
  */
  if (HAL_DAC_ConfigChannel(&hdac3, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC3_Init 2 */

  /* USER CODE END DAC3_Init 2 */

}

/**
  * @brief OPAMP1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP1_Init(void)
{

  /* USER CODE BEGIN OPAMP1_Init 0 */

  /* USER CODE END OPAMP1_Init 0 */

  /* USER CODE BEGIN OPAMP1_Init 1 */

  /* USER CODE END OPAMP1_Init 1 */
  hopamp1.Instance = OPAMP1;
  hopamp1.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp1.Init.Mode = OPAMP_PGA_MODE;
  hopamp1.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp1.Init.InternalOutput = ENABLE;
  hopamp1.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp1.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_IO0_BIAS;
  hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp1.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP1_Init 2 */

  /* USER CODE END OPAMP1_Init 2 */

}

/**
  * @brief OPAMP4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP4_Init(void)
{

  /* USER CODE BEGIN OPAMP4_Init 0 */

  /* USER CODE END OPAMP4_Init 0 */

  /* USER CODE BEGIN OPAMP4_Init 1 */

  /* USER CODE END OPAMP4_Init 1 */
  hopamp4.Instance = OPAMP4;
  hopamp4.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp4.Init.Mode = OPAMP_PGA_MODE;
  hopamp4.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp4.Init.InternalOutput = ENABLE;
  hopamp4.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp4.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_IO0_BIAS;
  hopamp4.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp4.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP4_Init 2 */

  /* USER CODE END OPAMP4_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 143.75-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65534;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4095;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 4095;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
