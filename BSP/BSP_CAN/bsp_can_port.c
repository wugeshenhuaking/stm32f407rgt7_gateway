/**
 ******************************************************************************
 * @file    bsp_can_port.c
 * @brief   MCU-agnostic CAN port layer.
 *
 * Zero vendor/HAL includes ¡ª only calls bsp_can_hw_xxx() primitives.
 ******************************************************************************
 */

#include "bsp_can_port.h"
#include "bsp_can.h"

/* --------------------------------------------------------------------------
 * Internal: map hw error ¡ú port status
 * --------------------------------------------------------------------------*/
static BSP_CAN_Status_t _hw_to_status(bsp_can_hw_err_t err)
{
    switch (err)
    {
        case BSP_CAN_HW_OK:      return BSP_CAN_OK;
        case BSP_CAN_HW_EMPTY:   return BSP_CAN_ERR_EMPTY;
        case BSP_CAN_HW_TIMEOUT: return BSP_CAN_ERR_TX;
        default:                 return BSP_CAN_ERR_RX;
    }
}

/* --------------------------------------------------------------------------
 * BSP_CAN_Init
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_Init(BSP_CAN_Mode_t mode)
{
    bsp_can_hw_mode_t hw_mode;
    switch (mode)
    {
        case BSP_CAN_MODE_LOOPBACK: hw_mode = BSP_CAN_HW_MODE_LOOPBACK; break;
        case BSP_CAN_MODE_SILENT:   hw_mode = BSP_CAN_HW_MODE_SILENT;   break;
        default:                    hw_mode = BSP_CAN_HW_MODE_NORMAL;   break;
    }

    return (bsp_can_hw_configure(hw_mode) == BSP_CAN_HW_OK)
           ? BSP_CAN_OK : BSP_CAN_ERR_INIT;
}

/* --------------------------------------------------------------------------
 * BSP_CAN_DeInit
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_DeInit(void)
{
    bsp_can_hw_stop();
    return BSP_CAN_OK;
}

/* --------------------------------------------------------------------------
 * BSP_CAN_SetFilter
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_SetFilter(const BSP_CAN_Filter_t *filter)
{
    if (filter == NULL) { return BSP_CAN_ERR_PARAM; }

    bsp_can_hw_err_t ret = bsp_can_hw_set_filter(filter->filter_id,
                                                   filter->filter_mask,
                                                   filter->bank,
                                                   filter->fifo);
    return (ret == BSP_CAN_HW_OK) ? BSP_CAN_OK : BSP_CAN_ERR_FILTER;
}

/* --------------------------------------------------------------------------
 * BSP_CAN_Send
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_Send(const BSP_CAN_Msg_t *msg)
{
    if (msg == NULL || msg->len > 8U) { return BSP_CAN_ERR_PARAM; }

    bsp_can_hw_err_t ret = bsp_can_hw_send(msg->id, msg->data, msg->len,
                                            msg->is_ext, msg->is_rtr);
    return (ret == BSP_CAN_HW_OK) ? BSP_CAN_OK : BSP_CAN_ERR_TX;
}

/* --------------------------------------------------------------------------
 * BSP_CAN_Recv ¡ª direct FIFO poll (loopback test only)
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_Recv(BSP_CAN_Msg_t *msg)
{
    if (msg == NULL) { return BSP_CAN_ERR_PARAM; }

    return _hw_to_status(bsp_can_hw_recv(&msg->id, msg->data, &msg->len,
                                          &msg->is_ext, &msg->is_rtr));
}

bool BSP_CAN_IsDataAvailable(void)
{
    return bsp_can_hw_is_rx_pending();
}

/* --------------------------------------------------------------------------
 * BSP_CAN_RecvFromBuffer ¡ª pop one frame from interrupt ring buffer
 * --------------------------------------------------------------------------*/
BSP_CAN_Status_t BSP_CAN_RecvFromBuffer(BSP_CAN_Msg_t *msg)
{
    if (msg == NULL) { return BSP_CAN_ERR_PARAM; }

    return _hw_to_status(bsp_can_hw_recv_from_buffer(&msg->id, msg->data,
                                                      &msg->len, &msg->is_ext,
                                                      &msg->is_rtr));
}

bool BSP_CAN_BufferHasData(void)
{
    return bsp_can_hw_buffer_has_data();
}