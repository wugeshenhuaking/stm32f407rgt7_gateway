/*
*********************************************************************************************************
*
*	模块名称 : GT9xxx 电容触摸屏芯片驱动
*	文件名称 : gt9xxx.c
*	说    明 : 包含软件 I2C 驱动（位于本文件内部）和 GT9xxx 寄存器操作。
*	           对外只暴露 GT9XXX_Init() 和 GT9XXX_Scan()，
*	           上层通过 bsp_touch_port.c 间接调用。
*
*	软件 I2C 时序 :
*		SCL 在高电平期间采样 SDA，标准模式（约 400kHz）
*		延时通过空循环实现，可根据系统主频调整 gt9xxx_delay() 中的循环次数
*
*********************************************************************************************************
*/

#include <string.h>
#include "gt9xxx.h"

/* ============================================================
 * 内部全局变量
 * ============================================================ */
static uint8_t s_gt_tnum = 5;      /* 当前芯片支持的触点数 */

/* ============================================================
 * 软件 I2C 延时（约 1us，168MHz 主频下循环约 40 次）
 * 若时序不稳定，可适当增大循环次数
 * ============================================================ */
static void gt9xxx_delay(void)
{
    /* 正点原子例程使用 delay_us(2)，168MHz 下约需 100 次空循环 */
    volatile uint32_t i = 100;
    while (i--);
}

/* ============================================================
 * 软件 I2C 基本操作
 * SCL/SDA 均为开漏输出 + 上拉，无需切换输入/输出方向：
 *   写 0 → 拉低总线
 *   写 1 → 释放总线（外部上拉拉高），同时可直接读取引脚电平
 * ============================================================ */
static void iic_start(void)
{
    GT9XXX_SDA(1);
    GT9XXX_SCL(1);
    gt9xxx_delay();
    GT9XXX_SDA(0);  /* START: SCL 高时 SDA 下降沿 */
    gt9xxx_delay();
    GT9XXX_SCL(0);
    gt9xxx_delay();
}

static void iic_stop(void)
{
    GT9XXX_SDA(0);
    gt9xxx_delay();
    GT9XXX_SCL(1);
    gt9xxx_delay();
    GT9XXX_SDA(1);  /* STOP: SCL 高时 SDA 上升沿 */
    gt9xxx_delay();
}

static void iic_send_byte(uint8_t data)
{
    uint8_t i;
    GT9XXX_SCL(0);
    for (i = 0; i < 8; i++)
    {
        GT9XXX_SDA((data & 0x80) ? 1 : 0);
        data <<= 1;
        gt9xxx_delay();
        GT9XXX_SCL(1);
        gt9xxx_delay();
        GT9XXX_SCL(0);
        gt9xxx_delay();
    }
}

/* 返回 0 = ACK，1 = NACK/超时 */
static uint8_t iic_wait_ack(void)
{
    uint32_t timeout = 0;
    GT9XXX_SDA(1);  /* 释放 SDA，等待从机拉低 */
    gt9xxx_delay();
    GT9XXX_SCL(1);
    gt9xxx_delay();
    while (GT9XXX_SDA_READ)
    {
        if (++timeout > 10000) { iic_stop(); return 1; }
    }
    GT9XXX_SCL(0);
    gt9xxx_delay();
    return 0;
}

static void iic_ack(void)
{
    GT9XXX_SCL(0);
    GT9XXX_SDA(0);  /* ACK: SDA 拉低 */
    gt9xxx_delay();
    GT9XXX_SCL(1);
    gt9xxx_delay();
    GT9XXX_SCL(0);
    gt9xxx_delay();
    GT9XXX_SDA(1);  /* 释放 SDA */
    gt9xxx_delay();
}

static void iic_nack(void)
{
    GT9XXX_SCL(0);
    GT9XXX_SDA(1);  /* NACK: SDA 保持高 */
    gt9xxx_delay();
    GT9XXX_SCL(1);
    gt9xxx_delay();
    GT9XXX_SCL(0);
    gt9xxx_delay();
}

