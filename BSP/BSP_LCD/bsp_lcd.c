#include "bsp_lcd.h"
#include "fsmc.h"

/* 在你的LCD驱动头部定义操作地址 */
/* LCD Interface模式下，CubeMX已经处理了RS信号 */
#define LCD_CMD   (*(volatile uint16_t*)0x6C000000)  // A6=0，写命令
#define LCD_DATA  (*(volatile uint16_t*)0x6C000080)  // A6=1，写数据

/* 写命令 */
void LCD_WriteCmd(uint16_t cmd) {
    LCD_CMD = cmd;
}

/* 写数据 */
void LCD_WriteData(uint16_t data) {
    LCD_DATA = data;
}

/* 设置显示窗口（以ILI9341为例）*/
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_WriteCmd(0x2A);         // 列地址设置
    LCD_WriteData(x0 >> 8);
    LCD_WriteData(x0 & 0xFF);
    LCD_WriteData(x1 >> 8);
    LCD_WriteData(x1 & 0xFF);

    LCD_WriteCmd(0x2B);         // 行地址设置
    LCD_WriteData(y0 >> 8);
    LCD_WriteData(y0 & 0xFF);
    LCD_WriteData(y1 >> 8);
    LCD_WriteData(y1 & 0xFF);

    LCD_WriteCmd(0x2C);         // 开始写入像素
}

/* 填充整屏颜色 */
void LCD_FillScreen(uint16_t color) {
    LCD_SetWindow(0, 0, 239, 319);  // 240x320分辨率
    for (uint32_t i = 0; i < 240 * 320; i++) {
        LCD_WriteData(color);
    }
}
