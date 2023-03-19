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
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include "usbd_cdc_if.h"
#include<stdio.h>
#include<stdlib.h>


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
OPAMP_HandleTypeDef hopamp3;
OPAMP_HandleTypeDef hopamp4;
OPAMP_HandleTypeDef hopamp6;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
//1 -> int between 1 and 4095
uint16_t xRes = 200;
//2 -> int between 1 and 4095
uint16_t yRes = 200;
//3 -> int between 1 and 4095
uint16_t skipSteps = 1;

//4 -> int between 0 and 4095
uint16_t xOffset = 2000;
//5 -> int between 0 and 4095
uint16_t yOffset = 2000;
//8 -> int between 1 and 20
uint8_t adcAvg = 8;

//7 -> int between 0 and 2500
uint16_t laserPower = 2000;

int pgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;

int scanOffsetX = 0;
int scanOffsetY = 0;

//uint8_t image[500][500]; //causes RAM overflow -> sadge I'll have to send line by line in the scan function (use uint16_t for kicks?)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC5_Init(void);
static void MX_DAC1_Init(void);
static void MX_DAC2_Init(void);
static void MX_DAC3_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_OPAMP4_Init(void);
static void MX_OPAMP6_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//microsecond delay (not used)
//https://controllerstech.com/create-1-microsecond-delay-stm32/
void delay_us(uint16_t us)
{
	__HAL_TIM_SET_COUNTER(&htim1,0);
	while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}

//USB cdc transmit
//https://github.com/alexeykosinov/Redirect-printf-to-USB-VCP-on-STM32H7-MCU
int _write(int file, char *ptr, int len) {
    static uint8_t rc = USBD_OK;

    do {
        rc = CDC_Transmit_FS((unsigned char*)ptr, len);
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
uint32_t analogRead(uint8_t channel)
{
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

  return ADCValue;
}

//parse usb serial input
uint16_t getNum() {
	  uint16_t num = 0;
	  for (int i=1; i<=4; i++) {
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
	int8_t precision = 3;

	//set offsets
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);

	//reset fine focus
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);

	//delay to settle
	HAL_Delay(100);

	for (int i=0; i<4096; i+=precision) { //can do +=5?
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i);
		//delay_us(1000);
		HAL_Delay(2);
		currentAdc = 0;
		for (int a=1; a<=adcAvg; a++) { //adc averaging
			currentAdc += analogRead(5)-analogRead(1);
			delay_us(50);
		}
		currentAdc = (currentAdc/adcAvg);
		if (currentAdc > topAdc) {
			topAdc = currentAdc;
			topDac = i;
		} else if (currentAdc < bottomAdc) {
			bottomAdc = currentAdc;
			bottomDac = i;
		}
		//delay_us(1000); //give time for VCM to catch up - maybe not needed
		HAL_Delay(2);
		printf("%d,", i);
		printf("%d\r\n", currentAdc); //print to see curve
	}
	printf("Done\r\n");
	middleDac = ((topDac-bottomDac)/2)+bottomDac;

	for (int i = 4095; i >= middleDac; i--) { //offset middleDac-65
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i); //is blocking?
		HAL_Delay(4);
		currentAdc = analogRead(5)-analogRead(1);
		if (i != middleDac) {
			printf("%d,", i);
			printf("%d\r\n", currentAdc); //print to see curve
		}
	}
	printf("Done\r\n"); //print to see curve
	/*
	int32_t distance;
	int32_t minDistance = 10000;
	uint16_t lowerDac;
	uint16_t higherDac;
	if (bottomDac < topDac) {
		lowerDac = bottomDac;
		higherDac = topDac;
	} else {
		lowerDac = topDac;
		higherDac = bottomDac;
	}

	for (int i = lowerDac; i <= higherDac; i+=precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i);
		if (i <= lowerDac+1) {
			HAL_Delay(50);
		} else {
			HAL_Delay(2);
		}
		currentAdc = analogRead(5)-analogRead(1);
		//middle ADC value VS 0 as the target:
		//0: won't be offset by poor s-shape, but also won't account for OPU bias
		//ADC middle: will account for bias, but poor s-shape could make it unable to focus
		distance = 0-currentAdc;
		if (distance < 0) {
			distance *= -1;
		}
		if (distance < minDistance) { //what about noise?
			minDistance = distance;
		} else {
			HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, i-precision);
			HAL_Delay(10); //leave time to settle
			return topAdc-bottomAdc; //return difference, aka size of S-shape
		}
	}*/

}

