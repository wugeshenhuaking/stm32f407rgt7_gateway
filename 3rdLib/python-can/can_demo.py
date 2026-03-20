# -*- coding: utf-8 -*-
"""
CAN 收发演示 v3
完全基于官方 SDK（usb_device.py + usb2can.py），不自定义结构体。

目录结构：
    python-can/
        can_demo.py       ← 本文件
        usb_device.py     ← 官方SDK
        usb2can.py        ← 官方SDK
        libs/             ← 官方SDK libs文件夹
            windows/
                x86_64/
                    libusb-1.0.dll
                    USB2XXX.dll

运行前：关闭 SW CanLinProSw 上位机软件
"""

from ctypes import *
from time import sleep
from usb_device import *
from usb2can import *

# ─────────────────────────────────────────────
# 通道配置 — 根据实际接线修改
# ─────────────────────────────────────────────
CAN_CHANNEL = 2      # 0=CAN1, 1=CAN2，根据你接的通道改这里
BITRATE     = 500000 # 波特率，和开发板一致


# ─────────────────────────────────────────────
# 设备初始化
# ─────────────────────────────────────────────
def init_device():
    """扫描并打开设备，返回 DevHandle"""
    DevHandles = (c_uint * 20)()

    ret = USB_ScanDevice(byref(DevHandles))
    if ret == 0:
        raise RuntimeError("未找到 USB2XXX 设备，请检查连接")
    print(f"[USB2XXX] 发现 {ret} 个设备")

    if not bool(USB_OpenDevice(DevHandles[0])):
        raise RuntimeError("USB_OpenDevice 失败")

    # 打印设备信息
    info     = DEVICE_INFO()
    func_str = (c_char * 256)()
    if bool(DEV_GetDeviceInfo(DevHandles[0], byref(info), byref(func_str))):
        fw = info.FirmwareVersion
        hw = info.HardwareVersion
        print(f"[USB2XXX] 固件: {bytes(info.FirmwareName).decode(errors='ignore').strip()}")
        print(f"[USB2XXX] FwVer: v{(fw>>24)&0xFF}.{(fw>>16)&0xFF}.{fw&0xFFFF}"
              f"  HwVer: v{(hw>>24)&0xFF}.{(hw>>16)&0xFF}.{hw&0xFFFF}")
        print(f"[USB2XXX] 日期: {bytes(info.BuildDate).decode(errors='ignore').strip()}")

    return DevHandles[0]


# ─────────────────────────────────────────────
# CAN 初始化
# ─────────────────────────────────────────────
def init_can(handle, channel, bitrate=500000):
    cfg = CAN_INIT_CONFIG()
    cfg.CAN_BRP_CFG3 = 6    # ← 正确值
    cfg.CAN_SJW      = 1
    cfg.CAN_BS1_CFG1 = 11   # ← 正确值
    cfg.CAN_BS2_CFG2 = 4    # ← 正确值
    cfg.CAN_Mode     = 0
    cfg.CAN_ABOM     = 0
    cfg.CAN_NART     = 1
    cfg.CAN_RFLM     = 0
    cfg.CAN_TXFP     = 1

    CAN_Init(handle, channel, byref(cfg))

    can_filter = CAN_FILTER_CONFIG()
    can_filter.Enable       = 1
    can_filter.ExtFrame     = 0
    can_filter.FilterIndex  = 0
    can_filter.FilterMode   = 0
    can_filter.MASK_IDE     = 0
    can_filter.MASK_RTR     = 0
    can_filter.MASK_Std_Ext = 0
    CAN_Filter_Init(handle, channel, byref(can_filter))

    CAN_StartGetMsg(handle, channel)
    print(f"[CAN] 通道 {channel} 初始化完成 ✓")
# ```

# 另外注意：例程里 `CAN_Mode = 0x80` 是自发自收，我们要用 `0` 正常模式，还有波特率参数 `BRP=4, BS1=16, BS2=4` 算出来是：
# ```
# 100MHz / 4 / (1+16+4) = 100M / 4 / 21 ≈ 1.19MHz  ← 不对！


# ─────────────────────────────────────────────
# 发送一帧
# ─────────────────────────────────────────────
def send_frame(handle, channel, arb_id, data,
               is_ext=False, is_remote=False):
    if len(data) > 8:
        raise ValueError("CAN 数据最多 8 字节")

    msg            = CAN_MSG()
    msg.ID         = arb_id
    msg.ExternFlag = 1 if is_ext    else 0
    msg.RemoteFlag = 1 if is_remote else 0
    msg.DataLen    = len(data)   # 官方SDK字段名是 DataLen，不是 Len
    for i, b in enumerate(data):
        msg.Data[i] = b

    ret = CAN_SendMsg(handle, channel, byref(msg), 1)
    return ret >= 0


# ─────────────────────────────────────────────
# 接收帧
# ─────────────────────────────────────────────
def recv_frames(handle, channel, max_num=1024):
    buf   = (CAN_MSG * max_num)()
    count = CAN_GetMsg(handle, channel, byref(buf))
    print(f"  [DEBUG] CAN_GetMsg 返回: {count}")   # ← 加这行
    if count <= 0:
        return []

    results = []
    for i in range(count):
        f = buf[i]
        results.append({
            "id":        f.ID,
            "data":      list(f.Data[:f.DataLen]),  # 用 DataLen
            "len":       f.DataLen,
            "is_ext":    bool(f.ExternFlag),
            "is_remote": bool(f.RemoteFlag),
            "timestamp": f.TimeStamp,
        })
    return results


# ─────────────────────────────────────────────
# 主程序
# ─────────────────────────────────────────────
if __name__ == "__main__":

    handle = init_device()

    # 同时初始化两个通道
    init_can(handle, 0, BITRATE)
    init_can(handle, 1, BITRATE)
    CAN_ClearMsg(handle, 0)
    CAN_ClearMsg(handle, 1)

    print("\n=== 扫描两个通道，看哪个能收到 0x123 ===\n")

    try:
        while True:
            for ch in [0, 1]:
                buf   = (CAN_MSG * 64)()
                count = CAN_GetMsg(handle, ch, byref(buf))
                if count > 0:
                    for i in range(count):
                        f = buf[i]
                        data_str = " ".join(f"{f.Data[j]:02X}" for j in range(f.DataLen))
                        print(f"CH{ch+1} RX  id=0x{f.ID:03X}  len={f.DataLen}  data=[{data_str}]")
                else:
                    print(f"CH{ch+1} count={count}")
            sleep(0.5)

    except KeyboardInterrupt:
        print("\n已停止")

    finally:
        CAN_StopGetMsg(handle, 0)
        CAN_StopGetMsg(handle, 1)
        USB_CloseDevice(handle)
        print("[USB2XXX] 设备已关闭")