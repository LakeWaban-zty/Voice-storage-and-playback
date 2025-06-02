/* zuolan_inside_flash.c */
#include "zuolan_inside_flash.h"

/**
 * @brief  初始化Flash操作
 * @param  无
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_Init(void)
{
    HAL_StatusTypeDef status;

    /* 解锁Flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return status;
    }

    /* 清除所有Flash标志位 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    return HAL_OK;
}

/**
 * @brief  擦除指定扇区
 * @param  sector: 要擦除的扇区号
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_EraseSector(uint32_t sector)
{
    HAL_StatusTypeDef status;
    uint32_t sectorError = 0;

    /* 检查扇区号是否有效 */
    if (sector > FLASH_SECTOR_11)
    {
        return HAL_ERROR;
    }

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* 根据系统电压选择 */
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1;

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &sectorError);
    if (status != HAL_OK || sectorError != 0xFFFFFFFF)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief  擦除多个扇区（倒序：从高扇区号到低扇区号）
 * @param  startSector: 起始扇区号（高扇区号）
 * @param  endSector: 结束扇区号（低扇区号）
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_EraseMultiSectors_Reverse(uint32_t startSector, uint32_t endSector)
{
    HAL_StatusTypeDef status = HAL_OK;

    /* 检查参数有效性 */
    if (startSector > FLASH_SECTOR_11 || endSector > FLASH_SECTOR_11 || startSector < endSector)
    {
        return HAL_ERROR;
    }

    /* 倒序擦除扇区（从高扇区号到低扇区号） */
    for (int32_t sector = startSector; sector >= (int32_t)endSector; sector--)
    {
        status = ZuoLan_FLASH_EraseSector(sector);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    return HAL_OK;
}

/**
 * @brief  擦除多个扇区（正序：从低扇区号到高扇区号）
 * @param  startSector: 起始扇区号（低扇区号）
 * @param  endSector: 结束扇区号（高扇区号）
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_EraseMultiSectors(uint32_t startSector, uint32_t endSector)
{
    HAL_StatusTypeDef status = HAL_OK;

    /* 检查参数有效性 */
    if (startSector > FLASH_SECTOR_11 || endSector > FLASH_SECTOR_11 || startSector > endSector)
    {
        return HAL_ERROR;
    }

    /* 正序擦除扇区（从低扇区号到高扇区号） */
    for (uint32_t sector = startSector; sector <= endSector; sector++)
    {
        status = ZuoLan_FLASH_EraseSector(sector);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    return HAL_OK;
}

/**
 * @brief  写入数据到Flash
 * @param  address: 写入的起始地址
 * @param  data: 要写入的数据数组
 * @param  dataSize: 数据数组长度（单位：字，4字节）
 * @param  reverse: 1-从高地址向低地址写入，0-从低地址向高地址写入
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_WriteData(uint32_t address, uint32_t *data, uint32_t dataSize, uint8_t reverse)
{
    HAL_StatusTypeDef status = HAL_OK;

    /* 检查地址是否在Flash范围内 */
    if (address < FLASH_BASE_ADDR || address > FLASH_END_ADDR)
    {
        return HAL_ERROR;
    }

    /* 确保地址4字节对齐 */
    address = address & 0xFFFFFFFC;

    if (reverse)
    {
        /* 从高地址向低地址写入 */
        for (int32_t i = dataSize - 1; i >= 0; i--)
        {
            uint32_t currentAddress = address - ((dataSize - 1 - i) * 4);

            /* 检查地址是否在Flash范围内 */
            if (currentAddress < FLASH_BASE_ADDR)
            {
                return HAL_ERROR;
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddress, data[i]);
            if (status != HAL_OK)
            {
                return status;
            }
        }
    }
    else
    {
        /* 从低地址向高地址写入 */
        for (uint32_t i = 0; i < dataSize; i++)
        {
            uint32_t currentAddress = address + (i * 4);

            /* 检查地址是否在Flash范围内 */
            if (currentAddress > FLASH_END_ADDR)
            {
                return HAL_ERROR;
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddress, data[i]);
            if (status != HAL_OK)
            {
                return status;
            }
        }
    }

    return HAL_OK;
}

