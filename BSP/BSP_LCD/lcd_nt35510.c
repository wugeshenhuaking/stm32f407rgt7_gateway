#include "lcd_nt35510.h"

/* ============================================================
 * 内部工具
 * ============================================================ */
static void lcd_opt_delay(uint32_t i)
{
    while (i--);
}

/* ============================================================
 * 基础读写（对外暴露，LCDConf.c 用）
 * ============================================================ */
void NT35510_WriteCmd(uint16_t cmd)
{
    cmd = cmd;
    LCD->LCD_REG = cmd;
}

void NT35510_WriteData(uint16_t data)
{
    data = data;
    LCD->LCD_RAM = data;
}

uint16_t NT35510_ReadData(void)
{
    volatile uint16_t d;
    lcd_opt_delay(2);
    d = LCD->LCD_RAM;
    return d;
}

/* ============================================================
 * 内部寄存器写（NT35510 寄存器地址是16位）
 * ============================================================ */
static void nt_write_reg(uint16_t regno, uint16_t data)
{
    regno = regno;
    LCD->LCD_REG = regno;
    lcd_opt_delay(1);
    LCD->LCD_RAM = data;
}

static void nt_write_regno(uint16_t regno)
{
    regno = regno;
    LCD->LCD_REG = regno;
}

/* ============================================================
 * NT35510 完整寄存器初始化序列
 * ============================================================ */
