/**
 ******************************************************************************
 * @file    bsp_can_test.c
 * @brief   BSP CAN unit / integration tests.
 *
 * All tests run in LOOPBACK mode.
 * Uses BSP_CAN_RecvFromBuffer() because the RX interrupt is always active
 * and moves frames into the ring buffer before direct FIFO poll can see them.
 *
 * main.c setup:
 *   MX_CAN1_Init();           // keep ！ CubeMX manages GPIO
 *   BSP_CAN_Test_RunAll();    // loopback tests
 *   BSP_CAN_Init(BSP_CAN_MODE_NORMAL);  // back to normal after tests
 ******************************************************************************
 */

#include "bsp_can_test.h"
#include "bsp_can_port.h"
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"

#define LOOPBACK_DELAY_MS   5U
#define TEST_LOG(fmt, ...)  printf("[CAN TEST] " fmt "\r\n", ##__VA_ARGS__)

/* --------------------------------------------------------------------------
 * Test 1 ！ Init / DeInit cycle
 * --------------------------------------------------------------------------*/
BSP_CAN_TestResult_t BSP_CAN_Test_Init(void)
{
    BSP_CAN_DeInit();

    if (BSP_CAN_Init(BSP_CAN_MODE_LOOPBACK) != BSP_CAN_OK)
    {
        TEST_LOG("Init FAIL");
        return TEST_FAIL;
    }

    BSP_CAN_DeInit();
    return TEST_PASS;
}

/* --------------------------------------------------------------------------
 * Test 2 ！ Loopback send / receive via ring buffer
 * --------------------------------------------------------------------------*/
