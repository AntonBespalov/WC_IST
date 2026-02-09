/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define EXTWDG_OUT_Pin GPIO_PIN_2
#define EXTWDG_OUT_GPIO_Port GPIOC
#define COMX_IRQ_Pin GPIO_PIN_0
#define COMX_IRQ_GPIO_Port GPIOA
#define COMX_RESET_Pin GPIO_PIN_1
#define COMX_RESET_GPIO_Port GPIOA
#define SKYPER_ERR_IN_Pin GPIO_PIN_9
#define SKYPER_ERR_IN_GPIO_Port GPIOC
#define SKYPER_ERROUT2_Pin GPIO_PIN_8
#define SKYPER_ERROUT2_GPIO_Port GPIOA
#define SKYPER_ERROUT1_Pin GPIO_PIN_9
#define SKYPER_ERROUT1_GPIO_Port GPIOA
#define USER_LED1_Pin GPIO_PIN_6
#define USER_LED1_GPIO_Port GPIOG
#define USER_LED2_Pin GPIO_PIN_7
#define USER_LED2_GPIO_Port GPIOG
#define USER_TEST2_Pin GPIO_PIN_9
#define USER_TEST2_GPIO_Port GPIOG
#define AD7606_CONVST_Pin GPIO_PIN_3
#define AD7606_CONVST_GPIO_Port GPIOD
#define AD7606_BUSY2_Pin GPIO_PIN_4
#define AD7606_BUSY2_GPIO_Port GPIOB
#define AD7606_BUSY1_Pin GPIO_PIN_5
#define AD7606_BUSY1_GPIO_Port GPIOB
#define AD7380_CS2_Pin GPIO_PIN_6
#define AD7380_CS2_GPIO_Port GPIOB
#define AD7380_CS1_Pin GPIO_PIN_7
#define AD7380_CS1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
