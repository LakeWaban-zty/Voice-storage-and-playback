/* zuolan_inside_flash.h */

#ifndef __ZUOLAN_INSIDE_FLASH_H
#define __ZUOLAN_INSIDE_FLASH_H

#ifdef __cplusplus
extern "C"
{
#endif

/* 包含需要的头文件 */
#include "stm32f4xx_hal.h"

/* Flash基地址和结束地址定义 */
#define FLASH_BASE_ADDR 0x08000000 /* Flash基地址 */
#define FLASH_END_ADDR 0x080FFFFF  /* Flash结束地址 - 1MB */

/* STM32F4xx Flash扇区地址定义 */
/* Bank 1 */
#define ADDR_FLASH_SECTOR_0 0x08000000 /* 扇区0: 16KB */
#define ADDR_FLASH_SECTOR_1 0x08004000 /* 扇区1: 16KB */
#define ADDR_FLASH_SECTOR_2 0x08008000 /* 扇区2: 16KB */
#define ADDR_FLASH_SECTOR_3 0x0800C000 /* 扇区3: 16KB */
#define ADDR_FLASH_SECTOR_4 0x08010000 /* 扇区4: 64KB */
#define ADDR_FLASH_SECTOR_5 0x08020000 /* 扇区5: 128KB */
#define ADDR_FLASH_SECTOR_6 0x08040000 /* 扇区6: 128KB */
#define ADDR_FLASH_SECTOR_7 0x08060000 /* 扇区7: 128KB */
/* Bank 2 */
#define ADDR_FLASH_SECTOR_8 0x08080000  /* 扇区8: 128KB */
#define ADDR_FLASH_SECTOR_9 0x080A0000  /* 扇区9: 128KB */
#define ADDR_FLASH_SECTOR_10 0x080C0000 /* 扇区10: 128KB */
#define ADDR_FLASH_SECTOR_11 0x080E0000 /* 扇区11: 128KB */

/* STM32F4xx Flash扇区大小定义 */
#define FLASH_SECTOR_0_SIZE 0x4000   /* 16KB */
#define FLASH_SECTOR_1_SIZE 0x4000   /* 16KB */
#define FLASH_SECTOR_2_SIZE 0x4000   /* 16KB */
#define FLASH_SECTOR_3_SIZE 0x4000   /* 16KB */
#define FLASH_SECTOR_4_SIZE 0x10000  /* 64KB */
#define FLASH_SECTOR_5_SIZE 0x20000  /* 128KB */
#define FLASH_SECTOR_6_SIZE 0x20000  /* 128KB */
#define FLASH_SECTOR_7_SIZE 0x20000  /* 128KB */
#define FLASH_SECTOR_8_SIZE 0x20000  /* 128KB */
#define FLASH_SECTOR_9_SIZE 0x20000  /* 128KB */
#define FLASH_SECTOR_10_SIZE 0x20000 /* 128KB */
#define FLASH_SECTOR_11_SIZE 0x20000 /* 128KB */

    /* 函数原型声明 */
    HAL_StatusTypeDef ZuoLan_FLASH_Init(void);
    HAL_StatusTypeDef ZuoLan_FLASH_EraseSector(uint32_t sector);
    HAL_StatusTypeDef ZuoLan_FLASH_EraseMultiSectors(uint32_t startSector, uint32_t endSector);
    HAL_StatusTypeDef ZuoLan_FLASH_EraseMultiSectors_Reverse(uint32_t startSector, uint32_t endSector);

    /* 原始32位数据操作函数 */
    HAL_StatusTypeDef ZuoLan_FLASH_WriteData(uint32_t address, uint32_t *data, uint32_t dataSize, uint8_t reverse);
    uint8_t ZuoLan_FLASH_ReadData(uint32_t address, uint32_t *data, uint32_t dataSize, uint8_t reverse);
    HAL_StatusTypeDef ZuoLan_FLASH_WriteDataAutoSector(uint32_t address, uint32_t *data, uint32_t dataSize);
    uint8_t ZuoLan_FLASH_ReadDataAutoSector(uint32_t address, uint32_t *data, uint32_t dataSize);

    /* 新增的字节级(uchar)数据操作函数 */
    HAL_StatusTypeDef ZuoLan_FLASH_WriteBytes(uint32_t address, uint8_t *data, uint32_t dataSize, uint8_t reverse);
    uint8_t ZuoLan_FLASH_ReadBytes(uint32_t address, uint8_t *data, uint32_t dataSize, uint8_t reverse);
    HAL_StatusTypeDef ZuoLan_FLASH_WriteBytesAutoSector(uint32_t address, uint8_t *data, uint32_t dataSize);
    uint8_t ZuoLan_FLASH_ReadBytesAutoSector(uint32_t address, uint8_t *data, uint32_t dataSize);

    /* 工具函数 */
    uint32_t ZuoLan_FLASH_GetSectorStartAddress(uint32_t sector);
    uint32_t ZuoLan_FLASH_GetSectorSize(uint32_t sector);
    uint32_t ZuoLan_FLASH_GetSectorNumber(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif /* __ZUOLAN_INSIDE_FLASH_H */
