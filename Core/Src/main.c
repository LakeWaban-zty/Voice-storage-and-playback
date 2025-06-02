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
#include "stdio.h"
#include "my_hmi.h"              // 自定义的HMI操作函数
#include "my_usart.h"            // 自定义的串口通信函数
#include "zuolan_inside_flash.h" // 自定义的内部Flash操作函数
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
#define BUFFER_SIZE 1024                                       // ADC/DAC缓冲区大小
#define SAMPLES_PER_SECOND 10000                               // 10KHz采样率
#define MAX_RECORD_TIME 60                                     // 最大录音时间(秒)
#define FLASH_DATA_SIZE (MAX_RECORD_TIME * SAMPLES_PER_SECOND) // 最大600000个样本
#define RECORD_SECTOR_START 11                                 // 从扇区11开始
#define CODE_SECTOR_END 3                                      // 程序存储在0-3扇区

uint8_t dma_running = 0;    // 标记DMA状态
uint8_t is_recording = 0;   // 录音状态
uint8_t is_playing = 0;     // 播放状态
uint32_t record_index = 0;  // 录音数据索引（以样本数计）
uint32_t play_index = 0;    // 播放数据索引（以样本数计）
uint32_t flash_address;     // 当前Flash操作地址
uint32_t code_area_end;     // 程序区域结束地址
uint32_t flash_end_address; // Flash结束地址
uint8_t testSuccess = 0;    // Flash测试结果标志

// 新增的全局变量
uint32_t recording_buffer_pos = 0;     // flash_data_buffer当前位置
uint8_t recording_last_sample_odd = 0; // 上一个样本是否为奇数样本
uint32_t playback_start_address;       // 播放起始地址
uint32_t playback_current_offset;      // 当前播放偏移(字数)

