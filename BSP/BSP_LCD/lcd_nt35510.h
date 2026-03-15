#ifndef __LCD_NT35510_H
#define __LCD_NT35510_H

#include "stm32f4xx_hal.h"

/* ============================================================
 * LCD 操作结构体映射到 FSMC 地址
 * NE4 基地址: 0x6C000000，RS = FSMC_A6
 * ============================================================ */
typedef struct
{
    volatile uint16_t LCD_REG;   /* A6=0 → 命令 */
    volatile uint16_t LCD_RAM;   /* A6=1 → 数据 */
} LCD_TypeDef;

#define LCD_BASE    ((uint32_t)(0x6C000000 | 0x0000007E))
#define LCD         ((LCD_TypeDef *)LCD_BASE)

/* ============================================================
 * 屏幕分辨率
 * ============================================================ */
#define NT35510_WIDTH   480
#define NT35510_HEIGHT  800

/* ============================================================
 * RGB565 常用颜色
 * ============================================================ */
#define NT35510_WHITE    0xFFFF
#define NT35510_BLACK    0x0000
#define NT35510_RED      0xF800
#define NT35510_GREEN    0x07E0
#define NT35510_BLUE     0x001F
#define NT35510_YELLOW   0xFFE0
#define NT35510_CYAN     0x07FF
#define NT35510_MAGENTA  0xF81F

/* ============================================================
 * 背光控制（PB15）
 * ============================================================ */
#define NT35510_BL_PORT   GPIOB
#define NT35510_BL_PIN    GPIO_PIN_15
#define NT35510_BL(x)     HAL_GPIO_WritePin(NT35510_BL_PORT, NT35510_BL_PIN, \
                               (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

/* ============================================================
 * 基础功能函数
 * ============================================================ */
void     NT35510_Init(void);
void     NT35510_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     NT35510_SetCursor(uint16_t x, uint16_t y);
void     NT35510_FillScreen(uint16_t color);
void     NT35510_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     NT35510_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void     NT35510_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void     NT35510_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void     NT35510_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

/* ============================================================
 * 底层读写（LCDConf.c 使用）
 * ============================================================ */
void     NT35510_WriteCmd(uint16_t cmd);
void     NT35510_WriteData(uint16_t data);
uint16_t NT35510_ReadData(void);

/* ============================================================
 * emWin 专用接口（由 bsp_lcd_port.c 调用，不直接给用户用）
 * 参考 RA8875 驱动的 PutPixelGUI / GetPixelGUI 命名风格
 * ============================================================ */
void     NT35510_PutPixelGUI(uint16_t x, uint16_t y, uint16_t color);
uint16_t NT35510_GetPixelGUI(uint16_t x, uint16_t y);
void     NT35510_DrawHLineGUI(uint16_t x0, uint16_t y, uint16_t x1, uint16_t color);
void     NT35510_DrawVLineGUI(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);
void     NT35510_FillRectGUI(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void     NT35510_DrawHColorLineGUI(uint16_t x, uint16_t y, uint16_t xsize, const uint16_t *pColor);

#endif /* __LCD_NT35510_H */