/**
 * @brief  从Flash读取数据
 * @param  address: 读取的起始地址
 * @param  data: 存储读取数据的数组
 * @param  dataSize: 要读取的数据长度（单位：字，4字节）
 * @param  reverse: 1-从高地址向低地址读取，0-从低地址向高地址读取
 * @retval 1-读取成功，0-读取失败
 */
uint8_t ZuoLan_FLASH_ReadData(uint32_t address, uint32_t *data, uint32_t dataSize, uint8_t reverse)
{
    /* 检查地址是否在Flash范围内 */
    if (address < FLASH_BASE_ADDR || address > FLASH_END_ADDR)
    {
        return 0;
    }

    /* 确保地址4字节对齐 */
    address = address & 0xFFFFFFFC;

    if (reverse)
    {
        /* 从高地址向低地址读取 */
        for (int32_t i = dataSize - 1; i >= 0; i--)
        {
            uint32_t currentAddress = address - ((dataSize - 1 - i) * 4);

            /* 检查地址是否在Flash范围内 */
            if (currentAddress < FLASH_BASE_ADDR)
            {
                return 0;
            }

            data[i] = *(__IO uint32_t *)(currentAddress);
        }
    }
    else
    {
        /* 从低地址向高地址读取 */
        for (uint32_t i = 0; i < dataSize; i++)
        {
            uint32_t currentAddress = address + (i * 4);

            /* 检查地址是否在Flash范围内 */
            if (currentAddress > FLASH_END_ADDR)
            {
                return 0;
            }

            data[i] = *(__IO uint32_t *)(currentAddress);
        }
    }

    return 1;
}
/**
 * @brief  获取扇区的起始地址
 * @param  sector: 扇区号
 * @retval 扇区起始地址
 */
uint32_t ZuoLan_FLASH_GetSectorStartAddress(uint32_t sector)
{
    switch (sector)
    {
    case FLASH_SECTOR_0:
        return ADDR_FLASH_SECTOR_0;
    case FLASH_SECTOR_1:
        return ADDR_FLASH_SECTOR_1;
    case FLASH_SECTOR_2:
        return ADDR_FLASH_SECTOR_2;
    case FLASH_SECTOR_3:
        return ADDR_FLASH_SECTOR_3;
    case FLASH_SECTOR_4:
        return ADDR_FLASH_SECTOR_4;
    case FLASH_SECTOR_5:
        return ADDR_FLASH_SECTOR_5;
    case FLASH_SECTOR_6:
        return ADDR_FLASH_SECTOR_6;
    case FLASH_SECTOR_7:
        return ADDR_FLASH_SECTOR_7;
    case FLASH_SECTOR_8:
        return ADDR_FLASH_SECTOR_8;
    case FLASH_SECTOR_9:
        return ADDR_FLASH_SECTOR_9;
    case FLASH_SECTOR_10:
        return ADDR_FLASH_SECTOR_10;
    case FLASH_SECTOR_11:
        return ADDR_FLASH_SECTOR_11;
    default:
        return 0; /* 无效扇区号 */
    }
}

/**
 * @brief  获取扇区的大小
 * @param  sector: 扇区号
 * @retval 扇区大小
 */
