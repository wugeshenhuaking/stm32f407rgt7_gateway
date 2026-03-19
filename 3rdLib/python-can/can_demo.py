# -*- coding: utf-8 -*-
"""
USB2XXX CAN Python 封装
基于 canlin.h 的 ctypes 封装，支持 CAN 收发
"""

import ctypes
import os
import time

# ─────────────────────────────────────────────
# 1. 加载 DLL
# ─────────────────────────────────────────────
_dll_path = os.path.join(os.path.dirname(__file__), "USB2XXX.dll")
_dll = ctypes.WinDLL(_dll_path)

# ─────────────────────────────────────────────
# 2. 对应 canlin.h 的结构体定义
# ─────────────────────────────────────────────

class TDevInfo(ctypes.Structure):
    _fields_ = [
        ("FwName",  ctypes.c_char * 32),
        ("Date",    ctypes.c_char * 32),
        ("HwVer",   ctypes.c_int),
        ("FwVer",   ctypes.c_int),
        ("SN",      ctypes.c_int * 3),
        ("Funs",    ctypes.c_int),
    ]

class TCanInfo(ctypes.Structure):
    """单帧 CAN 报文，对应 canlin.h TCanInfo"""
    _fields_ = [
        ("ID",          ctypes.c_uint32),   # 帧 ID
        ("TimeStamp",   ctypes.c_uint32),   # 时间戳
        ("RemoteFlag",  ctypes.c_uint8),    # 0=数据帧 1=远程帧
        ("ExternFlag",  ctypes.c_uint8),    # 0=标准帧 1=扩展帧
        ("Len",         ctypes.c_uint8),    # 数据长度 0~8
        ("Data",        ctypes.c_uint8 * 8),# 数据
        ("Res",         ctypes.c_uint8),    # 保留
    ]

class TCanCfg(ctypes.Structure):
    """CAN 初始化配置，对应 canlin.h TCanCfg"""
    _fields_ = [
        ("BRP",  ctypes.c_uint32),  # 波特率分频
        ("SJW",  ctypes.c_uint8),   # 同步跳跃宽度
        ("BS1",  ctypes.c_uint8),   # 时间段1
        ("BS2",  ctypes.c_uint8),   # 时间段2
        ("Mode", ctypes.c_uint8),   # 0=正常 1=回环 2=静默
        ("ABOM", ctypes.c_uint8),   # 自动离线管理
        ("NART", ctypes.c_uint8),   # 禁止自动重传 0=允许重传
        ("RFLM", ctypes.c_uint8),   # FIFO 锁定模式
        ("TXFP", ctypes.c_uint8),   # 发送优先级
    ]

# ─────────────────────────────────────────────
# 3. 函数签名绑定
# ─────────────────────────────────────────────

# 设备管理
_dll.USB_ScanDevice.restype  = ctypes.c_int
_dll.USB_ScanDevice.argtypes = [ctypes.POINTER(ctypes.c_int)]

_dll.USB_OpenDevice.restype  = ctypes.c_bool
_dll.USB_OpenDevice.argtypes = [ctypes.c_int]

_dll.USB_CloseDevice.restype  = ctypes.c_bool
_dll.USB_CloseDevice.argtypes = [ctypes.c_int]

_dll.USB_ResetDevice.restype  = ctypes.c_bool
_dll.USB_ResetDevice.argtypes = [ctypes.c_int]

_dll.DEV_GetDeviceInfo.restype  = ctypes.c_bool
_dll.DEV_GetDeviceInfo.argtypes = [ctypes.c_int,
                                    ctypes.POINTER(TDevInfo),
                                    ctypes.c_char_p]

# CAN
_dll.CAN_Init.restype  = ctypes.c_int
_dll.CAN_Init.argtypes = [ctypes.c_int, ctypes.c_uint8,
                           ctypes.POINTER(TCanCfg)]

_dll.CAN_StartGetMsg.restype  = ctypes.c_int
_dll.CAN_StartGetMsg.argtypes = [ctypes.c_int, ctypes.c_uint8]

_dll.CAN_StopGetMsg.restype  = ctypes.c_int
_dll.CAN_StopGetMsg.argtypes = [ctypes.c_int, ctypes.c_uint8]

_dll.CAN_SendMsg.restype  = ctypes.c_int
_dll.CAN_SendMsg.argtypes = [ctypes.c_int, ctypes.c_uint8,
                              ctypes.POINTER(TCanInfo), ctypes.c_uint32]

_dll.CAN_GetMsgWithSize.restype  = ctypes.c_int
_dll.CAN_GetMsgWithSize.argtypes = [ctypes.c_int, ctypes.c_uint8,
                                     ctypes.POINTER(TCanInfo), ctypes.c_int]

_dll.CAN_ClearMsg.restype  = ctypes.c_int
_dll.CAN_ClearMsg.argtypes = [ctypes.c_int, ctypes.c_uint8]

# ─────────────────────────────────────────────
# 4. 封装类
# ─────────────────────────────────────────────

# 500Kbps @ APB1=42MHz 的时序参数（和开发板一致）
CAN_500KBPS_CFG = TCanCfg(
    BRP  = 4,
    SJW  = 2,
    BS1  = 16,
    BS2  = 7,
    Mode = 0,
    ABOM = 0,
    NART = 0,
    RFLM = 0,
    TXFP = 0,
)

