# -*- coding: utf-8 -*-
"""
radar_module.py

PC side radar CANopen-like slave simulator based on USB2XXX SDK.

Goals:
1. Simulate one radar module node on CAN bus.
2. Allow a master/upper-computer to configure it by NMT + minimal SDO server.
3. Module does NOT send PDO continuously. It only sends TPDO after an
   application-layer trigger.
4. Module can also send a custom broadcast frame to control / notify other
   modules, matching the original Modbus product behaviour where one device
   can also broadcast commands.

This is intentionally a practical simulation script, not a full CANopen stack.
Implemented subset:
- NMT receive: Start / Stop / Pre-op / Reset communication / Reset node
- Boot-up + heartbeat producer
- Minimal SDO server: expedited upload / download for selected OD entries
- Event-triggered TPDO1 / TPDO2
- Custom broadcast frame

Recommended use:
- Run this script on a PC with USB-CAN connected.
- Use your STM32 or another PC script as CANopen master.
- Observe frames in USB-CAN analyser.
"""

from ctypes import *
from datetime import datetime
from time import sleep, time
import argparse
import os
import threading
import queue

# Keep cwd at script directory so usb_device.py can find ./libs
os.chdir(os.path.dirname(os.path.abspath(__file__)))

from usb_device import *
from usb2can import *


# -----------------------------------------------------------------------------
# User configuration
# -----------------------------------------------------------------------------
CAN_CHANNEL = int(os.getenv("RADAR_CAN_CHANNEL", "0"), 0)
BITRATE = 500000
RADAR_NODE_ID = 0x21          # 33, avoids overlap with your original 1..250 Modbus idea
HEARTBEAT_MS = 1000           # OD 0x1017 default heartbeat producer time
DEFAULT_VERBOSE_RX = os.getenv("RADAR_VERBOSE_RX", "0").strip().lower() not in ("", "0", "false", "no", "off")
DEFAULT_HIDE_HEARTBEAT_LOG = os.getenv("RADAR_HIDE_HEARTBEAT_LOG", "1").strip().lower() not in ("", "0", "false", "no", "off")
TPDO1_COB_ID = 0x180 + RADAR_NODE_ID
TPDO2_COB_ID = 0x280 + RADAR_NODE_ID
SDO_RX_COB_ID = 0x600 + RADAR_NODE_ID
SDO_TX_COB_ID = 0x580 + RADAR_NODE_ID
HEARTBEAT_COB_ID = 0x700 + RADAR_NODE_ID

# Custom application broadcast.
# Chosen in 0x7C0+nodeID range, outside common CiA301 predefined CANopen IDs.
APP_BROADCAST_BASE = 0x7C0
APP_BROADCAST_COB_ID = APP_BROADCAST_BASE + RADAR_NODE_ID

# Application command trigger frame, also placed outside the common CiA301
# predefined ID ranges.
APP_TRIGGER_COB_ID = 0x780 + RADAR_NODE_ID

# NMT states / commands
CO_NMT_INITIALIZING = 0x00
CO_NMT_STOPPED = 0x04
CO_NMT_OPERATIONAL = 0x05
CO_NMT_PRE_OPERATIONAL = 0x7F

NMT_CMD_START = 0x01
NMT_CMD_STOP = 0x02
NMT_CMD_PREOP = 0x80
NMT_CMD_RESET_NODE = 0x81
NMT_CMD_RESET_COMM = 0x82
NMT_COB_ID = 0x000

# SDO command specifiers (minimal subset)
SDO_CCS_UPLOAD_REQ = 0x40
SDO_CCS_DOWNLOAD_1B = 0x2F
SDO_CCS_DOWNLOAD_2B = 0x2B
SDO_CCS_DOWNLOAD_4B = 0x23
SDO_SCS_UPLOAD_1B = 0x4F
SDO_SCS_UPLOAD_2B = 0x4B
SDO_SCS_UPLOAD_4B = 0x43
SDO_SCS_DOWNLOAD_RSP = 0x60
SDO_ABORT = 0x80

ABORT_TOGGLE = 0x05030000
ABORT_TIMEOUT = 0x05040000
ABORT_CMD = 0x05040001
ABORT_UNSUPPORTED = 0x06010000
ABORT_WRITEONLY = 0x06010001
ABORT_READONLY = 0x06010002
ABORT_NOT_EXISTS = 0x06020000
ABORT_TYPE_MISMATCH = 0x06070010
ABORT_DATA_RANGE = 0x06090030
ABORT_DEVICE_STATE = 0x08000022


