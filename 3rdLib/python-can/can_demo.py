# -*- coding: utf-8 -*-
"""
CANopen 主站测试工具
用于测试 STM32 网关（作为 CANopen 从站，Node ID = 10）

测试流程 (Phase 1)：
1. 监听心跳帧 (ID=0x70A)
2. 发送 NMT 启动命令 (ID=0x000, Data=[0x01, 0x0A])，使节点进入 Operational 状态
3. 发送 SDO 读请求 (读 0x1000 0x00 Device Type)，接收响应 (ID=0x58A)
4. 接收 PDO 数据
"""

from ctypes import *
from time import sleep, time
from usb_device import *
from usb2can import *

# ─────────────────────────────────────────────
# 配置
# ─────────────────────────────────────────────
CAN_CHANNEL = 1      # 根据实际接线改
STM32_NODE_ID = 10   # STM32 网关的节点 ID

# 主站发送的 SDO 帧 ID (发送给 STM32)
SDO_TX_ID = 0x600 + STM32_NODE_ID  # 0x60A
# STM32 回复的 SDO 帧 ID
SDO_RX_ID = 0x580 + STM32_NODE_ID  # 0x58A

# STM32 的心跳帧 ID
HB_RX_ID  = 0x700 + STM32_NODE_ID  # 0x70A

# SDO 命令字 (主站发读请求)
SDO_CMD_READ_REQ = 0x40

# NMT 命令
NMT_ID = 0x000
NMT_CMD_START = 0x01

# ─────────────────────────────────────────────
# 设备 + CAN 初始化
# ─────────────────────────────────────────────
def init_device():
    DevHandles = (c_uint * 20)()
    ret = USB_ScanDevice(byref(DevHandles))
    if ret == 0:
        raise RuntimeError("未找到 USB2XXX 设备")
    USB_OpenDevice(DevHandles[0])
    print(f"[USB2XXX] 设备已打开")
    return DevHandles[0]


def init_can(handle, channel):
    cfg = CAN_INIT_CONFIG()
    cfg.CAN_BRP_CFG3 = 6
    cfg.CAN_SJW      = 1
    cfg.CAN_BS1_CFG1 = 11
    cfg.CAN_BS2_CFG2 = 4
    cfg.CAN_Mode     = 0
    cfg.CAN_ABOM     = 0
    cfg.CAN_NART     = 1
    cfg.CAN_RFLM     = 0
    cfg.CAN_TXFP     = 1
    CAN_Init(handle, channel, byref(cfg))

    # Filter 0: accept standard frames (ExtFrame=0)
    f = CAN_FILTER_CONFIG()
    f.Enable       = 1
    f.FilterIndex  = 0
    f.FilterMode   = 0
    f.ExtFrame     = 0      # 接收标准帧
    f.ID_Std_Ext   = 0
    f.ID_IDE       = 0
    f.ID_RTR       = 0
    f.MASK_Std_Ext = 0
    f.MASK_IDE     = 0
    f.MASK_RTR     = 0
    CAN_Filter_Init(handle, channel, byref(f))

    print(f"[CAN] 通道 {channel} 初始化完成 ✓")


# ─────────────────────────────────────────────
# CAN 收发
# ─────────────────────────────────────────────
def send_frame(handle, channel, arb_id, data):
    msg            = CAN_MSG()
    msg.ID         = arb_id
    msg.ExternFlag = 0
    msg.RemoteFlag = 0
    msg.DataLen    = len(data)
    for i, b in enumerate(data):
        msg.Data[i] = b
    CAN_SendMsg(handle, channel, byref(msg), 1)


def recv_frames(handle, channel):
    buf   = (CAN_MSG * 64)()
    count = CAN_GetMsg(handle, channel, byref(buf))
    if count <= 0:
        return []
    result = []
    for i in range(count):
        f = buf[i]
        result.append({
            "id":   f.ID,
            "data": list(f.Data[:f.DataLen]),
            "len":  f.DataLen,
            "is_ext": bool(f.ExternFlag),
        })
    return result


# ─────────────────────────────────────────────
# 测试动作
# ─────────────────────────────────────────────
def send_nmt_start(handle, channel, node_id):
    """发送 NMT Start 命令"""
    data = [NMT_CMD_START, node_id]
    send_frame(handle, channel, NMT_ID, data)
    print(f"[TX] 发送 NMT Start 命令给节点 {node_id}")

def send_sdo_read_device_type(handle, channel):
    """发送 SDO 读取 Device Type (0x1000 0x00)"""
    index = 0x1000
    subindex = 0x00
    data = [
        SDO_CMD_READ_REQ,
        index & 0xFF, (index >> 8) & 0xFF,
        subindex,
        0x00, 0x00, 0x00, 0x00
    ]
    send_frame(handle, channel, SDO_TX_ID, data)
    print(f"[TX] 发送 SDO 读请求: 0x{index:04X} sub {subindex:02X}")

# ─────────────────────────────────────────────
# 主程序
# ─────────────────────────────────────────────
if __name__ == "__main__":

    try:
        handle = init_device()
        init_can(handle, CAN_CHANNEL)
        CAN_ClearMsg(handle, CAN_CHANNEL)

        print(f"""
╔══════════════════════════════════════════╗
║   CANopen 主站测试工具 (测试 STM32 Node 10) 
║   SDO 发送 ID = 0x{SDO_TX_ID:03X}               
║   SDO 接收 ID = 0x{SDO_RX_ID:03X}               
║   心跳接收 ID = 0x{HB_RX_ID:03X}               
╚══════════════════════════════════════════╝
""")

        # 启动测试流程
        sleep(1)
        send_nmt_start(handle, CAN_CHANNEL, STM32_NODE_ID)
        sleep(0.5)
        send_sdo_read_device_type(handle, CAN_CHANNEL)

        while True:
            # 接收并处理帧
            for frame in recv_frames(handle, CAN_CHANNEL):
                fid  = frame["id"]
                data = frame["data"]
                is_ext = frame.get("is_ext", False)
                
                # 扩展帧ID需要屏蔽高位，还原出实际11位CAN ID
                effective_id = fid & 0x7FF if is_ext else fid

                data_str = " ".join(f"{b:02X}" for b in data)

                if effective_id == HB_RX_ID:
                    state = data[0] if len(data) > 0 else 0
                    state_str = "Operational" if state == 5 else ("Pre-Op" if state == 127 else str(state))
                    print(f"[RX] STM32 心跳帧: 状态 = {state_str}")

                elif effective_id == SDO_RX_ID:
                    print(f"[RX] STM32 SDO响应: [{data_str}]")
                    # 如果是成功响应，解析数据
                    if data[0] in [0x43, 0x4B, 0x4F]:
                        val = data[4] | (data[5]<<8) | (data[6]<<16) | (data[7]<<24)
                        print(f"     -> 读取到的值 = 0x{val:08X}")
                    elif data[0] == 0x80:
                        print("     -> 收到 SDO 中止响应 (Abort)")

                elif effective_id >= 0x180 and effective_id <= 0x500: # PDO 范围
                    print(f"[RX] STM32 PDO数据 ID=0x{effective_id:03X} [{data_str}]")
                else:
                    print(f"[RX] 其他帧 ID=0x{effective_id:03X} [{data_str}]")

            sleep(0.01)  # 10ms 轮询间隔

    except Exception as e:
        print(f"异常: {e}")
    except KeyboardInterrupt:
        print("\n已停止")
    finally:
        try:
            CAN_StopGetMsg(handle, CAN_CHANNEL)
            USB_CloseDevice(handle)
            print("[USB2XXX] 已关闭")
        except:
            pass