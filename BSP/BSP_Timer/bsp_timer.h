#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

/*
*********************************************************************************************************
*
*   Module Name : Software Timer Module
*   File Name   : bsp_timer.h
*   Description : Multi-channel software timers based on SysTick, 1ms resolution.
*                 Supports single-shot mode and auto-reload mode.
*                 Check timer expiration in main loop using bsp_CheckTimer().
*                 No application code executed in ISR, safe and simple.
*
*   Usage:
*       1. Define timer count TMR_COUNT in bsp_timer.c
*       2. Create timer ID enum, e.g.:
*              typedef enum { TMR_LED=0, TMR_TOUCH, TMR_COUNT } TMR_ID_E;
*       3. Start timers:
*              bsp_StartTimer(TMR_LED, 500);      // Single-shot, 500ms
*              bsp_StartAutoTimer(TMR_TOUCH, 10); // Auto-reload, 10ms
*       4. Check in main loop:
*              if (bsp_CheckTimer(TMR_LED)) { ... }
*
*   Dependencies:
*       - HAL_IncTick() called in SysTick_Handler (default in CubeMX)
*       - Call bsp_SysTick_ISR() in SysTick_Handler
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * Number of software timers
 * ============================================================ */
#define TMR_COUNT       8

/* ============================================================
 * Timer operating modes
 * ============================================================ */
#define TMR_ONCE_MODE   0   /* Single-shot: stop after expiration, Flag = 1 */
#define TMR_AUTO_MODE   1   /* Auto-reload: reload after expiration, Flag = 1 */

/* ============================================================
 * Software timer structure
 * ============================================================ */
typedef struct
{
    volatile uint32_t Count;    /* Decrement counter, decreases every 1ms */
    uint32_t          PreLoad;  /* Reload value for AUTO mode */
    volatile uint8_t  Flag;     /* Expiration flag: 1 = expired */
    uint8_t           Mode;     /* Timer mode: TMR_ONCE_MODE / TMR_AUTO_MODE */
} SOFT_TMR;

/* ============================================================
 * Public functions
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

/* Must be called in SysTick_Handler */
void bsp_SysTick_ISR(void);

#endif /* __BSP_TIMER_H */
