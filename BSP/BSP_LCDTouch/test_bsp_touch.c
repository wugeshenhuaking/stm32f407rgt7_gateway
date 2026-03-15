/*
*********************************************************************************************************
*
*	模块名称 : 触摸屏驱动测试
*	文件名称 : touch_test.c
*	说    明 : 针对 GT9xxx + bsp_touch_port 的硬件在环（HIL）测试。
*	           测试在真实硬件上运行，结果通过 printf → USART1 输出。
*	           本项目硬件固定为 NT35510 + GT9xxx，无需芯片 ID 判断，
*	           直接测试触摸功能是否正常。
*
*	前提条件 :
*		1. USART1 已初始化，printf 已重定向
*		2. NT35510_Init() 已完成（坐标范围依赖屏幕分辨率宏）
*		3. BSP_Touch_Init() 已完成
*
*	每个测试的结构 :
*		- 打印测试名称和测试目的
*		- 执行操作
*		- 打印实际结果
*		- 与预期值对比，打印 PASS / FAIL 及原因
*		- 返回 0=PASS，1=FAIL
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include "test_bsp_touch.h"
#include "bsp_touch_port.h"
#include "gt9xxx.h"
#include "lcd_nt35510.h"

/* ============================================================
 * 内部辅助常量
 * ============================================================ */
#define WAIT_TIMEOUT_MS     10000U   /* 等用户触摸的最长时间：10 秒 */
#define STRESS_SCAN_COUNT   1000U    /* 压力测试扫描次数 */
#define SCAN_INTERVAL_MS    10U      /* 扫描间隔 ms，模拟真实调用节奏 */

/* 坐标合法范围（对应 NT35510 横屏 800×480） */
#define X_MAX   (BSP_TOUCH_LCD_WIDTH  - 1)   /* 799 */
#define Y_MAX   (BSP_TOUCH_LCD_HEIGHT - 1)   /* 479 */


/* ============================================================
 * Diagnose：I2C 通信诊断
 *
 * 目的   : 在触摸测试失败时，先确认 I2C 是否能正常通信
 *          直接读写芯片寄存器，打印原始结果
 * 不需要操作屏幕，上电后自动运行
 * ============================================================ */
/* ============================================================
 * 内部辅助：等待触摸完全释放 + 冲洗残余帧
 * 在每个需要用户操作的测试结束后调用，防止上一次触摸污染下一个测试
 * ============================================================ */
static void Touch_WaitRelease(void)
{
    uint8_t clean = 0;
    while (clean < 5)
    {
        HAL_Delay(SCAN_INTERVAL_MS);
        BSP_Touch_Scan();
        if (!BSP_Touch_IsPressed())
            clean++;
        else
            clean = 0;
    }
    /* 额外冲洗 3 帧，确保 GSTID 已清零 */
    HAL_Delay(50);
    BSP_Touch_Scan();
    HAL_Delay(SCAN_INTERVAL_MS);
    BSP_Touch_Scan();
    HAL_Delay(SCAN_INTERVAL_MS);
    BSP_Touch_Scan();
}

