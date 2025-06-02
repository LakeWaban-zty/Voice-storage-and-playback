/**
 ******************************************************************************
 * @file           : audio_recorder.c
 * @brief          : Audio recording and playback functions
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "audio_recorder.h"
#include "my_hmi.h" // 包含HMI接口

/* Private variables ---------------------------------------------------------*/
__ALIGN_BEGIN uint8_t adc_dac_buffer[BUFFER_SIZE] __ALIGN_END; // 共享缓冲区
uint8_t flash_data_buffer[1024];                               // Flash写入缓冲区

uint8_t dma_running = 0;      // 标记DMA状态
uint8_t is_recording = 0;     // 录音状态
uint8_t is_playing = 0;       // 播放状态
uint32_t record_index = 0;    // 录音数据索引
uint32_t play_index = 0;      // 播放数据索引
uint32_t flash_address;       // 当前Flash操作地址
uint32_t code_area_end;       // 程序区域结束地址
uint32_t flash_start_address; // Flash起始地址
uint32_t flash_end_address;   // Flash结束地址
uint8_t testSuccess = 0;      // Flash测试结果标志
uint32_t current_sector;      // 当前正在使用的扇区

// 调试计数器
uint32_t debug_write_count = 0;   // 写入Flash次数
uint32_t debug_read_count = 0;    // 读取Flash次数
uint32_t total_bytes_written = 0; // 总写入字节数
uint32_t total_bytes_read = 0;    // 总读取字节数

// 录音信息缓存
RecordInfo_t current_record_info;

/* Private function prototypes -----------------------------------------------*/
static uint32_t Calculate_Checksum(RecordInfo_t *info);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 计算校验和
 * @param info 录音信息结构体
 * @retval 校验和值
 */
static uint32_t Calculate_Checksum(RecordInfo_t *info)
{
    uint32_t sum = 0;
    uint32_t *data = (uint32_t *)info;

    // 累加魔术数、大小、起始地址、结束地址和采样率
    for (int i = 0; i < sizeof(RecordInfo_t) / sizeof(uint32_t) - 1; i++)
    {
        sum += data[i];
    }

    return sum;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 获取扇区大小的函数
 * @param sector 扇区号
 * @retval 扇区大小（字节）
 */
uint32_t Get_Sector_Size(uint32_t sector)
{
    switch (sector)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        return FLASH_SECTOR_0_SIZE; // 16KB
    case 4:
        return FLASH_SECTOR_4_SIZE; // 64KB
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
        return FLASH_SECTOR_5_SIZE; // 128KB
    default:
        return 0; // 无效扇区
    }
}

/**
 * @brief 获取扇区起始地址的函数
 * @param sector 扇区号
 * @retval 扇区起始地址
 */
uint32_t Get_Sector_Address(uint32_t sector)
{
    switch (sector)
    {
    case 0:
        return ADDR_FLASH_SECTOR_0;
    case 1:
        return ADDR_FLASH_SECTOR_1;
    case 2:
        return ADDR_FLASH_SECTOR_2;
    case 3:
        return ADDR_FLASH_SECTOR_3;
    case 4:
        return ADDR_FLASH_SECTOR_4;
    case 5:
        return ADDR_FLASH_SECTOR_5;
    case 6:
        return ADDR_FLASH_SECTOR_6;
    case 7:
        return ADDR_FLASH_SECTOR_7;
    case 8:
        return ADDR_FLASH_SECTOR_8;
    case 9:
        return ADDR_FLASH_SECTOR_9;
    case 10:
        return ADDR_FLASH_SECTOR_10;
    case 11:
        return ADDR_FLASH_SECTOR_11;
    default:
        return 0; // 无效扇区
    }
}

/**
 * @brief 根据地址获取扇区号
 * @param address Flash地址
 * @retval 扇区号
 */
