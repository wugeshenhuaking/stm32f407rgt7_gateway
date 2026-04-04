/*
*********************************************************************************************************
*
*   Module Name : Soft Timer Module
*   File Name   : bsp_timer.c
*   Description : Multi-channel software timer implementation based on SysTick, precision 1ms.
*
*   Key Design :
*       In SysTick_Handler (stm32f4xx_it.c):
*           HAL_IncTick();          // Keep original HAL tick
*           bsp_SysTick_ISR();      // Driver for this module
*
*       bsp_SysTick_ISR() runs every 1ms:
*           - Decrement Count for all software timers
*           - Increment g_iRunTime by 1
*           - Call bsp_RunPer1ms()  every 1ms  (user-implemented, can be empty)
*           - Call bsp_RunPer10ms() every 10ms (user-implemented, can be empty)
*
*********************************************************************************************************
*/

#include "bsp_timer.h"
#include "bsp_touch_port.h"

/* ============================================================
 * Internal Variables
 * ============================================================ */
static SOFT_TMR s_tTmr[TMR_COUNT];

/* Used for bsp_DelayMS() */
static volatile uint32_t s_uiDelayCount  = 0;
static volatile uint8_t  s_ucTimeOutFlag = 0;

/* System runtime, unit: ms, max ~24.85 days */
static volatile uint32_t g_iRunTime = 0;

/* ============================================================
 * User Periodic Callbacks
 * (Weak defined at the end of bsp_timer.c, user can override in other files)
 * ============================================================ */
__weak void bsp_RunPer1ms(void)  { }
__weak void bsp_RunPer10ms(void) { BSP_Touch_EmWinSchedule(); }

/* ============================================================
 * Internal: Decrement software timer single step
 * ============================================================ */
static void bsp_SoftTimerDec(SOFT_TMR *_tmr)
{
    if (_tmr->Count == 0) return;

    if (--_tmr->Count == 0)
    {
        _tmr->Flag = 1;
        if (_tmr->Mode == TMR_AUTO_MODE)
        {
            _tmr->Count = _tmr->PreLoad;    /* Auto reload */
        }
    }
}

/*
*********************************************************************************************************
*   Function Name : bsp_InitTimer
*   Description   : Initialize all software timers (clear to 0),
*                   call once at the beginning of main()
*   Arguments     : None
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_InitTimer(void)
{
    uint8_t i;
    for (i = 0; i < TMR_COUNT; i++)
    {
        s_tTmr[i].Count   = 0;
        s_tTmr[i].PreLoad = 0;
        s_tTmr[i].Flag    = 0;
        s_tTmr[i].Mode    = TMR_ONCE_MODE;
    }
    g_iRunTime = 0;
}

/*
*********************************************************************************************************
*   Function Name : bsp_SysTick_ISR
*   Description   : Called by SysTick_Handler every 1ms,
*                   drives all software timers
*   Arguments     : None
*   Return Value  : None
*   Note          : This function runs in interrupt context,
*                   do not perform time-consuming operations here
*********************************************************************************************************
*/
void bsp_SysTick_ISR(void)
{
    static uint8_t s_10ms = 0;
    uint8_t i;

    /* Delay counter */
    if (s_uiDelayCount > 0)
    {
        if (--s_uiDelayCount == 0)
        {
            s_ucTimeOutFlag = 1;
        }
    }

    /* Decrement software timers */
    for (i = 0; i < TMR_COUNT; i++)
    {
        bsp_SoftTimerDec(&s_tTmr[i]);
    }

    /* System runtime, auto wrap around after uint32_t overflow, no manual handling needed */
    g_iRunTime++;

    /* User 1ms callback */
    bsp_RunPer1ms();

    /* User 10ms callback */
    if (++s_10ms >= 10)
    {
        s_10ms = 0;
        bsp_RunPer10ms();
    }
}