class USB2XXX_CAN:
    """
    USB2XXX CAN 通道封装

    用法：
        with USB2XXX_CAN(channel=0) as can:
            can.send(0x123, [1, 2, 3, 4])
            frames = can.recv()
    """

    RX_BUF_SIZE = 64  # 单次最多读取帧数

    def __init__(self, channel: int = 0, cfg: TCanCfg = None):
        self._handle  = ctypes.c_int(0)
        self._channel = channel
        self._cfg     = cfg or CAN_500KBPS_CFG
        self._open()

    # ── 内部：打开设备 ──────────────────────────
    def _open(self):
        # ── 扫描设备 ──────────────────────────────────
        # USB_ScanDevice 返回设备数量，handle 通过指针输出
        handle = ctypes.c_int(0)
        count = _dll.USB_ScanDevice(ctypes.byref(handle))
        if count <= 0:
            raise RuntimeError("未找到 USB2XXX 设备")
        '''
        description: 
        return {*}
        '''
        print(f"[USB2XXX] 发现 {count} 个设备，handle={handle.value}")

        # ── 打开设备 ──────────────────────────────────
        if not _dll.USB_OpenDevice(handle.value):
            raise RuntimeError("USB_OpenDevice 失败")
        self._handle = handle

        # ── 获取设备信息 ──────────────────────────────
        # 第三个参数给一个真实 buffer，不能传 None
        info = TDevInfo()
        str_buf = ctypes.create_string_buffer(256)   # ← 修复点
        _dll.DEV_GetDeviceInfo(self._handle.value,
                                ctypes.byref(info),
                                str_buf)              # ← 不再传 None
        print(f"[USB2XXX] 固件: {info.FwName.decode(errors='ignore').strip()}")

        # 初始化 CAN 通道
        ret = _dll.CAN_Init(self._handle.value,
                             self._channel,
                             ctypes.byref(self._cfg))
        if ret != 1:
            raise RuntimeError(f"CAN_Init 失败 ret={ret}")

        # 开始接收
        ret = _dll.CAN_StartGetMsg(self._handle.value, self._channel)
        if ret != 1:
            raise RuntimeError(f"CAN_StartGetMsg 失败 ret={ret}")

        print(f"[USB2XXX] CAN{self._channel} 初始化完成，500Kbps 正常模式")

    # ── 发送 ────────────────────────────────────
    def send(self, arb_id: int, data: list,
             is_ext: bool = False, is_remote: bool = False):
        """
        发送一帧 CAN 报文
        :param arb_id:    帧 ID
        :param data:      数据列表，最多 8 字节
        :param is_ext:    True = 扩展帧（29位ID）
        :param is_remote: True = 远程帧
        """
        if len(data) > 8:
            raise ValueError("CAN 数据最多 8 字节")

        frame = TCanInfo()
        frame.ID          = arb_id
        frame.ExternFlag  = 1 if is_ext    else 0
        frame.RemoteFlag  = 1 if is_remote else 0
        frame.Len         = len(data)
        for i, b in enumerate(data):
            frame.Data[i] = b

        ret = _dll.CAN_SendMsg(self._handle.value,
                                self._channel,
                                ctypes.byref(frame), 1)
        if ret != 1:
            raise RuntimeError(f"CAN_SendMsg 失败 ret={ret}")

    # ── 接收 ────────────────────────────────────
    def recv(self, max_frames: int = None) -> list:
        """
        非阻塞读取所有待接收帧
        :return: list of dict {id, data, len, is_ext, is_remote, timestamp}
        """
        size   = max_frames or self.RX_BUF_SIZE
        buf    = (TCanInfo * size)()
        count  = _dll.CAN_GetMsgWithSize(self._handle.value,
                                          self._channel,
                                          buf, size)
        results = []
        for i in range(max(0, count)):
            f = buf[i]
            results.append({
                "id":        f.ID,
                "data":      list(f.Data[:f.Len]),
                "len":       f.Len,
                "is_ext":    bool(f.ExternFlag),
                "is_remote": bool(f.RemoteFlag),
                "timestamp": f.TimeStamp,
            })
        return results

    # ── 清空接收缓冲 ─────────────────────────────
    def clear(self):
        _dll.CAN_ClearMsg(self._handle.value, self._channel)

    # ── 关闭 ────────────────────────────────────
    def close(self):
        _dll.CAN_StopGetMsg(self._handle.value, self._channel)
        _dll.USB_CloseDevice(self._handle.value)
        print("[USB2XXX] 设备已关闭")

    # ── with 语句支持 ────────────────────────────
    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# ─────────────────────────────────────────────
# 5. 测试入口
# ─────────────────────────────────────────────
if __name__ == "__main__":

    with USB2XXX_CAN(channel=0) as can:

        # ── 模拟车窗节点发送 ──────────────────────
        print("\n=== 模拟车窗节点，每 500ms 发一帧，Ctrl+C 停止 ===\n")
        can.clear()

        try:
            counter = 0
            while True:
                # 发送车窗状态帧
                can.send(0x200, [0x01, 0x00, counter & 0xFF, 0x00])
                print(f"TX  id=0x200  data=[01 00 {counter & 0xFF:02X} 00]")

                # 顺便读一下有没有开发板发过来的帧
                frames = can.recv()
                for f in frames:
                    data_str = " ".join(f"{b:02X}" for b in f["data"])
                    print(f"RX  id=0x{f['id']:03X}  "
                          f"len={f['len']}  data=[{data_str}]  "
                          f"ext={f['is_ext']}")

                counter += 1
                time.sleep(0.5)

        except KeyboardInterrupt:
            print("\n停止发送")