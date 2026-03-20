# -*- coding: utf-8 -*-
"""
车身节点模拟程序
PC 模拟 BCM（车身控制模块），STM32 模拟执行节点

功能：
  1. 每 1s 发送一次控制命令（车窗/车门/车灯）
  2. 实时接收并解析 STM32 上报的状态帧
  3. 键盘交互：按键发送不同命令

运行前：
  - 关闭 SW CanLinProSw 上位机
  - STM32 开发板保持运行
"""

from ctypes import *
from time import sleep, time
import threading
import sys
import os

# 切换到脚本所在目录，确保能找到 libs
os.chdir(os.path.dirname(os.path.abspath(__file__)))

from usb_device import *
from usb2can import *
from body_bus_def import *

# ─────────────────────────────────────────────
# 配置
# ─────────────────────────────────────────────
CAN_CHANNEL = 1      # CAN2
BITRATE     = 500000

# ─────────────────────────────────────────────
# 全局状态（模拟 BCM 内部状态）
# ─────────────────────────────────────────────
g_handle  = None
g_running = True

# BCM 当前命令状态
g_window_cmd = [WINDOW_STOP, WINDOW_STOP, WINDOW_STOP, WINDOW_STOP]
g_door_cmd   = DOOR_UNLOCK_ALL
g_light_cmd  = [0x00, 0x00]   # [主灯, 转向灯]


# ─────────────────────────────────────────────
# 设备初始化
# ─────────────────────────────────────────────
def init_device():
    DevHandles = (c_uint * 20)()
    ret = USB_ScanDevice(byref(DevHandles))
    if ret == 0:
        raise RuntimeError("未找到设备")
    USB_OpenDevice(DevHandles[0])
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

    f = CAN_FILTER_CONFIG()
    f.Enable       = 1
    f.ExtFrame     = 0
    f.FilterIndex  = 0
    f.FilterMode   = 0
    f.MASK_IDE     = 0
    f.MASK_RTR     = 0
    f.MASK_Std_Ext = 0
    CAN_Filter_Init(handle, channel, byref(f))
    CAN_StartGetMsg(handle, channel)
    print(f"[CAN] 通道 {channel} 初始化完成 ✓\n")


# ─────────────────────────────────────────────
# 发送帧
# ─────────────────────────────────────────────
def send_frame(handle, channel, arb_id, data):
    msg            = CAN_MSG()
    msg.ID         = arb_id
    msg.ExternFlag = 0
    msg.RemoteFlag = 0
    msg.DataLen    = len(data)
    for i, b in enumerate(data):
        msg.Data[i] = b
    ret = CAN_SendMsg(handle, channel, byref(msg), 1)
    return ret >= 0


# ─────────────────────────────────────────────
# 接收线程：持续读取并解析
# ─────────────────────────────────────────────
def recv_thread(handle, channel):
    while g_running:
        buf   = (CAN_MSG * 64)()
        count = CAN_GetMsg(handle, channel, byref(buf))
        if count > 0:
            for i in range(count):
                f        = buf[i]
                data     = list(f.Data[:f.DataLen])
                desc     = parse_frame(f.ID, data)
                data_str = " ".join(f"{b:02X}" for b in data)
                print(f"\n[RX] id=0x{f.ID:03X}  {desc}")
                print(f"     raw=[{data_str}]")
        sleep(0.05)


# ─────────────────────────────────────────────
# 定时发送线程：每 1s 发送当前命令状态
# ─────────────────────────────────────────────
def send_thread(handle, channel):
    while g_running:
        # 发送车窗命令
        data = make_window_cmd(*g_window_cmd)
        ok   = send_frame(handle, channel, CMD_WINDOW_ID, data)
        print(f"[TX] 车窗命令 0x{CMD_WINDOW_ID:03X}  "
              f"{parse_window_cmd(data)}  {'OK' if ok else 'FAIL'}")

        # 发送车门命令
        data = make_door_cmd(g_door_cmd)
        ok   = send_frame(handle, channel, CMD_DOOR_ID, data)
        print(f"[TX] 车门命令 0x{CMD_DOOR_ID:03X}  "
              f"{parse_door_cmd(data)}  {'OK' if ok else 'FAIL'}")

        # 发送车灯命令
        data = make_light_cmd(g_light_cmd[0], g_light_cmd[1])
        ok   = send_frame(handle, channel, CMD_LIGHT_ID, data)
        print(f"[TX] 车灯命令 0x{CMD_LIGHT_ID:03X}  "
              f"{parse_light_cmd(data)}  {'OK' if ok else 'FAIL'}")

        print("-" * 50)
        sleep(1.0)