// 使用已经定义的共享缓冲区: adc_dac_buffer
__ALIGN_BEGIN uint16_t adc_dac_buffer[BUFFER_SIZE] __ALIGN_END; // 共享缓冲区
uint32_t flash_data_buffer[BUFFER_SIZE / 2];                    // Flash写入缓冲区（32位）
uint8_t mode = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Start_ADC_DAC_DMA(void);
void Stop_ADC_DAC_DMA(void);
HAL_StatusTypeDef Start_Recording(void);
void Stop_Recording(void);
HAL_StatusTypeDef Start_Playing(void);
void Stop_Playing(void);
HAL_StatusTypeDef test_flash(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// #define BUFFER_SIZE 1024 // 根据需求调整大小

//__ALIGN_BEGIN uint16_t adc_dac_buffer[BUFFER_SIZE] __ALIGN_END; // 共享缓冲区

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
  // 计算程序区域结束地址
  code_area_end = ZuoLan_FLASH_GetSectorStartAddress(CODE_SECTOR_END) +
                  ZuoLan_FLASH_GetSectorSize(CODE_SECTOR_END);
  // 初始状态
  dma_running = 0;
  is_recording = 0;
  is_playing = 0;
  record_index = 0;
  play_index = 0;
  mode = 0;

  my_printf(&huart2, "System initialized. Available commands:\r\n");

  // my_printf(&huart2, "record - Start recording\r\n");
  // HAL_UART_Transmit(&huart2, (uint8_t *)"record - Start recording\r\n", 27, 1000);
  //  my_printf(&huart2, "play - Play recording\r\n");
  //  my_printf(&huart2, "stop - Stop current operation\r\n");
  //  my_printf(&huart2, "test_flash - Test Flash functionality\r\n");

  HMI_Debug_Print("System initialized. Available commands:");
  HMI_Send_String("page0.t4", "record - Start recording\r\nplay - Play recording\r\nstop - Stop current operation\r\ntest_flash - Test Flash functionality");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    // 检查是否有命令接收完成
    if (commandReceived2)
    {
      // 重置命令接收标志
      commandReceived2 = 0;
    }

    // 检查接收缓冲区中是否有特定命令
    if (strcmp((const char *)rxBuffer2, "record") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      if (!is_recording && !is_playing)
      {
        if (Start_Recording() != HAL_OK)
        {
          HMI_Debug_Print("Recording start failed");
        }
      }
      else if (is_recording)
      {
        HMI_Debug_Print("Already recording");
      }
      else if (is_playing)
      {
        HMI_Debug_Print("Currently playing, please stop playback first");
      }
    }
    else if (strcmp((const char *)rxBuffer2, "stop") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      if (is_recording)
      {
        Stop_Recording();
      }
      else if (is_playing)
      {
        Stop_Playing();
      }
      else
      {
        HMI_Debug_Print("No current operation in progress");
      }
    }
    else if (strcmp((const char *)rxBuffer2, "play") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      if (!is_playing && !is_recording)
      {
        if (Start_Playing() != HAL_OK)
        {
          HMI_Debug_Print("Playback start failed");
        }
      }
      else if (is_playing)
      {
        HMI_Debug_Print("Already playing");
      }
      else if (is_recording)
      {
        HMI_Debug_Print("Currently recording, please stop recording first");
      }
    }
    else if (strcmp((const char *)rxBuffer2, "test_flash") == 0)
    {
      // 清空接收缓冲区
      memset(rxBuffer2, 0, RX_BUFFER_SIZE);
      rxIndex2 = 0;

      // 运行Flash测试程序
      if (!is_recording && !is_playing)
      {
        test_flash();
        HMI_Debug_Print("Flash test completed, result: %s", testSuccess ? "Success" : "Failed");
      }
      else
      {
        HMI_Debug_Print("Please stop current operation first");
      }
    }
    /* USER CODE END WHILE */

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
    HMI_Debug_Print("ADC DMA start failed");
    Error_Handler();
  }
  else
  {
    HMI_Debug_Print("ADC DMA started successfully in circular mode");
  }

  // 启动DAC DMA (连续模式)
  if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1,
                        (uint32_t *)adc_dac_buffer,
                        BUFFER_SIZE,
                        DAC_ALIGN_12B_R) != HAL_OK)
  {
    HAL_ADC_Stop_DMA(&hadc1);
    HMI_Debug_Print("DAC DMA start failed");
    Error_Handler();
  }
  else
  {
    HMI_Debug_Print("DAC DMA started successfully in circular mode");
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

  HMI_Debug_Print("ADC/DAC DMA stopped");
  dma_running = 0; // 标记DMA已停止
}

/**
 * @brief 开始录音过程
 * @retval HAL状态
 */
HAL_StatusTypeDef Start_Recording(void)
{
  HAL_StatusTypeDef status;

  // 停止当前可能正在进行的DMA传输
  if (dma_running)
  {
    Stop_ADC_DAC_DMA();
    HAL_Delay(100); // 给系统一点时间完成停止操作
  }

  // 初始化Flash
  status = ZuoLan_FLASH_Init();
  if (status != HAL_OK)
  {
    HMI_Debug_Print("Flash initialization failed");
    return status;
  }

  // 只擦除扇区11作为起始扇区
  status = ZuoLan_FLASH_EraseSector(RECORD_SECTOR_START);
  if (status != HAL_OK)
  {
    HAL_FLASH_Lock();
    HMI_Debug_Print("Flash sector erase failed");
    return status;
  }

  // 设置起始地址为扇区11的结束地址减去4字节
  flash_address = ZuoLan_FLASH_GetSectorStartAddress(RECORD_SECTOR_START) +
                  ZuoLan_FLASH_GetSectorSize(RECORD_SECTOR_START) - 4;

  // 初始化录音状态
  record_index = 0;
  memset(flash_data_buffer, 0, sizeof(flash_data_buffer));

  // 全局变量，用于跟踪flash_data_buffer中当前写入位置
  recording_buffer_pos = 0;
  recording_last_sample_odd = 0;

  // 设置模式为录音
  mode = 1;
  is_recording = 1;

  // 启动ADC的DMA传输
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dac_buffer, BUFFER_SIZE) != HAL_OK)
  {
    HMI_Debug_Print("ADC DMA start failed");
    HAL_FLASH_Lock();
    is_recording = 0;
    return HAL_ERROR;
  }

  dma_running = 1;
  HMI_Debug_Print("Start recording from sector %d downward, address: 0x%08lX",
                  RECORD_SECTOR_START, flash_address);

  return HAL_OK;
}

