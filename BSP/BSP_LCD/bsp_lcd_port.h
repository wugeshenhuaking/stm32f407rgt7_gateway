#ifndef __BSP_LCD_PORT_H
#define __BSP_LCD_PORT_H

/*
*********************************************************************************************************
*
*	模块名称 : LCD 硬件接口移植层
*	文件名称 : bsp_lcd_port.h
*	说    明 : 本文件定义 emWin 驱动（GUIDRV_Template.c）所需调用的全部接口函数。
*
*	设计原则 :
*		- GUIDRV_Template.c 只调用本文件中声明的 LCD_Xxx() 函数
*		- bsp_lcd_port.c 内部调用底层驱动 NT35510_XxxGUI()
*		- 若以后更换屏幕，只需修改 bsp_lcd_port.c，GUIDRV_Template.c 不动
*
*	调用层次 :
*		GUIDRV_Template.c
*		        ↓  调用 LCD_Xxx()
*		bsp_lcd_port.c
*		        ↓  调用 NT35510_XxxGUI()
*		lcd_nt35510.c
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"
#include "lcd_nt35510.h"

/* ============================================================
 * 全局变量：LCD 分辨率（在 bsp_lcd_port.c 中定义）
 * GUIDRV_Template.c 通过 LCD_GetWidth/Height 读取
 * ============================================================ */
extern uint16_t g_LcdWidth;
extern uint16_t g_LcdHeight;

/* ============================================================
 * 屏幕信息查询
 * ============================================================ */
uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);

/* ============================================================
 * emWin 对接接口
 * 对应 GUIDRV_Template.c 中各 _Xxx 函数的硬件调用
 *
 * 函数对照表（RA8875 例程 → 本工程接口）：
 *   RA8875_PutPixelGUI      → BSP_LCD_PutPixelGUI
 *   RA8875_GetPixelGUI      → BSP_LCD_GetPixelGUI
 *   RA8875_DrawHLine        → BSP_LCD_DrawHLine
 *   RA8875_DrawVLine        → BSP_LCD_DrawVLine
 *   RA8875_RTERect          → BSP_LCD_FillRectGUI
 *   RA8875_DrawHColorLine   → BSP_LCD_DrawHColorLine
 * ============================================================ */

/*
 * BSP_LCD_PutPixelGUI
 * 对应 _SetPixelIndex：emWin 写单个像素
 * 参数：x, y 为物理坐标；color 为 RGB565 颜色值
 */
void BSP_LCD_PutPixelGUI(uint16_t x, uint16_t y, uint16_t color);

/*
 * BSP_LCD_GetPixelGUI
 * 对应 _GetPixelIndex：emWin 读单个像素（用于 XOR 模式等）
 * 返回：RGB565 颜色值
 */
uint16_t BSP_LCD_GetPixelGUI(uint16_t x, uint16_t y);

/*
 * BSP_LCD_DrawHLine
 * 对应 _DrawHLine：emWin 画单色水平线（高频调用，需高效）
 * 参数：x0 起始X，y 行坐标，x1 结束X，color RGB565
 */
void BSP_LCD_DrawHLine(uint16_t x0, uint16_t y, uint16_t x1, uint16_t color);

/*
 * BSP_LCD_DrawVLine
 * 对应 _DrawVLine：emWin 画单色垂直线
 * 参数：x 列坐标，y0 起始Y，y1 结束Y，color RGB565
 */
void BSP_LCD_DrawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);

/*
 * BSP_LCD_FillRectGUI
 * 对应 _FillRect：emWin 填充矩形（传入端点坐标，不是宽高）
 * 参数：x0,y0 左上角；x1,y1 右下角；color RGB565
 */
void BSP_LCD_FillRectGUI(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/*
 * BSP_LCD_DrawHColorLine
 * 对应 _DrawBitLine16BPP / _DrawBitLine32BPP：
 * emWin 画彩色水平线，颜色来自像素数组（位图/图片显示核心路径）
 * 参数：x,y 起始坐标；xsize 像素数；pColor 颜色数组指针
 */
void BSP_LCD_DrawHColorLine(uint16_t x, uint16_t y, uint16_t xsize, const uint16_t *pColor);

#endif /* __BSP_LCD_PORT_H */

