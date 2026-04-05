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
 *   1 = print simple touch logs on USART
 *   0 = silent mode
 */
#ifndef BSP_TOUCH_EMWIN_DEBUG
  #define BSP_TOUCH_EMWIN_DEBUG 1
#endif

/* GT9xxx typical report rate is around 50~100Hz, keep polling aligned to that */
#ifndef BSP_TOUCH_SCAN_PERIOD_MS
  #define BSP_TOUCH_SCAN_PERIOD_MS 20U
#endif

/*
 * Software timer slot used by the non-interrupt touch task.
 * Override this macro if timer id 0 is already occupied elsewhere.
 */
#ifndef BSP_TOUCH_TIMER_ID
  #define BSP_TOUCH_TIMER_ID 0U
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

/* Legacy compatibility hooks kept only for source compatibility */
void BSP_Touch_EmWinExec(void);
void BSP_Touch_EmWinSchedule(void);
void BSP_Touch_EmWinFlush(void);

/*
 * Non-interrupt touch task entry.
 * Call BSP_Touch_TaskInit() once after BSP_Touch_Init(), then call
 * BSP_Touch_Task20ms() repeatedly from main loop / GUI idle / other task.
 */
void BSP_Touch_TaskInit(void);
void BSP_Touch_Task20ms(void);

/* Compatibility alias, same behavior as BSP_Touch_Task20ms() */
void PID_X_Exec(void);


#endif /* __BSP_TOUCH_PORT_H */
