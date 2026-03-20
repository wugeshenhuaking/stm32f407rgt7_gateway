# -*- coding: utf-8 -*-
"""
车身总线帧格式定义
相当于简化版 DBC 文件

帧 ID 分配：
  0x100  BCM → 车窗控制命令   (PC发，MCU收)
  0x101  BCM → 车门锁控制命令 (PC发，MCU收)
  0x102  BCM → 车灯控制命令   (PC发，MCU收)
  0x200  MCU → 车窗状态上报   (MCU发，PC收)
  0x201  MCU → 车门锁状态上报 (MCU发，PC收)
  0x202  MCU → 车灯状态上报   (MCU发，PC收)
"""

# ── 帧 ID 定义 ─────────────────────────────────
# 命令帧（PC → MCU）
CMD_WINDOW_ID   = 0x100
CMD_DOOR_ID     = 0x101
CMD_LIGHT_ID    = 0x102

# 状态帧（MCU → PC）
STS_WINDOW_ID   = 0x200
STS_DOOR_ID     = 0x201
STS_LIGHT_ID    = 0x202

# ── 车窗命令帧 0x100，DLC=4 ───────────────────
# Byte0: 前左窗  0=停止 1=上升 2=下降
# Byte1: 前右窗  同上
# Byte2: 后左窗  同上
# Byte3: 后右窗  同上
WINDOW_STOP = 0x00
WINDOW_UP   = 0x01
WINDOW_DOWN = 0x02

def make_window_cmd(fl=0, fr=0, rl=0, rr=0):
    return [fl, fr, rl, rr, 0x00, 0x00, 0x00, 0x00]

def parse_window_cmd(data):
    names = ["前左窗", "前右窗", "后左窗", "后右窗"]
    acts  = {0: "停止", 1: "上升", 2: "下降"}
    return {names[i]: acts.get(data[i], "?") for i in range(4)}

# ── 车门锁命令帧 0x101，DLC=1 ────────────────
# Byte0: 0x01=全锁 0x02=全解锁 0x10=前左锁 0x20=前右锁
DOOR_LOCK_ALL   = 0x01
DOOR_UNLOCK_ALL = 0x02
DOOR_LOCK_FL    = 0x10
DOOR_LOCK_FR    = 0x20

def make_door_cmd(cmd):
    return [cmd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]

def parse_door_cmd(data):
    cmds = {
        0x01: "全部上锁",
        0x02: "全部解锁",
        0x10: "前左锁",
        0x20: "前右锁",
    }
    return cmds.get(data[0], f"未知命令 0x{data[0]:02X}")

# ── 车灯命令帧 0x102，DLC=2 ──────────────────
# Byte0: bit0=近光 bit1=远光 bit2=日行灯 bit3=危险灯
# Byte1: bit0=左转向 bit1=右转向
LIGHT_LOW_BEAM  = 0x01
LIGHT_HIGH_BEAM = 0x02
LIGHT_DRL       = 0x04
LIGHT_HAZARD    = 0x08
TURN_LEFT       = 0x01
TURN_RIGHT      = 0x02

def make_light_cmd(main_lights=0, turn=0):
    return [main_lights, turn, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]

def parse_light_cmd(data):
    main  = data[0]
    turn  = data[1]
    bits  = []
    if main & LIGHT_LOW_BEAM:  bits.append("近光")
    if main & LIGHT_HIGH_BEAM: bits.append("远光")
    if main & LIGHT_DRL:       bits.append("日行灯")
    if main & LIGHT_HAZARD:    bits.append("危险灯")
    if turn & TURN_LEFT:       bits.append("左转向")
    if turn & TURN_RIGHT:      bits.append("右转向")
    return bits if bits else ["全灭"]

# ── 通用帧解析入口 ────────────────────────────
def parse_frame(frame_id, data):
    if frame_id == STS_WINDOW_ID:
        pos = ["前左", "前右", "后左", "后右"]
        pct = {0: "关", 1: "开", 2: "半开"}
        result = {pos[i]: pct.get(data[i], "?") for i in range(4)}
        return f"车窗状态: {result}"

    elif frame_id == STS_DOOR_ID:
        pos   = ["前左", "前右", "后左", "后右"]
        state = {0: "关闭", 1: "打开"}
        result = {pos[i]: state.get(data[i], "?") for i in range(4)}
        return f"车门状态: {result}"

    elif frame_id == STS_LIGHT_ID:
        bits = []
        if data[0] & LIGHT_LOW_BEAM:  bits.append("近光")
        if data[0] & LIGHT_HIGH_BEAM: bits.append("远光")
        if data[0] & LIGHT_DRL:       bits.append("日行灯")
        if data[0] & LIGHT_HAZARD:    bits.append("危险灯")
        if data[1] & TURN_LEFT:       bits.append("左转向")
        if data[1] & TURN_RIGHT:      bits.append("右转向")
        return f"车灯状态: {bits if bits else ['全灭']}"

    elif frame_id == CMD_WINDOW_ID:
        return f"车窗命令: {parse_window_cmd(data)}"

    elif frame_id == CMD_DOOR_ID:
        return f"车门命令: {parse_door_cmd(data)}"

    elif frame_id == CMD_LIGHT_ID:
        return f"车灯命令: {parse_light_cmd(data)}"

    else:
        data_str = " ".join(f"{b:02X}" for b in data)
        return f"未知帧 id=0x{frame_id:03X} data=[{data_str}]"