static uint8_t iic_recv_byte(uint8_t ack)
{
    uint8_t i, data = 0;
    GT9XXX_SDA(1);  /* 释放 SDA，准备接收 */
    for (i = 0; i < 8; i++)
    {
        GT9XXX_SCL(0);
        gt9xxx_delay();
        GT9XXX_SCL(1);
        gt9xxx_delay();
        data <<= 1;
        if (GT9XXX_SDA_READ) data++;
    }
    if (ack) iic_ack();
    else     iic_nack();
    return data;
}

/* ============================================================
 * GT9xxx 寄存器读写（16bit 寄存器地址）
 * ============================================================ */

/*
*********************************************************************************************************
*	函 数 名: GT9XXX_WrReg
*	功能说明: 向 GT9xxx 写入数据
*	形    参: reg - 16bit 寄存器地址
*	          buf - 数据缓冲区
*	          len - 写入字节数
*	返 回 值: 0 = 成功，1 = 失败
*********************************************************************************************************
*/
uint8_t GT9XXX_WrReg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    iic_start();
    iic_send_byte(GT9XXX_CMD_WR);   /* 发送写地址 */
    if (iic_wait_ack()) { iic_stop(); return 1; }

    iic_send_byte(reg >> 8);        /* 寄存器地址高字节 */
    if (iic_wait_ack()) { iic_stop(); return 1; }

    iic_send_byte(reg & 0xFF);      /* 寄存器地址低字节 */
    if (iic_wait_ack()) { iic_stop(); return 1; }

    for (i = 0; i < len; i++)
    {
        iic_send_byte(buf[i]);
        if (iic_wait_ack()) { iic_stop(); return 1; }
    }

    iic_stop();
    return 0;
}

/*
*********************************************************************************************************
*	函 数 名: GT9XXX_RdReg
*	功能说明: 从 GT9xxx 读取数据
*	形    参: reg - 16bit 寄存器地址
*	          buf - 数据缓冲区
*	          len - 读取字节数
*	返 回 值: 无
*********************************************************************************************************
*/
void GT9XXX_RdReg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    /* 先写寄存器地址 */
    iic_start();
    iic_send_byte(GT9XXX_CMD_WR);
    iic_wait_ack();
    iic_send_byte(reg >> 8);
    iic_wait_ack();
    iic_send_byte(reg & 0xFF);
    iic_wait_ack();

    /* 再发起读操作 */
    iic_start();
    iic_send_byte(GT9XXX_CMD_RD);
    iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = iic_recv_byte(i < (len - 1)); /* 最后一个字节发 NACK */
    }

    iic_stop();
}