static void Touch_Diagnose(void)
{
    uint8_t  status = 0;
    uint8_t  pid[5] = {0};
    uint8_t  wr_ret;
    uint8_t  dummy = 0;

    printf("\r\n----- I2C Diagnose -----\r\n");

    /* 1. 扫描地址 0x28
     *    GT9xxx 地址由复位时 INT 引脚电平决定：
     *      INT=0 -> 0x28/0x29  (默认，常用)
     *      INT=1 -> 0xBA/0xBB  (备用)
     */
    wr_ret = GT9XXX_WrReg(GT9XXX_CTRL_REG, &dummy, 1);
    printf("Addr 0x28: %s\r\n", wr_ret == 0 ? "ACK  <-- chip here" : "NACK");

    if (wr_ret != 0)
    {
        printf("Addr 0x28 no response. Check:\r\n");
        printf("  [A] Address wrong: INT was HIGH during reset -> chip at 0xBA\r\n");
        printf("      Ensure INT(PB1) is driven LOW before RST goes HIGH\r\n");
        printf("  [B] Wiring: SCL=PB0, SDA=PF11, RST=PC13, INT=PB1\r\n");
        printf("  [C] Pull-up: need 4.7k on SCL->3.3V and SDA->3.3V\r\n");
        printf("  [D] Power: touch IC VCC must be 3.3V\r\n");
        printf("  [E] GT9XXX_Init() must be called before running tests\r\n");
        printf("\r\nMeasure with multimeter:\r\n");
        printf("  SCL(PB0) idle = ~3.3V, SDA(PF11) idle = ~3.3V\r\n");
        printf("  If 0V: pull-up missing or pin shorted\r\n");
        printf("------------------------\r\n");
        return;
    }

    /* 2. I2C OK - 读 PID 寄存器（仅诊断用） */
    GT9XXX_RdReg(GT9XXX_PID_REG, pid, 4);
    pid[4] = 0;
    printf("PID: [%s] (0x%02X 0x%02X 0x%02X 0x%02X)\r\n",
           pid, pid[0], pid[1], pid[2], pid[3]);

    /* 3. 读 GSTID，看触摸状态原始值 */
    GT9XXX_RdReg(GT9XXX_GSTID_REG, &status, 1);
    printf("GSTID: 0x%02X  BufReady=%d  touch_num=%d\r\n",
           status, (status >> 7) & 1, status & 0x0F);

    printf("I2C OK.\r\n");
    printf("------------------------\r\n");
}
/* ============================================================
 * Test1：单点触摸坐标验证
 *
 * 目的   : 验证触摸坐标能被正确读取，且在屏幕逻辑坐标范围内
 * 操作   : 用手随意触摸屏幕 5 次
 * 预期   : x ∈ [0, 799]，y ∈ [0, 479]，坐标每次不同（说明不是固定值）
 * 失败原因:
 *   - 坐标越界：coord_convert() 方向配置错误
 *   - 坐标始终为 0：Scan 没有正确读取数据
 *   - 超时无响应：触摸屏未触摸或 BSP_Touch_Scan 未调用
 * ============================================================ */
uint8_t Touch_Test_SingleTouch(void)
{
    uint32_t start_tick;
    uint16_t x, y;
    uint8_t  got_touch = 0;
    uint8_t  touch_count = 0;
    uint8_t  out_of_range = 0;

    printf("\r\n===== Test1: Single Touch Coordinate =====\r\n");
    printf("Purpose : Verify touch coordinates are valid and within screen bounds\r\n");
    printf("Expected: x in [0, %d], y in [0, %d]\r\n", X_MAX, Y_MAX);
    printf("Action  : >>> Please touch the screen 5 times (within 10s) <<<\r\n");

    start_tick = HAL_GetTick();

    while (touch_count < 5)
    {
        if (HAL_GetTick() - start_tick > WAIT_TIMEOUT_MS)
        {
            printf("TIMEOUT: only got %d/5 touches\r\n", touch_count);
            break;
        }

        BSP_Touch_Scan();

        if (BSP_Touch_GetPoint(0, &x, &y))
        {
            /* 等抬起后才算一次，避免一次按下打印多行 */
            if (!got_touch)
            {
                touch_count++;
                got_touch = 1;

                printf("Touch #%d: x=%-4d y=%-4d", touch_count, x, y);

                if (x > X_MAX || y > Y_MAX)
                {
                    printf("  --> OUT OF RANGE!\r\n");
                    out_of_range++;
                }
                else
                {
                    printf("  --> OK\r\n");
                }
            }
        }
        else
        {
            /* 释放防抖：连续 3 次扫描无触摸才复位标志
             * 避免手指刚抬起时芯片残余数据引起重复计数 */
            if (got_touch)
            {
                uint8_t stable = 0;
                while (stable < 3)
                {
                    HAL_Delay(SCAN_INTERVAL_MS);
                    BSP_Touch_Scan();
                    if (!BSP_Touch_GetPoint(0, &x, &y))
                        stable++;
                    else
                        stable = 0;
                }
                got_touch = 0;
            }
        }

        HAL_Delay(SCAN_INTERVAL_MS);
    }

    if (touch_count == 0)
    {
        printf("Test1 FAIL! No touch detected.\r\n");
        printf("  >> Check I2C lines: SCL=PB0, SDA=PF11, RST=PC13, INT=PB1\r\n");
        printf("  >> Check 3.3V power to touch controller\r\n");
        return 1;
    }
    else if (out_of_range > 0)
    {
        printf("Test1 FAIL! %d coordinate(s) out of range.\r\n", out_of_range);
        printf("  >> Check BSP_TOUCH_LCD_WIDTH/HEIGHT and coord_convert()\r\n");
        return 1;
    }
    else
    {
        printf("Test1 PASS! All %d touch(es) in valid range.\r\n", touch_count);
        return 0;
    }
}