//coarse focus
void coarseFocus() {
	uint32_t sum = 0;
	uint16_t minDac = 0;
	uint8_t precision = 3;
	uint16_t minSum = 10000;

	//set offsets
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, xOffset);
	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, yOffset);

	//set fine focus to close-to-center to avoid center instability and coarse to start
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2800);
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0);

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);

	//delay to settle
	HAL_Delay(100);

	for (int i=0; i<4096; i+=precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, i);
		HAL_Delay(2);
		sum = 0;
		for (int a=1; a<=adcAvg; a++) {
			sum += analogRead(5)+analogRead(1);
			delay_us(50);
		}
		sum = (sum/adcAvg);
		if (sum < minSum) { //values get smaller as light increases (inverted)
			minSum = sum;
			minDac = i;
		}
		HAL_Delay(2); //give time for VCM to catch up - maybe not needed
		printf("%d,", i);
		printf("%d\r\n", sum); //print to see curve
	}
	printf("Done\r\n");


	//printf("%d\r\n", minDac);
	for (int i = 4095; i >= minDac; i-=precision) {
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, i);
		HAL_Delay(4);
		sum = analogRead(5)+analogRead(1);
		if (i != minDac) {
			printf("%d,", i);
			printf("%d\r\n", sum); //print to see curve
		}
	}
	printf("Done\r\n");


	HAL_Delay(20); //time to settle
	//maybe print DAC value at max, to know general position (aka make sure it's not at a max?)
}

