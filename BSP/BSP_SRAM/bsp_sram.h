#ifndef __BSP_SRAM_H
#define __BSP_SRAM_H

/*
*********************************************************************************************************
*
*   模块名称 : SRAM 硬件驱动层
*   文件名称 : bsp_sram.h
*   说    明 : 定义 SRAM 硬件相关的地址、尺寸和底层读写宏。
*              本文件是平台相关层，移植时只需修改此文件。
*
*   硬件信息 :
*       芯片    : IS62WV51216  (512K x 16bit = 1MB)
*       接口    : FSMC NE3，8080 并口
*       基地址  : 0x68000000
*
*   移植说明 :
*       如果换用非 STM32 平台，修改以下宏即可：
*           SRAM_BASE_ADDR  → 新平台映射地址
*           SRAM_SIZE       → 实际容量
*           SRAM_READ8 / SRAM_WRITE8 → 替换为新平台的读写实现
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * 硬件参数（修改此处适配不同平台）
 * ============================================================ */
#define SRAM_BASE_ADDR      0x68000000UL            /* FSMC NE3 基地址 */
#define SRAM_SIZE           (512UL * 1024UL * 2UL)  /* IS62WV51216: 512K x 16bit = 1MB */
#define EXT_SRAM_ADDR       SRAM_BASE_ADDR

/* ============================================================
 * 底层读写宏（平台相关，移植时替换）
 * STM32 FSMC 方式：直接读写映射地址，等价于访问片外 SRAM
 * ============================================================ */
#define SRAM_WRITE8(addr, data)  (*(volatile uint8_t  *)(SRAM_BASE_ADDR + (addr)) = (data))
#define SRAM_READ8(addr)         (*(volatile uint8_t  *)(SRAM_BASE_ADDR + (addr)))

#define SRAM_WRITE16(addr, data) (*(volatile uint16_t *)(SRAM_BASE_ADDR + (addr)) = (data))
#define SRAM_READ16(addr)        (*(volatile uint16_t *)(SRAM_BASE_ADDR + (addr)))

/* ============================================================
 * 函数声明
 * ============================================================ */
void BSP_SRAM_HwInit(void);

#endif /* __BSP_SRAM_H */