uint32_t ZuoLan_FLASH_GetSectorSize(uint32_t sector)
{
    switch (sector)
    {
    case FLASH_SECTOR_0:
        return FLASH_SECTOR_0_SIZE;
    case FLASH_SECTOR_1:
        return FLASH_SECTOR_1_SIZE;
    case FLASH_SECTOR_2:
        return FLASH_SECTOR_2_SIZE;
    case FLASH_SECTOR_3:
        return FLASH_SECTOR_3_SIZE;
    case FLASH_SECTOR_4:
        return FLASH_SECTOR_4_SIZE;
    case FLASH_SECTOR_5:
        return FLASH_SECTOR_5_SIZE;
    case FLASH_SECTOR_6:
        return FLASH_SECTOR_6_SIZE;
    case FLASH_SECTOR_7:
        return FLASH_SECTOR_7_SIZE;
    case FLASH_SECTOR_8:
        return FLASH_SECTOR_8_SIZE;
    case FLASH_SECTOR_9:
        return FLASH_SECTOR_9_SIZE;
    case FLASH_SECTOR_10:
        return FLASH_SECTOR_10_SIZE;
    case FLASH_SECTOR_11:
        return FLASH_SECTOR_11_SIZE;
    default:
        return 0; /* 无效扇区号 */
    }
}

/**
 * @brief  根据地址获取扇区号
 * @param  address: Flash地址
 * @retval 扇区号，如果地址无效则返回0xFF
 */
uint32_t ZuoLan_FLASH_GetSectorNumber(uint32_t address)
{
    /* 检查地址是否在Flash范围内 */
    if (address < FLASH_BASE_ADDR || address >= (FLASH_END_ADDR + 1))
    {
        return 0xFF;
    }

    /* 根据地址范围确定扇区号 */
    if (address < ADDR_FLASH_SECTOR_1)
        return FLASH_SECTOR_0;
    else if (address < ADDR_FLASH_SECTOR_2)
        return FLASH_SECTOR_1;
    else if (address < ADDR_FLASH_SECTOR_3)
        return FLASH_SECTOR_2;
    else if (address < ADDR_FLASH_SECTOR_4)
        return FLASH_SECTOR_3;
    else if (address < ADDR_FLASH_SECTOR_5)
        return FLASH_SECTOR_4;
    else if (address < ADDR_FLASH_SECTOR_6)
        return FLASH_SECTOR_5;
    else if (address < ADDR_FLASH_SECTOR_7)
        return FLASH_SECTOR_6;
    else if (address < ADDR_FLASH_SECTOR_8)
        return FLASH_SECTOR_7;
    else if (address < ADDR_FLASH_SECTOR_9)
        return FLASH_SECTOR_8;
    else if (address < ADDR_FLASH_SECTOR_10)
        return FLASH_SECTOR_9;
    else if (address < ADDR_FLASH_SECTOR_11)
        return FLASH_SECTOR_10;
    else
        return FLASH_SECTOR_11;
}

