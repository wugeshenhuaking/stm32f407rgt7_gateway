/**
 ******************************************************************************
 * @file    bsp_can_test.h
 * @brief   BSP CAN unit / integration test interface.
 *
 * @note    Tests are designed to run on-target. Results are reported via
 *          printf (USART). Each test returns a BSP_CAN_TestResult_t.
 *
 *          Usage (in main or a test task):
 *            BSP_CAN_Test_RunAll();
 ******************************************************************************
 */

#ifndef BSP_CAN_TEST_H
#define BSP_CAN_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Test result type
 * -------------------------------------------------------------------------*/
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
} BSP_CAN_TestResult_t;

/* -------------------------------------------------------------------------
 * Individual tests
 * -------------------------------------------------------------------------*/

/**
 * @brief  Test 1: Verify CAN init succeeds in loopback mode.
 *         Checks that BSP_CAN_Init returns BSP_CAN_OK.
 */
BSP_CAN_TestResult_t BSP_CAN_Test_Init(void);

/**
 * @brief  Test 2: Loopback send/receive.
 *         Sends a known frame in loopback mode and verifies the received
 *         data, ID and DLC match exactly.
 */
BSP_CAN_TestResult_t BSP_CAN_Test_Loopback(void);

/**
 * @brief  Test 3: Send multiple frames and confirm all are received.
 *         Verifies FIFO ordering and that IsDataAvailable() is accurate.
 */
BSP_CAN_TestResult_t BSP_CAN_Test_MultiBurst(void);

/**
 * @brief  Test 4: Parameter validation.
 *         Passes NULL and oversized DLC to BSP_CAN_Send/Recv and confirms
 *         BSP_CAN_ERR_PARAM is returned.
 */
BSP_CAN_TestResult_t BSP_CAN_Test_ParamValidation(void);

/**
 * @brief  Run all tests in sequence, print a summary via printf.
 * @return Number of tests that failed (0 = all passed).
 */
uint8_t BSP_CAN_Test_RunAll(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAN_TEST_H */
