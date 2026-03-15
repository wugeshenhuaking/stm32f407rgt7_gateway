#ifndef __BSP_SRAM_PORT_H
#define __BSP_SRAM_PORT_H

/*
*********************************************************************************************************
*
*   模块名称 : SRAM 接口层
*   文件名称 : bsp_sram_port.h
*   说    明 : 提供平台无关的 SRAM 读写接口。
*              上层代码（应用、测试）只调用本层函数，不直接操作硬件宏。
*
*   接口列表 :
*       BSP_SRAM_Init()          初始化（调用硬件层）
*       BSP_SRAM_WriteByte()     写 1 字节
*       BSP_SRAM_ReadByte()      读 1 字节
*       BSP_SRAM_Write()         写 n 字节
*       BSP_SRAM_Read()          读 n 字节
*       BSP_SRAM_Fill()          填充区域为固定值
*       BSP_SRAM_GetSize()       获取 SRAM 总容量
*       BSP_SRAM_GetBaseAddr()   获取 SRAM 基地址
*
*********************************************************************************************************
*/

#include "bsp_sram.h"

/* ============================================================
 * 函数声明
 * ============================================================ */
void     BSP_SRAM_Init(void);

void     BSP_SRAM_WriteByte(uint32_t addr, uint8_t data);
uint8_t  BSP_SRAM_ReadByte(uint32_t addr);

void     BSP_SRAM_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
void     BSP_SRAM_Read(uint32_t addr, uint8_t *buf, uint32_t len);

void     BSP_SRAM_Fill(uint32_t addr, uint8_t val, uint32_t len);

uint32_t BSP_SRAM_GetSize(void);
uint32_t BSP_SRAM_GetBaseAddr(void);

#endif /* __BSP_SRAM_PORT_H */