#ifndef __GT9XXX_H
#define __GT9XXX_H

/*
*********************************************************************************************************
*
*   功能说明 ：GT9xxx 触摸屏驱动头文件
*   文件名称 ：gt9xxx.h
*   适用芯片 ：兼容 GT911 / GT9147 / GT917S / GT9271 等芯片
*   实验平台 ：正点原子 STM32F407ZGT6
*
*   硬件连接：
*       CT_RST  --> PC13  (触摸屏复位引脚)
*       CT_INT  --> PB1   (中断引脚，可配置为上升沿/下降沿)
*       CT_SCL  --> PB0   (软件模拟 I2C 时钟引脚)
*       CT_SDA  --> PF11  (软件模拟 I2C 数据引脚)
*
*   I2C 地址：
*       写地址 0x28  /  读地址 0x29  (7位地址 = 0x14)
*
*   注意：
*       SCL/SDA 引脚需要外部上拉电阻，否则通信不稳定
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * 引脚定义（根据硬件修改）
 * ============================================================ */
#define GT9XXX_RST_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOC_CLK_ENABLE()
#define GT9XXX_RST_GPIO_PORT            GPIOC
#define GT9XXX_RST_GPIO_PIN             GPIO_PIN_13

#define GT9XXX_INT_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()
#define GT9XXX_INT_GPIO_PORT            GPIOB
#define GT9XXX_INT_GPIO_PIN             GPIO_PIN_1

#define GT9XXX_SCL_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()
#define GT9XXX_SCL_GPIO_PORT            GPIOB
#define GT9XXX_SCL_GPIO_PIN             GPIO_PIN_0

#define GT9XXX_SDA_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOF_CLK_ENABLE()
#define GT9XXX_SDA_GPIO_PORT            GPIOF
#define GT9XXX_SDA_GPIO_PIN             GPIO_PIN_11

/* ============================================================
 * GPIO 操作宏
 * ============================================================ */
#define GT9XXX_RST(x)   HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_INT(x)   HAL_GPIO_WritePin(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_INT_READ HAL_GPIO_ReadPin(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN)

#define GT9XXX_SCL(x)   HAL_GPIO_WritePin(GT9XXX_SCL_GPIO_PORT, GT9XXX_SCL_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_SDA(x)   HAL_GPIO_WritePin(GT9XXX_SDA_GPIO_PORT, GT9XXX_SDA_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_SDA_READ HAL_GPIO_ReadPin(GT9XXX_SDA_GPIO_PORT, GT9XXX_SDA_GPIO_PIN)

/* ============================================================
 * I2C 地址
 * ============================================================ */
#define GT9XXX_CMD_WR   0x28    /* 写命令 */
#define GT9XXX_CMD_RD   0x29    /* 读命令 */

/* ============================================================
 * 触摸芯片寄存器地址（16位寄存器）
 * ============================================================ */
#define GT9XXX_CTRL_REG     0x8040  /* 控制寄存器：0x02=软复位, 0x00=正常工作 */
#define GT9XXX_CFGS_REG     0x8047  /* 配置参数起始地址 */
#define GT9XXX_CHECK_REG    0x80FF  /* 校验和寄存器 */
#define GT9XXX_PID_REG      0x8140  /* 产品ID寄存器 */
#define GT9XXX_GSTID_REG    0x814E  /* 触摸状态：bit7=缓冲区就绪, bit3:0=触摸点数 */

/* 每个触摸点坐标：依次为 X低字节、X高字节、Y低字节、Y高字节 */
#define GT9XXX_TP1_REG      0x8150
#define GT9XXX_TP2_REG      0x8158
#define GT9XXX_TP3_REG      0x8160
#define GT9XXX_TP4_REG      0x8168
#define GT9XXX_TP5_REG      0x8170
#define GT9XXX_TP6_REG      0x8178
#define GT9XXX_TP7_REG      0x8180
#define GT9XXX_TP8_REG      0x8188
#define GT9XXX_TP9_REG      0x8190
#define GT9XXX_TP10_REG     0x8198

/* ============================================================
 * 最大支持触摸点数
 * ============================================================ */
#define GT9XXX_MAX_TOUCH    5       /* 多数芯片为5点；GT9271为10点 */

/* ============================================================
 * 触摸点数据结构体
 * ============================================================ */
typedef struct
{
    uint16_t x;     /* X 坐标 */
    uint16_t y;     /* Y 坐标 */
    uint8_t  valid; /* 1=有效 0=无效 */
} GT9XXX_PointTypeDef;

/* ============================================================
 * 对外接口函数
 * ============================================================ */
uint8_t GT9XXX_Init(void);
uint8_t GT9XXX_Scan(GT9XXX_PointTypeDef *points, uint8_t max_points);

/* 底层 I2C 读写函数 */
uint8_t GT9XXX_WrReg(uint16_t reg, uint8_t *buf, uint8_t len);
void    GT9XXX_RdReg(uint16_t reg, uint8_t *buf, uint8_t len);

#endif /* __GT9XXX_H */