/**
 * @brief 停止录音
 * @retval None
 */
void Stop_Recording(void)
{
  // 停止ADC DMA
  HAL_ADC_Stop_DMA(&hadc1);
  dma_running = 0;

  // 如果有剩余数据，写入最后一部分
  if (recording_buffer_pos > 0 || recording_last_sample_odd == 1)
  {
    uint32_t remaining_words = recording_buffer_pos;
    if (recording_last_sample_odd)
    {
      remaining_words++; // 确保最后一个不完整的字也被写入
    }

    if (remaining_words > 0)
    {
      // 计算写入地址
      uint32_t write_address = flash_address - (remaining_words - 1) * 4;

      // 检查是否与程序区域冲突
      if (write_address <= code_area_end)
      {
        HMI_Debug_Print("Warning: Cannot write remaining data, conflicts with program area");
      }
      else
      {
        // 写入剩余数据
        __disable_irq();
        HAL_StatusTypeDef status = ZuoLan_FLASH_WriteData(write_address, flash_data_buffer, remaining_words, 1);
        __enable_irq();

        if (status != HAL_OK)
        {
          HMI_Debug_Print("Warning: Failed to write remaining data");
        }
        else
        {
          // 更新Flash地址
          flash_address = write_address - 4;
        }
      }
    }
  }

  // 保存录音结束地址
  flash_end_address = flash_address + 4;

  // 锁定Flash
  HAL_FLASH_Lock();

  // 重置状态
  is_recording = 0;
  mode = 0;

  // 计算使用的扇区
  uint32_t start_sector = RECORD_SECTOR_START;
  uint32_t end_sector = ZuoLan_FLASH_GetSectorNumber(flash_end_address);
  uint32_t used_sectors = (end_sector <= start_sector) ? (start_sector - end_sector + 1) : 0;

  HMI_Debug_Print("Recording stopped");
  HMI_Debug_Print("Recorded %lu samples, used %lu sectors (%lu-%lu)",
                  record_index, used_sectors, end_sector, start_sector);

  // 验证录音数据
  uint32_t record_start_address = ZuoLan_FLASH_GetSectorStartAddress(RECORD_SECTOR_START) +
                                  ZuoLan_FLASH_GetSectorSize(RECORD_SECTOR_START) - 4;

  if (record_index > 0)
  {
    uint32_t test_data[2];
    ZuoLan_FLASH_ReadData(record_start_address, test_data, 2, 0);
    //HMI_Debug_Print("First samples: 0x%08lX, 0x%08lX", test_data[0], test_data[1]);
  }
}

/**
 * @brief 开始播放Flash中的数据
 * @retval HAL状态
 */