uint32_t Get_Sector_From_Address(uint32_t address)
{
    if (address < ADDR_FLASH_SECTOR_1)
        return 0;
    else if (address < ADDR_FLASH_SECTOR_2)
        return 1;
    else if (address < ADDR_FLASH_SECTOR_3)
        return 2;
    else if (address < ADDR_FLASH_SECTOR_4)
        return 3;
    else if (address < ADDR_FLASH_SECTOR_5)
        return 4;
    else if (address < ADDR_FLASH_SECTOR_6)
        return 5;
    else if (address < ADDR_FLASH_SECTOR_7)
        return 6;
    else if (address < ADDR_FLASH_SECTOR_8)
        return 7;
    else if (address < ADDR_FLASH_SECTOR_9)
        return 8;
    else if (address < ADDR_FLASH_SECTOR_10)
        return 9;
    else if (address < ADDR_FLASH_SECTOR_11)
        return 10;
    else
        return 11;
}

/**
 * @brief 计算可用的最大录音大小（字节）
 * @retval 最大录音大小（字节）
 */
uint32_t AUDIO_GetMaxRecordSize(void)
{
    uint32_t total_size = 0;

    // 计算从RECORD_SECTOR_START到RECORD_SECTOR_END的总大小
    for (uint32_t sector = RECORD_SECTOR_START; sector <= RECORD_SECTOR_END; sector++)
    {
        total_size += Get_Sector_Size(sector);
    }

    return total_size;
}

/**
 * @brief 初始化音频录制/播放模块
 * @retval None
 */
void AUDIO_Init(void)
{
    // 计算程序区域结束地址
    code_area_end = Get_Sector_Address(CODE_SECTOR_END) + Get_Sector_Size(CODE_SECTOR_END);

    // 初始状态
    dma_running = 0;
    is_recording = 0;
    is_playing = 0;
    record_index = 0;
    play_index = 0;

    // 初始化Flash
    ZuoLan_FLASH_Init();
}

/**
 * @brief 读取录音信息
 * @param info 录音信息结构体指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AUDIO_ReadRecordInfo(RecordInfo_t *info)
{
    // 读取录音信息
    if (!ZuoLan_FLASH_ReadBytes(SIZE_INFO_ADDRESS, (uint8_t *)info, sizeof(RecordInfo_t), 0))
    {
        HMI_Debug_Print("Failed to read record info");
        return HAL_ERROR;
    }

    // 验证魔术数和校验和
    uint32_t calculated_checksum = Calculate_Checksum(info);
    if (info->magic != RECORD_INFO_MAGIC || info->checksum != calculated_checksum)
    {
        HMI_Debug_Print("Record info invalid/corrupted");
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief 写入录音信息
 * @param info 录音信息结构体指针
 * @retval HAL状态
 */
HAL_StatusTypeDef AUDIO_WriteRecordInfo(RecordInfo_t *info)
{
    HAL_StatusTypeDef status;

    // 计算校验和
    info->checksum = Calculate_Checksum(info);

    // 擦除信息扇区
    status = ZuoLan_FLASH_EraseSector(SIZE_INFO_SECTOR);
    if (status != HAL_OK)
    {
        HMI_Debug_Print("Failed to erase info sector");
        return status;
    }

    // 写入录音信息
    __disable_irq();
    status = ZuoLan_FLASH_WriteBytes(SIZE_INFO_ADDRESS, (uint8_t *)info, sizeof(RecordInfo_t), 0);
    __enable_irq();

    if (status != HAL_OK)
    {
        HMI_Debug_Print("Failed to write record info");
        return status;
    }

    return HAL_OK;
}

/**
 * @brief 擦除录音信息
 * @retval HAL状态
 */
HAL_StatusTypeDef AUDIO_EraseRecordInfo(void)
{
    // 擦除信息扇区
    return ZuoLan_FLASH_EraseSector(SIZE_INFO_SECTOR);
}

/**
 * @brief 检查是否有有效的录音
 * @retval 1:有效录音存在 0:无有效录音
 */
