/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_HRTIM_MspPostInit(HRTIM_HandleTypeDef *hhrtim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC3_BIAS_Pin GPIO_PIN_1
#define ADC3_BIAS_GPIO_Port GPIOA
#define DAC_FINE_Pin GPIO_PIN_2
#define DAC_FINE_GPIO_Port GPIOA
#define DAC_Coarse_Pin GPIO_PIN_4
#define DAC_Coarse_GPIO_Port GPIOA
#define ADC4_BIAS_Pin GPIO_PIN_5
#define ADC4_BIAS_GPIO_Port GPIOA
#define DAC_Laser_Pin GPIO_PIN_6
#define DAC_Laser_GPIO_Port GPIOA
#define ADC4_IN_Pin GPIO_PIN_0
#define ADC4_IN_GPIO_Port GPIOB
#define ADC1_BIAS_Pin GPIO_PIN_2
#define ADC1_BIAS_GPIO_Port GPIOB
#define ADC3_IN_Pin GPIO_PIN_12
#define ADC3_IN_GPIO_Port GPIOB
#define ADC1_IN_Pin GPIO_PIN_13
#define ADC1_IN_GPIO_Port GPIOB
#define ADC2_IN_Pin GPIO_PIN_14
#define ADC2_IN_GPIO_Port GPIOB
#define PWM_Y_Pin GPIO_PIN_8
#define PWM_Y_GPIO_Port GPIOC
#define PWM_X_Pin GPIO_PIN_8
#define PWM_X_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
