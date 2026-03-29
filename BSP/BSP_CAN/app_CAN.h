/**
 * @file    app_CAN.h
 * @brief   CANopen SDO application layer - public interface
 *
 * Provides functions to initialize the CANopen SDO client and
 * query remote nodes on the CAN bus using the SDO protocol.
 *
 * Usage:
 *   1. Call APP_CAN_Init() once after CANopen stack is started
 *   2. Call APP_CAN_Process() periodically from the main loop
 */

#ifndef APP_CAN_H
#define APP_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "301/CO_SDOserver.h"

/**
 * @brief  Initialize the CANopen application layer.
 *
 * Must be called after the CANopen stack (CANopenNode) has been
 * successfully started. Retrieves the SDO client handle from the
 * global CO object.
 */
void APP_CAN_Init(void);

/**
 * @brief  Query Device Type (object 0x1000) from node 1 via SDO upload.
 *
 * Sends an SDO upload request to node ID 1, reads object index 0x1000
 * subindex 0x00 (Device Type), and waits for the response.
 *
 * @note   This is a blocking call. Do not call from an ISR.
 *
 * @return CO_SDOcli_ok_communicationEnd  on success
 * @return negative value                 on error (see CO_SDOclient_return_t)
 */
CO_SDO_return_t APP_CAN_QueryNode1(void);

/**
 * @brief  Periodic processing task for the CAN application.
 *
 * Triggers APP_CAN_QueryNode1() every 5 seconds. Call this function
 * from the main loop or a low-priority task.
 */
void APP_CAN_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_H */
