/**
 ******************************************************************************
 * @file           : audio_recorder.h
 * @brief          : Audio recording and playback functions
 ******************************************************************************
 */

#ifndef __AUDIO_RECORDER_H
#define __AUDIO_RECORDER_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "my_usart.h"            // 自定义的串口通信函数
#include "zuolan_inside_flash.h" // 自定义的内部Flash操作函数

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define BUFFER_SIZE 1024         // DMA缓冲区大小
#define SAMPLES_PER_SECOND 10000 // 10KHz采样率

// Flash存储配置
#define SIZE_INFO_SECTOR 3                                       // 录音信息存储扇区
#define SIZE_INFO_ADDRESS (Get_Sector_Address(SIZE_INFO_SECTOR)) // 大小信息存储地址
#define RECORD_SECTOR_START 4                                    // 从扇区4开始存储录音数据
#define RECORD_SECTOR_END 11                                     // 到扇区11结束
#define CODE_SECTOR_END 2                                        // 程序存储在0-2扇区

    // 录音信息结构体(存储在SIZE_INFO_SECTOR)
    typedef struct
    {
        uint32_t magic;         // 魔术数字(用于验证数据有效性)
        uint32_t record_size;   // 录音数据大小(字节数)
        uint32_t start_address; // 录音数据起始地址
        uint32_t end_address;   // 录音数据结束地址
        uint32_t sample_rate;   // 采样率
        uint32_t checksum;      // 校验和
    } RecordInfo_t;

#define RECORD_INFO_MAGIC 0x52454344 // "RECD" in ASCII

    /* Exported macro ------------------------------------------------------------*/
    /* Exported functions prototypes ---------------------------------------------*/
    void AUDIO_Init(void);
    HAL_StatusTypeDef AUDIO_ReadRecordInfo(RecordInfo_t *info);
    HAL_StatusTypeDef AUDIO_WriteRecordInfo(RecordInfo_t *info);
    HAL_StatusTypeDef AUDIO_EraseRecordInfo(void);
    HAL_StatusTypeDef AUDIO_Start_Recording(void);
    void AUDIO_Stop_Recording(void);
    HAL_StatusTypeDef AUDIO_Start_Playing(void);
    void AUDIO_Stop_Playing(void);
    void AUDIO_Start_ADC_DAC_DMA(void);
    void AUDIO_Stop_ADC_DAC_DMA(void);
    HAL_StatusTypeDef AUDIO_Test_Flash(void);
    uint32_t AUDIO_GetMaxRecordSize(void);
    uint8_t AUDIO_HasValidRecording(void);
    void AUDIO_Process_Command(const char *command);
    //void AUDIO_AutoPlay_OnBoot(void);

    // 辅助函数原型
    uint32_t Get_Sector_Size(uint32_t sector);
    uint32_t Get_Sector_Address(uint32_t sector);
    uint32_t Get_Sector_From_Address(uint32_t address);

    // ADC/DAC DMA回调函数声明(需在外部文件调用)
    void AUDIO_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
    void AUDIO_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc);
    void AUDIO_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac);
    void AUDIO_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac);

    /* External variables --------------------------------------------------------*/
    extern __ALIGN_BEGIN uint8_t adc_dac_buffer[BUFFER_SIZE] __ALIGN_END; // 共享缓冲区
    extern uint8_t is_recording;                                          // 录音状态
    extern uint8_t is_playing;                                            // 播放状态
    extern uint8_t dma_running;                                           // DMA状态

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_RECORDER_H */