/**
 * @brief  自动跨扇区写入数据到Flash
 * @param  address: 写入的起始地址
 * @param  data: 要写入的数据数组
 * @param  dataSize: 数据数组长度（单位：字，4字节）
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_WriteDataAutoSector(uint32_t address, uint32_t *data, uint32_t dataSize)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t remainBytes = dataSize * 4; // 总共需要写入的字节数
    uint32_t bytesWritten = 0;           // 已写入的字节数
    uint32_t currentSector;
    uint32_t sectorEndAddr;
    uint32_t currentAddr = address;
    uint32_t bytesToWriteInSector;
    uint32_t wordsToWriteInSector;

    /* 检查起始地址是否在Flash范围内 */
    if (currentAddr < FLASH_BASE_ADDR || currentAddr > FLASH_END_ADDR)
    {
        return HAL_ERROR;
    }

    /* 确保地址4字节对齐 */
    currentAddr = currentAddr & 0xFFFFFFFC;

    while (remainBytes > 0)
    {
        /* 获取当前地址所在的扇区号 */
        currentSector = ZuoLan_FLASH_GetSectorNumber(currentAddr);
        if (currentSector == 0xFF)
        {
            return HAL_ERROR; // 无效地址
        }

        /* 获取当前扇区的结束地址 */
        sectorEndAddr = ZuoLan_FLASH_GetSectorStartAddress(currentSector) +
                        ZuoLan_FLASH_GetSectorSize(currentSector) - 1;

        /* 计算在当前扇区内可以写入的字节数 */
        if ((sectorEndAddr - currentAddr + 1) >= remainBytes)
        {
            bytesToWriteInSector = remainBytes;
        }
        else
        {
            bytesToWriteInSector = sectorEndAddr - currentAddr + 1;
        }

        /* 确保写入的字节数是4的倍数 */
        bytesToWriteInSector = bytesToWriteInSector & 0xFFFFFFFC;

        /* 计算要写入的字数 */
        wordsToWriteInSector = bytesToWriteInSector / 4;

        if (wordsToWriteInSector > 0)
        {
            /* 擦除当前扇区 */
            status = ZuoLan_FLASH_EraseSector(currentSector);
            if (status != HAL_OK)
            {
                return status;
            }

            /* 在当前扇区写入数据 */
            for (uint32_t i = 0; i < wordsToWriteInSector; i++)
            {
                uint32_t dataIndex = (bytesWritten / 4) + i;

                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                           currentAddr + (i * 4),
                                           data[dataIndex]);
                if (status != HAL_OK)
                {
                    return status;
                }
            }

            /* 更新计数器 */
            bytesWritten += bytesToWriteInSector;
            remainBytes -= bytesToWriteInSector;
            currentAddr += bytesToWriteInSector;
        }
        else
        {
            /* 当前扇区没有足够空间写入至少一个字(4字节) */
            currentAddr = ZuoLan_FLASH_GetSectorStartAddress(currentSector + 1);
            if (currentAddr > FLASH_END_ADDR)
            {
                return HAL_ERROR; // 超出Flash范围
            }
        }
    }

    return HAL_OK;
}

/**
 * @brief  自动跨扇区读取Flash数据
 * @param  address: 读取的起始地址
 * @param  data: 存储读取数据的数组
 * @param  dataSize: 要读取的数据长度（单位：字，4字节）
 * @retval 1-读取成功，0-读取失败
 */