/*
*********************************************************************************************************
*   Function Name : bsp_StartTimer
*   Description   : Start one-shot timer.
*                   Flag = 1 when timeout, no auto reload.
*   Arguments     : _id  - Timer index [0, TMR_COUNT-1]
*                   _ms  - Timer duration, unit: ms
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_StartTimer(uint8_t _id, uint32_t _ms)
{
    if (_id >= TMR_COUNT) return;

    __disable_irq();
    s_tTmr[_id].Count   = _ms;
    s_tTmr[_id].PreLoad = _ms;
    s_tTmr[_id].Flag    = 0;
    s_tTmr[_id].Mode    = TMR_ONCE_MODE;
    __enable_irq();
}

/*
*********************************************************************************************************
*   Function Name : bsp_StartAutoTimer
*   Description   : Start auto-reload timer.
*                   Flag = 1 when timeout and automatically restart counting.
*   Arguments     : _id  - Timer index [0, TMR_COUNT-1]
*                   _ms  - Timer period, unit: ms
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_StartAutoTimer(uint8_t _id, uint32_t _ms)
{
    if (_id >= TMR_COUNT) return;

    __disable_irq();
    s_tTmr[_id].Count   = _ms;
    s_tTmr[_id].PreLoad = _ms;
    s_tTmr[_id].Flag    = 0;
    s_tTmr[_id].Mode    = TMR_AUTO_MODE;
    __enable_irq();
}

/*
*********************************************************************************************************
*   Function Name : bsp_StopTimer
*   Description   : Stop a software timer
*   Arguments     : _id - Timer index
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_StopTimer(uint8_t _id)
{
    if (_id >= TMR_COUNT) return;

    __disable_irq();
    s_tTmr[_id].Count = 0;
    s_tTmr[_id].Flag  = 0;
    __enable_irq();
}

/*
*********************************************************************************************************
*   Function Name : bsp_CheckTimer
*   Description   : Check if timer has expired.
*                   Automatically clear Flag (read-clear) after timeout.
*   Arguments     : _id - Timer index
*   Return Value  : 1 = expired; 0 = not expired
*   Usage Example :
*       if (bsp_CheckTimer(TMR_TOUCH)) { BSP_Touch_Scan(); }
*********************************************************************************************************
*/
uint8_t bsp_CheckTimer(uint8_t _id)
{
    if (_id >= TMR_COUNT) return 0;

    if (s_tTmr[_id].Flag)
    {
        s_tTmr[_id].Flag = 0;   /* Read-clear flag */
        return 1;
    }
    return 0;
}

/*
*********************************************************************************************************
*   Function Name : bsp_DelayMS
*   Description   : Blocking delay in ms, precision ±1ms
*                   Note: CPU blocks in this function during delay,
*                   do NOT call in interrupt service routine
*   Arguments     : _ms - Delay duration, unit: ms, recommend >= 2
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_DelayMS(uint32_t _ms)
{
    if (_ms == 0) return;
    if (_ms == 1) _ms = 2;  /* Min 2ms to avoid precision issue */

    __disable_irq();
    s_uiDelayCount  = _ms;
    s_ucTimeOutFlag = 0;
    __enable_irq();

    while (!s_ucTimeOutFlag)
    {
        /* Wait for timeout, can add __WFI() for low-power waiting */
    }
}

/*
*********************************************************************************************************
*   Function Name : bsp_DelayUS
*   Description   : Delay in us, directly read SysTick->VAL counter, precision ~±1us
*                   Must be called AFTER SysTick is enabled
*   Arguments     : _us - Delay duration, unit: us
*   Return Value  : None
*********************************************************************************************************
*/
void bsp_DelayUS(uint32_t _us)
{
    uint32_t ticks  = _us * (SystemCoreClock / 1000000);
    uint32_t told   = SysTick->VAL;
    uint32_t reload = SysTick->LOAD;
    uint32_t tcnt   = 0;

    while (1)
    {
        uint32_t tnow = SysTick->VAL;
        if (tnow != told)
        {
            /* SysTick is a down-counter */
            tcnt += (tnow < told) ? (told - tnow) : (reload - tnow + told);
            told = tnow;
            if (tcnt >= ticks) break;
        }
    }
}

/*
*********************************************************************************************************
*   Function Name : bsp_GetRunTime
*   Description   : Get system runtime, unit: ms
*   Arguments     : None
*   Return Value  : Runtime (ms), wraps to 0 after ~24.85 days
*********************************************************************************************************
*/
uint32_t bsp_GetRunTime(void)
{
    uint32_t t;
    __disable_irq();
    t = g_iRunTime;
    __enable_irq();
    return t;
}

/*
*********************************************************************************************************
*   Function Name : bsp_CheckRunTime
*   Description   : Calculate time difference between current time and given start time,
*                   handles counter overflow automatically
*   Arguments     : _last - Start timestamp (obtained by bsp_GetRunTime())
*   Return Value  : Elapsed time, unit: ms
*   Usage Example :
*       int32_t t0 = bsp_GetRunTime();
*       // ... some operations ...
*       int32_t elapsed = bsp_CheckRunTime(t0);
*********************************************************************************************************
*/
uint32_t bsp_CheckRunTime(uint32_t _last)
{
    /* uint32_t subtraction works correctly after overflow (mod 2^32 math), no special handling */
    return bsp_GetRunTime() - _last;
}
