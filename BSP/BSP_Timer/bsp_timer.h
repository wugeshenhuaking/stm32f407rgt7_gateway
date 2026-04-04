#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

/*
*********************************************************************************************************
*
*   妯″潡鍚嶇О : 杞欢瀹氭椂鍣ㄦā鍧?
*   鏂囦欢鍚嶇О : bsp_timer.h
*   璇?   鏄?: 鍩轰簬 SysTick 瀹炵幇澶氳矾杞欢瀹氭椂鍣紝绮惧害 1ms銆?
*              鏀寔鍗曟妯″紡鍜岃嚜鍔ㄩ噸瑁呮ā寮忋€?
*              涓诲惊鐜€氳繃 bsp_CheckTimer() 杞鍒版湡鏍囧織锛?
*              涓嶅湪涓柇閲屾墽琛屼笟鍔￠€昏緫锛屽畨鍏ㄧ畝鍗曘€?
*
*   浣跨敤鏂规硶 :
*       1. 鍦?bsp_timer.c 椤堕儴淇敼 TMR_COUNT 瀹氫箟瀹氭椂鍣ㄤ釜鏁?
*       2. 鐢ㄦ灇涓惧畾涔夊悇瀹氭椂鍣?ID锛屼緥濡傦細
*              typedef enum { TMR_LED=0, TMR_TOUCH, TMR_COUNT } TMR_ID_E;
*       3. 鍚姩瀹氭椂鍣細
*              bsp_StartTimer(TMR_LED, 500);        // 鍗曟锛?00ms
*              bsp_StartAutoTimer(TMR_TOUCH, 10);   // 鑷姩閲嶈锛?0ms
*       4. 涓诲惊鐜娴嬶細
*              if (bsp_CheckTimer(TMR_LED)) { ... }
*
*   渚濊禆 :
*       - HAL_IncTick() 宸插湪 SysTick_Handler 涓皟鐢紙CubeMX 榛樿宸叉湁锛?
*       - 鍦?SysTick_Handler 涓澶栬皟鐢?bsp_SysTick_ISR()
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * 杞欢瀹氭椂鍣ㄤ釜鏁帮紝鎸夐渶澧炲姞
 * ============================================================ */
#define TMR_COUNT       8

/* ============================================================
 * 瀹氭椂鍣ㄥ伐浣滄ā寮?
 * ============================================================ */
#define TMR_ONCE_MODE   0   /* 鍗曟妯″紡锛氬埌鏈熷悗鍋滄锛孎lag 缃?1 */
#define TMR_AUTO_MODE   1   /* 鑷姩妯″紡锛氬埌鏈熷悗鑷姩閲嶈锛孎lag 缃?1 */

/* ============================================================
 * 杞欢瀹氭椂鍣ㄧ粨鏋勪綋
 * ============================================================ */
typedef struct
{
    volatile uint32_t Count;    /* 瀹炴椂鍊掕鏁帮紝姣?1ms 鍑?1 */
    uint32_t          PreLoad;  /* 鑷姩閲嶈鍊硷紙AUTO 妯″紡浣跨敤锛?*/
    volatile uint8_t  Flag;     /* 鍒版湡鏍囧織锛? = 宸插埌鏈?*/
    uint8_t           Mode;     /* 宸ヤ綔妯″紡锛歍MR_ONCE_MODE / TMR_AUTO_MODE */
} SOFT_TMR;

/* ============================================================
 * 鍑芥暟澹版槑
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

/* 鍦?SysTick_Handler 涓皟鐢?*/
void bsp_SysTick_ISR(void);

#endif /* __BSP_TIMER_H */


