/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "stdint.h"
#define RX_BUFFER_SIZE 128 // 每个串口的接收缓冲区大小
// 数据包
#define MIN_FRAME_SIZE 3  // 最小帧大小
extern uint8_t rxTemp1, rxTemp2, rxTemp3;
extern uint8_t rxBuffer1[RX_BUFFER_SIZE], rxBuffer2[RX_BUFFER_SIZE], rxBuffer3[RX_BUFFER_SIZE];
extern uint16_t rxIndex1, rxIndex2, rxIndex3;
extern volatile uint8_t commandReceived1, commandReceived3;
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
/* USER CODE END Includes */

extern UART_HandleTypeDef huart2;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_USART2_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

