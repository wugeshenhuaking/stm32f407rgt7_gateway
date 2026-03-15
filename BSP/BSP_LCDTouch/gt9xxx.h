#ifndef __GT9XXX_H
#define __GT9XXX_H

/*
*********************************************************************************************************
*
*	??????? : GT9xxx ?????????§à?????
*	??????? : gt9xxx.h
*	?    ?? : ?????? GT911 / GT9147 / GT917S / GT9271 ?????
*	????? : ??????? ????? STM32F407ZGT6
*
*	??????? :
*		CT_RST  ?? PC13  (??¦Ë?????????)
*		CT_INT  ?? PB1   (?§Ø???????????????????)
*		CT_SCL  ?? PB0   (???? I2C ?????????? + ????)
*		CT_SDA  ?? PF11  (???? I2C ??????????? + ????)
*
*	I2C ??? :
*		§Õ??? 0x28  /  ????? 0x29  (7bit??? = 0x14)
*
*	??? :
*		SCL/SDA ?????????????????????????????
*
*********************************************************************************************************
*/

#include "stm32f4xx_hal.h"

/* ============================================================
 * ??????‰Í????????????????????
 * ============================================================ */
#define GT9XXX_RST_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOC_CLK_ENABLE()
#define GT9XXX_RST_GPIO_PORT            GPIOC
#define GT9XXX_RST_GPIO_PIN             GPIO_PIN_13

#define GT9XXX_INT_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()
#define GT9XXX_INT_GPIO_PORT            GPIOB
#define GT9XXX_INT_GPIO_PIN             GPIO_PIN_1

#define GT9XXX_SCL_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()
#define GT9XXX_SCL_GPIO_PORT            GPIOB
#define GT9XXX_SCL_GPIO_PIN             GPIO_PIN_0

#define GT9XXX_SDA_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOF_CLK_ENABLE()
#define GT9XXX_SDA_GPIO_PORT            GPIOF
#define GT9XXX_SDA_GPIO_PIN             GPIO_PIN_11

/* ============================================================
 * GPIO ??????
 * ============================================================ */
#define GT9XXX_RST(x)   HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_INT(x)   HAL_GPIO_WritePin(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_INT_READ HAL_GPIO_ReadPin(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN)

#define GT9XXX_SCL(x)   HAL_GPIO_WritePin(GT9XXX_SCL_GPIO_PORT, GT9XXX_SCL_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_SDA(x)   HAL_GPIO_WritePin(GT9XXX_SDA_GPIO_PORT, GT9XXX_SDA_GPIO_PIN, \
                            (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define GT9XXX_SDA_READ HAL_GPIO_ReadPin(GT9XXX_SDA_GPIO_PORT, GT9XXX_SDA_GPIO_PIN)

/* ============================================================
 * I2C ???
 * ============================================================ */
#define GT9XXX_CMD_WR   0x28    /* Write command */
#define GT9XXX_CMD_RD   0x29    /* Read command */

/* ============================================================
 * §à?????????????? 16bit ?????
 * ============================================================ */
#define GT9XXX_CTRL_REG     0x8040  /* Control reg: 0x02=soft reset, 0x00=normal */
#define GT9XXX_CFGS_REG     0x8047  /* Config start register */
#define GT9XXX_CHECK_REG    0x80FF  /* Checksum register */
#define GT9XXX_PID_REG      0x8140  /* Product ID register (reserved, not used when HW is fixed) */
#define GT9XXX_GSTID_REG    0x814E  /* Touch status: bit7=Buffer ready, bit3:0=touch count */

/* ???????????????????????? 4 ????X_low, X_high, Y_low, Y_high?? */
#define GT9XXX_TP1_REG      0x8150
#define GT9XXX_TP2_REG      0x8158
#define GT9XXX_TP3_REG      0x8160
#define GT9XXX_TP4_REG      0x8168
#define GT9XXX_TP5_REG      0x8170
#define GT9XXX_TP6_REG      0x8178
#define GT9XXX_TP7_REG      0x8180
#define GT9XXX_TP8_REG      0x8188
#define GT9XXX_TP9_REG      0x8190
#define GT9XXX_TP10_REG     0x8198

/* ============================================================
 * ???????????
 * ============================================================ */
#define GT9XXX_MAX_TOUCH    5       /* Most models: 5 points; GT9271: 10 points */

/* ============================================================
 * ???????????
 * ============================================================ */
typedef struct
{
    uint16_t x;     /* X ????§à????????? */
    uint16_t y;     /* Y ????§à????????? */
    uint8_t  valid; /* 1 = ??§¹????0 = ??§¹ */
} GT9XXX_PointTypeDef;

/* ============================================================
 * ????????
 * ============================================================ */
uint8_t GT9XXX_Init(void);
uint8_t GT9XXX_Scan(GT9XXX_PointTypeDef *points, uint8_t max_points);

/* ??? I2C??????????????????????? */
uint8_t GT9XXX_WrReg(uint16_t reg, uint8_t *buf, uint8_t len);
void    GT9XXX_RdReg(uint16_t reg, uint8_t *buf, uint8_t len);

#endif /* __GT9XXX_H */