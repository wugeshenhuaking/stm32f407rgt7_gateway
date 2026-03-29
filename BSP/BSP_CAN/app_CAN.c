/**
 * @file app_CAN.c
 * @brief CANopen SDO example - Query Device Type from node 1 (non-blocking)
 */
#include "app_CAN.h"
#include "CO_app_STM32.h"
#include "301/CO_SDOclient.h"
#include <string.h>
#include <stdio.h>

/* SDO client handle */
static CO_SDOclient_t *SDO_C = NULL;

/**
 * @brief Initialize the CANopen application layer.
 *        Must be called after canopen_app_init() succeeds.
 */
void APP_CAN_Init(void)
{
    SDO_C = canopenNodeSTM32->canOpenStack->SDOclient;
    printf("APP_CAN_Init: SDO_C = %p\r\n", (void*)SDO_C);

}

/**
 * @brief Periodic task - call from main loop (non-blocking state machine).
 *        Queries node 1 every 5 seconds.
 */
void APP_CAN_Process(void)
{
    typedef enum { SDO_IDLE, SDO_UPLOADING } SdoState_t;
    static SdoState_t state = SDO_IDLE;
    static uint32_t lastQuery = 0;
    static uint32_t lastTick  = 0;

    uint32_t now = HAL_GetTick();

//    switch (state) {
//        case SDO_IDLE:
//            if ((now - lastQuery) > 5000U) {
//                // ← 加这行
//                printf("SDO_IDLE: send request, SDO_C=%p\r\n", (void*)SDO_C);
//                if (SDO_C == NULL) {
//                    lastQuery = now;
//                    break;
//                }
//        
//                CO_SDO_return_t ret;
//                ret = CO_SDOclient_setup(SDO_C,
//                                         CO_CAN_ID_SDO_CLI + 1,
//                                         CO_CAN_ID_SDO_SRV + 1,
//                                         1);
//                printf("CO_SDOclient_setup ret=%d\r\n", ret);  // ← 加这行

//                if (ret != CO_SDO_RT_ok_communicationEnd) {
//                    lastQuery = now;
//                    break;
//                }
//                ret = CO_SDOclientUploadInitiate(SDO_C, 0x1000, 0x00, 1000, false);
//                printf("UploadInitiate ret=%d\r\n", ret);  // ← 加这行

//                if (ret != CO_SDO_RT_ok_communicationEnd) {
//                    CO_SDOclientClose(SDO_C);
//                    lastQuery = now;
//                    break;
//                }
//                lastTick = now;
//                printf("in SDO_UPLOADING status\r\n");  // ← 加这行

//                state = SDO_UPLOADING;
//            }
//            break;

//        case SDO_UPLOADING: {
//            CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
//            size_t sizeIndicated = 0, sizeTransferred = 0;
//            uint32_t timeDiff_us = (now - lastTick) * 1000U;
//            lastTick = now;

//            CO_SDO_return_t ret = CO_SDOclientUpload(SDO_C,
//                                                      timeDiff_us,
//                                                      false,
//                                                      &abortCode,
//                                                      &sizeIndicated,
//                                                      &sizeTransferred,
//                                                      NULL);
//            if (ret == CO_SDO_RT_waitingResponse) {
//                break; /* still in progress, come back next cycle */
//            }

//            /* transfer finished (success or error) */
//            if (ret == CO_SDO_RT_ok_communicationEnd) {
//                uint32_t deviceType = 0;
//                if (sizeTransferred == sizeof(deviceType)) {
//                    CO_SDOclientUploadBufRead(SDO_C, (uint8_t *)&deviceType, sizeof(deviceType));
//                    printf("Node1 DeviceType=0x%08X\r\n", (unsigned)deviceType);
//                }
//            }
//            CO_SDOclientClose(SDO_C);
//            lastQuery = now;
//            state = SDO_IDLE;
//            break;
//        }
//    }
}