uint8_t AUDIO_HasValidRecording(void)
{
    RecordInfo_t info;

    if (AUDIO_ReadRecordInfo(&info) != HAL_OK)
    {
        return 0;
    }

    // 检查录音大小是否合理
    if (info.record_size == 0 || info.record_size > AUDIO_GetMaxRecordSize())
    {
        return 0;
    }

    // 检查地址是否在合理范围
    if (info.start_address < Get_Sector_Address(RECORD_SECTOR_START) ||
        info.end_address > (Get_Sector_Address(RECORD_SECTOR_END) + Get_Sector_Size(RECORD_SECTOR_END)))
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 启动ADC和DAC的DMA传输
 * @retval None
 */
void AUDIO_Start_ADC_DAC_DMA(void)
{
    // 启动ADC DMA (连续模式)
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dac_buffer, BUFFER_SIZE) != HAL_OK)
    {
        HMI_Debug_Print("ADC DMA start failed");
        Error_Handler();
    }
    else
    {
        HMI_Debug_Print("ADC DMA started");
    }

    // 启动DAC DMA (连续模式)
    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)adc_dac_buffer, BUFFER_SIZE, DAC_ALIGN_8B_R) != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hadc1);
        HMI_Debug_Print("DAC DMA start failed");
        Error_Handler();
    }
    else
    {
        HMI_Debug_Print("DAC DMA started");
        dma_running = 1; // 标记DMA已启动
    }
}

/**
 * @brief 停止ADC和DAC的DMA传输
 * @retval None
 */
void AUDIO_Stop_ADC_DAC_DMA(void)
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
HAL_StatusTypeDef AUDIO_Start_Recording(void)
{
    HAL_StatusTypeDef status;

    // 重置调试计数器
    HMI_Debug_Print("Recording starting...");
    debug_write_count = 0;
    total_bytes_written = 0;

    // 停止当前可能正在进行的DMA传输
    if (dma_running)
    {
        AUDIO_Stop_ADC_DAC_DMA();
        HAL_Delay(100); // 给系统一点时间完成停止操作
    }

    // 初始化Flash
    status = ZuoLan_FLASH_Init();
    if (status != HAL_OK)
    {
        HMI_Debug_Print("Flash init failed");
        return status;
    }

    // 擦除扇区4作为录音数据起始扇区
    status = ZuoLan_FLASH_EraseSector(RECORD_SECTOR_START);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        HMI_Debug_Print("Flash erase failed");
        return status;
    }

    // 设置起始地址为扇区4的起始地址
    flash_address = Get_Sector_Address(RECORD_SECTOR_START);
    flash_start_address = flash_address;
    current_sector = RECORD_SECTOR_START;

    // 输出起始地址信息
    HMI_Debug_Print("Recording at sector %d", RECORD_SECTOR_START);

    record_index = 0;
    is_recording = 1;

    // 启动ADC的DMA传输
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dac_buffer, BUFFER_SIZE) != HAL_OK)
    {
        HMI_Debug_Print("ADC DMA start failed");
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    dma_running = 1;
    HMI_Debug_Print("Recording started");

    return HAL_OK;
}

/**
 * @brief 停止录音
 * @retval None
 */