//main scan
void scan(bool focusType) {
	int raw;
	unsigned char row[(xRes*6)+1]; //each number can be 5 characters long (4+sign), and there's a delimiter following. The +4 is for the \n
	//unsigned char row[xRes*4]; //max 8192 = 4 digits long
	uint16_t usedCounter = 0; //keep track of how much of the array has been filled

	//set laser power
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower);

	HAL_Delay(100);
	//printf("starting scan loop\r\n"); //disabling so I can straight copy values
	for (int y=0; y<yRes; y++) {
		  //memset(row, 0, sizeof row); //see if this fixes corruption - doesn't work -- no longer needed?
		  HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, scanOffsetY+(y*skipSteps));
		  HAL_Delay(5+(yRes*skipSteps)/50); //8ms per pixel
		  //delay_us(20);
		  usedCounter = 0;
		  for (int x=0; x<xRes; x++) {
			  raw = 0;
			  HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, scanOffsetX+(x*skipSteps));
			  if (x <= 1) {
				  HAL_Delay(xRes/3); //delay based on resolution when resetting
			  } else {
				  //math below gives about 2ms step covered, regardless of other variables --> needs work
				  HAL_Delay(2+((xRes*skipSteps)/200)); //I need a setup that calculates based on time-per-pixel, which should stay the same, whether it's skipped or not
			  }
			  //delay_us(500);
			  for (int i=1; i<=adcAvg; i++) {
				  if (focusType == 1) {
					  raw += analogRead(5)-analogRead(1); //scan FES instead
				  } else {
					  raw += analogRead(5)+analogRead(1);
				  }
				  delay_us(50);
			  }
			  raw = (raw/adcAvg);
			  if (focusType == 0) {
				  raw = 8191 - raw; //invert the image
			  }
			  //printf("%d\r\n", raw);
			  if (x+1 != xRes) {
				  usedCounter += sprintf(row+usedCounter, "%d,", raw);
			  } else {
				  usedCounter += sprintf(row+usedCounter, "%d\r\n", raw);
			  }
		  }

		  HAL_Delay(xRes/40); //needs some time
		  static uint8_t rc = USBD_OK;
		  do {
		          rc = CDC_Transmit_FS(row, usedCounter);
		  } while (USBD_BUSY == rc);

		  if (USBD_FAIL == rc) {
			  printf("Failed to send image\r\n");
			  return;
		  }
		  HAL_Delay(xRes/40);
	}
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
  MX_DAC1_Init();
  MX_DAC2_Init();
  MX_DAC3_Init();
  MX_OPAMP1_Init();
  MX_OPAMP3_Init();
  MX_OPAMP4_Init();
  MX_OPAMP6_Init();
  MX_USB_Device_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  //DAC setup
  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
  HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac3, DAC_CHANNEL_2);

  //ADC setup
  HAL_ADC_Start(&hadc1);
  HAL_ADC_Start(&hadc5);

  //opamp setup for DAC outputs
  HAL_OPAMP_Start(&hopamp3);
  HAL_OPAMP_Start(&hopamp6);

  //opamp setup for ADC inputs
  HAL_OPAMP_Start(&hopamp1);
  HAL_OPAMP_Start(&hopamp4);

  //timer setup
  HAL_TIM_Base_Start(&htim1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* USER CODE END WHILE */

	  //turn off between commands
	  HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);

	  //TODO: Have interrupt to kill program if USB is disconnected?
	  //TODO: change ADC opamps to "fast mode"?

	  //All DACS
	  //out1-ch1 = vcm-fine
	  //out1-ch2 = vcm-coarse
	  //out2-ch1 = laser
	  //out3-ch1 (opamp3) = vcm-x
	  //out3-ch2 (opamp6) = vcm-y

	  //protocol:
	  //first char -> 0-9:
	  //0=start scan
	  //1=change x resolution
	  //2=change y resolution
	  //3=change number of steps to skip (aka zoom -> steps to skip * resolution = scan area out of 4096)
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

	  //when starting a scan, do checks to make sure you aren't exceeding 4096 for any VCM output, and not exceeding 2500 for laser input.
	  //avoid floats?
	  if (newReceived) {
		  //printf("Received\r\n");
		  //hangs on line below -> when I send loads of 0s, it only shows one in the buffer.
		  if (buffer[0]=='0') {
				  start = 0;
		  } else {
			  start = buffer[0]-'0';
		  }
		  num = getNum();
		  switch (start) {
			case 0:
				//printf("Scanning\r\n");
				scanOffsetX = xOffset - (xRes * skipSteps / 2); // set focus to center of scanned image (make sure > 0)
				scanOffsetY = yOffset - (yRes * skipSteps / 2);

				if (scanOffsetX < 4096 && scanOffsetY < 4096) { //doesn't go to else properly
					if (scanOffsetX >= 0 && scanOffsetY >= 0) {
						if (num == 0 || num == 1) {
							//printf("passed checks\r\n");
						    //call scan function
						    //might have issues sending if char count is over 512 in the X axis (maybe break into chunks if an issue)
							scan(num);

							HAL_Delay(1);
							//printf("done\r\n");
						}
					} else {
						printf("Offset too small\r\n");
					}
				} else {
				  	printf("Dimensions too large\r\n");
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
					HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, laserPower); //don't need
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
	  HAL_Delay(100); //100ms polling delay

	  /* USER CODE BEGIN 3 */
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
  hadc1.Init.OversamplingMode = DISABLE;
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
  hadc5.Init.OversamplingMode = DISABLE;
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
  hopamp1.Init.PgaGain = pgaGain;
  hopamp1.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP1_Init 2 */

  /* USER CODE END OPAMP1_Init 2 */

}

/**
  * @brief OPAMP3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp3.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_DAC;
  hopamp3.Init.InternalOutput = DISABLE;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

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
  hopamp4.Init.PgaGain = pgaGain;
  hopamp4.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP4_Init 2 */

  /* USER CODE END OPAMP4_Init 2 */

}

/**
  * @brief OPAMP6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP6_Init(void)
{

  /* USER CODE BEGIN OPAMP6_Init 0 */

  /* USER CODE END OPAMP6_Init 0 */

  /* USER CODE BEGIN OPAMP6_Init 1 */

  /* USER CODE END OPAMP6_Init 1 */
  hopamp6.Instance = OPAMP6;
  hopamp6.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp6.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp6.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_DAC;
  hopamp6.Init.InternalOutput = DISABLE;
  hopamp6.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp6.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP6_Init 2 */

  /* USER CODE END OPAMP6_Init 2 */

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
  htim1.Init.Period = 65535;
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

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
  while (1)
  {
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