uint8_t ZuoLan_FLASH_ReadDataAutoSector(uint32_t address, uint32_t *data, uint32_t dataSize)
{
    //uint32_t remainBytes = dataSize * 4; // 总共需要读取的字节数
    uint32_t bytesRead = 0;              // 已读取的字节数
    uint32_t currentAddr = address;

    /* 检查起始地址是否在Flash范围内 */
    if (currentAddr < FLASH_BASE_ADDR || currentAddr > FLASH_END_ADDR)
    {
        return 0;
    }

    /* 确保地址4字节对齐 */
    currentAddr = currentAddr & 0xFFFFFFFC;

    /* 从指定地址开始连续读取指定数量的字 */
    for (uint32_t i = 0; i < dataSize; i++)
    {
        /* 检查当前读取地址是否超出Flash范围 */
        if (currentAddr > FLASH_END_ADDR)
        {
            return 0;
        }

        /* 读取一个字(4字节) */
        data[i] = *(__IO uint32_t *)(currentAddr);

        /* 更新计数器和地址 */
        bytesRead += 4;
        currentAddr += 4;

        /* 检查是否已读取完所有数据 */
        if (bytesRead >= (dataSize * 4))
        {
            break;
        }
    }

    return 1;
}
/**
 * @brief  写入字节数据到Flash
 * @param  address: 写入的起始地址
 * @param  data: 要写入的字节数据数组(uint8_t)
 * @param  dataSize: 数据数组长度（单位：字节）
 * @param  reverse: 1-从高地址向低地址写入，0-从低地址向高地址写入
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_WriteBytes(uint32_t address, uint8_t *data, uint32_t dataSize, uint8_t reverse)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t tempWord = 0;
    uint32_t originalAddress = address;
    uint32_t alignedAddress;
    uint32_t alignOffset;
    uint32_t remainingBytes = dataSize;
    uint32_t i = 0;

    /* 检查地址是否在Flash范围内 */
    if (address < FLASH_BASE_ADDR || (address + dataSize - 1) > FLASH_END_ADDR)
    {
        return HAL_ERROR;
    }

    /* 计算对齐地址和偏移 */
    alignedAddress = address & 0xFFFFFFFC; /* 向下对齐到4字节边界 */
    alignOffset = address & 0x00000003;    /* 获取地址在字内的偏移量 */

    if (!reverse)
    {
        /* 从低地址向高地址写入 */

        /* 处理不对齐的起始部分 */
        if (alignOffset != 0)
        {
            /* 读取当前地址所在的整个字 */
            tempWord = *(__IO uint32_t *)alignedAddress;

            /* 填充这个字中的数据（从偏移量开始） */
            for (i = 0; i < (4 - alignOffset) && i < dataSize; i++)
            {
                ((uint8_t *)&tempWord)[alignOffset + i] = data[i];
            }

            /* 写回修改后的字 */
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, alignedAddress, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }

            /* 更新已处理的字节数和起始地址 */
            remainingBytes -= i;
            address += i;
        }

        /* 处理对齐的完整字 */
        while (remainingBytes >= 4)
        {
            tempWord = 0;
            for (i = 0; i < 4; i++)
            {
                ((uint8_t *)&tempWord)[i] = data[dataSize - remainingBytes + i];
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }

            remainingBytes -= 4;
            address += 4;
        }

        /* 处理剩余的不足一个字的字节 */
        if (remainingBytes > 0)
        {
            tempWord = *(__IO uint32_t *)address;

            for (i = 0; i < remainingBytes; i++)
            {
                ((uint8_t *)&tempWord)[i] = data[dataSize - remainingBytes + i];
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }
        }
    }
    else
    {
        /* 从高地址向低地址写入 */
        uint32_t endAddress = originalAddress + dataSize - 1;
        uint32_t alignedEndAddress = (endAddress & 0xFFFFFFFC);
        uint32_t endAlignOffset = endAddress & 0x00000003;

        /* 处理不对齐的结束部分 */
        if (endAlignOffset < 3)
        {
            tempWord = *(__IO uint32_t *)alignedEndAddress;

            for (i = 0; i <= endAlignOffset && i < dataSize; i++)
            {
                ((uint8_t *)&tempWord)[endAlignOffset - i] = data[dataSize - 1 - i];
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, alignedEndAddress, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }

            remainingBytes -= (i);
        }

        /* 处理对齐的完整字（从高地址向低地址） */
        uint32_t currentWordAddr = alignedEndAddress - 4;
        while (remainingBytes >= 4 && currentWordAddr >= alignedAddress)
        {
            tempWord = 0;
            for (i = 0; i < 4; i++)
            {
                ((uint8_t *)&tempWord)[3 - i] = data[remainingBytes - 4 + i];
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentWordAddr, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }

            remainingBytes -= 4;
            currentWordAddr -= 4;
        }

        /* 处理剩余的不足一个字的字节 */
        if (remainingBytes > 0 && alignOffset > 0)
        {
            tempWord = *(__IO uint32_t *)alignedAddress;

            for (i = 0; i < alignOffset && i < remainingBytes; i++)
            {
                ((uint8_t *)&tempWord)[alignOffset - 1 - i] = data[i];
            }

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, alignedAddress, tempWord);
            if (status != HAL_OK)
            {
                return status;
            }
        }
    }

    return HAL_OK;
}

/**
 * @brief  从Flash读取字节数据
 * @param  address: 读取的起始地址
 * @param  data: 存储读取数据的字节数组(uint8_t)
 * @param  dataSize: 要读取的数据长度（单位：字节）
 * @param  reverse: 1-从高地址向低地址读取，0-从低地址向高地址读取
 * @retval 1-读取成功，0-读取失败
 */