void AUDIO_Stop_Recording(void)
{
    // 如果有剩余数据未写入Flash，写入最后一部分
    uint32_t remaining = record_index % 1024;
    if (remaining > 0)
    {
        // 从低地址向高地址写入
        uint32_t write_address = flash_address;

        // 检查写入地址是否超过最大扇区
        if (current_sector <= RECORD_SECTOR_END)
        {
            // 禁用中断，防止Flash操作被打断
            __disable_irq();

            // 从低地址向高地址写入剩余数据
            ZuoLan_FLASH_WriteBytes(write_address, flash_data_buffer, remaining, 0);

            // 启用中断
            __enable_irq();

            // 更新地址
            flash_address += remaining;

            // 更新调试信息
            HMI_Debug_Print("Saving final %lu bytes", remaining);
            total_bytes_written += remaining;
        }
        else
        {
            HMI_Debug_Print("Warning: Max sector exceeded");
        }
    }

    // 保存真实的录音结束地址（最后写入的地址）
    if (record_index > 0)
    {
        flash_end_address = flash_address;
    }
    else
    {
        flash_end_address = flash_start_address;
    }

    // 停止ADC DMA
    HAL_ADC_Stop_DMA(&hadc1);

    // 保存录音信息
    if (record_index > 0)
    {
        // 填充录音信息结构体
        RecordInfo_t info;
        info.magic = RECORD_INFO_MAGIC;
        info.record_size = record_index;
        info.start_address = flash_start_address;
        info.end_address = flash_end_address;
        info.sample_rate = SAMPLES_PER_SECOND;

        // 写入录音信息
        if (AUDIO_WriteRecordInfo(&info) != HAL_OK)
        {
            HMI_Debug_Print("Failed to save record info");
        }
        else
        {
            HMI_Debug_Print("Record info saved to sector %d", SIZE_INFO_SECTOR);
        }

        // 保存当前录音信息到缓存
        memcpy(&current_record_info, &info, sizeof(RecordInfo_t));
    }

    // 锁定Flash
    HAL_FLASH_Lock();

    dma_running = 0;
    is_recording = 0;

    // 计算使用了多少扇区
    uint32_t start_sector = RECORD_SECTOR_START;
    uint32_t end_sector = Get_Sector_From_Address(flash_end_address - 1);

    // 直接使用计算结果而不是存储到变量
    HMI_Debug_Print("Recording stopped: %lu bytes", record_index);
    HMI_Debug_Print("Used sectors: %lu-%lu (%lu total)",
                    start_sector, end_sector,
                    (end_sector >= start_sector) ? (end_sector - start_sector + 1) : 0);
    HMI_Debug_Print("Duration: %.1f sec", (float)record_index / SAMPLES_PER_SECOND);
}

/**
 * @brief 开始播放Flash中的数据
 * @retval HAL状态
 */
HAL_StatusTypeDef AUDIO_Start_Playing(void)
{
    RecordInfo_t info;

    // 重置计数器
    HMI_Debug_Print("Playback starting...");
    debug_read_count = 0;
    total_bytes_read = 0;

    // 读取录音信息
    if (AUDIO_ReadRecordInfo(&info) != HAL_OK)
    {
        // 如果读取失败，检查是否有旧的录音数据可用
        if (record_index == 0)
        {
            HMI_Debug_Print("No data for playback");
            return HAL_ERROR;
        }

        // 使用当前会话中的录音数据
        info.record_size = record_index;
        info.start_address = flash_start_address;
        info.end_address = flash_end_address;
    }
    else
    {
        // 使用从Flash中读取的录音信息
        record_index = info.record_size;
        flash_start_address = info.start_address;
        flash_end_address = info.end_address;

        HMI_Debug_Print("Record: %lu bytes, %.1f sec",
                        info.record_size, (float)info.record_size / info.sample_rate);
    }

    // 停止当前可能正在进行的DMA传输
    if (dma_running)
    {
        AUDIO_Stop_ADC_DAC_DMA();
        HAL_Delay(100); // 给系统一点时间完成停止操作
    }

    // 计算播放的起始地址（最低地址）
    uint32_t play_start_address = flash_start_address;

    // 读取第一块数据
    uint32_t bytes_to_read = record_index > BUFFER_SIZE ? BUFFER_SIZE : record_index;
    if (!ZuoLan_FLASH_ReadBytes(play_start_address, adc_dac_buffer, bytes_to_read, 0))
    {
        HMI_Debug_Print("Failed to read from Flash");
        return HAL_ERROR;
    }

    HMI_Debug_Print("Read %lu bytes from flash", bytes_to_read);
    total_bytes_read += bytes_to_read;

    is_playing = 1;
    play_index = bytes_to_read;
    flash_address = play_start_address; // 记录当前播放地址

    // 启动DAC DMA输出
    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)adc_dac_buffer,
                          bytes_to_read, DAC_ALIGN_8B_R) != HAL_OK)
    {
        HMI_Debug_Print("Failed to start DAC DMA");
        is_playing = 0;
        return HAL_ERROR;
    }

    dma_running = 1;
    HMI_Debug_Print("Playback started");
    HMI_Debug_Print("Duration: %.1f sec", (float)record_index / SAMPLES_PER_SECOND);

    return HAL_OK;
}