static void NT35510_RegInit(void)
{
    nt_write_reg(0xF000, 0x55); nt_write_reg(0xF001, 0xAA);
    nt_write_reg(0xF002, 0x52); nt_write_reg(0xF003, 0x08); nt_write_reg(0xF004, 0x01);

    nt_write_reg(0xB000, 0x0D); nt_write_reg(0xB001, 0x0D); nt_write_reg(0xB002, 0x0D);
    nt_write_reg(0xB600, 0x34); nt_write_reg(0xB601, 0x34); nt_write_reg(0xB602, 0x34);
    nt_write_reg(0xB100, 0x0D); nt_write_reg(0xB101, 0x0D); nt_write_reg(0xB102, 0x0D);
    nt_write_reg(0xB700, 0x34); nt_write_reg(0xB701, 0x34); nt_write_reg(0xB702, 0x34);
    nt_write_reg(0xB200, 0x00); nt_write_reg(0xB201, 0x00); nt_write_reg(0xB202, 0x00);
    nt_write_reg(0xB800, 0x24); nt_write_reg(0xB801, 0x24); nt_write_reg(0xB802, 0x24);
    nt_write_reg(0xBF00, 0x01);
    nt_write_reg(0xB300, 0x0F); nt_write_reg(0xB301, 0x0F); nt_write_reg(0xB302, 0x0F);
    nt_write_reg(0xB900, 0x34); nt_write_reg(0xB901, 0x34); nt_write_reg(0xB902, 0x34);
    nt_write_reg(0xB500, 0x08); nt_write_reg(0xB501, 0x08); nt_write_reg(0xB502, 0x08);
    nt_write_reg(0xC200, 0x03);
    nt_write_reg(0xBA00, 0x24); nt_write_reg(0xBA01, 0x24); nt_write_reg(0xBA02, 0x24);
    nt_write_reg(0xBC00, 0x00); nt_write_reg(0xBC01, 0x78); nt_write_reg(0xBC02, 0x00);
    nt_write_reg(0xBD00, 0x00); nt_write_reg(0xBD01, 0x78); nt_write_reg(0xBD02, 0x00);
    nt_write_reg(0xBE00, 0x00); nt_write_reg(0xBE01, 0x64);

    nt_write_reg(0xD100,0x00); nt_write_reg(0xD101,0x33); nt_write_reg(0xD102,0x00); nt_write_reg(0xD103,0x34);
    nt_write_reg(0xD104,0x00); nt_write_reg(0xD105,0x3A); nt_write_reg(0xD106,0x00); nt_write_reg(0xD107,0x4A);
    nt_write_reg(0xD108,0x00); nt_write_reg(0xD109,0x5C); nt_write_reg(0xD10A,0x00); nt_write_reg(0xD10B,0x81);
    nt_write_reg(0xD10C,0x00); nt_write_reg(0xD10D,0xA6); nt_write_reg(0xD10E,0x00); nt_write_reg(0xD10F,0xE5);
    nt_write_reg(0xD110,0x01); nt_write_reg(0xD111,0x13); nt_write_reg(0xD112,0x01); nt_write_reg(0xD113,0x54);
    nt_write_reg(0xD114,0x01); nt_write_reg(0xD115,0x82); nt_write_reg(0xD116,0x01); nt_write_reg(0xD117,0xCA);
    nt_write_reg(0xD118,0x02); nt_write_reg(0xD119,0x00); nt_write_reg(0xD11A,0x02); nt_write_reg(0xD11B,0x01);
    nt_write_reg(0xD11C,0x02); nt_write_reg(0xD11D,0x34); nt_write_reg(0xD11E,0x02); nt_write_reg(0xD11F,0x67);
    nt_write_reg(0xD120,0x02); nt_write_reg(0xD121,0x84); nt_write_reg(0xD122,0x02); nt_write_reg(0xD123,0xA4);
    nt_write_reg(0xD124,0x02); nt_write_reg(0xD125,0xB7); nt_write_reg(0xD126,0x02); nt_write_reg(0xD127,0xCF);
    nt_write_reg(0xD128,0x02); nt_write_reg(0xD129,0xDE); nt_write_reg(0xD12A,0x02); nt_write_reg(0xD12B,0xF2);
    nt_write_reg(0xD12C,0x02); nt_write_reg(0xD12D,0xFE); nt_write_reg(0xD12E,0x03); nt_write_reg(0xD12F,0x10);
    nt_write_reg(0xD130,0x03); nt_write_reg(0xD131,0x33); nt_write_reg(0xD132,0x03); nt_write_reg(0xD133,0x6D);

    nt_write_reg(0xD200,0x00); nt_write_reg(0xD201,0x33); nt_write_reg(0xD202,0x00); nt_write_reg(0xD203,0x34);
    nt_write_reg(0xD204,0x00); nt_write_reg(0xD205,0x3A); nt_write_reg(0xD206,0x00); nt_write_reg(0xD207,0x4A);
    nt_write_reg(0xD208,0x00); nt_write_reg(0xD209,0x5C); nt_write_reg(0xD20A,0x00); nt_write_reg(0xD20B,0x81);
    nt_write_reg(0xD20C,0x00); nt_write_reg(0xD20D,0xA6); nt_write_reg(0xD20E,0x00); nt_write_reg(0xD20F,0xE5);
    nt_write_reg(0xD210,0x01); nt_write_reg(0xD211,0x13); nt_write_reg(0xD212,0x01); nt_write_reg(0xD213,0x54);
    nt_write_reg(0xD214,0x01); nt_write_reg(0xD215,0x82); nt_write_reg(0xD216,0x01); nt_write_reg(0xD217,0xCA);
    nt_write_reg(0xD218,0x02); nt_write_reg(0xD219,0x00); nt_write_reg(0xD21A,0x02); nt_write_reg(0xD21B,0x01);
    nt_write_reg(0xD21C,0x02); nt_write_reg(0xD21D,0x34); nt_write_reg(0xD21E,0x02); nt_write_reg(0xD21F,0x67);
    nt_write_reg(0xD220,0x02); nt_write_reg(0xD221,0x84); nt_write_reg(0xD222,0x02); nt_write_reg(0xD223,0xA4);
    nt_write_reg(0xD224,0x02); nt_write_reg(0xD225,0xB7); nt_write_reg(0xD226,0x02); nt_write_reg(0xD227,0xCF);
    nt_write_reg(0xD228,0x02); nt_write_reg(0xD229,0xDE); nt_write_reg(0xD22A,0x02); nt_write_reg(0xD22B,0xF2);
    nt_write_reg(0xD22C,0x02); nt_write_reg(0xD22D,0xFE); nt_write_reg(0xD22E,0x03); nt_write_reg(0xD22F,0x10);
    nt_write_reg(0xD230,0x03); nt_write_reg(0xD231,0x33); nt_write_reg(0xD232,0x03); nt_write_reg(0xD233,0x6D);

    nt_write_reg(0xD300,0x00); nt_write_reg(0xD301,0x33); nt_write_reg(0xD302,0x00); nt_write_reg(0xD303,0x34);
    nt_write_reg(0xD304,0x00); nt_write_reg(0xD305,0x3A); nt_write_reg(0xD306,0x00); nt_write_reg(0xD307,0x4A);
    nt_write_reg(0xD308,0x00); nt_write_reg(0xD309,0x5C); nt_write_reg(0xD30A,0x00); nt_write_reg(0xD30B,0x81);
    nt_write_reg(0xD30C,0x00); nt_write_reg(0xD30D,0xA6); nt_write_reg(0xD30E,0x00); nt_write_reg(0xD30F,0xE5);
    nt_write_reg(0xD310,0x01); nt_write_reg(0xD311,0x13); nt_write_reg(0xD312,0x01); nt_write_reg(0xD313,0x54);
    nt_write_reg(0xD314,0x01); nt_write_reg(0xD315,0x82); nt_write_reg(0xD316,0x01); nt_write_reg(0xD317,0xCA);
    nt_write_reg(0xD318,0x02); nt_write_reg(0xD319,0x00); nt_write_reg(0xD31A,0x02); nt_write_reg(0xD31B,0x01);
    nt_write_reg(0xD31C,0x02); nt_write_reg(0xD31D,0x34); nt_write_reg(0xD31E,0x02); nt_write_reg(0xD31F,0x67);
    nt_write_reg(0xD320,0x02); nt_write_reg(0xD321,0x84); nt_write_reg(0xD322,0x02); nt_write_reg(0xD323,0xA4);
    nt_write_reg(0xD324,0x02); nt_write_reg(0xD325,0xB7); nt_write_reg(0xD326,0x02); nt_write_reg(0xD327,0xCF);
    nt_write_reg(0xD328,0x02); nt_write_reg(0xD329,0xDE); nt_write_reg(0xD32A,0x02); nt_write_reg(0xD32B,0xF2);
    nt_write_reg(0xD32C,0x02); nt_write_reg(0xD32D,0xFE); nt_write_reg(0xD32E,0x03); nt_write_reg(0xD32F,0x10);
    nt_write_reg(0xD330,0x03); nt_write_reg(0xD331,0x33); nt_write_reg(0xD332,0x03); nt_write_reg(0xD333,0x6D);

    nt_write_reg(0xD400,0x00); nt_write_reg(0xD401,0x33); nt_write_reg(0xD402,0x00); nt_write_reg(0xD403,0x34);
    nt_write_reg(0xD404,0x00); nt_write_reg(0xD405,0x3A); nt_write_reg(0xD406,0x00); nt_write_reg(0xD407,0x4A);
    nt_write_reg(0xD408,0x00); nt_write_reg(0xD409,0x5C); nt_write_reg(0xD40A,0x00); nt_write_reg(0xD40B,0x81);
    nt_write_reg(0xD40C,0x00); nt_write_reg(0xD40D,0xA6); nt_write_reg(0xD40E,0x00); nt_write_reg(0xD40F,0xE5);
    nt_write_reg(0xD410,0x01); nt_write_reg(0xD411,0x13); nt_write_reg(0xD412,0x01); nt_write_reg(0xD413,0x54);
    nt_write_reg(0xD414,0x01); nt_write_reg(0xD415,0x82); nt_write_reg(0xD416,0x01); nt_write_reg(0xD417,0xCA);
    nt_write_reg(0xD418,0x02); nt_write_reg(0xD419,0x00); nt_write_reg(0xD41A,0x02); nt_write_reg(0xD41B,0x01);
    nt_write_reg(0xD41C,0x02); nt_write_reg(0xD41D,0x34); nt_write_reg(0xD41E,0x02); nt_write_reg(0xD41F,0x67);
    nt_write_reg(0xD420,0x02); nt_write_reg(0xD421,0x84); nt_write_reg(0xD422,0x02); nt_write_reg(0xD423,0xA4);
    nt_write_reg(0xD424,0x02); nt_write_reg(0xD425,0xB7); nt_write_reg(0xD426,0x02); nt_write_reg(0xD427,0xCF);
    nt_write_reg(0xD428,0x02); nt_write_reg(0xD429,0xDE); nt_write_reg(0xD42A,0x02); nt_write_reg(0xD42B,0xF2);
    nt_write_reg(0xD42C,0x02); nt_write_reg(0xD42D,0xFE); nt_write_reg(0xD42E,0x03); nt_write_reg(0xD42F,0x10);
    nt_write_reg(0xD430,0x03); nt_write_reg(0xD431,0x33); nt_write_reg(0xD432,0x03); nt_write_reg(0xD433,0x6D);

    nt_write_reg(0xD500,0x00); nt_write_reg(0xD501,0x33); nt_write_reg(0xD502,0x00); nt_write_reg(0xD503,0x34);
    nt_write_reg(0xD504,0x00); nt_write_reg(0xD505,0x3A); nt_write_reg(0xD506,0x00); nt_write_reg(0xD507,0x4A);
    nt_write_reg(0xD508,0x00); nt_write_reg(0xD509,0x5C); nt_write_reg(0xD50A,0x00); nt_write_reg(0xD50B,0x81);
    nt_write_reg(0xD50C,0x00); nt_write_reg(0xD50D,0xA6); nt_write_reg(0xD50E,0x00); nt_write_reg(0xD50F,0xE5);
    nt_write_reg(0xD510,0x01); nt_write_reg(0xD511,0x13); nt_write_reg(0xD512,0x01); nt_write_reg(0xD513,0x54);
    nt_write_reg(0xD514,0x01); nt_write_reg(0xD515,0x82); nt_write_reg(0xD516,0x01); nt_write_reg(0xD517,0xCA);
    nt_write_reg(0xD518,0x02); nt_write_reg(0xD519,0x00); nt_write_reg(0xD51A,0x02); nt_write_reg(0xD51B,0x01);
    nt_write_reg(0xD51C,0x02); nt_write_reg(0xD51D,0x34); nt_write_reg(0xD51E,0x02); nt_write_reg(0xD51F,0x67);
    nt_write_reg(0xD520,0x02); nt_write_reg(0xD521,0x84); nt_write_reg(0xD522,0x02); nt_write_reg(0xD523,0xA4);
    nt_write_reg(0xD524,0x02); nt_write_reg(0xD525,0xB7); nt_write_reg(0xD526,0x02); nt_write_reg(0xD527,0xCF);
    nt_write_reg(0xD528,0x02); nt_write_reg(0xD529,0xDE); nt_write_reg(0xD52A,0x02); nt_write_reg(0xD52B,0xF2);
    nt_write_reg(0xD52C,0x02); nt_write_reg(0xD52D,0xFE); nt_write_reg(0xD52E,0x03); nt_write_reg(0xD52F,0x10);
    nt_write_reg(0xD530,0x03); nt_write_reg(0xD531,0x33); nt_write_reg(0xD532,0x03); nt_write_reg(0xD533,0x6D);

    nt_write_reg(0xD600,0x00); nt_write_reg(0xD601,0x33); nt_write_reg(0xD602,0x00); nt_write_reg(0xD603,0x34);
    nt_write_reg(0xD604,0x00); nt_write_reg(0xD605,0x3A); nt_write_reg(0xD606,0x00); nt_write_reg(0xD607,0x4A);
    nt_write_reg(0xD608,0x00); nt_write_reg(0xD609,0x5C); nt_write_reg(0xD60A,0x00); nt_write_reg(0xD60B,0x81);
    nt_write_reg(0xD60C,0x00); nt_write_reg(0xD60D,0xA6); nt_write_reg(0xD60E,0x00); nt_write_reg(0xD60F,0xE5);
    nt_write_reg(0xD610,0x01); nt_write_reg(0xD611,0x13); nt_write_reg(0xD612,0x01); nt_write_reg(0xD613,0x54);
    nt_write_reg(0xD614,0x01); nt_write_reg(0xD615,0x82); nt_write_reg(0xD616,0x01); nt_write_reg(0xD617,0xCA);
    nt_write_reg(0xD618,0x02); nt_write_reg(0xD619,0x00); nt_write_reg(0xD61A,0x02); nt_write_reg(0xD61B,0x01);
    nt_write_reg(0xD61C,0x02); nt_write_reg(0xD61D,0x34); nt_write_reg(0xD61E,0x02); nt_write_reg(0xD61F,0x67);
    nt_write_reg(0xD620,0x02); nt_write_reg(0xD621,0x84); nt_write_reg(0xD622,0x02); nt_write_reg(0xD623,0xA4);
    nt_write_reg(0xD624,0x02); nt_write_reg(0xD625,0xB7); nt_write_reg(0xD626,0x02); nt_write_reg(0xD627,0xCF);
    nt_write_reg(0xD628,0x02); nt_write_reg(0xD629,0xDE); nt_write_reg(0xD62A,0x02); nt_write_reg(0xD62B,0xF2);
    nt_write_reg(0xD62C,0x02); nt_write_reg(0xD62D,0xFE); nt_write_reg(0xD62E,0x03); nt_write_reg(0xD62F,0x10);
    nt_write_reg(0xD630,0x03); nt_write_reg(0xD631,0x33); nt_write_reg(0xD632,0x03); nt_write_reg(0xD633,0x6D);

    nt_write_reg(0xF000,0x55); nt_write_reg(0xF001,0xAA);
    nt_write_reg(0xF002,0x52); nt_write_reg(0xF003,0x08); nt_write_reg(0xF004,0x00);

    nt_write_reg(0xB100, 0xCC); nt_write_reg(0xB101, 0x00);
    nt_write_reg(0xB600, 0x05);
    nt_write_reg(0xB700, 0x70); nt_write_reg(0xB701, 0x70);
    nt_write_reg(0xB800, 0x01); nt_write_reg(0xB801, 0x03);
    nt_write_reg(0xB802, 0x03); nt_write_reg(0xB803, 0x03);
    nt_write_reg(0xBC00, 0x02); nt_write_reg(0xBC01, 0x00); nt_write_reg(0xBC02, 0x00);
    nt_write_reg(0xC900, 0xD0); nt_write_reg(0xC901, 0x02);
    nt_write_reg(0xC902, 0x50); nt_write_reg(0xC903, 0x50); nt_write_reg(0xC904, 0x50);

    nt_write_reg(0x3500, 0x00);
    nt_write_reg(0x3A00, 0x55);  /* RGB565 */

    nt_write_regno(0x1100);
    HAL_Delay(120);
    nt_write_regno(0x2900);
    HAL_Delay(20);

//    nt_write_reg(0x3600, 0x00);  /* 竖屏 */
    nt_write_reg(0x3600, NT35510_MADCTL);  /* 旋转屏幕每次90度，0x00 / 0x60 / 0xA0 / 0xC0 */


    nt_write_reg(0x2A00, 0x00); nt_write_reg(0x2A01, 0x00);
    nt_write_reg(0x2A02, (NT35510_WIDTH  - 1) >> 8); nt_write_reg(0x2A03, (NT35510_WIDTH  - 1) & 0xFF);
    nt_write_reg(0x2B00, 0x00); nt_write_reg(0x2B01, 0x00);
    nt_write_reg(0x2B02, (NT35510_HEIGHT - 1) >> 8); nt_write_reg(0x2B03, (NT35510_HEIGHT - 1) & 0xFF);

    nt_write_regno(0x2C00);
}

