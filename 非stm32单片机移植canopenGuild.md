好，这是一个移植问题。CANopenNode 的架构设计就是为了方便移植的，我们来分析一下需要做什么。

---

## CANopenNode 的分层结构

```
CANopenNode 核心层（不用动）
    CO_driver.h     ← 定义接口规范
    CANopen.c/h     ← 协议栈核心
    OD.c/h          ← 对象字典

平台适配层（需要你写）
    CO_driver.c     ← CAN 收发的具体实现
    CO_app.c        ← 定时器/任务调用
```

你只需要实现 `CO_driver.c` 里的几个函数，核心层完全不动。

---

## 需要实现的函数清单

```c
/* 1. CAN 初始化 */
CO_ReturnError_t CO_CANmodule_init(...)
{
    // 调用 AT32 的 CAN 初始化
    // at32f415 用的是 bxCAN，和 STM32F4 几乎一样！
}

/* 2. 发送一帧 */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule,
                             CO_CANtx_t *buffer)
{
    // 调用 AT32 HAL 或寄存器发送
}

/* 3. 接收处理（在 CAN 中断里调用）*/
void CO_CANinterrupt(CO_CANmodule_t *CANmodule)
{
    // 读取接收到的帧，交给 CANopen 处理
}
```

---

## 好消息：AT32F415 的 CAN 和 STM32F4 几乎一样

AT32F415 用的也是 **bxCAN 控制器**，寄存器结构和 STM32F4 的 bxCAN 基本相同：

```
STM32F4  bxCAN  ←→  AT32F415 bxCAN
HAL_CAN_Init         can_init
HAL_CAN_AddTxMessage can_transmit_data
HAL_CAN_GetRxMessage can_receive_data（通过中断）
CAN_IT_RX_FIFO0      CAN_INT_RFN（接收FIFO非空）
```

所以移植工作量很小，主要是把 HAL 函数名替换成 AT32 的函数名。

---

## 移植步骤

**第一步：从 STM32 移植版里复制 `CO_driver.c`**

去 `CANopenNode/CANopenSTM32` 里找到 `CO_driver.c`，这是最接近的参考。

**第二步：替换 HAL 函数名**

```c
/* STM32 版本 */
HAL_CAN_Init(&hcan1);
HAL_CAN_AddTxMessage(&hcan1, &TxHeader, data, &mailbox);
HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, data);

/* 改成 AT32 版本 */
can_init(CAN1, &can_init_struct);
can_transmit_data(CAN1, &tx_message);
can_receive(CAN1, CAN_RFN_FIFO0, &rx_message);
```

**第三步：替换中断函数名**

```c
/* STM32 */
void CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan1);
}

/* AT32F415 */
void CAN1_RX0_IRQHandler(void)  // 中断名一样
{
    CO_CANinterrupt(CANmodule);  // 直接调 CANopen 的处理函数
}
```

---

## AT32F415 用的是哪个库

你现在用的是雅特力的 **AT32 标准库** 还是 **AT32 BSP（类HAL）**？

这决定了具体的函数名，你告诉我用的哪个，我帮你把 `CO_driver.c` 对应的函数全部替换好。