/**
 * @brief 停止播放
 * @retval None
 */
void AUDIO_Stop_Playing(void)
{
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    is_playing = 0;
    dma_running = 0;
    HMI_Debug_Print("Playback stopped");
    HMI_Debug_Print("Read: %lu bytes (%.1f%%)",
                    total_bytes_read, (total_bytes_read * 100.0f) / record_index);
}

/**
 * @brief 测试Flash读写功能
 * @retval HAL状态
 */
HAL_StatusTypeDef AUDIO_Test_Flash(void)
{
#define TEST_DATA_SIZE 1024 // 测试数据大小(字节)

    /* Flash操作状态 */
    HAL_StatusTypeDef status = HAL_OK;
    uint8_t verificationResult = 0;

    /* 测试数据 - 字节数组 */
    uint8_t writeDataArray[TEST_DATA_SIZE]; // 写入测试数据
    uint8_t readDataArray[TEST_DATA_SIZE];  // 读取数据缓冲区

    /* 测试地址 - 使用扇区4起始地址 */
    uint32_t testAddress = Get_Sector_Address(RECORD_SECTOR_START);

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
        HMI_Debug_Print("Flash init failed");
        testSuccess = 0;
        return status;
    }

    /* 擦除测试扇区 */
    HMI_Debug_Print("Erasing sector %d", RECORD_SECTOR_START);
    status = ZuoLan_FLASH_EraseSector(RECORD_SECTOR_START);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock(); // 确保退出前Flash已锁定
        __enable_irq();
        HMI_Debug_Print("Sector erase failed");
        testSuccess = 0;
        return status;
    }

    /* 向Flash写入字节数组 - 从低地址向高地址写入 */
    HMI_Debug_Print("Writing test data");
    status = ZuoLan_FLASH_WriteBytes(testAddress, writeDataArray, TEST_DATA_SIZE, 0);
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
    HMI_Debug_Print("Reading test data");
    verificationResult = ZuoLan_FLASH_ReadBytes(testAddress, readDataArray, TEST_DATA_SIZE, 0);
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
        HMI_Debug_Print("Flash test SUCCESS");
    }
    else
    {
        HMI_Debug_Print("Data validation FAILED");
    }

    return testSuccess ? HAL_OK : HAL_ERROR;
}

/**
 * @brief 上电自动播放检测
 * @retval None
 */
void AUDIO_AutoPlay_OnBoot(void)
{
    // 检查是否有有效的录音
    if (AUDIO_HasValidRecording())
    {
        HMI_Debug_Print("Valid recording detected");
        HMI_Debug_Print("Use 'play' command to play it");
    }
    else
    {
        HMI_Debug_Print("No valid recording found");
    }
}

/**
 * @brief 处理命令
 * @param command 命令字符串
 * @retval None
 */
void AUDIO_Process_Command(const char *command)
{
    if (strcmp(command, "record") == 0)
    {
        if (!is_recording && !is_playing)
        {
            if (AUDIO_Start_Recording() != HAL_OK)
            {
                HMI_Debug_Print("Failed to start recording");
            }
        }
        else if (is_recording)
        {
            HMI_Debug_Print("Already recording");
        }
        else if (is_playing)
        {
            HMI_Debug_Print("Stop playback first");
        }
    }
    else if (strcmp(command, "stop") == 0)
    {
        if (is_recording)
        {
            AUDIO_Stop_Recording();
        }
        else if (is_playing)
        {
            AUDIO_Stop_Playing();
        }
        else
        {
            HMI_Debug_Print("No operation to stop");
        }
    }
    else if (strcmp(command, "play") == 0)
    {
        if (!is_playing && !is_recording)
        {
            if (AUDIO_Start_Playing() != HAL_OK)
            {
                HMI_Debug_Print("Failed to start playback");
            }
        }
        else if (is_playing)
        {
            HMI_Debug_Print("Already playing");
        }
        else if (is_recording)
        {
            HMI_Debug_Print("Stop recording first");
        }
    }
    else if (strcmp(command, "test_flash") == 0)
    {
        if (!is_recording && !is_playing)
        {
            AUDIO_Test_Flash();
            HMI_Debug_Print("Flash test: %s", testSuccess ? "Success" : "Failure");
        }
        else
        {
            HMI_Debug_Print("Stop current operation first");
        }
    }
}

