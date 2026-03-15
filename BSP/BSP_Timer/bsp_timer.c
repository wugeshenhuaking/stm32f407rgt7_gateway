/*
*********************************************************************************************************
*
*   模块名称 : 软件定时器模块
*   文件名称 : bsp_timer.c
*   说    明 : 基于 SysTick 实现多路软件定时器，精度 1ms。
*
*   关键设计 :
*       SysTick_Handler (stm32f4xx_it.c) 中：
*           HAL_IncTick();          // HAL 计时，保持原有
*           bsp_SysTick_ISR();      // 驱动本模块
*
*       bsp_SysTick_ISR() 每 1ms：
*           - 所有软件定时器 Count 减 1
*           - g_iRunTime 加 1
*           - 每 1ms  调用 bsp_RunPer1ms()  （用户实现，可为空）
*           - 每 10ms 调用 bsp_RunPer10ms() （用户实现，可为空）
*
*********************************************************************************************************
*/

#include "bsp_timer.h"

/* ============================================================
 * 内部变量
 * ============================================================ */
static SOFT_TMR s_tTmr[TMR_COUNT];

/* 用于 bsp_DelayMS() */
static volatile uint32_t s_uiDelayCount  = 0;
static volatile uint8_t  s_ucTimeOutFlag = 0;

/* 系统运行时间，单位 ms，最大 24.85 天 */
static volatile uint32_t g_iRunTime = 0;

/* ============================================================
 * 用户周期回调（在 bsp_timer.c 末尾弱定义，用户可在其他文件覆盖）
 * ============================================================ */
__weak void bsp_RunPer1ms(void)  { }
__weak void bsp_RunPer10ms(void) { }

/* ============================================================
 * 内部：软件定时器单步递减
 * ============================================================ */
static void bsp_SoftTimerDec(SOFT_TMR *_tmr)
{
    if (_tmr->Count == 0) return;

    if (--_tmr->Count == 0)
    {
        _tmr->Flag = 1;
        if (_tmr->Mode == TMR_AUTO_MODE)
        {
            _tmr->Count = _tmr->PreLoad;    /* 自动重装 */
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_InitTimer
*   功能说明: 初始化所有软件定时器（清零），在 main() 最前面调用一次
*   形    参: 无
*   返 回 值: 无
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
*   函 数 名: bsp_SysTick_ISR
*   功能说明: 每 1ms 被 SysTick_Handler 调用一次，驱动所有软件定时器
*   形    参: 无
*   返 回 值: 无
*   注    意: 本函数在中断上下文执行，不要在这里做耗时操作
*********************************************************************************************************
*/
void bsp_SysTick_ISR(void)
{
    static uint8_t s_10ms = 0;
    uint8_t i;

    /* 延时计数器 */
    if (s_uiDelayCount > 0)
    {
        if (--s_uiDelayCount == 0)
        {
            s_ucTimeOutFlag = 1;
        }
    }

    /* 软件定时器递减 */
    for (i = 0; i < TMR_COUNT; i++)
    {
        bsp_SoftTimerDec(&s_tTmr[i]);
    }

    /* 系统运行时间：uint32_t 溢出后自然归零，无需手动处理 */
    g_iRunTime++;

    /* 用户 1ms 回调 */
    bsp_RunPer1ms();

    /* 用户 10ms 回调 */
    if (++s_10ms >= 10)
    {
        s_10ms = 0;
        bsp_RunPer10ms();
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_StartTimer
*   功能说明: 启动单次定时器。到期后 Flag=1，不会自动重装。
*   形    参: _id  定时器编号 [0, TMR_COUNT-1]
*             _ms  定时时长，单位 ms
*   返 回 值: 无
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
*   函 数 名: bsp_StartAutoTimer
*   功能说明: 启动自动重装定时器。到期后 Flag=1 并自动重新开始计时。
*   形    参: _id  定时器编号 [0, TMR_COUNT-1]
*             _ms  定时周期，单位 ms
*   返 回 值: 无
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
*   函 数 名: bsp_StopTimer
*   功能说明: 停止定时器
*   形    参: _id  定时器编号
*   返 回 值: 无
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
*   函 数 名: bsp_CheckTimer
*   功能说明: 检测定时器是否到期。到期后自动清除 Flag（读清）。
*   形    参: _id  定时器编号
*   返 回 值: 1 = 已到期；0 = 未到期
*   用    法:
*       if (bsp_CheckTimer(TMR_TOUCH)) { BSP_Touch_Scan(); }
*********************************************************************************************************
*/
uint8_t bsp_CheckTimer(uint8_t _id)
{
    if (_id >= TMR_COUNT) return 0;

    if (s_tTmr[_id].Flag)
    {
        s_tTmr[_id].Flag = 0;   /* 读清标志 */
        return 1;
    }
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: bsp_DelayMS
*   功能说明: ms 级阻塞延时，精度 ±1ms
*             注意：延时期间 CPU 停在此函数，不要在中断中调用
*   形    参: _ms  延时长度，单位 ms，建议 >= 2
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_DelayMS(uint32_t _ms)
{
    if (_ms == 0) return;
    if (_ms == 1) _ms = 2;  /* 最小 2ms，避免精度问题 */

    __disable_irq();
    s_uiDelayCount  = _ms;
    s_ucTimeOutFlag = 0;
    __enable_irq();

    while (!s_ucTimeOutFlag)
    {
        /* 等待超时，可在此加入 __WFI() 低功耗等待 */
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_DelayUS
*   功能说明: us 级延时，直接读 SysTick->VAL 计数，精度约 ±1us
*             必须在 SysTick 启动后调用
*   形    参: _us  延时长度，单位 us
*   返 回 值: 无
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
            /* SysTick 是递减计数器 */
            tcnt += (tnow < told) ? (told - tnow) : (reload - tnow + told);
            told = tnow;
            if (tcnt >= ticks) break;
        }
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_GetRunTime
*   功能说明: 获取系统运行时间，单位 ms
*   形    参: 无
*   返 回 值: 运行时间（ms），最大约 24.85 天后归零
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
*   函 数 名: bsp_CheckRunTime
*   功能说明: 计算当前时间与给定时刻的差值，处理了计数器溢出
*   形    参: _last  起始时刻（由 bsp_GetRunTime() 取得）
*   返 回 值: 时间差，单位 ms
*   用    法:
*       int32_t t0 = bsp_GetRunTime();
*       // ... 一段操作 ...
*       int32_t elapsed = bsp_CheckRunTime(t0);
*********************************************************************************************************
*/
uint32_t bsp_CheckRunTime(uint32_t _last)
{
    /* uint32_t 减法在溢出时自动正确（模 2^32 算术），无需特殊处理 */
    return bsp_GetRunTime() - _last;
}