BSP_CAN_TestResult_t BSP_CAN_Test_Loopback(void)
{
    BSP_CAN_DeInit();

    if (BSP_CAN_Init(BSP_CAN_MODE_LOOPBACK) != BSP_CAN_OK)
    {
        TEST_LOG("Loopback FAIL (init error)");
        return TEST_FAIL;
    }

    BSP_CAN_Msg_t tx = {
        .id     = 0x123U,
        .len    = 8U,
        .is_ext = false,
        .is_rtr = false,
        .data   = {0x01, 0x02, 0x03, 0x04, 0xDE, 0xAD, 0xBE, 0xEF},
    };

    if (BSP_CAN_Send(&tx) != BSP_CAN_OK)
    {
        TEST_LOG("Loopback FAIL (send error)");
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    HAL_Delay(LOOPBACK_DELAY_MS);   /* wait for ISR to push frame into buffer */

    if (!BSP_CAN_BufferHasData())
    {
        TEST_LOG("Loopback FAIL (buffer empty after send)");
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    BSP_CAN_Msg_t rx = {0};
    if (BSP_CAN_RecvFromBuffer(&rx) != BSP_CAN_OK)
    {
        TEST_LOG("Loopback FAIL (RecvFromBuffer error)");
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    if (rx.id != tx.id)
    {
        TEST_LOG("Loopback FAIL (ID: got 0x%03X, exp 0x%03X)",
                 (unsigned)rx.id, (unsigned)tx.id);
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    if (rx.len != tx.len)
    {
        TEST_LOG("Loopback FAIL (DLC: got %d, exp %d)", rx.len, tx.len);
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    if (memcmp(rx.data, tx.data, tx.len) != 0)
    {
        TEST_LOG("Loopback FAIL (data mismatch)");
        BSP_CAN_DeInit();
        return TEST_FAIL;
    }

    BSP_CAN_DeInit();
    return TEST_PASS;
}

/* --------------------------------------------------------------------------
 * Test 3 ！ Multi-frame burst via ring buffer
 * --------------------------------------------------------------------------*/
#define BURST_COUNT 3U

BSP_CAN_TestResult_t BSP_CAN_Test_MultiBurst(void)
{
    BSP_CAN_DeInit();

    if (BSP_CAN_Init(BSP_CAN_MODE_LOOPBACK) != BSP_CAN_OK)
    {
        TEST_LOG("MultiBurst FAIL (init error)");
        return TEST_FAIL;
    }

    for (uint8_t i = 0; i < BURST_COUNT; i++)
    {
        BSP_CAN_Msg_t tx = {
            .id     = (uint32_t)(0x100U + i),
            .len    = 4U,
            .is_ext = false,
            .is_rtr = false,
            .data   = {i, (uint8_t)(i+1U), (uint8_t)(i+2U), (uint8_t)(i+3U)},
        };

        if (BSP_CAN_Send(&tx) != BSP_CAN_OK)
        {
            TEST_LOG("MultiBurst FAIL (send %d error)", i);
            BSP_CAN_DeInit();
            return TEST_FAIL;
        }
        HAL_Delay(LOOPBACK_DELAY_MS);
    }

    for (uint8_t i = 0; i < BURST_COUNT; i++)
    {
        if (!BSP_CAN_BufferHasData())
        {
            TEST_LOG("MultiBurst FAIL (no data at frame %d)", i);
            BSP_CAN_DeInit();
            return TEST_FAIL;
        }

        BSP_CAN_Msg_t rx = {0};
        if (BSP_CAN_RecvFromBuffer(&rx) != BSP_CAN_OK)
        {
            TEST_LOG("MultiBurst FAIL (recv %d error)", i);
            BSP_CAN_DeInit();
            return TEST_FAIL;
        }

        if (rx.id != (uint32_t)(0x100U + i) || rx.data[0] != i)
        {
            TEST_LOG("MultiBurst FAIL (frame %d wrong: id=0x%X data[0]=%d)",
                     i, (unsigned)rx.id, rx.data[0]);
            BSP_CAN_DeInit();
            return TEST_FAIL;
        }
    }

    BSP_CAN_DeInit();
    return TEST_PASS;
}

/* --------------------------------------------------------------------------
 * Test 4 ！ Parameter validation
 * --------------------------------------------------------------------------*/
BSP_CAN_TestResult_t BSP_CAN_Test_ParamValidation(void)
{
    BSP_CAN_DeInit();
    BSP_CAN_Init(BSP_CAN_MODE_LOOPBACK);

    if (BSP_CAN_Send(NULL) != BSP_CAN_ERR_PARAM)
    {
        TEST_LOG("ParamValidation FAIL (Send NULL)");
        BSP_CAN_DeInit(); return TEST_FAIL;
    }

    BSP_CAN_Msg_t bad = { .id = 0x1U, .len = 9U };
    if (BSP_CAN_Send(&bad) != BSP_CAN_ERR_PARAM)
    {
        TEST_LOG("ParamValidation FAIL (Send len=9)");
        BSP_CAN_DeInit(); return TEST_FAIL;
    }

    if (BSP_CAN_RecvFromBuffer(NULL) != BSP_CAN_ERR_PARAM)
    {
        TEST_LOG("ParamValidation FAIL (RecvFromBuffer NULL)");
        BSP_CAN_DeInit(); return TEST_FAIL;
    }

    if (BSP_CAN_SetFilter(NULL) != BSP_CAN_ERR_PARAM)
    {
        TEST_LOG("ParamValidation FAIL (SetFilter NULL)");
        BSP_CAN_DeInit(); return TEST_FAIL;
    }

    BSP_CAN_DeInit();
    return TEST_PASS;
}

/* --------------------------------------------------------------------------
 * Run all
 * --------------------------------------------------------------------------*/
uint8_t BSP_CAN_Test_RunAll(void)
{
    typedef struct {
        const char           *name;
        BSP_CAN_TestResult_t (*fn)(void);
    } TestCase_t;

    static const TestCase_t tests[] = {
        { "Init",            BSP_CAN_Test_Init            },
        { "Loopback",        BSP_CAN_Test_Loopback        },
        { "MultiBurst",      BSP_CAN_Test_MultiBurst      },
        { "ParamValidation", BSP_CAN_Test_ParamValidation },
    };

    const uint8_t total = (uint8_t)(sizeof(tests) / sizeof(tests[0]));
    uint8_t       fails = 0U;

    TEST_LOG("========== CAN BSP Test Suite ==========");
    for (uint8_t i = 0; i < total; i++)
    {
        BSP_CAN_TestResult_t r = tests[i].fn();
        TEST_LOG("%-20s %s", tests[i].name, (r == TEST_PASS) ? "PASS" : "FAIL  <---");
        if (r != TEST_PASS) { fails++; }
    }
    TEST_LOG("========================================");
    TEST_LOG("Result: %d/%d passed", (int)(total - fails), (int)total);
    TEST_LOG("========================================\r\n");

    return fails;
}

