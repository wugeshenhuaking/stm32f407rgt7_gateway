#include "lcd_nt35510.h"

/* ============================================================
 * NT35510 驱动测试用例
 * 使用方法：在 main.c 里 NT35510_Init() 之后调用
 *           NT35510_RunAllTests();
 * ============================================================ */

/* ---- 内部工具：在屏幕上显示当前测试名称（用色块代替文字）---- */
static void test_banner(uint16_t color)
{
    /* 顶部画一条20像素高的色条作为测试标识 */
    NT35510_FillRect(0, 0, NT35510_WIDTH, 20, color);
    HAL_Delay(300);
}

/* ============================================================
 * 测试1：全屏填充三原色
 * 预期结果：屏幕依次显示红、绿、蓝，各持续1秒
 * ============================================================ */
void NT35510_Test_FillScreen(void)
{
    NT35510_FillScreen(NT35510_RED);
    HAL_Delay(1000);

    NT35510_FillScreen(NT35510_GREEN);
    HAL_Delay(1000);

    NT35510_FillScreen(NT35510_BLUE);
    HAL_Delay(1000);

    NT35510_FillScreen(NT35510_BLACK);
    HAL_Delay(500);
}

/* ============================================================
 * 测试2：矩形填充
 * 预期结果：黑底上出现5个不同颜色的矩形色块
 * ============================================================ */
void NT35510_Test_FillRect(void)
{
    NT35510_FillScreen(NT35510_BLACK);
    test_banner(NT35510_WHITE);

    /* 5个矩形均匀排列 */
    NT35510_FillRect(10,  30, 85, 200, NT35510_RED);
    NT35510_FillRect(105, 30, 85, 200, NT35510_GREEN);
    NT35510_FillRect(200, 30, 85, 200, NT35510_BLUE);
    NT35510_FillRect(295, 30, 85, 200, NT35510_YELLOW);
    NT35510_FillRect(390, 30, 85, 200, NT35510_CYAN);

    HAL_Delay(2000);
}

/* ============================================================
 * 测试3：画线
 * 预期结果：黑底上出现水平线、垂直线、对角线
 * ============================================================ */
void NT35510_Test_DrawLine(void)
{
    NT35510_FillScreen(NT35510_BLACK);
    test_banner(NT35510_CYAN);

    /* 水平线 */
    NT35510_DrawLine(0,   200, 479, 200, NT35510_RED);
    /* 垂直线 */
    NT35510_DrawLine(240, 30,  240, 770, NT35510_GREEN);
    /* 左上到右下对角线 */
    NT35510_DrawLine(0,   30,  479, 799, NT35510_YELLOW);
    /* 右上到左下对角线 */
    NT35510_DrawLine(479, 30,  0,   799, NT35510_CYAN);

    HAL_Delay(2000);
}

/* ============================================================
 * 测试4：画空心矩形
 * 预期结果：黑底上出现3个嵌套矩形框
 * ============================================================ */
void NT35510_Test_DrawRect(void)
{
    NT35510_FillScreen(NT35510_BLACK);
    test_banner(NT35510_YELLOW);

    NT35510_DrawRect(20,  40,  460, 760, NT35510_RED);
    NT35510_DrawRect(60,  100, 420, 700, NT35510_GREEN);
    NT35510_DrawRect(100, 160, 380, 640, NT35510_BLUE);

    HAL_Delay(2000);
}

/* ============================================================
 * 测试5：画圆
 * 预期结果：黑底上出现3个不同半径的同心圆
 * ============================================================ */
void NT35510_Test_DrawCircle(void)
{
    NT35510_FillScreen(NT35510_BLACK);
    test_banner(NT35510_MAGENTA);

    /* 以屏幕中心为圆心，画3个同心圆 */
    NT35510_DrawCircle(240, 400, 50,  NT35510_RED);
    NT35510_DrawCircle(240, 400, 100, NT35510_GREEN);
    NT35510_DrawCircle(240, 400, 150, NT35510_BLUE);

    HAL_Delay(2000);
}

/* ============================================================
 * 测试6：单点绘制
 * 预期结果：屏幕上出现一条由像素点组成的斜线（较慢）
 * ============================================================ */
void NT35510_Test_DrawPixel(void)
{
    NT35510_FillScreen(NT35510_BLACK);
    test_banner(NT35510_WHITE);

    /* 画一条白色像素斜线 */
    for (uint16_t i = 0; i < 400; i++)
    {
        NT35510_DrawPixel(40 + i, 80 + i, NT35510_WHITE);
    }

    HAL_Delay(2000);
}

/* ============================================================
 * 测试7：棋盘格（压力测试，验证SetWindow是否正确）
 * 预期结果：黑白相间的棋盘格铺满屏幕
 * ============================================================ */
void NT35510_Test_Checkerboard(void)
{
    NT35510_FillScreen(NT35510_BLACK);

    uint16_t block = 40;  /* 每个格子 40x40 像素 */
    for (uint16_t row = 0; row < NT35510_HEIGHT / block; row++)
    {
        for (uint16_t col = 0; col < NT35510_WIDTH / block; col++)
        {
            uint16_t color = ((row + col) % 2 == 0) ? NT35510_WHITE : NT35510_BLACK;
            NT35510_FillRect(col * block, row * block, block, block, color);
        }
    }

    HAL_Delay(2000);
}

/* ============================================================
 * 测试8：颜色渐变条
 * 预期结果：屏幕上显示红绿蓝三色渐变条
 * ============================================================ */
void NT35510_Test_Gradient(void)
{
    NT35510_FillScreen(NT35510_BLACK);

    /* 红色渐变（顶部 1/3） */
    for (uint16_t i = 0; i < 480; i++)
    {
        uint8_t r = (uint8_t)(i * 255 / 479);
        uint16_t color = (uint16_t)((r >> 3) << 11);  /* RGB565 红通道 */
        NT35510_DrawLine(i, 0, i, 260, color);
    }

    /* 绿色渐变（中间 1/3） */
    for (uint16_t i = 0; i < 480; i++)
    {
        uint8_t g = (uint8_t)(i * 255 / 479);
        uint16_t color = (uint16_t)((g >> 2) << 5);   /* RGB565 绿通道 */
        NT35510_DrawLine(i, 270, i, 530, color);
    }

    /* 蓝色渐变（底部 1/3） */
    for (uint16_t i = 0; i < 480; i++)
    {
        uint8_t b = (uint8_t)(i * 255 / 479);
        uint16_t color = (uint16_t)(b >> 3);           /* RGB565 蓝通道 */
        NT35510_DrawLine(i, 540, i, 799, color);
    }

    HAL_Delay(3000);
}

/* ============================================================
 * 运行全部测试（按顺序执行）
 * ============================================================ */
void NT35510_RunAllTests(void)
{
    /* 测试1：全屏颜色填充 */
    NT35510_Test_FillScreen();

    /* 测试2：矩形色块 */
    NT35510_Test_FillRect();

    /* 测试3：画线 */
    NT35510_Test_DrawLine();

    /* 测试4：空心矩形 */
    NT35510_Test_DrawRect();

    /* 测试5：圆形 */
    NT35510_Test_DrawCircle();

    /* 测试6：单点绘制 */
    NT35510_Test_DrawPixel();

    /* 测试7：棋盘格 */
    NT35510_Test_Checkerboard();

    /* 测试8：颜色渐变 */
    NT35510_Test_Gradient();

    /* 全部完成，显示白屏表示结束 */
    NT35510_FillScreen(NT35510_WHITE);
    HAL_Delay(500);
    NT35510_FillScreen(NT35510_BLACK);
}