# ─────────────────────────────────────────────
# 交互菜单
# ─────────────────────────────────────────────
def print_menu():
    print("""
╔══════════════════════════════════════╗
║         车身节点模拟 - BCM            ║
╠══════════════════════════════════════╣
║ 车窗控制:                            ║
║   1 - 前左窗上升   2 - 前左窗下降    ║
║   3 - 前右窗上升   4 - 前右窗下降    ║
║   0 - 所有车窗停止                   ║
╠══════════════════════════════════════╣
║ 车门控制:                            ║
║   L - 全部上锁     U - 全部解锁      ║
╠══════════════════════════════════════╣
║ 车灯控制:                            ║
║   l - 近光灯切换   h - 远光灯切换    ║
║   < - 左转向灯     > - 右转向灯      ║
║   z - 危险灯切换                     ║
╠══════════════════════════════════════╣
║   m - 显示菜单     q - 退出          ║
╚══════════════════════════════════════╝
""")


def handle_input(key):
    global g_window_cmd, g_door_cmd, g_light_cmd

    if key == '1':
        g_window_cmd[0] = WINDOW_UP
        print(">> 前左窗上升")
    elif key == '2':
        g_window_cmd[0] = WINDOW_DOWN
        print(">> 前左窗下降")
    elif key == '3':
        g_window_cmd[1] = WINDOW_UP
        print(">> 前右窗上升")
    elif key == '4':
        g_window_cmd[1] = WINDOW_DOWN
        print(">> 前右窗下降")
    elif key == '0':
        g_window_cmd = [WINDOW_STOP] * 4
        print(">> 所有车窗停止")
    elif key == 'L':
        g_door_cmd = DOOR_LOCK_ALL
        print(">> 全部上锁")
    elif key == 'U':
        g_door_cmd = DOOR_UNLOCK_ALL
        print(">> 全部解锁")
    elif key == 'l':
        g_light_cmd[0] ^= LIGHT_LOW_BEAM
        print(f">> 近光灯 {'开' if g_light_cmd[0] & LIGHT_LOW_BEAM else '关'}")
    elif key == 'h':
        g_light_cmd[0] ^= LIGHT_HIGH_BEAM
        print(f">> 远光灯 {'开' if g_light_cmd[0] & LIGHT_HIGH_BEAM else '关'}")
    elif key == '<':
        g_light_cmd[1] ^= TURN_LEFT
        print(f">> 左转向灯 {'开' if g_light_cmd[1] & TURN_LEFT else '关'}")
    elif key == '>':
        g_light_cmd[1] ^= TURN_RIGHT
        print(f">> 右转向灯 {'开' if g_light_cmd[1] & TURN_RIGHT else '关'}")
    elif key == 'z':
        g_light_cmd[0] ^= LIGHT_HAZARD
        print(f">> 危险灯 {'开' if g_light_cmd[0] & LIGHT_HAZARD else '关'}")
    elif key == 'm':
        print_menu()


# ─────────────────────────────────────────────
# 主程序
# ─────────────────────────────────────────────
if __name__ == "__main__":
    g_handle = init_device()
    init_can(g_handle, CAN_CHANNEL)

    print_menu()

    # 启动接收线程
    t_recv = threading.Thread(target=recv_thread,
                               args=(g_handle, CAN_CHANNEL), daemon=True)
    t_recv.start()

    # 启动定时发送线程
    t_send = threading.Thread(target=send_thread,
                               args=(g_handle, CAN_CHANNEL), daemon=True)
    t_send.start()

    # 主线程处理键盘输入
    try:
        while True:
            key = input()
            if key == 'q':
                break
            handle_input(key)
    except KeyboardInterrupt:
        pass
    finally:
        g_running = False
        sleep(0.2)
        CAN_StopGetMsg(g_handle, CAN_CHANNEL)
        USB_CloseDevice(g_handle)
        print("\n[USB2XXX] 已关闭")