HAL_StatusTypeDef Start_Playing(void)
{
  // 检查是否有录制的数据
  if (record_index == 0)
  {
    HMI_Debug_Print("No playable data available");
    return HAL_ERROR;
  }

  // 停止当前可能正在进行的DMA传输
  if (dma_running)
  {
    Stop_ADC_DAC_DMA();
    HAL_Delay(100); // 给系统一点时间完成停止操作
  }

  // 计算播放的起始地址（最高地址）
  uint32_t play_start_address = ZuoLan_FLASH_GetSectorStartAddress(RECORD_SECTOR_START) +
                                ZuoLan_FLASH_GetSectorSize(RECORD_SECTOR_START) - 4;

  // 计算要读取的样本数和32位字数
  uint32_t samples_to_read = record_index > BUFFER_SIZE ? BUFFER_SIZE : record_index;
  uint32_t words_to_read = (samples_to_read + 1) / 2;

  // 计算读取起始地址
  uint32_t read_address = play_start_address - (words_to_read - 1) * 4;

  HMI_Debug_Print("Starting playback from address 0x%08lX", read_address);

  // 读取数据
  if (!ZuoLan_FLASH_ReadData(read_address, flash_data_buffer, words_to_read, 0))
  {
    HMI_Debug_Print("Failed to read Flash data");
    return HAL_ERROR;
  }

  // 将32位数据拆分为16位样本
  for (uint32_t i = 0; i < words_to_read; i++)
  {
    uint32_t word_data = flash_data_buffer[i];

    // 低16位
    adc_dac_buffer[i * 2] = word_data & 0xFFFF;

    // 高16位(如果在范围内)
    if (i * 2 + 1 < samples_to_read)
    {
      adc_dac_buffer[i * 2 + 1] = (word_data >> 16) & 0xFFFF;
    }
  }

  // 设置播放状态
  mode = 2;
  is_playing = 1;
  play_index = samples_to_read;

  // 保存起始播放位置信息
  playback_start_address = play_start_address;
  playback_current_offset = samples_to_read / 2; // 已读取的字数

  // 启动DAC DMA
  if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)adc_dac_buffer,
                        samples_to_read, DAC_ALIGN_12B_R) != HAL_OK)
  {
    HMI_Debug_Print("DAC DMA start failed");
    mode = 0;
    is_playing = 0;
    return HAL_ERROR;
  }

  dma_running = 1;

  return HAL_OK;
}

/**
 * @brief 停止播放
 * @retval None
 */
void Stop_Playing(void)
{
  // 停止DAC DMA
  HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);

  // 重置状态
  is_playing = 0;
  dma_running = 0;
  mode = 0;

  HMI_Debug_Print("Playback stopped");
}