# -----------------------------------------------------------------------------
# USB-CAN helpers
# -----------------------------------------------------------------------------
def channel_name(channel):
    if channel == 0:
        return "CAN1"
    if channel == 1:
        return "CAN2"
    return f"CAN{channel + 1}"


def decode_remote_flag(raw_flag):
    raw_flag = int(raw_flag)
    return {
        "is_remote": bool(raw_flag & 0x01),
        "sdk_channel": (raw_flag >> 5) & 0x03,
        "is_tx": bool(raw_flag & 0x80),
        "raw": raw_flag,
    }


def decode_extern_flag(raw_flag):
    raw_flag = int(raw_flag)
    return {
        "is_ext": bool(raw_flag & 0x01),
        "is_error": bool(raw_flag & 0x80),
        "raw": raw_flag,
    }


def dump_can_status(handle, channel, prefix="[CAN]"):
    status = CAN_STATUS()
    ret = CAN_GetStatus(handle, channel, byref(status))
    if ret != CAN_SUCCESS:
        print(f"{prefix} failed to read CAN status on {channel_name(channel)} (index={channel}), ret={ret}")
        return

    print(
        f"{prefix} status {channel_name(channel)} (index={channel}): "
        f"ESR=0x{status.ESR:08X} "
        f"RECounter={status.RECounter} "
        f"TECounter={status.TECounter} "
        f"LECode=0x{status.LECode:02X}"
    )


def init_device():
    dev_handles = (c_uint * 20)()
    ret = USB_ScanDevice(byref(dev_handles))
    if ret == 0:
        raise RuntimeError("No USB2XXX device found")
    if not USB_OpenDevice(dev_handles[0]):
        raise RuntimeError("Failed to open USB2XXX device")
    print(f"[USB] device opened, handle={dev_handles[0]}")
    return dev_handles[0]


def init_can(handle, channel):
    cfg = CAN_INIT_CONFIG()
    cfg.CAN_BRP_CFG3 = 6
    cfg.CAN_SJW = 1
    cfg.CAN_BS1_CFG1 = 11
    cfg.CAN_BS2_CFG2 = 4
    cfg.CAN_Mode = 0
    cfg.CAN_ABOM = 0
    cfg.CAN_NART = 1
    cfg.CAN_RFLM = 0
    cfg.CAN_TXFP = 1

    ret = CAN_Init(handle, channel, byref(cfg))
    if ret != CAN_SUCCESS:
        raise RuntimeError(f"CAN_Init failed: {ret}")

    filt = CAN_FILTER_CONFIG()
    filt.Enable = 1
    filt.FilterIndex = 0
    filt.FilterMode = 0
    filt.ExtFrame = 0
    filt.ID_Std_Ext = 0
    filt.ID_IDE = 0
    filt.ID_RTR = 0
    filt.MASK_Std_Ext = 0
    filt.MASK_IDE = 0
    filt.MASK_RTR = 0
    CAN_Filter_Init(handle, channel, byref(filt))

    # Clear stale frames before starting host-side buffering. This avoids
    # accidentally wiping a just-arrived one-shot NMT frame after StartGetMsg().
    CAN_ClearMsg(handle, channel)
    CAN_StartGetMsg(handle, channel)
    print(f"[CAN] channel_index={channel} ({channel_name(channel)}), bitrate={BITRATE}")
    print("[CAN] USB2XXX channel map: 0=CAN1, 1=CAN2")
    dump_can_status(handle, channel)


def send_frame(handle, channel, cob_id, data):
    msg = CAN_MSG()
    msg.ID = cob_id
    msg.ExternFlag = 0
    msg.RemoteFlag = 0
    msg.DataLen = len(data)
    for i, value in enumerate(data[:8]):
        msg.Data[i] = value & 0xFF
    ret = CAN_SendMsg(handle, channel, byref(msg), 1)
    return ret >= 0


def recv_frames(handle, channel, max_count=128):
    buf = (CAN_MSG * max_count)()
    count = CAN_GetMsg(handle, channel, byref(buf))
    if count <= 0:
        return []

    frames = []
    for i in range(count):
        frame = buf[i]
        remote_info = decode_remote_flag(frame.RemoteFlag)
        ext_info = decode_extern_flag(frame.ExternFlag)
        frames.append({
            "id": frame.ID,
            "len": frame.DataLen,
            "data": list(frame.Data[:frame.DataLen]),
            "is_ext": ext_info["is_ext"],
            "is_error": ext_info["is_error"],
            "is_remote": remote_info["is_remote"],
            "is_tx": remote_info["is_tx"],
            "sdk_channel": remote_info["sdk_channel"],
            "raw_remote_flag": remote_info["raw"],
            "raw_extern_flag": ext_info["raw"],
            "timestamp": int(getattr(frame, "TimeStamp", 0)),
        })
    return frames