/**
 * @brief ADC DMA传输完成回调函数
 * @param hadc ADC句柄
 * @retval None
 */
void AUDIO_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1 && is_recording)
    {
        // 处理ADC缓冲区的后半部分
        uint32_t halfSize = BUFFER_SIZE / 2;
        uint32_t offset = halfSize;

        // 检查是否超出最大录制空间
        uint32_t max_record_size = AUDIO_GetMaxRecordSize();
        if (record_index >= max_record_size)
        {
            AUDIO_Stop_Recording();
            HMI_Debug_Print("Max recording space reached");
            return;
        }

        // 直接将ADC采样的数据保存到flash_data_buffer中
        for (uint32_t i = 0; i < halfSize && record_index < max_record_size; i++)
        {
            // 直接保存ADC数据（已经是8位）
            flash_data_buffer[record_index % 1024] = adc_dac_buffer[offset + i];
            record_index++;

            // 当缓冲区满时写入Flash
            if (record_index % 1024 == 0)
            {
                // 计算当前扇区结束地址
                uint32_t sector_end_addr = Get_Sector_Address(current_sector) +
                                           Get_Sector_Size(current_sector);
                uint32_t remaining_space = sector_end_addr - flash_address;

                // 检查是否需要跨扇区
                if (remaining_space < 1024)
                {
                    // 先写入当前扇区剩余空间（如果有的话）
                    if (remaining_space > 0)
                    {
                        // 禁用中断，防止Flash操作被打断
                        __disable_irq();

                        // 从低地址向高地址写入
                        ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, remaining_space, 0);

                        // 启用中断
                        __enable_irq();

                        // 更新已写入的数据
                        for (uint32_t j = remaining_space; j < 1024; j++)
                        {
                            flash_data_buffer[j - remaining_space] = flash_data_buffer[j];
                        }

                        // 更新调试信息
                        total_bytes_written += remaining_space;
                    }

                    // 准备下一个扇区
                    current_sector++;

                    // 检查是否超出最大扇区
                    if (current_sector > RECORD_SECTOR_END)
                    {
                        AUDIO_Stop_Recording();
                        HMI_Debug_Print("Max sector reached");
                        return;
                    }

                    // 擦除下一个扇区
                    __disable_irq();
                    HAL_StatusTypeDef status = ZuoLan_FLASH_EraseSector(current_sector);
                    __enable_irq();

                    if (status != HAL_OK)
                    {
                        AUDIO_Stop_Recording();
                        HMI_Debug_Print("Sector %lu erase failed", current_sector);
                        return;
                    }

                    // 设置新扇区的起始地址
                    flash_address = Get_Sector_Address(current_sector);

                    // 写入剩余数据
                    uint32_t remaining_data = 1024 - remaining_space;

                    if (remaining_data > 0)
                    {
                        // 禁用中断，防止Flash操作被打断
                        __disable_irq();

                        // 从低地址向高地址写入
                        ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, remaining_data, 0);

                        // 启用中断
                        __enable_irq();

                        // 更新Flash地址
                        flash_address += remaining_data;

                        // 更新调试信息
                        total_bytes_written += remaining_data;
                    }

                    // 增加写入计数并输出调试信息
                    debug_write_count++;
                    if (debug_write_count % 10 == 0)
                    {
                        HMI_Debug_Print("Sector %lu->%lu written", current_sector - 1, current_sector);
                    }
                }
                else
                {
                    // 有足够空间，直接写入
                    // 禁用中断，防止Flash操作被打断
                    __disable_irq();

                    // 从低地址向高地址写入
                    ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, 1024, 0);

                    // 启用中断
                    __enable_irq();

                    // 更新Flash地址
                    flash_address += 1024;

                    // 增加写入计数并输出调试信息
                    debug_write_count++;
                    if (debug_write_count % 20 == 0)
                    { // 减少输出频率
                        HMI_Debug_Print("Recording: %lu KB", total_bytes_written / 1024);
                    }
                    total_bytes_written += 1024;
                }
            }
        }
    }
}