/*
*********************************************************************************************************
*	函 数 名: GT9XXX_Init
*	功能说明: 初始化 GT9xxx 触摸芯片
*	          复位时序完全对照正点原子参考例程：
*	          INT 始终保持输入模式，不人为拉低，由硬件外部决定 I2C 地址。
*	          本项目硬件固定，I2C 地址为 0x28/0x29。
*	形    参: 无
*	返 回 值: 0 = 初始化成功
*********************************************************************************************************
*/
uint8_t GT9XXX_Init(void)
{
    uint8_t temp[1];
    GPIO_InitTypeDef gpio;

    /* --- RST 引脚：推挽输出 --- */
    GT9XXX_RST_GPIO_CLK_ENABLE();
    gpio.Pin   = GT9XXX_RST_GPIO_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GT9XXX_RST_GPIO_PORT, &gpio);

    /* --- INT 引脚：输入 + 上拉（参考例程：复位期间保持输入，不主动拉低）--- */
    GT9XXX_INT_GPIO_CLK_ENABLE();
    gpio.Pin  = GT9XXX_INT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio);

    /* --- SCL/SDA：开漏输出 + 上拉 --- */
    GT9XXX_SCL_GPIO_CLK_ENABLE();
    gpio.Pin  = GT9XXX_SCL_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GT9XXX_SCL_GPIO_PORT, &gpio);

    GT9XXX_SDA_GPIO_CLK_ENABLE();
    gpio.Pin  = GT9XXX_SDA_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GT9XXX_SDA_GPIO_PORT, &gpio);

    /* --- 硬件复位时序（对照参考例程）---
     * RST=0 (>10ms) → RST=1 (>10ms) → INT 改为浮空输入 → delay 100ms
     * INT 在整个过程中不被主动驱动
     */
    GT9XXX_RST(0);
    HAL_Delay(10);
    GT9XXX_RST(1);
    HAL_Delay(10);

    /* INT 最终配置：浮空输入（与参考例程一致：NOPULL） */
    gpio.Pin  = GT9XXX_INT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio);

    HAL_Delay(100);

    /* --- 固定 5 点触摸 --- */
    s_gt_tnum = 5;

    /* --- 软件复位 --- */
    temp[0] = 0x02;
    GT9XXX_WrReg(GT9XXX_CTRL_REG, temp, 1);
    HAL_Delay(10);
    temp[0] = 0x00;
    GT9XXX_WrReg(GT9XXX_CTRL_REG, temp, 1);

    return 0;
}

/* GT9xxx 各触点寄存器地址表 */
static const uint16_t s_tpx_tbl[10] =
{
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG,
    GT9XXX_TP4_REG, GT9XXX_TP5_REG, GT9XXX_TP6_REG,
    GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG,
    GT9XXX_TP10_REG,
};

/*
*********************************************************************************************************
*	函 数 名: GT9XXX_Scan
*	功能说明: 扫描触摸屏，读取当前所有触点的原始坐标
*	形    参: points    - 输出缓冲区，存放各触点的原始坐标
*	          max_points - 最大读取触点数（不超过 GT9XXX_MAX_TOUCH）
*	返 回 值: 当前有效触点数（0 = 无触摸）
*	注      : 返回的是芯片物理坐标（未经屏幕方向转换），
*	          坐标转换由上层 bsp_touch_port.c 负责
*********************************************************************************************************
*/
uint8_t GT9XXX_Scan(GT9XXX_PointTypeDef *points, uint8_t max_points)
{
    uint8_t  status;
    uint8_t  touch_num;
    uint8_t  buf[4];
    uint8_t  i;
    uint8_t  clear = 0;

    if (max_points > s_gt_tnum) max_points = s_gt_tnum;

    /* 清空输出缓冲 */
    for (i = 0; i < max_points; i++) points[i].valid = 0;

    /* 读触摸状态寄存器 */
    GT9XXX_RdReg(GT9XXX_GSTID_REG, &status, 1);

    touch_num = status & 0x0F;  /* 低4位 = 触点数 */

    /* 无论 BufReady 是否置位，只要读到了 GSTID 就立刻清零
     * 防止旧触摸帧数据在下次扫描时被重复读取（粘连问题）*/
    GT9XXX_WrReg(GT9XXX_GSTID_REG, &clear, 1);

    /* Buffer ready 未置位，或触点数不合法，直接返回 */
    if ((status & 0x80) == 0 || touch_num == 0 || touch_num > s_gt_tnum)
    {
        return 0;
    }

    /* 读取各触点坐标 */
    for (i = 0; i < touch_num && i < max_points; i++)
    {
        GT9XXX_RdReg(s_tpx_tbl[i], buf, 4);
        /*
         * buf[0] = X 低字节，buf[1] = X 高字节
         * buf[2] = Y 低字节，buf[3] = Y 高字节
         */
        points[i].x     = ((uint16_t)buf[1] << 8) | buf[0];
        points[i].y     = ((uint16_t)buf[3] << 8) | buf[2];
        points[i].valid = 1;
    }

    return touch_num;
}