/* ============================================================
 * Test2：四角边界坐标验证
 *
 * 目的   : 验证屏幕四个角的坐标映射是否正确
 *          （确认 X/Y 方向、镜像是否与屏幕方向一致）
 * 操作   : 按提示依次触摸四个角
 * 预期   :
 *   左上角 → x 接近 0，y 接近 0
 *   右上角 → x 接近 479，y 接近 0
 *   左下角 → x 接近 0，y 接近 799
 *   右下角 → x 接近 479，y 接近 799
 * 允许误差: ±50 像素（手指触摸精度）
 * 失败原因:
 *   - 坐标 X/Y 互换：BSP_TOUCH_SCREEN_DIR 配置错误
 *   - 坐标镜像：coord_convert() 中需要加镜像翻转
 * ============================================================ */
uint8_t Touch_Test_CoordBound(void)
{
    const char *corner_name[4] = { "Top-Left", "Top-Right", "Bot-Left", "Bot-Right" };

    /* 期望范围：[x_min, x_max, y_min, y_max]，±50 像素容差 */
    /* 期望范围 [x_min, x_max, y_min, y_max]，±100 像素容差
     * 触摸屏边缘有物理死区，手指难以触到像素0，放宽容差
     * 对应竖屏 480×800：x 轴 0~479，y 轴 0~799 */
    const uint16_t expected[4][4] =
    {
        {   0, 100,   0, 100 },    /* 左上 */
        { 379, 479,   0, 100 },    /* 右上 */
        {   0, 100, 699, 799 },    /* 左下 */
        { 379, 479, 699, 799 },    /* 右下 */
    };

    uint8_t  fail_count = 0;
    uint32_t start_tick;
    uint16_t x, y;
    uint8_t  got;
    uint8_t  i;

    printf("\r\n===== Test2: Corner Boundary Coordinates =====\r\n");
    printf("Purpose : Verify X/Y direction and screen mapping are correct\r\n");
    printf("Tolerance: +-100 pixels\r\n");

    for (i = 0; i < 4; i++)
    {
        printf("Action  : >>> Touch the %s corner (within 10s) <<<\r\n", corner_name[i]);

        start_tick = HAL_GetTick();
        got = 0;

        while (!got)
        {
            if (HAL_GetTick() - start_tick > WAIT_TIMEOUT_MS)
            {
                printf("  TIMEOUT for %s\r\n", corner_name[i]);
                fail_count++;
                break;
            }

            BSP_Touch_Scan();

            if (BSP_Touch_IsPressed())
            {
                BSP_Touch_GetPoint(0, &x, &y);

                /* 等抬起后读一次，防止一次按下重复处理 */
                while (BSP_Touch_IsPressed())
                {
                    BSP_Touch_Scan();
                    HAL_Delay(SCAN_INTERVAL_MS);
                }

                printf("  Got: x=%-4d y=%-4d  ", x, y);
                printf("Expected x[%d~%d] y[%d~%d]  ",
                       expected[i][0], expected[i][1],
                       expected[i][2], expected[i][3]);

                if (x >= expected[i][0] && x <= expected[i][1] &&
                    y >= expected[i][2] && y <= expected[i][3])
                {
                    printf("PASS\r\n");
                }
                else
                {
                    printf("FAIL\r\n");
                    fail_count++;
                }
                got = 1;
            }

            HAL_Delay(SCAN_INTERVAL_MS);
        }
    }

    if (fail_count == 0)
    {
        printf("Test2 PASS! All corners mapped correctly.\r\n");
        return 0;
    }
    else
    {
        printf("Test2 FAIL! %d corner(s) wrong.\r\n", fail_count);
        printf("  >> If X/Y swapped: check BSP_TOUCH_SCREEN_DIR\r\n");
        printf("  >> If mirrored: adjust coord_convert() in bsp_touch_port.c\r\n");
        return 1;
    }
}

/* ============================================================
 * Test3：按下 / 抬起状态切换验证
 *
 * 目的   : 验证 BSP_Touch_IsPressed() 状态与实际手指动作一致
 *          以及抬起后状态能正确清零（不粘连）
 * 操作   : 按一下再松开，重复 3 次
 * 预期   :
 *   按下时 IsPressed() == 1
 *   松开后 IsPressed() == 0（不超过 3 个扫描周期仍为 1）
 * 失败原因:
 *   - 按下时一直是 0：is_pressed 标志更新逻辑错误
 *   - 松开后长时间是 1（粘连）：GT9XXX_Scan 没有正确清除 Buffer ready 标志
 * ============================================================ */