uint8_t ZuoLan_FLASH_ReadBytes(uint32_t address, uint8_t *data, uint32_t dataSize, uint8_t reverse)
{
    uint32_t tempWord;
    uint32_t currentAddr;
    uint32_t i = 0;

    /* 检查地址是否在Flash范围内 */
    if (address < FLASH_BASE_ADDR || (address + dataSize - 1) > FLASH_END_ADDR)
    {
        return 0;
    }

    if (reverse)
    {
        /* 从高地址向低地址读取 */
        currentAddr = address + dataSize - 1;
        for (i = 0; i < dataSize; i++)
        {
            /* 计算当前字的地址和字内偏移 */
            uint32_t alignedAddr = currentAddr & 0xFFFFFFFC;
            uint8_t offset = currentAddr & 0x3;

            /* 读取一个字(4字节) */
            tempWord = *(__IO uint32_t *)(alignedAddr);

            /* 提取对应的字节 */
            data[i] = ((uint8_t *)&tempWord)[offset];

            /* 更新地址 */
            currentAddr--;
        }
    }
    else
    {
        /* 从低地址向高地址读取 */
        currentAddr = address;
        for (i = 0; i < dataSize; i++)
        {
            /* 计算当前字的地址和字内偏移 */
            uint32_t alignedAddr = currentAddr & 0xFFFFFFFC;
            uint8_t offset = currentAddr & 0x3;

            /* 读取一个字(4字节) */
            tempWord = *(__IO uint32_t *)(alignedAddr);

            /* 提取对应的字节 */
            data[i] = ((uint8_t *)&tempWord)[offset];

            /* 更新地址 */
            currentAddr++;
        }
    }

    return 1;
}

/**
 * @brief  自动跨扇区写入字节数据到Flash
 * @param  address: 写入的起始地址
 * @param  data: 要写入的字节数据数组(uint8_t)
 * @param  dataSize: 数据数组长度（单位：字节）
 * @retval HAL状态
 */