/**
 * @brief ADC DMA传输一半完成回调函数
 * @param hadc ADC句柄
 * @retval None
 */
void AUDIO_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1 && is_recording)
    {
        // 处理ADC缓冲区的前半部分
        uint32_t halfSize = BUFFER_SIZE / 2;

        // 检查是否超出最大录制空间
        uint32_t max_record_size = AUDIO_GetMaxRecordSize();
        if (record_index >= max_record_size)
        {
            AUDIO_Stop_Recording();
            HMI_Debug_Print("Max recording space reached");
            return;
        }

        // 直接将ADC采样的数据保存到flash_data_buffer中
        for (uint32_t i = 0; i < halfSize && record_index < max_record_size; i++)
        {
            // 直接保存ADC数据（已经是8位）
            flash_data_buffer[record_index % 1024] = adc_dac_buffer[i];
            record_index++;

            // 当缓冲区满时写入Flash
            if (record_index % 1024 == 0)
            {
                // 计算当前扇区结束地址
                uint32_t sector_end_addr = Get_Sector_Address(current_sector) +
                                           Get_Sector_Size(current_sector);
                uint32_t remaining_space = sector_end_addr - flash_address;

                // 检查是否需要跨扇区
                if (remaining_space < 1024)
                {
                    // 先写入当前扇区剩余空间（如果有的话）
                    if (remaining_space > 0)
                    {
                        // 禁用中断，防止Flash操作被打断
                        __disable_irq();

                        // 从低地址向高地址写入
                        ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, remaining_space, 0);

                        // 启用中断
                        __enable_irq();

                        // 更新已写入的数据
                        for (uint32_t j = remaining_space; j < 1024; j++)
                        {
                            flash_data_buffer[j - remaining_space] = flash_data_buffer[j];
                        }

                        // 更新调试信息
                        total_bytes_written += remaining_space;
                    }

                    // 准备下一个扇区
                    current_sector++;

                    // 检查是否超出最大扇区
                    if (current_sector > RECORD_SECTOR_END)
                    {
                        AUDIO_Stop_Recording();
                        HMI_Debug_Print("Max sector reached");
                        return;
                    }

                    // 擦除下一个扇区
                    __disable_irq();
                    HAL_StatusTypeDef status = ZuoLan_FLASH_EraseSector(current_sector);
                    __enable_irq();

                    if (status != HAL_OK)
                    {
                        AUDIO_Stop_Recording();
                        HMI_Debug_Print("Sector %lu erase failed", current_sector);
                        return;
                    }

                    // 设置新扇区的起始地址
                    flash_address = Get_Sector_Address(current_sector);

                    // 写入剩余数据
                    uint32_t remaining_data = 1024 - remaining_space;

                    if (remaining_data > 0)
                    {
                        // 禁用中断，防止Flash操作被打断
                        __disable_irq();

                        // 从低地址向高地址写入
                        ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, remaining_data, 0);

                        // 启用中断
                        __enable_irq();

                        // 更新Flash地址
                        flash_address += remaining_data;

                        // 更新调试信息
                        total_bytes_written += remaining_data;
                    }

                    // 增加写入计数并输出调试信息
                    debug_write_count++;
                    if (debug_write_count % 10 == 0)
                    {
                        HMI_Debug_Print("Sector %lu->%lu written", current_sector - 1, current_sector);
                    }
                }
                else
                {
                    // 有足够空间，直接写入
                    // 禁用中断，防止Flash操作被打断
                    __disable_irq();

                    // 从低地址向高地址写入
                    ZuoLan_FLASH_WriteBytes(flash_address, flash_data_buffer, 1024, 0);

                    // 启用中断
                    __enable_irq();

                    // 更新Flash地址
                    flash_address += 1024;

                    // 增加写入计数并输出调试信息
                    debug_write_count++;
                    if (debug_write_count % 20 == 0)
                    { // 减少输出频率
                        HMI_Debug_Print("Recording: %lu KB", total_bytes_written / 1024);
                    }
                    total_bytes_written += 1024;
                }
            }
        }
    }
}