/* ============================================================
 * 总初始化
 * ============================================================ */
void NT35510_Init(void)
{
    NT35510_BL(0);
    HAL_Delay(50);
    NT35510_RegInit();
    NT35510_FillScreen(NT35510_BLACK);
    NT35510_BL(1);
}

/* ============================================================
 * 设置显示窗口
 * ============================================================ */
void NT35510_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    nt_write_reg(0x2A00, x0 >> 8);   nt_write_reg(0x2A01, x0 & 0xFF);
    nt_write_reg(0x2A02, x1 >> 8);   nt_write_reg(0x2A03, x1 & 0xFF);
    nt_write_reg(0x2B00, y0 >> 8);   nt_write_reg(0x2B01, y0 & 0xFF);
    nt_write_reg(0x2B02, y1 >> 8);   nt_write_reg(0x2B03, y1 & 0xFF);
    nt_write_regno(0x2C00);
}

void NT35510_SetCursor(uint16_t x, uint16_t y)
{
    NT35510_SetWindow(x, y, x, y);
}

/* ============================================================
 * 基础绘图
 * ============================================================ */
void NT35510_FillScreen(uint16_t color)
{
    NT35510_FillRect(0, 0, NT35510_WIDTH, NT35510_HEIGHT, color);
}

void NT35510_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t total = (uint32_t)w * h;
    NT35510_SetWindow(x, y, x + w - 1, y + h - 1);
    while (total--) LCD->LCD_RAM = color;
}