HAL_StatusTypeDef ZuoLan_FLASH_WriteBytesAutoSector(uint32_t address, uint8_t *data, uint32_t dataSize)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t remainBytes = dataSize; // 总共需要写入的字节数
    uint32_t bytesWritten = 0;       // 已写入的字节数
    uint32_t currentSector;
    uint32_t sectorEndAddr;
    uint32_t currentAddr = address;
    uint32_t bytesToWriteInSector;
    uint32_t tempWord;

    /* 检查起始地址是否在Flash范围内 */
    if (currentAddr < FLASH_BASE_ADDR || currentAddr > FLASH_END_ADDR)
    {
        return HAL_ERROR;
    }

    while (remainBytes > 0)
    {
        /* 获取当前地址所在的扇区号 */
        currentSector = ZuoLan_FLASH_GetSectorNumber(currentAddr);
        if (currentSector == 0xFF)
        {
            return HAL_ERROR; // 无效地址
        }

        /* 获取当前扇区的结束地址 */
        sectorEndAddr = ZuoLan_FLASH_GetSectorStartAddress(currentSector) +
                        ZuoLan_FLASH_GetSectorSize(currentSector) - 1;

        /* 计算在当前扇区内可以写入的字节数 */
        if ((sectorEndAddr - currentAddr + 1) >= remainBytes)
        {
            bytesToWriteInSector = remainBytes;
        }
        else
        {
            bytesToWriteInSector = sectorEndAddr - currentAddr + 1;
        }

        if (bytesToWriteInSector > 0)
        {
            /* 擦除当前扇区 */
            status = ZuoLan_FLASH_EraseSector(currentSector);
            if (status != HAL_OK)
            {
                return status;
            }

            /* 处理不对齐的起始部分 */
            uint32_t alignedAddr = currentAddr & 0xFFFFFFFC;
            uint32_t alignOffset = currentAddr & 0x3;

            if (alignOffset != 0)
            {
                /* 读取当前地址所在的整个字 */
                tempWord = *(__IO uint32_t *)alignedAddr;

                /* 填充这个字中的数据（从偏移量开始） */
                uint32_t bytesToFill = (4 - alignOffset) < bytesToWriteInSector ? (4 - alignOffset) : bytesToWriteInSector;
                for (uint32_t i = 0; i < bytesToFill; i++)
                {
                    ((uint8_t *)&tempWord)[alignOffset + i] = data[bytesWritten + i];
                }

                /* 写回修改后的字 */
                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, alignedAddr, tempWord);
                if (status != HAL_OK)
                {
                    return status;
                }

                /* 更新计数器 */
                bytesWritten += bytesToFill;
                remainBytes -= bytesToFill;
                currentAddr += bytesToFill;
                bytesToWriteInSector -= bytesToFill;
            }

            /* 处理完整字对齐的部分 */
            while (bytesToWriteInSector >= 4)
            {
                /* 组装要写入的字 */
                tempWord = 0;
                for (uint32_t i = 0; i < 4; i++)
                {
                    ((uint8_t *)&tempWord)[i] = data[bytesWritten + i];
                }

                /* 写入一个字 */
                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddr, tempWord);
                if (status != HAL_OK)
                {
                    return status;
                }

                /* 更新计数器 */
                bytesWritten += 4;
                remainBytes -= 4;
                currentAddr += 4;
                bytesToWriteInSector -= 4;
            }

            /* 处理剩余的不足一个字的字节 */
            if (bytesToWriteInSector > 0)
            {
                tempWord = *(__IO uint32_t *)currentAddr;

                for (uint32_t i = 0; i < bytesToWriteInSector; i++)
                {
                    ((uint8_t *)&tempWord)[i] = data[bytesWritten + i];
                }

                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddr, tempWord);
                if (status != HAL_OK)
                {
                    return status;
                }

                /* 更新计数器 */
                bytesWritten += bytesToWriteInSector;
                remainBytes -= bytesToWriteInSector;
                currentAddr += bytesToWriteInSector;
            }
        }
        else
        {
            /* 当前扇区没有足够空间写入任何字节 */
            currentAddr = ZuoLan_FLASH_GetSectorStartAddress(currentSector + 1);
            if (currentAddr > FLASH_END_ADDR)
            {
                return HAL_ERROR; // 超出Flash范围
            }
        }
    }

    return HAL_OK;
}

/**
 * @brief  自动跨扇区读取Flash字节数据
 * @param  address: 读取的起始地址
 * @param  data: 存储读取数据的字节数组(uint8_t)
 * @param  dataSize: 要读取的数据长度（单位：字节）
 * @retval 1-读取成功，0-读取失败
 */
uint8_t ZuoLan_FLASH_ReadBytesAutoSector(uint32_t address, uint8_t *data, uint32_t dataSize)
{
    uint32_t currentAddr = address;
    uint32_t readBytes = 0;
    uint32_t tempWord;

    /* 检查起始地址是否在Flash范围内 */
    if (currentAddr < FLASH_BASE_ADDR || currentAddr > FLASH_END_ADDR)
    {
        return 0;
    }

    /* 按字节读取数据 */
    while (readBytes < dataSize)
    {
        /* 检查当前读取地址是否超出Flash范围 */
        if (currentAddr > FLASH_END_ADDR)
        {
            return 0;
        }

        /* 计算当前字的地址和字内偏移 */
        uint32_t alignedAddr = currentAddr & 0xFFFFFFFC;
        uint8_t offset = currentAddr & 0x3;

        /* 读取一个字(4字节) */
        tempWord = *(__IO uint32_t *)(alignedAddr);

        /* 提取对应的字节 */
        data[readBytes] = ((uint8_t *)&tempWord)[offset];

        /* 更新计数器和地址 */
        readBytes++;
        currentAddr++;

        /* 检查是否已读取完所有数据 */
        if (readBytes >= dataSize)
        {
            break;
        }
    }

    return 1;
}
