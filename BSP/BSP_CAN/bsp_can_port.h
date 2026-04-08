/**
 ******************************************************************************
 * @file    bsp_can_port.h
 * @brief   MCU-agnostic CAN interface for application layer.
 *
 * Application code includes ONLY this header ！ no vendor SDK types here.
 *
 * Two receive paths:
 *   BSP_CAN_Recv()           ！ direct FIFO poll, used by test suite (loopback)
 *   BSP_CAN_RecvFromBuffer() ！ interrupt ring buffer, used in normal operation
 ******************************************************************************
 */

#ifndef BSP_CAN_PORT_H
#define BSP_CAN_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Status codes
 * --------------------------------------------------------------------------*/
typedef enum {
    BSP_CAN_OK          =  0,
    BSP_CAN_ERR_INIT    = -1,
    BSP_CAN_ERR_START   = -2,
    BSP_CAN_ERR_TX      = -3,
    BSP_CAN_ERR_RX      = -4,
    BSP_CAN_ERR_EMPTY   = -5,
    BSP_CAN_ERR_FILTER  = -6,
    BSP_CAN_ERR_PARAM   = -7,
} BSP_CAN_Status_t;

/* --------------------------------------------------------------------------
 * Operating modes
 * --------------------------------------------------------------------------*/
typedef enum {
    BSP_CAN_MODE_NORMAL   = 0x00U,
    BSP_CAN_MODE_LOOPBACK = 0x02U,
    BSP_CAN_MODE_SILENT   = 0x01U,
} BSP_CAN_Mode_t;

/* --------------------------------------------------------------------------
 * Message descriptor ！ no vendor types
 * --------------------------------------------------------------------------*/
typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;
    bool     is_ext;
    bool     is_rtr;
} BSP_CAN_Msg_t;

/* --------------------------------------------------------------------------
 * Filter descriptor
 * --------------------------------------------------------------------------*/
typedef struct {
    uint32_t filter_id;
    uint32_t filter_mask;
    uint8_t  bank;
    uint8_t  fifo;
} BSP_CAN_Filter_t;

/* --------------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_Init(BSP_CAN_Mode_t mode);
BSP_CAN_Status_t BSP_CAN_DeInit(void);
BSP_CAN_Status_t BSP_CAN_SetFilter(const BSP_CAN_Filter_t *filter);
BSP_CAN_Status_t BSP_CAN_Send(const BSP_CAN_Msg_t *msg);

/* Direct FIFO poll ！ for loopback test only */
BSP_CAN_Status_t BSP_CAN_Recv(BSP_CAN_Msg_t *msg);
bool             BSP_CAN_IsDataAvailable(void);

/* Interrupt ring buffer ！ use this in normal operation */
BSP_CAN_Status_t BSP_CAN_RecvFromBuffer(BSP_CAN_Msg_t *msg);
bool             BSP_CAN_BufferHasData(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAN_PORT_H */