# -----------------------------------------------------------------------------
# Radar node simulator
# -----------------------------------------------------------------------------
class ODEntry:
    def __init__(self, value, size, access="rw"):
        self.value = value
        self.size = size
        self.access = access


class RadarModuleSimulator:
    def __init__(self, handle, channel, node_id, verbose_rx=False, hide_heartbeat_log=True):
        self.handle = handle
        self.channel = channel
        self.node_id = node_id
        self.verbose_rx = verbose_rx
        self.hide_heartbeat_log = hide_heartbeat_log
        self.running = True
        self.start_ts = time()
        self.last_sdk_ts_by_id = {}

        self.nmt_state = CO_NMT_INITIALIZING
        self.heartbeat_period_ms = HEARTBEAT_MS
        self.last_hb_ts = 0.0
        self.bootup_sent = False

        self.event_queue = queue.Queue()
        self.tx_lock = threading.Lock()

        self.tpdo_enabled = True
        self.broadcast_enabled = True
        self.motion_detected = 0
        self.target_distance_mm = 0
        self.target_speed_cms = 0
        self.zone_mask = 0
        self.last_trigger_source = 0

        # Minimal object dictionary for demo.
        # Keys are (index, subindex)
        self.od = {
            (0x1000, 0x00): ODEntry(0x00000000, 4, "ro"),
            (0x1001, 0x00): ODEntry(0x00, 1, "ro"),
            (0x1008, 0x00): ODEntry(b"RadarModulePC", 0, "ro"),
            (0x1009, 0x00): ODEntry(b"SimHW1", 0, "ro"),
            (0x100A, 0x00): ODEntry(b"SimFW1.0", 0, "ro"),
            (0x1017, 0x00): ODEntry(self.heartbeat_period_ms, 2, "rw"),
            (0x1018, 0x01): ODEntry(0x00000001, 4, "ro"),
            (0x1018, 0x02): ODEntry(0x00000021, 4, "ro"),
            (0x1018, 0x03): ODEntry(0x00010000, 4, "ro"),
            (0x1018, 0x04): ODEntry(0x20260331, 4, "ro"),
            (0x2000, 0x00): ODEntry(1, 1, "rw"),          # tpdo_enable
            (0x2001, 0x00): ODEntry(1, 1, "rw"),          # broadcast_enable
            (0x2002, 0x00): ODEntry(1500, 2, "rw"),       # report threshold mm
            (0x2003, 0x00): ODEntry(APP_BROADCAST_COB_ID, 2, "rw"),
            (0x2100, 0x00): ODEntry(0, 1, "ro"),          # motion detected
            (0x2101, 0x00): ODEntry(0, 2, "ro"),          # distance mm
            (0x2102, 0x00): ODEntry(0, 2, "ro"),          # speed cm/s
            (0x2103, 0x00): ODEntry(0, 1, "ro"),          # zone bitmap
            (0x2200, 0x00): ODEntry(0, 1, "rw"),          # app trigger register
            (0x2201, 0x00): ODEntry(0, 1, "rw"),          # custom broadcast command
        }

    # ------------------------------------------------------------------
    # Logging helpers
    # ------------------------------------------------------------------
    @staticmethod
    def _hex_bytes(data):
        return " ".join(f"{b:02X}" for b in data)

    @staticmethod
    def _format_duration(seconds):
        total_ms = int(round(max(seconds, 0.0) * 1000.0))
        hours, remainder = divmod(total_ms, 3600 * 1000)
        minutes, remainder = divmod(remainder, 60 * 1000)
        secs, millis = divmod(remainder, 1000)
        if hours > 0:
            return f"{hours:02d}:{minutes:02d}:{secs:02d}.{millis:03d}"
        return f"{minutes:02d}:{secs:02d}.{millis:03d}"

    def _log_prefix(self, tag):
        wall = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        elapsed = self._format_duration(time() - self.start_ts)
        return f"[{tag} {wall} +{elapsed}]"

    def _heartbeat_summary(self):
        if self.heartbeat_period_ms > 0:
            return f"{self.heartbeat_period_ms} ms ({1000.0 / self.heartbeat_period_ms:.3f} Hz)"
        return "disabled"

    @staticmethod
    def _effective_cob_id(cob_id, is_ext=False):
        return cob_id & (0x1FFFFFFF if is_ext else 0x7FF)

    @classmethod
    def _is_heartbeat_cob_id(cls, cob_id, is_ext=False):
        effective_id = cls._effective_cob_id(cob_id, is_ext)
        return (not is_ext) and 0x700 <= effective_id <= 0x77F

    @classmethod
    def _is_heartbeat_payload(cls, cob_id, data, is_ext=False):
        if not cls._is_heartbeat_cob_id(cob_id, is_ext):
            return False
        if len(data) != 1:
            return False
        # 0x00 is CANopen boot-up; keep that visible by default.
        return data[0] != 0x00

    def _should_log_tx(self, cob_id, data, desc=""):
        if self.hide_heartbeat_log and "heartbeat" in desc.lower():
            return False
        return True

    def _should_log_rx(self, cob_id, data, desc=""):
        if self.hide_heartbeat_log and "heartbeat" in desc.lower():
            return False
        return True

    def _should_log_raw_frame(self, frame):
        if not self.hide_heartbeat_log:
            return True
        return not self._is_heartbeat_payload(frame["id"], frame["data"], frame.get("is_ext", False))

    def log_tx(self, cob_id, data, desc=""):
        if not self._should_log_tx(cob_id, data, desc):
            return
        suffix = f"  {desc}" if desc else ""
        print(f"{self._log_prefix('TX')} id=0x{cob_id:03X}  data=[{self._hex_bytes(data)}]{suffix}")

    def log_rx(self, cob_id, data, desc=""):
        if not self._should_log_rx(cob_id, data, desc):
            return
        suffix = f"  {desc}" if desc else ""
        print(f"{self._log_prefix('RX')} id=0x{cob_id:03X}  data=[{self._hex_bytes(data)}]{suffix}")

    def log_raw_frame(self, frame):
        if not self._should_log_raw_frame(frame):
            return
        effective_id = self._effective_cob_id(frame["id"], frame.get("is_ext", False))
        flags = ["EXT" if frame.get("is_ext") else "STD"]
        if frame.get("is_remote"):
            flags.append("RTR")
        if frame.get("is_error"):
            flags.append("ERR")
        if frame.get("is_tx"):
            flags.append("TX")

        sdk_seconds = frame.get("timestamp", 0) * 0.0001
        frame_key = (frame["id"], frame.get("is_ext", False), frame.get("is_remote", False))
        prev_sdk_seconds = self.last_sdk_ts_by_id.get(frame_key)
        self.last_sdk_ts_by_id[frame_key] = sdk_seconds
        interval = ""
        if prev_sdk_seconds is not None and sdk_seconds >= prev_sdk_seconds:
            interval = f"  dt={self._format_duration(sdk_seconds - prev_sdk_seconds)}"

        print(
            f"{self._log_prefix('RX-RAW')} raw_id=0x{frame['id']:08X} "
            f"cob_id=0x{effective_id:03X} "
            f"dlc={frame['len']} "
            f"flags={'/'.join(flags)} "
            f"sdk_ch={channel_name(frame.get('sdk_channel', self.channel))} "
            f"sdk_ts={self._format_duration(sdk_seconds)} "
            f"rflag=0x{frame.get('raw_remote_flag', 0):02X} "
            f"eflag=0x{frame.get('raw_extern_flag', 0):02X} "
            f"data=[{self._hex_bytes(frame['data'])}]"
            f"{interval}"
        )

    # ------------------------------------------------------------------
    # Object dictionary helpers
    # ------------------------------------------------------------------
    def _read_od(self, index, subindex):
        key = (index, subindex)
        if key not in self.od:
            return None, ABORT_NOT_EXISTS

        entry = self.od[key]
        if entry.access == "wo":
            return None, ABORT_WRITEONLY

        if index == 0x1017 and subindex == 0x00:
            entry.value = self.heartbeat_period_ms
        elif index == 0x2000 and subindex == 0x00:
            entry.value = 1 if self.tpdo_enabled else 0
        elif index == 0x2001 and subindex == 0x00:
            entry.value = 1 if self.broadcast_enabled else 0
        elif index == 0x2100 and subindex == 0x00:
            entry.value = self.motion_detected
        elif index == 0x2101 and subindex == 0x00:
            entry.value = self.target_distance_mm
        elif index == 0x2102 and subindex == 0x00:
            entry.value = self.target_speed_cms & 0xFFFF
        elif index == 0x2103 and subindex == 0x00:
            entry.value = self.zone_mask
        elif index == 0x2003 and subindex == 0x00:
            entry.value = APP_BROADCAST_COB_ID

        return entry, 0

    def _write_od(self, index, subindex, raw_value, size):
        key = (index, subindex)
        if key not in self.od:
            return ABORT_NOT_EXISTS

        entry = self.od[key]
        if entry.access == "ro":
            return ABORT_READONLY

        if entry.size != size:
            return ABORT_TYPE_MISMATCH

        if index == 0x1017 and subindex == 0x00:
            if raw_value > 60000:
                return ABORT_DATA_RANGE
            self.heartbeat_period_ms = raw_value
            entry.value = raw_value
            return 0

        if index == 0x2000 and subindex == 0x00:
            self.tpdo_enabled = bool(raw_value & 0x01)
            entry.value = 1 if self.tpdo_enabled else 0
            return 0

        if index == 0x2001 and subindex == 0x00:
            self.broadcast_enabled = bool(raw_value & 0x01)
            entry.value = 1 if self.broadcast_enabled else 0
            return 0

        if index == 0x2002 and subindex == 0x00:
            entry.value = raw_value
            return 0

        if index == 0x2200 and subindex == 0x00:
            entry.value = raw_value
            self.trigger_application_event(source=raw_value)
            return 0

        if index == 0x2201 and subindex == 0x00:
            entry.value = raw_value
            self.send_custom_broadcast(command=raw_value)
            return 0

        entry.value = raw_value
        return 0

    # ------------------------------------------------------------------
    # CAN TX primitives
    # ------------------------------------------------------------------
    def _send(self, cob_id, data, desc=""):
        with self.tx_lock:
            ok = send_frame(self.handle, self.channel, cob_id, data)
        if ok:
            self.log_tx(cob_id, data, desc)
        else:
            print(f"[ERR] CAN send failed: id=0x{cob_id:03X}")
        return ok

    def send_bootup(self):
        self._send(HEARTBEAT_COB_ID, [0x00], "boot-up")
        self.bootup_sent = True

    def send_heartbeat(self):
        self._send(HEARTBEAT_COB_ID, [self.nmt_state], f"heartbeat state=0x{self.nmt_state:02X}")

    def send_sdo_abort(self, index, subindex, abort_code):
        data = [
            SDO_ABORT,
            index & 0xFF, (index >> 8) & 0xFF,
            subindex,
            abort_code & 0xFF,
            (abort_code >> 8) & 0xFF,
            (abort_code >> 16) & 0xFF,
            (abort_code >> 24) & 0xFF,
        ]
        self._send(SDO_TX_COB_ID, data, f"sdo abort 0x{abort_code:08X}")

    def send_sdo_upload_response(self, index, subindex, entry):
        if isinstance(entry.value, (bytes, bytearray)):
            raw = bytes(entry.value)
            if len(raw) > 4:
                self.send_sdo_abort(index, subindex, ABORT_UNSUPPORTED)
                return
            if len(raw) == 1:
                cmd = SDO_SCS_UPLOAD_1B
            elif len(raw) == 2:
                cmd = SDO_SCS_UPLOAD_2B
            else:
                cmd = SDO_SCS_UPLOAD_4B
            payload = list(raw) + [0x00] * (4 - len(raw))
        else:
            raw = int(entry.value)
            if entry.size == 1:
                cmd = SDO_SCS_UPLOAD_1B
            elif entry.size == 2:
                cmd = SDO_SCS_UPLOAD_2B
            elif entry.size == 4:
                cmd = SDO_SCS_UPLOAD_4B
            else:
                self.send_sdo_abort(index, subindex, ABORT_UNSUPPORTED)
                return
            payload = [(raw >> (8 * i)) & 0xFF for i in range(4)]

        data = [cmd, index & 0xFF, (index >> 8) & 0xFF, subindex] + payload[:4]
        self._send(SDO_TX_COB_ID, data, f"sdo upload 0x{index:04X}:{subindex:02X}")

    def send_sdo_download_response(self, index, subindex):
        data = [SDO_SCS_DOWNLOAD_RSP, index & 0xFF, (index >> 8) & 0xFF, subindex, 0, 0, 0, 0]
        self._send(SDO_TX_COB_ID, data, f"sdo download ack 0x{index:04X}:{subindex:02X}")

    def send_tpdo1(self):
        if not self.tpdo_enabled or self.nmt_state != CO_NMT_OPERATIONAL:
            return
        data = [
            self.motion_detected & 0xFF,
            self.target_distance_mm & 0xFF,
            (self.target_distance_mm >> 8) & 0xFF,
            self.target_speed_cms & 0xFF,
            (self.target_speed_cms >> 8) & 0xFF,
            self.zone_mask & 0xFF,
            self.last_trigger_source & 0xFF,
            0x00,
        ]
        self._send(TPDO1_COB_ID, data, "tpdo1 event report")

    def send_tpdo2(self):
        if not self.tpdo_enabled or self.nmt_state != CO_NMT_OPERATIONAL:
            return
        data = [
            0xA5,
            self.node_id,
            self.motion_detected & 0xFF,
            self.broadcast_enabled & 0x01,
            self.tpdo_enabled & 0x01,
            0x00,
            0x00,
            0x00,
        ]
        self._send(TPDO2_COB_ID, data, "tpdo2 status summary")

    def send_custom_broadcast(self, command=0x01):
        if not self.broadcast_enabled:
            return
        data = [
            command & 0xFF,
            self.node_id,
            self.motion_detected & 0xFF,
            self.zone_mask & 0xFF,
            self.target_distance_mm & 0xFF,
            (self.target_distance_mm >> 8) & 0xFF,
            0x00,
            0x00,
        ]
        self._send(APP_BROADCAST_COB_ID, data, "custom broadcast")

    # ------------------------------------------------------------------
    # Behaviour
    # ------------------------------------------------------------------
    def reset_communication(self):
        self.nmt_state = CO_NMT_PRE_OPERATIONAL
        self.last_hb_ts = time()
        self.send_bootup()
        self.send_heartbeat()

    def reset_node(self):
        self.nmt_state = CO_NMT_INITIALIZING
        self.bootup_sent = False
        sleep(0.05)
        self.reset_communication()

    def apply_nmt_command(self, command):
        if command == NMT_CMD_START:
            self.nmt_state = CO_NMT_OPERATIONAL
            self.send_heartbeat()
        elif command == NMT_CMD_STOP:
            self.nmt_state = CO_NMT_STOPPED
            self.send_heartbeat()
        elif command == NMT_CMD_PREOP:
            self.nmt_state = CO_NMT_PRE_OPERATIONAL
            self.send_heartbeat()
        elif command == NMT_CMD_RESET_COMM:
            self.reset_communication()
        elif command == NMT_CMD_RESET_NODE:
            self.reset_node()

    def trigger_application_event(self, source=1):
        # Simulate one radar detection event.
        self.last_trigger_source = source & 0xFF
        self.motion_detected = 1
        self.target_distance_mm = 850 + (source * 10)
        self.target_speed_cms = 120 + source
        self.zone_mask = 0x01 | ((source & 0x03) << 1)

        self.od[(0x2100, 0x00)].value = self.motion_detected
        self.od[(0x2101, 0x00)].value = self.target_distance_mm
        self.od[(0x2102, 0x00)].value = self.target_speed_cms
        self.od[(0x2103, 0x00)].value = self.zone_mask

        if self.nmt_state == CO_NMT_OPERATIONAL:
            self.send_tpdo1()
            self.send_tpdo2()
            self.send_custom_broadcast(command=0x31)
        else:
            print("[INFO] trigger received, but node is not operational, TPDO suppressed")

    # ------------------------------------------------------------------
    # Frame handlers
    # ------------------------------------------------------------------
    def handle_nmt(self, data):
        if len(data) < 2:
            return
        command = data[0]
        target = data[1]
        self.log_rx(NMT_COB_ID, data, f"nmt target=0x{target:02X}")
        if target in (0x00, self.node_id):
            self.apply_nmt_command(command)
        else:
            print(f"[INFO] nmt ignored, target node=0x{target:02X}, local node=0x{self.node_id:02X}")

    def handle_sdo(self, data):
        if len(data) != 8:
            return
        cmd = data[0]
        index = data[1] | (data[2] << 8)
        subindex = data[3]

        self.log_rx(SDO_RX_COB_ID, data, f"sdo 0x{index:04X}:{subindex:02X}")

        if self.nmt_state == CO_NMT_STOPPED:
            self.send_sdo_abort(index, subindex, ABORT_DEVICE_STATE)
            return

        if cmd == SDO_CCS_UPLOAD_REQ:
            entry, abort = self._read_od(index, subindex)
            if abort != 0:
                self.send_sdo_abort(index, subindex, abort)
                return
            self.send_sdo_upload_response(index, subindex, entry)
            return

        if cmd in (SDO_CCS_DOWNLOAD_1B, SDO_CCS_DOWNLOAD_2B, SDO_CCS_DOWNLOAD_4B):
            size = {SDO_CCS_DOWNLOAD_1B: 1, SDO_CCS_DOWNLOAD_2B: 2, SDO_CCS_DOWNLOAD_4B: 4}[cmd]
            raw_value = 0
            for i in range(size):
                raw_value |= data[4 + i] << (8 * i)
            abort = self._write_od(index, subindex, raw_value, size)
            if abort != 0:
                self.send_sdo_abort(index, subindex, abort)
                return
            self.send_sdo_download_response(index, subindex)
            return

        self.send_sdo_abort(index, subindex, ABORT_CMD)

    def handle_app_trigger(self, data):
        self.log_rx(APP_TRIGGER_COB_ID, data, "app trigger")
        source = data[0] if len(data) > 0 else 1
        self.trigger_application_event(source=source)

    def process_frame(self, frame):
        if frame.get("is_error"):
            print("[WARN] CAN error frame received from USB-CAN adapter, ignored")
            return

        if frame.get("is_remote"):
            if self.verbose_rx:
                print(f"[INFO] remote frame ignored, id=0x{frame['id']:03X}")
            return

        cob_id = self._effective_cob_id(frame["id"], frame.get("is_ext", False))
        data = frame["data"]

        if cob_id == NMT_COB_ID:
            self.handle_nmt(data)
        elif cob_id == SDO_RX_COB_ID:
            self.handle_sdo(data)
        elif cob_id == APP_TRIGGER_COB_ID:
            self.handle_app_trigger(data)
        else:
            # Ignore own heartbeat / tpdo echo in bus monitor mode.
            pass

    # ------------------------------------------------------------------
    # Threads
    # ------------------------------------------------------------------
    def receiver_loop(self):
        while self.running:
            try:
                frames = recv_frames(self.handle, self.channel)
                for frame in frames:
                    if self.verbose_rx:
                        self.log_raw_frame(frame)
                    self.process_frame(frame)
            except Exception as exc:
                print(f"[ERR] receiver loop: {exc}")
            sleep(0.01)

    def print_status(self):
        dump_can_status(self.handle, self.channel, prefix="[APP]")
        print(
            f"{self._log_prefix('APP')} node_id=0x{self.node_id:02X} "
            f"nmt_state=0x{self.nmt_state:02X} "
            f"heartbeat={self._heartbeat_summary()} "
            f"heartbeat_log={'hidden' if self.hide_heartbeat_log else 'shown'} "
            f"verbose_rx={'on' if self.verbose_rx else 'off'}"
        )

    def timer_loop(self):
        # After power-up, send boot-up then enter pre-op.
        self.reset_communication()
        while self.running:
            now = time()
            if self.heartbeat_period_ms > 0 and self.nmt_state in (
                CO_NMT_PRE_OPERATIONAL,
                CO_NMT_OPERATIONAL,
                CO_NMT_STOPPED,
            ):
                if (now - self.last_hb_ts) * 1000.0 >= self.heartbeat_period_ms:
                    self.last_hb_ts = now
                    self.send_heartbeat()
            sleep(0.02)

    def keyboard_loop(self):
        self.print_menu()
        while self.running:
            try:
                cmd = input().strip()
            except EOFError:
                break
            except KeyboardInterrupt:
                break

            if cmd == "q":
                self.running = False
                break
            elif cmd == "m":
                self.print_menu()
            elif cmd == "t":
                self.trigger_application_event(source=1)
            elif cmd == "b":
                self.send_custom_broadcast(command=0x55)
            elif cmd == "op":
                self.apply_nmt_command(NMT_CMD_START)
            elif cmd == "pre":
                self.apply_nmt_command(NMT_CMD_PREOP)
            elif cmd == "stop":
                self.apply_nmt_command(NMT_CMD_STOP)
            elif cmd == "reset":
                self.apply_nmt_command(NMT_CMD_RESET_COMM)
            elif cmd == "status":
                self.print_status()
            elif cmd == "hb log on":
                self.hide_heartbeat_log = False
                print("[APP] heartbeat log enabled")
            elif cmd == "hb log off":
                self.hide_heartbeat_log = True
                print("[APP] heartbeat log hidden")
            elif cmd.startswith("hb "):
                try:
                    hb = int(cmd.split()[1])
                    self.heartbeat_period_ms = hb
                    self.od[(0x1017, 0x00)].value = hb
                    print(f"[APP] heartbeat={hb} ms")
                except Exception:
                    print("[APP] usage: hb <ms>")
            elif cmd == "pdo on":
                self.tpdo_enabled = True
                self.od[(0x2000, 0x00)].value = 1
                print("[APP] TPDO enabled")
            elif cmd == "pdo off":
                self.tpdo_enabled = False
                self.od[(0x2000, 0x00)].value = 0
                print("[APP] TPDO disabled")
            elif cmd == "bc on":
                self.broadcast_enabled = True
                self.od[(0x2001, 0x00)].value = 1
                print("[APP] broadcast enabled")
            elif cmd == "bc off":
                self.broadcast_enabled = False
                self.od[(0x2001, 0x00)].value = 0
                print("[APP] broadcast disabled")
            else:
                print("[APP] unknown command, type m for menu")

    def print_menu(self):
        print(
            f"""
============================================================
Radar module simulator running
node_id          : {self.node_id}
usbcan channel   : {self.channel} ({channel_name(self.channel)})
heartbeat period : {self._heartbeat_summary()}
heartbeat log    : {'hidden' if self.hide_heartbeat_log else 'shown'}
heartbeat cob-id : 0x{HEARTBEAT_COB_ID:03X}
sdo rx cob-id    : 0x{SDO_RX_COB_ID:03X}
sdo tx cob-id    : 0x{SDO_TX_COB_ID:03X}
tpdo1 cob-id     : 0x{TPDO1_COB_ID:03X}
tpdo2 cob-id     : 0x{TPDO2_COB_ID:03X}
app trigger id   : 0x{APP_TRIGGER_COB_ID:03X}
custom broadcast : 0x{APP_BROADCAST_COB_ID:03X}
------------------------------------------------------------
Commands:
  m       show menu
  q       quit
  t       trigger one radar event -> send TPDO + custom broadcast
  b       send one custom broadcast immediately
  op      switch local state to operational
  pre     switch local state to pre-operational
  stop    switch local state to stopped
  reset   reset communication
  status  show CAN controller status
  hb <n>  set heartbeat period ms
  hb log on / hb log off
  pdo on / pdo off
  bc on / bc off
============================================================
"""
        )

    def run(self):
        rx_thread = threading.Thread(target=self.receiver_loop, daemon=True)
        tm_thread = threading.Thread(target=self.timer_loop, daemon=True)
        rx_thread.start()
        tm_thread.start()
        self.keyboard_loop()


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
def parse_args():
    parser = argparse.ArgumentParser(description="USB2XXX radar CANopen-like slave simulator")
    parser.add_argument(
        "-c",
        "--channel",
        type=lambda value: int(value, 0),
        default=CAN_CHANNEL,
        help="USB2XXX CANIndex: 0=CAN1, 1=CAN2",
    )
    parser.add_argument(
        "--verbose-rx",
        action="store_true",
        default=DEFAULT_VERBOSE_RX,
        help="print every raw CAN frame from the SDK before protocol parsing",
    )
    parser.add_argument(
        "--quiet-rx",
        dest="verbose_rx",
        action="store_false",
        help="disable raw CAN frame logging",
    )
    parser.add_argument(
        "--hide-heartbeat-log",
        dest="hide_heartbeat_log",
        action="store_true",
        default=DEFAULT_HIDE_HEARTBEAT_LOG,
        help="suppress heartbeat frames in TX/RX logs",
    )
    parser.add_argument(
        "--show-heartbeat-log",
        dest="hide_heartbeat_log",
        action="store_false",
        help="show heartbeat frames in TX/RX logs",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    handle = None
    try:
        handle = init_device()
        init_can(handle, args.channel)
        print(
            f"[APP] radar simulator node=0x{RADAR_NODE_ID:02X} "
            f"usbcan={channel_name(args.channel)}(index={args.channel}) "
            f"verbose_rx={'on' if args.verbose_rx else 'off'} "
            f"heartbeat_log={'hidden' if args.hide_heartbeat_log else 'shown'}"
        )

        sim = RadarModuleSimulator(
            handle,
            args.channel,
            RADAR_NODE_ID,
            verbose_rx=args.verbose_rx,
            hide_heartbeat_log=args.hide_heartbeat_log,
        )
        sim.run()
    except KeyboardInterrupt:
        print("\n[APP] interrupted")
    except Exception as exc:
        print(f"[APP] exception: {exc}")
    finally:
        if handle is not None:
            try:
                CAN_StopGetMsg(handle, args.channel)
            except Exception:
                pass
            try:
                USB_CloseDevice(handle)
            except Exception:
                pass
            print("[USB] device closed")


if __name__ == "__main__":
    main()
