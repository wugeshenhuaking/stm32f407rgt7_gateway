/**
 * @file    app_CAN.h
 * @brief   CANopen application layer - NMT master demo
 *
 * Provides functions to initialize application-level CANopen services
 * and periodically emit NMT master commands on the CAN bus.
 */

#ifndef APP_CAN_H
#define APP_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "301/CO_NMT_Heartbeat.h"
#include "301/CO_SDOclient.h"

/**
 * @brief  Initialize the CANopen application layer.
 *
 * Must be called after the CANopen stack is started. Retrieves the
 * NMT master and SDO client handles from the global CO object.
 */
void APP_CAN_Init(void);

/**
 * @brief  Periodic processing task for the CAN application.
 *
 * Periodically sends an NMT master broadcast command, so the bus can
 * be observed even when no slave node is present.
 */
void APP_CAN_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_H */