void NT35510_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    NT35510_SetWindow(x, y, x, y);
    LCD->LCD_RAM = color;
}

void NT35510_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int xerr = 0, yerr = 0;
    int dx = x2 - x1, dy = y2 - y1;
    int incx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    int incy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    int row = x1, col = y1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int dist = (dx > dy) ? dx : dy;
    for (int t = 0; t <= dist; t++) {
        NT35510_DrawPixel(row, col, color);
        xerr += dx; yerr += dy;
        if (xerr > dist) { xerr -= dist; row += incx; }
        if (yerr > dist) { yerr -= dist; col += incy; }
    }
}

void NT35510_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    NT35510_DrawLine(x1, y1, x2, y1, color);
    NT35510_DrawLine(x1, y1, x1, y2, color);
    NT35510_DrawLine(x1, y2, x2, y2, color);
    NT35510_DrawLine(x2, y1, x2, y2, color);
}

void NT35510_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a = 0, b = r, di = 3 - (r << 1);
    while (a <= b) {
        NT35510_DrawPixel(x0+a, y0-b, color); NT35510_DrawPixel(x0+b, y0-a, color);
        NT35510_DrawPixel(x0+b, y0+a, color); NT35510_DrawPixel(x0+a, y0+b, color);
        NT35510_DrawPixel(x0-a, y0+b, color); NT35510_DrawPixel(x0-b, y0+a, color);
        NT35510_DrawPixel(x0-a, y0-b, color); NT35510_DrawPixel(x0-b, y0-a, color);
        a++;
        if (di < 0) di += 4*a + 6;
        else { di += 10 + 4*(a-b); b--; }
    }
}

