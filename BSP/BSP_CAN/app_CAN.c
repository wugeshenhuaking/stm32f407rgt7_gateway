/**
 * @file app_CAN.c
 * @brief CANopen NMT master demo application
 */
#include "app_CAN.h"
#include "CO_app_STM32.h"
#include <stdio.h>

/* Handles owned by the CANopen stack and exposed to the application layer. */
static CO_NMT_t *NMT_master = NULL;
static CO_SDOclient_t *SDO_C = NULL;

/**
 * @brief Initialize the CANopen application layer.
 *        Must be called after canopen_app_init() succeeds.
 */
void APP_CAN_Init(void)
{
    CO_t *co = canopenNodeSTM32->canOpenStack;

    if (co == NULL) {
        printf("APP_CAN_Init: CANopen stack not ready\r\n");
        return;
    }

    NMT_master = co->NMT;
    SDO_C = co->SDOclient;

    printf("APP_CAN_Init: NMT=%p, SDO_C=%p, nodeId=%u\r\n",
           (void*)NMT_master,
           (void*)SDO_C,
           (unsigned)canopenNodeSTM32->activeNodeID);
}

/**
 * @brief Periodic task - call from main loop.
 *        Broadcasts "Start Remote Node" every 2 seconds.
 */
void APP_CAN_Process(void)
{
    uint32_t now = HAL_GetTick();
    static uint32_t lastNmtBroadcast = 0U;
    CO_ReturnError_t err;

    if (NMT_master == NULL) {
        return;
    }

    if ((now - lastNmtBroadcast) < 2000U) {
        return;
    }

    lastNmtBroadcast = now;

    err = CO_NMT_sendCommand(NMT_master, CO_NMT_ENTER_OPERATIONAL, 0U);
    if (err == CO_ERROR_NO) {
        printf("NMT master TX: COB-ID=0x000 DATA=[0x01 0x00] at %lu ms\r\n",
               (unsigned long)now);
    } else {
        printf("NMT master TX failed: err=%d\r\n", (int)err);
    }
}