/**
 * @brief ADC DMA传输完成回调函数
 * @param hadc ADC句柄
 * @retval None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1 && is_recording)
  {
    uint32_t halfSize = BUFFER_SIZE / 2;
    uint32_t offset = halfSize;

    // 检查是否达到最大录音时间
    if (record_index >= FLASH_DATA_SIZE)
    {
      Stop_Recording();
      HMI_Debug_Print("Maximum recording time reached");
      return;
    }

    // 处理ADC数据 - 后半缓冲区
    for (uint32_t i = 0; i < halfSize && record_index < FLASH_DATA_SIZE; i++)
    {
      // 获取当前ADC样本
      uint16_t adc_value = adc_dac_buffer[offset + i];

      // 将ADC样本存入flash_data_buffer
      if (recording_last_sample_odd == 0)
      {
        // 偶数样本，存储在低16位
        flash_data_buffer[recording_buffer_pos] = adc_value;
        recording_last_sample_odd = 1;
      }
      else
      {
        // 奇数样本，存储在高16位
        flash_data_buffer[recording_buffer_pos] |= ((uint32_t)adc_value << 16);
        recording_buffer_pos++;
        recording_last_sample_odd = 0;

        // 如果flash_data_buffer已满，写入Flash
        if (recording_buffer_pos >= 512)
        {
          // 计算写入地址，从高地址向低地址写入
          uint32_t write_address = flash_address - 511 * 4;

          // 检查是否进入新扇区
          uint32_t current_sector = ZuoLan_FLASH_GetSectorNumber(flash_address);
          uint32_t write_sector = ZuoLan_FLASH_GetSectorNumber(write_address);

          if (write_sector != current_sector)
          {
            // 需要擦除新扇区
            if (write_sector <= CODE_SECTOR_END)
            {
              Stop_Recording();
              HMI_Debug_Print("Error: Sector %lu conflicts with program area", write_sector);
              return;
            }

            // 擦除新扇区
            __disable_irq();
            HAL_StatusTypeDef erase_status = ZuoLan_FLASH_EraseSector(write_sector);
            __enable_irq();

            if (erase_status != HAL_OK)
            {
              Stop_Recording();
              HMI_Debug_Print("Error: Failed to erase sector %lu", write_sector);
              return;
            }
          }

          // 检查是否与程序区域冲突
          if (write_address <= code_area_end)
          {
            Stop_Recording();
            HMI_Debug_Print("Error: Write address conflicts with program area");
            return;
          }

          // 写入Flash
          __disable_irq();
          HAL_StatusTypeDef write_status = ZuoLan_FLASH_WriteData(write_address, flash_data_buffer, 512, 1);
          __enable_irq();

          if (write_status != HAL_OK)
          {
            Stop_Recording();
            HMI_Debug_Print("Error: Failed to write to Flash");
            return;
          }

          // 更新Flash地址
          flash_address = write_address - 4;

          // 清空缓冲区
          memset(flash_data_buffer, 0, sizeof(flash_data_buffer));
          recording_buffer_pos = 0;
        }
      }

      record_index++;
    }
  }
}

/**
 * @brief ADC DMA传输一半完成回调函数
 * @param hadc ADC句柄
 * @retval None
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1 && is_recording)
  {
    uint32_t halfSize = BUFFER_SIZE / 2;

    // 检查是否达到最大录音时间
    if (record_index >= FLASH_DATA_SIZE)
    {
      Stop_Recording();
      HMI_Debug_Print("Maximum recording time reached");
      return;
    }

    // 处理ADC数据 - 前半缓冲区
    for (uint32_t i = 0; i < halfSize && record_index < FLASH_DATA_SIZE; i++)
    {
      // 获取当前ADC样本
      uint16_t adc_value = adc_dac_buffer[i];

      // 将ADC样本存入flash_data_buffer
      if (recording_last_sample_odd == 0)
      {
        // 偶数样本，存储在低16位
        flash_data_buffer[recording_buffer_pos] = adc_value;
        recording_last_sample_odd = 1;
      }
      else
      {
        // 奇数样本，存储在高16位
        flash_data_buffer[recording_buffer_pos] |= ((uint32_t)adc_value << 16);
        recording_buffer_pos++;
        recording_last_sample_odd = 0;

        // 如果flash_data_buffer已满，写入Flash
        if (recording_buffer_pos >= 512)
        {
          // 计算写入地址，从高地址向低地址写入
          uint32_t write_address = flash_address - 511 * 4;

          // 检查是否进入新扇区
          uint32_t current_sector = ZuoLan_FLASH_GetSectorNumber(flash_address);
          uint32_t write_sector = ZuoLan_FLASH_GetSectorNumber(write_address);

          if (write_sector != current_sector)
          {
            // 需要擦除新扇区
            if (write_sector <= CODE_SECTOR_END)
            {
              Stop_Recording();
              HMI_Debug_Print("Error: Sector %lu conflicts with program area", write_sector);
              return;
            }

            // 擦除新扇区
            __disable_irq();
            HAL_StatusTypeDef erase_status = ZuoLan_FLASH_EraseSector(write_sector);
            __enable_irq();

            if (erase_status != HAL_OK)
            {
              Stop_Recording();
              HMI_Debug_Print("Error: Failed to erase sector %lu", write_sector);
              return;
            }
          }

          // 检查是否与程序区域冲突
          if (write_address <= code_area_end)
          {
            Stop_Recording();
            HMI_Debug_Print("Error: Write address conflicts with program area");
            return;
          }

          // 写入Flash
          __disable_irq();
          HAL_StatusTypeDef write_status = ZuoLan_FLASH_WriteData(write_address, flash_data_buffer, 512, 1);
          __enable_irq();

          if (write_status != HAL_OK)
          {
            Stop_Recording();
            HMI_Debug_Print("Error: Failed to write to Flash");
            return;
          }

          // 更新Flash地址
          flash_address = write_address - 4;

          // 清空缓冲区
          memset(flash_data_buffer, 0, sizeof(flash_data_buffer));
          recording_buffer_pos = 0;
        }
      }

      record_index++;
    }
  }
}

/**
 * @brief DAC DMA传输完成回调函数
 * @param hdac DAC句柄
 * @retval None
 */
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
  if (is_playing)
  {
    uint32_t halfSize = BUFFER_SIZE / 2;
    uint32_t offset = halfSize;

    // 检查是否播放完毕
    if (play_index >= record_index)
    {
      Stop_Playing();
      return;
    }

    // 计算要读取的样本数和32位字数
    uint32_t remaining_samples = record_index - play_index;
    uint32_t samples_to_read = remaining_samples > halfSize ? halfSize : remaining_samples;
    uint32_t words_to_read = (samples_to_read + 1) / 2;

    // 计算读取地址，从高到低
    uint32_t read_address = playback_start_address - playback_current_offset * 4 - (words_to_read - 1) * 4;

    // 检查地址是否超出录音范围
    if (read_address < flash_end_address)
    {
      // 已到达录音末尾
      Stop_Playing();
      return;
    }

    // 读取数据
    if (!ZuoLan_FLASH_ReadData(read_address, flash_data_buffer, words_to_read, 0))
    {
      Stop_Playing();
      HMI_Debug_Print("Failed to read Flash data");
      return;
    }

    // 将32位数据拆分为16位样本
    for (uint32_t i = 0; i < words_to_read; i++)
    {
      uint32_t word_data = flash_data_buffer[i];

      // 低16位
      if (i * 2 < samples_to_read)
        adc_dac_buffer[offset + i * 2] = word_data & 0xFFFF;

      // 高16位(如果在范围内)
      if (i * 2 + 1 < samples_to_read)
        adc_dac_buffer[offset + i * 2 + 1] = (word_data >> 16) & 0xFFFF;
    }

    // 如果不足半缓冲区，填充0
    for (uint32_t i = samples_to_read; i < halfSize; i++)
    {
      adc_dac_buffer[offset + i] = 0;
    }

    // 更新播放状态
    play_index += samples_to_read;
    playback_current_offset += words_to_read;
  }
}