uint8_t Touch_Test_PressRelease(void)
{
    uint8_t  round;
    uint32_t start_tick;
    uint8_t  press_ok;
    uint8_t  release_ok;
    uint8_t  fail_count = 0;
    uint8_t  stale_count;

    printf("\r\n===== Test3: Press / Release State =====\r\n");
    printf("Purpose : Verify pressed/released state transitions correctly\r\n");
    printf("Action  : >>> Press and release the screen 3 times <<<\r\n");

    for (round = 1; round <= 3; round++)
    {
        press_ok   = 0;
        release_ok = 0;

        printf("Round #%d: waiting for press...\r\n", round);

        /* --- 等待按下 --- */
        start_tick = HAL_GetTick();
        while (HAL_GetTick() - start_tick < WAIT_TIMEOUT_MS)
        {
            BSP_Touch_Scan();
            if (BSP_Touch_IsPressed())
            {
                press_ok = 1;
                printf("  Pressed  : IsPressed()=1  OK\r\n");
                break;
            }
            HAL_Delay(SCAN_INTERVAL_MS);
        }

        if (!press_ok)
        {
            printf("  Pressed  : TIMEOUT - IsPressed() never became 1\r\n");
            fail_count++;
            continue;
        }

        /* --- 等待抬起 --- */
        start_tick = HAL_GetTick();
        while (HAL_GetTick() - start_tick < WAIT_TIMEOUT_MS)
        {
            BSP_Touch_Scan();
            if (!BSP_Touch_IsPressed())
            {
                release_ok = 1;
                break;
            }
            HAL_Delay(SCAN_INTERVAL_MS);
        }

        if (!release_ok)
        {
            printf("  Released : TIMEOUT - IsPressed() stuck at 1 (sticky!)\r\n");
            printf("  >> Check GT9XXX_GSTID_REG clear logic in GT9XXX_Scan()\r\n");
            fail_count++;
            continue;
        }

        /* --- 检查抬起后不粘连
         * 等待 50ms 让 GSTID 完成清零，再扫描 3 次
         * 允许 1 次残余（芯片最后一帧可能延迟到达）--- */
        HAL_Delay(150);   /* 等待芯片完成 GSTID 清零，至少需要 2 个报告周期(~14ms each) */
        stale_count = 0;
        for (uint8_t k = 0; k < 3; k++)
        {
            HAL_Delay(SCAN_INTERVAL_MS);
            BSP_Touch_Scan();
            if (BSP_Touch_IsPressed()) stale_count++;
        }

        if (stale_count <= 1)   /* 允许最多 1 次残余帧 */
        {
            printf("  Released : stale=%d/3  OK\r\n", stale_count);
        }
        else
        {
            printf("  Released : stale state %d/3 scans  FAIL\r\n", stale_count);
            printf("  >> Check GSTID clear logic in GT9XXX_Scan()\r\n");
            fail_count++;
        }
    }

    if (fail_count == 0)
    {
        printf("Test3 PASS! Press/release state transitions correctly.\r\n");
        return 0;
    }
    else
    {
        printf("Test3 FAIL! %d round(s) failed.\r\n", fail_count);
        return 1;
    }
}

/* ============================================================
 * Test4：多点触摸验证
 *
 * 目的   : 验证多个触点能被同时读取，且各点坐标相互独立
 * 操作   : 用两根手指同时按住屏幕
 * 预期   :
 *   touch_num >= 2
 *   两个触点坐标均在范围内
 *   两个触点坐标不完全相同（说明没有把同一个点复制到多个槽）
 * 失败原因:
 *   - touch_num 一直是 1：驱动 max_points 配置错误，或 GT9xxx 多点未使能
 *   - 两点坐标完全一样：坐标读取循环没有正确切换 s_tpx_tbl[i]
 * ============================================================ */
