#ifndef __TOUCH_TEST_H
#define __TOUCH_TEST_H

/*
*********************************************************************************************************
*
*	模块名称 : 触摸屏驱动测试
*	文件名称 : touch_test.h
*	说    明 : 针对 GT9xxx + bsp_touch_port 的硬件在环测试
*	           所有测试通过串口打印结果，需要提前初始化 USART1 和 printf 重定向
*
*	使用方法 :
*		在 main.c 的初始化完成后调用：
*
*		BSP_Touch_Init();
*		Touch_RunAllTests();
*
*	测试列表 :
*		Test1  ChipID      — 读取芯片 ID，验证 I2C 通信是否正常
*		Test2  SingleTouch — 等待单点触摸，打印坐标，验证坐标范围
*		Test3  CoordBound  — 引导用户触摸四角，验证边界坐标映射
*		Test4  PressRelease— 验证按下/抬起状态切换是否正确
*		Test5  MultiTouch  — 等待多点触摸，打印各触点坐标
*		Test6  ScanStress  — 连续高速扫描，检测有无崩溃或数据异常
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

uint8_t Touch_Test_ChipID(void);
uint8_t Touch_Test_SingleTouch(void);
uint8_t Touch_Test_CoordBound(void);
uint8_t Touch_Test_PressRelease(void);
uint8_t Touch_Test_MultiTouch(void);
uint8_t Touch_Test_ScanStress(void);

void Touch_RunAllTests(void);

#endif /* __TOUCH_TEST_H */