/**
 * @brief DAC DMA传输一半完成回调函数
 * @param hdac DAC句柄
 * @retval None
 */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
  if (is_playing)
  {
    uint32_t halfSize = BUFFER_SIZE / 2;

    // 检查是否播放完毕
    if (play_index >= record_index)
    {
      Stop_Playing();
      return;
    }

    // 计算要读取的样本数和32位字数
    uint32_t remaining_samples = record_index - play_index;
    uint32_t samples_to_read = remaining_samples > halfSize ? halfSize : remaining_samples;
    uint32_t words_to_read = (samples_to_read + 1) / 2;

    // 计算读取地址，从高到低
    uint32_t read_address = playback_start_address - playback_current_offset * 4 - (words_to_read - 1) * 4;

    // 检查地址是否超出录音范围
    if (read_address < flash_end_address)
    {
      // 已到达录音末尾
      Stop_Playing();
      return;
    }

    // 读取数据
    if (!ZuoLan_FLASH_ReadData(read_address, flash_data_buffer, words_to_read, 0))
    {
      Stop_Playing();
      HMI_Debug_Print("Failed to read Flash data");
      return;
    }

    // 将32位数据拆分为16位样本
    for (uint32_t i = 0; i < words_to_read; i++)
    {
      uint32_t word_data = flash_data_buffer[i];

      // 低16位
      if (i * 2 < samples_to_read)
        adc_dac_buffer[i * 2] = word_data & 0xFFFF;

      // 高16位(如果在范围内)
      if (i * 2 + 1 < samples_to_read)
        adc_dac_buffer[i * 2 + 1] = (word_data >> 16) & 0xFFFF;
    }

    // 如果不足半缓冲区，填充0
    for (uint32_t i = samples_to_read; i < halfSize; i++)
    {
      adc_dac_buffer[i] = 0;
    }

    // 更新播放状态
    play_index += samples_to_read;
    playback_current_offset += words_to_read;
  }
}