uint8_t Touch_Test_MultiTouch(void)
{
    uint32_t start_tick;
    uint16_t x0, y0, x1, y1;
    uint8_t  num;
    uint8_t  got = 0;

    printf("\r\n===== Test4: Multi-Touch =====\r\n");
    printf("Purpose : Verify 2+ touch points are read independently\r\n");
    printf("Action  : >>> Press with TWO fingers simultaneously (within 10s) <<<\r\n");

    start_tick = HAL_GetTick();

    while (HAL_GetTick() - start_tick < WAIT_TIMEOUT_MS)
    {
        BSP_Touch_Scan();
        num = BSP_Touch_GetTouchNum();

        if (num >= 2)
        {
            BSP_Touch_GetPoint(0, &x0, &y0);
            BSP_Touch_GetPoint(1, &x1, &y1);

            printf("touch_num = %d\r\n", num);
            printf("Point 0: x=%-4d y=%-4d\r\n", x0, y0);
            printf("Point 1: x=%-4d y=%-4d\r\n", x1, y1);

            int16_t dx = (int16_t)x0 - (int16_t)x1;
            int16_t dy = (int16_t)y0 - (int16_t)y1;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < 5 && dy < 5)
            {
                printf("Test4 FAIL! Two points at same position (dx=%d dy=%d)\r\n", dx, dy);
                printf("  >> Check s_tpx_tbl[] indexing in GT9XXX_Scan()\r\n");
                return 1;
            }

            if (x0 > X_MAX || y0 > Y_MAX || x1 > X_MAX || y1 > Y_MAX)
            {
                printf("Test4 FAIL! Coordinate out of range.\r\n");
                return 1;
            }

            got = 1;
            break;
        }

        HAL_Delay(SCAN_INTERVAL_MS);
    }

    if (!got)
    {
        printf("Test4 FAIL! Did not detect 2 touch points.\r\n");
        printf("  >> If screen supports single-touch only, skip this test\r\n");
        return 1;
    }

    printf("Test4 PASS! Multi-touch working correctly.\r\n");
    return 0;
}

/* ============================================================
 * Test5：扫描压力测试 + LCD 画点/画线联动
 *
 * 目的   : 验证扫描稳定性，同时通过 LCD 直观展示触摸轨迹
 * Phase1 : 无触摸扫描 500 次，检测误报（keep hands off）
 * Phase2 : 5000ms 内随意触摸，触摸轨迹实时绘制到 LCD
 *          - 按下瞬间：画一个 3x3 的小方块（标记触点）
 *          - 持续滑动：从上一个点到当前点画线
 *          - 抬起后再次按下：重新开始画线（不连接）
 * ============================================================ */
