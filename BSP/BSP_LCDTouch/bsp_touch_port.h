#ifndef __BSP_TOUCH_PORT_H
#define __BSP_TOUCH_PORT_H

/*
*********************************************************************************************************
*
*   Module : Touch panel BSP port layer
*   File   : bsp_touch_port.h
*   Notes  :
*       - Application layer only calls BSP_Touch_Xxx() interfaces
*       - bsp_touch_port.c translates GT9xxx data into logical screen coordinates
*       - emWin single-touch bridge also lives in bsp_touch_port.c
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"
#include "gt9xxx.h"
#include "lcd_nt35510.h"

/*
 * Touch/emWin debug switch
 *   1 = print touch bridge logs on USART
 *   0 = silent mode for formal UI running
 *
 * Typical usage:
 *   debug build   -> keep 1
 *   formal build  -> change to 0
 */
#ifndef BSP_TOUCH_EMWIN_DEBUG
  #define BSP_TOUCH_EMWIN_DEBUG 1
#endif

/*
 * Limit MOVE log frequency in debug mode to avoid flooding the UART.
 * Example:
 *   1 -> print every MOVE
 *   4 -> print one log for every 4 MOVE events
 */
#ifndef BSP_TOUCH_EMWIN_MOVE_LOG_DIV
  #define BSP_TOUCH_EMWIN_MOVE_LOG_DIV 4U
#endif

/* Maximum concurrent touch points reported by the controller */
#define BSP_TOUCH_MAX_POINTS    GT9XXX_MAX_TOUCH

/*
 * Keep touch mapping synchronized with the LCD orientation switch.
 * 0 = portrait  (480 x 800)
 * 1 = landscape (800 x 480)
 */
#if (NT35510_DIRECTION == NT35510_DIR_PORTRAIT) || \
    (NT35510_DIRECTION == NT35510_DIR_PORTRAIT_REV)
  #define BSP_TOUCH_SCREEN_DIR 0
#elif (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE) || \
      (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE_REV)
  #define BSP_TOUCH_SCREEN_DIR 1
#else
  #error Invalid NT35510_DIRECTION for touch mapping
#endif

/*
 * Raw GT9xxx coordinates are based on the physical touch panel.
 * GUI coordinates follow the current LCD logical orientation.
 */
#define BSP_TOUCH_RAW_WIDTH     NT35510_PANEL_WIDTH
#define BSP_TOUCH_RAW_HEIGHT    NT35510_PANEL_HEIGHT
#define BSP_TOUCH_GUI_WIDTH     NT35510_WIDTH
#define BSP_TOUCH_GUI_HEIGHT    NT35510_HEIGHT

/*
 * Backward-compatible aliases.
 * BSP_Touch_GetPoint() returns GUI logical coordinates, so keep these
 * aligned with the active GUI size.
 */
#define BSP_TOUCH_LCD_WIDTH     BSP_TOUCH_GUI_WIDTH
#define BSP_TOUCH_LCD_HEIGHT    BSP_TOUCH_GUI_HEIGHT

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t  pressed;
} BSP_TouchPointTypeDef;

typedef struct
{
    BSP_TouchPointTypeDef points[BSP_TOUCH_MAX_POINTS];
    uint8_t  touch_num;
    uint8_t  is_pressed;
} BSP_TouchStateTypeDef;

uint8_t BSP_Touch_Init(void);
uint8_t BSP_Touch_Scan(void);
void    BSP_Touch_GetState(BSP_TouchStateTypeDef *state);
uint8_t BSP_Touch_GetPoint(uint8_t index, uint16_t *x, uint16_t *y);
uint8_t BSP_Touch_IsPressed(void);
uint8_t BSP_Touch_GetTouchNum(void);

/*
 * Called periodically from the timer tick. This function scans the touch
 * controller and caches the latest DOWN / MOVE / RELEASE event for emWin.
 */
void BSP_Touch_EmWinExec(void);

/*
 * Called from the 10 ms timer hook. This only schedules one touch scan
 * request; the actual GT9xxx I2C transaction is executed later in the
 * GUI task context by BSP_Touch_EmWinExec().
 */
void BSP_Touch_EmWinSchedule(void);

/*
 * Called from the GUI main loop. This function flushes cached touch events
 * to emWin and can safely print debug traces via printf / USART1.
 */
void BSP_Touch_EmWinFlush(void);

#endif /* __BSP_TOUCH_PORT_H */