/**
 * @brief DAC DMA传输完成回调函数
 * @param hdac DAC句柄
 * @retval None
 */
void AUDIO_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
    if (is_playing)
    {
        // 处理DAC缓冲区的后半部分
        uint32_t halfSize = BUFFER_SIZE / 2;
        uint32_t offset = halfSize;

        // 检查是否播放完成
        if (play_index >= record_index)
        {
            AUDIO_Stop_Playing();
            return;
        }

        // 从Flash加载下一块数据
        uint32_t remainingBytes = record_index - play_index;
        uint32_t bytesToRead = remainingBytes > halfSize ? halfSize : remainingBytes;

        // 计算当前Flash地址（从低地址向高地址读取）
        uint32_t current_address = flash_start_address + play_index;

        // 检查边界情况
        if (current_address + bytesToRead > flash_end_address)
        {
            // 直接调整字节数，不使用临时变量
            bytesToRead = (flash_end_address > current_address) ? (flash_end_address - current_address) : 0;

            if (bytesToRead == 0)
            {
                AUDIO_Stop_Playing();
                return;
            }
        }

        // 直接读取数据到DAC缓冲区后半部分
        if (!ZuoLan_FLASH_ReadBytes(current_address, &adc_dac_buffer[offset], bytesToRead, 0))
        {
            AUDIO_Stop_Playing();
            HMI_Debug_Print("Flash read failed");
            return;
        }

        // 记录读取信息
        debug_read_count++;
        if (debug_read_count % 20 == 0)
        {
            HMI_Debug_Print("Playing: %.0f%%", (play_index * 100.0f) / record_index);
        }
        total_bytes_read += bytesToRead;

        // 如果读取的数据不足半缓冲区大小，其余部分填充0
        if (bytesToRead < halfSize)
        {
            for (uint32_t i = bytesToRead; i < halfSize; i++)
            {
                adc_dac_buffer[offset + i] = 0;
            }
        }

        // 更新索引
        play_index += bytesToRead;
    }
}

/**
 * @brief DAC DMA传输一半完成回调函数
 * @param hdac DAC句柄
 * @retval None
 */
void AUDIO_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
    if (is_playing)
    {
        // 处理DAC缓冲区的前半部分
        uint32_t halfSize = BUFFER_SIZE / 2;

        // 检查是否播放完成
        if (play_index >= record_index)
        {
            AUDIO_Stop_Playing();
            return;
        }

        // 从Flash加载下一块数据
        uint32_t remainingBytes = record_index - play_index;
        uint32_t bytesToRead = remainingBytes > halfSize ? halfSize : remainingBytes;

        // 计算当前Flash地址（从低地址向高地址读取）
        uint32_t current_address = flash_start_address + play_index;

        // 检查边界情况
        if (current_address + bytesToRead > flash_end_address)
        {
            // 直接调整字节数，不使用临时变量
            bytesToRead = (flash_end_address > current_address) ? (flash_end_address - current_address) : 0;

            if (bytesToRead == 0)
            {
                AUDIO_Stop_Playing();
                return;
            }
        }

        // 直接读取数据到DAC缓冲区前半部分
        if (!ZuoLan_FLASH_ReadBytes(current_address, adc_dac_buffer, bytesToRead, 0))
        {
            AUDIO_Stop_Playing();
            HMI_Debug_Print("Flash read failed");
            return;
        }

        // 记录读取信息
        debug_read_count++;
        if (debug_read_count % 20 == 0)
        {
            HMI_Debug_Print("Playing: %.0f%%", (play_index * 100.0f) / record_index);
        }
        total_bytes_read += bytesToRead;

        // 如果读取的数据不足半缓冲区大小，其余部分填充0
        if (bytesToRead < halfSize)
        {
            for (uint32_t i = bytesToRead; i < halfSize; i++)
            {
                adc_dac_buffer[i] = 0;
            }
        }

        // 更新索引
        play_index += bytesToRead;
    }
}
