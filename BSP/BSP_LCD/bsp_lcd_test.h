#ifndef __LCD_NT35510_TEST_H
#define __LCD_NT35510_TEST_H

/* 单项测试 */
void NT35510_Test_FillScreen(void);   /* 测试1：全屏填充三原色 */
void NT35510_Test_FillRect(void);     /* 测试2：矩形色块 */
void NT35510_Test_DrawLine(void);     /* 测试3：画线 */
void NT35510_Test_DrawRect(void);     /* 测试4：空心矩形 */
void NT35510_Test_DrawCircle(void);   /* 测试5：圆形 */
void NT35510_Test_DrawPixel(void);    /* 测试6：单点绘制 */
void NT35510_Test_Checkerboard(void); /* 测试7：棋盘格 */
void NT35510_Test_Gradient(void);     /* 测试8：颜色渐变 */

/* 一键运行全部测试 */
void NT35510_RunAllTests(void);

#endif /* __LCD_NT35510_TEST_H */
