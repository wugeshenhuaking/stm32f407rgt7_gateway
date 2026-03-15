#ifndef __BSP_TOUCH_PORT_H
#define __BSP_TOUCH_PORT_H

/*
*********************************************************************************************************
*
*	模块名称 : 触摸屏硬件接口移植层
*	文件名称 : bsp_touch_port.h
*	说    明 : 本文件定义对外暴露的触摸屏接口，所有函数以 BSP_Touch_ 开头，
*	           避免与 emWin、FreeRTOS 等库的函数名冲突。
*
*	设计原则 :
*		- 应用层只调用 BSP_Touch_Xxx() 函数
*		- bsp_touch_port.c 内部调用 GT9XXX_Xxx() 完成实际操作
*		- 若以后更换触摸芯片，只修改 bsp_touch_port.c，应用层不变
*
*	调用层次 :
*		应用层 / GUI_X.c / emWin 触摸接口
*		        ↓  调用 BSP_Touch_Xxx()
*		bsp_touch_port.c
*		        ↓  调用 GT9XXX_Xxx()
*		gt9xxx.c
*
*	坐标说明 :
*		BSP 层输出的是屏幕逻辑坐标（已根据屏幕方向转换完毕）：
*		  竖屏：X ∈ [0, 479]，Y ∈ [0, 799]（与 NT35510 分辨率对应）
*		  横屏：X ∈ [0, 799]，Y ∈ [0, 479]
*
*	多点触摸 :
*		支持最多 BSP_TOUCH_MAX_POINTS 个同时触点
*		emWin 单点触摸只读取 points[0]
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"
#include "gt9xxx.h"

/* ============================================================
 * 最大触点数（与底层芯片保持一致）
 * ============================================================ */
#define BSP_TOUCH_MAX_POINTS    GT9XXX_MAX_TOUCH    /* 5 或 10 */

/* ============================================================
 * 屏幕方向（与 LCD 初始化方向保持一致）
 * 0 = 竖屏（NT35510 默认，480×800）
 * 1 = 横屏（800×480）
 * ============================================================ */
#define BSP_TOUCH_SCREEN_DIR    0   /* 0=竖屏480x800, 1=横屏800x480 */

/* ============================================================
 * 屏幕物理分辨率（与 LCD 分辨率对应，用于坐标翻转计算）
 * ============================================================ */
#define BSP_TOUCH_LCD_WIDTH     480
#define BSP_TOUCH_LCD_HEIGHT    800

/* ============================================================
 * 单个触点数据结构（逻辑坐标）
 * ============================================================ */
typedef struct
{
    uint16_t x;         /* 逻辑 X 坐标（已映射到屏幕坐标系） */
    uint16_t y;         /* 逻辑 Y 坐标（已映射到屏幕坐标系） */
    uint8_t  pressed;   /* 1 = 当前触点有效（按下），0 = 无效 */
} BSP_TouchPointTypeDef;

/* ============================================================
 * 触摸状态结构
 * ============================================================ */
typedef struct
{
    BSP_TouchPointTypeDef points[BSP_TOUCH_MAX_POINTS]; /* 各触点数据 */
    uint8_t  touch_num;     /* 当前有效触点数 */
    uint8_t  is_pressed;    /* 屏幕是否被触摸（任一触点有效则为 1） */
} BSP_TouchStateTypeDef;

/* ============================================================
 * 接口函数声明
 * ============================================================ */

/*
 * BSP_Touch_Init
 * 初始化触摸控制器（GPIO、软件 I2C、GT9xxx 芯片复位及配置）
 * 返回：0 = 成功，1 = 失败
 */
uint8_t BSP_Touch_Init(void);

/*
 * BSP_Touch_Scan
 * 扫描触摸屏，更新内部状态。建议在主循环或定时器中周期调用（10~20ms 一次）。
 * 返回：当前有效触点数（0 = 无触摸）
 */
uint8_t BSP_Touch_Scan(void);

/*
 * BSP_Touch_GetState
 * 获取最近一次 Scan 得到的触摸状态（不重新扫描硬件）
 * 参数：state - 指向调用者提供的状态结构体，函数将结果写入其中
 */
void BSP_Touch_GetState(BSP_TouchStateTypeDef *state);

/*
 * BSP_Touch_GetPoint
 * 获取指定触点的逻辑坐标（不重新扫描硬件）
 * 参数：index - 触点索引（0 ~ BSP_TOUCH_MAX_POINTS-1）
 *        x, y  - 输出坐标指针
 * 返回：1 = 该触点有效，0 = 无效
 */
uint8_t BSP_Touch_GetPoint(uint8_t index, uint16_t *x, uint16_t *y);

/*
 * BSP_Touch_IsPressed
 * 返回屏幕是否被按下（任一触点有效）
 * 返回：1 = 有触摸，0 = 无触摸
 */
uint8_t BSP_Touch_IsPressed(void);

/*
 * BSP_Touch_GetTouchNum
 * 返回当前有效触点数
 */
uint8_t BSP_Touch_GetTouchNum(void);

#endif /* __BSP_TOUCH_PORT_H */