/* ============================================================
 * emWin 专用函数
 * 参考 RA8875 的 PutPixelGUI / GetPixelGUI / DrawHLine /
 * DrawVLine / RTERect / DrawHColorLine 实现方式
 * ============================================================ */

/*
 * NT35510_PutPixelGUI
 * 对应 RA8875_PutPixelGUI：直接操作寄存器，不走 SetWindow
 * emWin 的 _SetPixelIndex 调用此函数
 */
void NT35510_PutPixelGUI(uint16_t x, uint16_t y, uint16_t color)
{
    /* 设置光标：先写列地址，再写行地址 */
    nt_write_reg(0x2A00, x >> 8);   nt_write_reg(0x2A01, x & 0xFF);
    nt_write_reg(0x2A02, x >> 8);   nt_write_reg(0x2A03, x & 0xFF);
    nt_write_reg(0x2B00, y >> 8);   nt_write_reg(0x2B01, y & 0xFF);
    nt_write_reg(0x2B02, y >> 8);   nt_write_reg(0x2B03, y & 0xFF);
    nt_write_regno(0x2C00);
    LCD->LCD_RAM = color;
}

/*
 * NT35510_GetPixelGUI
 * 对应 RA8875_GetPixelGUI：读取指定坐标的像素颜色
 * emWin 的 _GetPixelIndex 调用此函数
 */