/**
 * @brief 测试Flash读写功能
 * @retval HAL状态
 */
HAL_StatusTypeDef test_flash(void)
{
#define TEST_DATA_SIZE 1024 // 测试数据大小(字节)

  /* Flash操作状态 */
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t verificationResult = 0;

  /* 测试数据 - 字节数组 */
  uint8_t writeDataArray[TEST_DATA_SIZE]; // 写入测试数据
  uint8_t readDataArray[TEST_DATA_SIZE];  // 读取数据缓冲区

  /* 测试地址 - 使用扇区11起始地址 */
  uint32_t testAddress = ZuoLan_FLASH_GetSectorStartAddress(RECORD_SECTOR_START);

  /* 准备测试数据 */
  /* 填充测试数组：使用循环索引值 */
  for (uint32_t i = 0; i < TEST_DATA_SIZE; i++)
  {
    writeDataArray[i] = i & 0xFF; // 循环值 0~255
  }

  /* 清空读取缓冲区 */
  memset(readDataArray, 0, TEST_DATA_SIZE);

  HMI_Debug_Print("Starting Flash test...");

  /* 禁用中断，防止Flash操作被打断 */
  __disable_irq();

  /* 初始化Flash操作(解锁Flash) */
  status = ZuoLan_FLASH_Init();
  if (status != HAL_OK)
  {
    __enable_irq();
    HMI_Debug_Print("Flash initialization failed");
    testSuccess = 0;
    return status;
  }

  /* 擦除测试扇区 */
  HMI_Debug_Print("Erasing sector %d...", RECORD_SECTOR_START);
  status = ZuoLan_FLASH_EraseSector(RECORD_SECTOR_START);
  if (status != HAL_OK)
  {
    HAL_FLASH_Lock(); // 确保退出前Flash已锁定
    __enable_irq();
    HMI_Debug_Print("Sector erase failed");
    testSuccess = 0;
    return status;
  }

  /* 向Flash写入字节数组 */
  HMI_Debug_Print("Writing test data...");
  uint32_t highAddress = testAddress + ZuoLan_FLASH_GetSectorSize(RECORD_SECTOR_START) - TEST_DATA_SIZE;
  status = ZuoLan_FLASH_WriteBytes(highAddress, writeDataArray, TEST_DATA_SIZE, 1);
  if (status != HAL_OK)
  {
    HAL_FLASH_Lock(); // 确保退出前Flash已锁定
    __enable_irq();
    HMI_Debug_Print("Data write failed");
    testSuccess = 0;
    return status;
  }

  /* 锁定Flash */
  status = HAL_FLASH_Lock();
  if (status != HAL_OK)
  {
    __enable_irq();
    HMI_Debug_Print("Flash lock failed");
    testSuccess = 0;
    return status;
  }

  /* 重新启用中断 */
  __enable_irq();

  /* 读取数据 */
  HMI_Debug_Print("Reading test data...");
  verificationResult = ZuoLan_FLASH_ReadBytes(highAddress, readDataArray, TEST_DATA_SIZE, 0);
  if (verificationResult != 1)
  {
    HMI_Debug_Print("Data read failed");
    testSuccess = 0;
    return HAL_ERROR;
  }

  /* 验证数据 */
  testSuccess = (memcmp(writeDataArray, readDataArray, TEST_DATA_SIZE) == 0) ? 1 : 0;

  if (testSuccess)
  {
    HMI_Debug_Print("Flash test successful!");
  }
  else
  {
    HMI_Debug_Print("Data verification failed!");
    /* 打印部分不匹配的数据用于调试 */
    for (uint32_t i = 0; i < 10; i++)
    {
      if (writeDataArray[i] != readDataArray[i])
      {
        HMI_Debug_Print("Position:%lu, Written:%d, Read:%d",
                        i, writeDataArray[i], readDataArray[i]);
      }
    }
  }

  return testSuccess ? HAL_OK : HAL_ERROR;
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
