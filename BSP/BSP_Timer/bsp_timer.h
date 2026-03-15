#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

/*
*********************************************************************************************************
*
*   模块名称 : 软件定时器模块
*   文件名称 : bsp_timer.h
*   说    明 : 基于 SysTick 实现多路软件定时器，精度 1ms。
*              支持单次模式和自动重装模式。
*              主循环通过 bsp_CheckTimer() 轮询到期标志，
*              不在中断里执行业务逻辑，安全简单。
*
*   使用方法 :
*       1. 在 bsp_timer.c 顶部修改 TMR_COUNT 定义定时器个数
*       2. 用枚举定义各定时器 ID，例如：
*              typedef enum { TMR_LED=0, TMR_TOUCH, TMR_COUNT } TMR_ID_E;
*       3. 启动定时器：
*              bsp_StartTimer(TMR_LED, 500);        // 单次，500ms
*              bsp_StartAutoTimer(TMR_TOUCH, 10);   // 自动重装，10ms
*       4. 主循环检测：
*              if (bsp_CheckTimer(TMR_LED)) { ... }
*
*   依赖 :
*       - HAL_IncTick() 已在 SysTick_Handler 中调用（CubeMX 默认已有）
*       - 在 SysTick_Handler 中额外调用 bsp_SysTick_ISR()
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * 软件定时器个数，按需增加
 * ============================================================ */
#define TMR_COUNT       8

/* ============================================================
 * 定时器工作模式
 * ============================================================ */
#define TMR_ONCE_MODE   0   /* 单次模式：到期后停止，Flag 置 1 */
#define TMR_AUTO_MODE   1   /* 自动模式：到期后自动重装，Flag 置 1 */

/* ============================================================
 * 软件定时器结构体
 * ============================================================ */
typedef struct
{
    volatile uint32_t Count;    /* 实时倒计数，每 1ms 减 1 */
    uint32_t          PreLoad;  /* 自动重装值（AUTO 模式使用） */
    volatile uint8_t  Flag;     /* 到期标志：1 = 已到期 */
    uint8_t           Mode;     /* 工作模式：TMR_ONCE_MODE / TMR_AUTO_MODE */
} SOFT_TMR;

/* ============================================================
 * 函数声明
 * ============================================================ */
void    bsp_InitTimer(void);

void    bsp_StartTimer(uint8_t _id, uint32_t _ms);
void    bsp_StartAutoTimer(uint8_t _id, uint32_t _ms);
void    bsp_StopTimer(uint8_t _id);
uint8_t bsp_CheckTimer(uint8_t _id);

void     bsp_DelayMS(uint32_t _ms);
void     bsp_DelayUS(uint32_t _us);

uint32_t bsp_GetRunTime(void);
uint32_t bsp_CheckRunTime(uint32_t _last);

/* 在 SysTick_Handler 中调用 */
void bsp_SysTick_ISR(void);

#endif /* __BSP_TIMER_H */


