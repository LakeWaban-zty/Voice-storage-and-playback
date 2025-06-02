/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "my_usart.h" // 自定义的串口通信函数
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

/* USER CODE BEGIN PV */
uint8_t dma_running = 0; // 标记DMA状态
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Start_ADC_DAC_DMA(void);
void Stop_ADC_DAC_DMA(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define BUFFER_SIZE 1024 // 根据需求调整大小

__ALIGN_BEGIN uint16_t adc_dac_buffer[BUFFER_SIZE] __ALIGN_END; // 共享缓冲区
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_DAC_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim2);
  HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
  HAL_UART_Receive_IT(&huart2, &rxTemp2, 1);
  // 初始状态下不启动DMA传输
  dma_running = 0;
  my_printf(&huart2, "System initialized. Send 'start' to begin ADC/DAC or 'stop' to halt.\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (strcmp((const char *)rxBuffer2, "start") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      // 如果DMA已经在运行，先停止
      if (dma_running)
      {
        Stop_ADC_DAC_DMA();
        HAL_Delay(100); // 给系统一点时间完成停止操作
      }

      // 启动ADC和DAC的DMA传输
      Start_ADC_DAC_DMA();
    }
    else if (strcmp((const char *)rxBuffer2, "stop") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      // 如果DMA正在运行，停止它
      if (dma_running)
      {
        Stop_ADC_DAC_DMA();
      }
      else
      {
        my_printf(&huart2, "ADC/DAC DMA is not running\r\n");
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* USER CODE END 3 */
  }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * @brief 启动ADC和DAC的DMA传输
 * @retval None
 */
void Start_ADC_DAC_DMA(void)
{
  // 启动ADC DMA (连续模式)
  if (HAL_ADC_Start_DMA(&hadc1,
                        (uint32_t *)adc_dac_buffer,
                        BUFFER_SIZE) != HAL_OK)
  {
    my_printf(&huart2, "ADC DMA start failed\r\n");
    Error_Handler();
  }
  else
  {
    my_printf(&huart2, "ADC DMA started successfully in circular mode\r\n");
  }

  // 启动DAC DMA (连续模式)
  if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1,
                        (uint32_t *)adc_dac_buffer,
                        BUFFER_SIZE,
                        DAC_ALIGN_12B_R) != HAL_OK)
  {
    HAL_ADC_Stop_DMA(&hadc1);
    my_printf(&huart2, "DAC DMA start failed\r\n");
    Error_Handler();
  }
  else
  {
    my_printf(&huart2, "DAC DMA started successfully in circular mode\r\n");
    dma_running = 1; // 标记DMA已启动
  }
}

/**
 * @brief 停止ADC和DAC的DMA传输
 * @retval None
 */
void Stop_ADC_DAC_DMA(void)
{
  // 先停止ADC的DMA
  HAL_ADC_Stop_DMA(&hadc1);

  // 再停止DAC的DMA
  HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);

  my_printf(&huart2, "ADC/DAC DMA stopped\r\n");
  dma_running = 0; // 标记DMA已停止
}
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

#ifdef USE_FULL_ASSERT
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