uint8_t Touch_Test_ScanStress(void)
{
    uint32_t i;
    uint8_t  num;
    uint16_t x, y;
    uint32_t idle_false_positive = 0;
    uint32_t active_out_of_range = 0;
    uint32_t active_over_max     = 0;
    uint32_t start_tick;

    printf("\r\n===== Test5: Scan Stress Test (%lu scans) =====\r\n",
           (unsigned long)STRESS_SCAN_COUNT);
    printf("Purpose : Verify stability under rapid repeated scanning\r\n");

    /* --- 阶段1：无触摸，扫描 500 次 --- */
    printf("[Phase 1/2] No-touch scan (500 times) - keep your hands off...\r\n");
    /* 冲洗上一个测试残余帧 */
    Touch_WaitRelease();
    start_tick = HAL_GetTick();

    for (i = 0; i < STRESS_SCAN_COUNT / 2; i++)
    {
        BSP_Touch_Scan();
        num = BSP_Touch_GetTouchNum();

        if (num != 0)
        {
            idle_false_positive++;
            if (idle_false_positive <= 3)
            {
                BSP_Touch_GetPoint(0, &x, &y);
                printf("  FalsePos #%lu at scan %lu: x=%d y=%d num=%d\r\n",
                       (unsigned long)idle_false_positive,
                       (unsigned long)i, x, y, num);
            }
        }

        HAL_Delay(2);
    }

    printf("  Done in %lu ms, false_positives=%lu\r\n",
           (unsigned long)(HAL_GetTick() - start_tick),
           (unsigned long)idle_false_positive);

    /* --- 阶段2：LCD 画线模式，持续 5000ms --- */
    printf("[Phase 2/2] Draw mode - touch and drag on screen (5000ms)...\r\n");

    /* 清屏，黑色背景 */
    NT35510_FillScreen(0x0000);

    {
        uint16_t last_x = 0xFFFF;   /* 0xFFFF 表示无上一个点（刚按下或刚抬起）*/
        uint16_t last_y = 0xFFFF;
        uint8_t  was_pressed = 0;
        uint32_t draw_count  = 0;   /* 累计绘制次数 */

        start_tick = HAL_GetTick();

        while (HAL_GetTick() - start_tick < 5000)
        {
            BSP_Touch_Scan();
            num = BSP_Touch_GetTouchNum();

            /* 坐标越界检查（顺带统计，不中断绘制）*/
            if (num > BSP_TOUCH_MAX_POINTS)
            {
                active_over_max++;
            }

            if (BSP_Touch_GetPoint(0, &x, &y))
            {
                if (x > X_MAX || y > Y_MAX)
                {
                    active_out_of_range++;
                }
                else
                {
                    if (!was_pressed || last_x == 0xFFFF)
                    {
                        /* 刚按下：画一个 3x3 小方块标记触点 */
                        NT35510_FillRect(x > 1 ? x-1 : 0,
                                         y > 1 ? y-1 : 0,
                                         3, 3, 0x07E0);  /* 绿色 */
                    }
                    else
                    {
                        /* 持续滑动：从上一点到当前点画线 */
                        NT35510_DrawLine(last_x, last_y, x, y, 0xFFFF);  /* 白色 */
                    }
                    last_x = x;
                    last_y = y;
                    was_pressed = 1;
                    draw_count++;
                }
            }
            else
            {
                /* 手指抬起：重置上一个点，下次按下重新开始画线 */
                last_x = 0xFFFF;
                last_y = 0xFFFF;
                was_pressed = 0;
            }

            HAL_Delay(10);  /* 10ms 扫描间隔 */
        }

        printf("  Done in 5000ms, draw_count=%lu, out_of_range=%lu, over_max=%lu\r\n",
               (unsigned long)draw_count,
               (unsigned long)active_out_of_range,
               (unsigned long)active_over_max);
    }

    printf("\r\nStress Test Summary:\r\n");
    printf("  Phase1 false positives : %lu  (expect 0)\r\n",
           (unsigned long)idle_false_positive);
    printf("  Phase2 coord out-range : %lu  (expect 0)\r\n",
           (unsigned long)active_out_of_range);
    printf("  Phase2 over max points : %lu  (expect 0)\r\n",
           (unsigned long)active_over_max);

    if (idle_false_positive == 0 && active_out_of_range == 0 && active_over_max == 0)
    {
        printf("Test5 PASS! Stable under stress.\r\n");
        return 0;
    }
    else
    {
        printf("Test5 FAIL!\r\n");
        if (idle_false_positive > 0)
            printf("  >> Possible cause: GSTID not cleared after read (check GT9XXX_Scan)\r\n");
        if (active_out_of_range > 0)
            printf("  >> Possible cause: coord_convert() overflow or wrong LCD size macros\r\n");
        return 1;
    }
}

/* ============================================================
 * 一键运行全部测试
 * ============================================================ */
void Touch_RunAllTests(void)
{
    uint8_t t1, t2, t3, t4, t5;

    printf("\r\n");
    printf("========================================\r\n");
    printf("  GT9xxx Touch Driver Test Suite\r\n");
    printf("  LCD: %dx%d  MaxPoints: %d\r\n",
           BSP_TOUCH_LCD_WIDTH, BSP_TOUCH_LCD_HEIGHT, BSP_TOUCH_MAX_POINTS);
    printf("========================================\r\n");

    Touch_Diagnose();
    Touch_WaitRelease();
    t1 = Touch_Test_SingleTouch();
    Touch_WaitRelease();
    t2 = Touch_Test_CoordBound();
    Touch_WaitRelease();
    t3 = Touch_Test_PressRelease();
    Touch_WaitRelease();
    t4 = Touch_Test_MultiTouch();
    Touch_WaitRelease();
    t5 = Touch_Test_ScanStress();

    printf("\r\n========================================\r\n");
    printf("  Test Summary\r\n");
    printf("========================================\r\n");
    printf("Test1 SingleTouch : %s\r\n", t1 == 0 ? "PASS" : "FAIL");
    printf("Test2 CoordBound  : %s\r\n", t2 == 0 ? "PASS" : "FAIL");
    printf("Test3 PressRelease: %s\r\n", t3 == 0 ? "PASS" : "FAIL");
    printf("Test4 MultiTouch  : %s\r\n", t4 == 0 ? "PASS" : "FAIL");
    printf("Test5 ScanStress  : %s\r\n", t5 == 0 ? "PASS" : "FAIL");

    uint8_t fail_total = t1 + t2 + t3 + t4 + t5;
    printf("----------------------------------------\r\n");
    if (fail_total == 0)
        printf("All 5 Tests PASSED! Touch driver OK.\r\n");
    else
        printf("%d Test(s) FAILED. See details above.\r\n", fail_total);
    printf("========================================\r\n");
}