uint16_t NT35510_GetPixelGUI(uint16_t x, uint16_t y)
{
    volatile uint16_t r, g, b;

    nt_write_reg(0x2A00, x >> 8);   nt_write_reg(0x2A01, x & 0xFF);
    nt_write_reg(0x2A02, x >> 8);   nt_write_reg(0x2A03, x & 0xFF);
    nt_write_reg(0x2B00, y >> 8);   nt_write_reg(0x2B01, y & 0xFF);
    nt_write_reg(0x2B02, y >> 8);   nt_write_reg(0x2B03, y & 0xFF);

    nt_write_regno(0x2E00);     /* 发送读 GRAM 命令 */
    r = LCD->LCD_RAM;           /* dummy read */
    r = LCD->LCD_RAM;           /* R[15:11] G[10:5] */
    b = LCD->LCD_RAM;           /* B[15:11] */

    g = r & 0xFF;
    g <<= 8;
    return (uint16_t)(((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}

/*
 * NT35510_DrawHLineGUI
 * 对应 RA8875_DrawHLine：画单色水平线
 * emWin 的 _DrawHLine 调用此函数（高频调用，需要高效）
 */
void NT35510_DrawHLineGUI(uint16_t x0, uint16_t y, uint16_t x1, uint16_t color)
{
    uint16_t len = x1 - x0 + 1;

    /* 设置窗口为一行，连续写像素，GRAM 地址自动递增 */
    NT35510_SetWindow(x0, y, x1, y);
    while (len--)
    {
        LCD->LCD_RAM = color;
    }
}

/*
 * NT35510_DrawVLineGUI
 * 对应 RA8875_DrawVLine：画单色垂直线
 * emWin 的 _DrawVLine 调用此函数
 */
void NT35510_DrawVLineGUI(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
    uint16_t len = y1 - y0 + 1;

    NT35510_SetWindow(x, y0, x, y1);
    while (len--)
    {
        LCD->LCD_RAM = color;
    }
}

/*
 * NT35510_FillRectGUI
 * 对应 RA8875_RTERect：用端点坐标填充矩形（emWin 传的是端点，不是宽高）
 * emWin 的 _FillRect 调用此函数
 */
void NT35510_FillRectGUI(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t total = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);

    NT35510_SetWindow(x0, y0, x1, y1);
    while (total--)
    {
        LCD->LCD_RAM = color;
    }
}

/*
 * NT35510_DrawHColorLineGUI
 * 对应 RA8875_DrawHColorLine：画彩色水平线，颜色来自数组
 * emWin 的 _DrawBitLine16BPP 调用此函数（位图/图片显示的核心路径）
 */
void NT35510_DrawHColorLineGUI(uint16_t x, uint16_t y, uint16_t xsize, const uint16_t *pColor)
{
    NT35510_SetWindow(x, y, x + xsize - 1, y);
    while (xsize--)
    {
        LCD->LCD_RAM = *pColor++;
    }
}
