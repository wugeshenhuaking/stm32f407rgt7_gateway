# -*- coding: utf-8 -*-
"""
Shared USB-CAN and CANopen-like helper code for the Python simulators.
"""

from ctypes import *
from datetime import datetime
from time import sleep, time
import argparse
import os
import threading

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(SCRIPT_DIR)

from usb_device import *
from usb2can import *
from building_can_def import (
    ADDR_CLAIM_REASON_STARTUP,
    ADDR_CLAIM_REASON_DISCOVERY,
    ADDR_CLAIM_REASON_UPDATE,
    APP_DISCOVERY_REQUEST_COB_ID,
    address_claim_cob_id,
    decode_address_claim_cob_id,
    decode_discovery_request,
    decode_address_claim,
    device_publish_cob_id,
    device_type_name,
    encode_address_claim,
    logic_addr_share_allowed,
)


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

SDO_CCS_UPLOAD_REQ = 0x40
SDO_CCS_DOWNLOAD_1B = 0x2F
SDO_CCS_DOWNLOAD_2B = 0x2B
SDO_CCS_DOWNLOAD_4B = 0x23
SDO_SCS_UPLOAD_1B = 0x4F
SDO_SCS_UPLOAD_2B = 0x4B
SDO_SCS_UPLOAD_4B = 0x43
SDO_SCS_DOWNLOAD_RSP = 0x60
SDO_ABORT = 0x80

ABORT_CMD = 0x05040001
ABORT_UNSUPPORTED = 0x06010000
ABORT_WRITEONLY = 0x06010001
ABORT_READONLY = 0x06010002
ABORT_NOT_EXISTS = 0x06020000
ABORT_TYPE_MISMATCH = 0x06070010
ABORT_DATA_RANGE = 0x06090030
ABORT_DEVICE_STATE = 0x08000022


def env_flag(name, default=False):
    value = os.getenv(name)
    if value is None:
        return default
    return value.strip().lower() not in ("", "0", "false", "no", "off")


def emit_log(logger, message):
    if logger is None:
        print(message)
        return
    logger(str(message))


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


def dump_can_status(handle, channel, prefix="[CAN]", logger=None):
    status = CAN_STATUS()
    ret = CAN_GetStatus(handle, channel, byref(status))
    if ret != CAN_SUCCESS:
        emit_log(logger, f"{prefix} failed to read CAN status on {channel_name(channel)} (index={channel}), ret={ret}")
        return

    emit_log(
        logger,
        f"{prefix} status {channel_name(channel)} (index={channel}): "
        f"ESR=0x{status.ESR:08X} "
        f"RECounter={status.RECounter} "
        f"TECounter={status.TECounter} "
        f"LECode=0x{status.LECode:02X}"
    )


def init_device(logger=None):
    dev_handles = (c_uint * 20)()
    ret = USB_ScanDevice(byref(dev_handles))
    if ret == 0:
        raise RuntimeError("No USB2XXX device found")
    if not USB_OpenDevice(dev_handles[0]):
        raise RuntimeError("Failed to open USB2XXX device")
    emit_log(logger, f"[USB] device opened, handle={dev_handles[0]}")
    return dev_handles[0]


def init_can(handle, channel, bitrate=500000, logger=None):
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

    CAN_ClearMsg(handle, channel)
    CAN_StartGetMsg(handle, channel)
    emit_log(logger, f"[CAN] channel_index={channel} ({channel_name(channel)}), bitrate={bitrate}")
    emit_log(logger, "[CAN] USB2XXX channel map: 0=CAN1, 1=CAN2")
    dump_can_status(handle, channel, logger=logger)


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


def build_common_arg_parser(description, env_prefix, default_channel=0, default_node_id=0x11, default_logic_addr=0x01):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "-c",
        "--channel",
        type=lambda value: int(value, 0),
        default=int(os.getenv(f"{env_prefix}_CAN_CHANNEL", str(default_channel)), 0),
        help="USB2XXX CANIndex: 0=CAN1, 1=CAN2",
    )
    parser.add_argument(
        "--node-id",
        type=lambda value: int(value, 0),
        default=int(os.getenv(f"{env_prefix}_NODE_ID", str(default_node_id)), 0),
        help="unique CANopen-like node id used by the gateway",
    )
    parser.add_argument(
        "--logic-addr",
        type=lambda value: int(value, 0),
        default=int(os.getenv(f"{env_prefix}_LOGIC_ADDR", str(default_logic_addr)), 0),
        help="business/group address used by app-layer bindings",
    )
    parser.add_argument(
        "--verbose-rx",
        action="store_true",
        default=env_flag(f"{env_prefix}_VERBOSE_RX", False),
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
        default=env_flag(f"{env_prefix}_HIDE_HEARTBEAT_LOG", True),
        help="suppress heartbeat frames in TX/RX logs",
    )
    parser.add_argument(
        "--show-heartbeat-log",
        dest="hide_heartbeat_log",
        action="store_false",
        help="show heartbeat frames in TX/RX logs",
    )
    return parser


class ODEntry:
    def __init__(self, value, size, access="rw"):
        self.value = value
        self.size = size
        self.access = access


class CANopenNodeSimulator:
    def __init__(
        self,
        handle,
        channel,
        node_id,
        logic_addr,
        device_type_code,
        product_name,
        hw_version="SimHW1",
        fw_version="SimFW1.0",
        verbose_rx=False,
        hide_heartbeat_log=True,
        heartbeat_ms=1000,
        logger=None,
        shared_tx_lock=None,
        local_dispatch=None,
        auto_operational=False,
    ):
        self.handle = handle
        self.channel = channel
        self.node_id = node_id & 0x7F
        self.logic_addr = logic_addr & 0xFF
        self.device_type_code = device_type_code & 0xFFFFFFFF
        self.product_name = product_name
        self.hw_version = hw_version
        self.fw_version = fw_version
        self.verbose_rx = verbose_rx
        self.hide_heartbeat_log = hide_heartbeat_log
        self.logger = logger or print
        self.local_dispatch = local_dispatch
        self.auto_operational = auto_operational
        self.running = True
        self.start_ts = time()
        self.last_sdk_ts_by_id = {}
        self.reported_addr_conflicts = set()
        self.tx_lock = shared_tx_lock or threading.Lock()
        self.rx_thread = None
        self.tm_thread = None

        self.nmt_state = CO_NMT_INITIALIZING
        self.heartbeat_period_ms = heartbeat_ms
        self.last_hb_ts = 0.0
        self.bootup_sent = False
        self.sequence = 0

        self.sdo_rx_cob_id = 0x600 + self.node_id
        self.sdo_tx_cob_id = 0x580 + self.node_id
        self.heartbeat_cob_id = 0x700 + self.node_id

        self.od = self._build_base_od()
        self.od.update(self.build_device_od())

    def _build_base_od(self):
        return {
            (0x1000, 0x00): ODEntry(self.device_type_code, 4, "ro"),
            (0x1001, 0x00): ODEntry(0x00, 1, "ro"),
            (0x1008, 0x00): ODEntry(self.product_name.encode("ascii", errors="ignore"), 0, "ro"),
            (0x1009, 0x00): ODEntry(self.hw_version.encode("ascii", errors="ignore"), 0, "ro"),
            (0x100A, 0x00): ODEntry(self.fw_version.encode("ascii", errors="ignore"), 0, "ro"),
            (0x1017, 0x00): ODEntry(self.heartbeat_period_ms, 2, "rw"),
            (0x1018, 0x01): ODEntry(0x00000001, 4, "ro"),
            (0x1018, 0x02): ODEntry(self.node_id, 4, "ro"),
            (0x1018, 0x03): ODEntry(0x00010000, 4, "ro"),
            (0x1018, 0x04): ODEntry(0x20260401, 4, "ro"),
            (0x2000, 0x00): ODEntry(self.logic_addr, 1, "rw"),
            (0x2001, 0x00): ODEntry(self.device_type_code & 0xFF, 1, "ro"),
        }

    def build_device_od(self):
        return {}

    def refresh_dynamic_od(self):
        if (0x1017, 0x00) in self.od:
            self.od[(0x1017, 0x00)].value = self.heartbeat_period_ms
        if (0x2000, 0x00) in self.od:
            self.od[(0x2000, 0x00)].value = self.logic_addr

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

    def set_logic_addr(self, logic_addr, announce=True, reason=ADDR_CLAIM_REASON_UPDATE):
        self.logic_addr = int(logic_addr) & 0xFF
        self.refresh_dynamic_od()
        if announce and self.running:
            self.send_address_claim(reason=reason)

    def emit(self, message):
        emit_log(self.logger, message)

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
        self.emit(f"{self._log_prefix('TX')} id=0x{cob_id:03X}  data=[{self._hex_bytes(data)}]{suffix}")

    def log_rx(self, cob_id, data, desc=""):
        if not self._should_log_rx(cob_id, data, desc):
            return
        suffix = f"  {desc}" if desc else ""
        self.emit(f"{self._log_prefix('RX')} id=0x{cob_id:03X}  data=[{self._hex_bytes(data)}]{suffix}")

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

        self.emit(
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

    def _read_od(self, index, subindex):
        self.refresh_dynamic_od()
        key = (index, subindex)
        if key not in self.od:
            return None, ABORT_NOT_EXISTS

        entry = self.od[key]
        if entry.access == "wo":
            return None, ABORT_WRITEONLY
        return entry, 0

    def handle_device_write_od(self, index, subindex, raw_value, size, entry):
        entry.value = raw_value
        return 0

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
            self.set_logic_addr(raw_value, announce=True, reason=ADDR_CLAIM_REASON_UPDATE)
            entry.value = self.logic_addr
            return 0

        abort = self.handle_device_write_od(index, subindex, raw_value, size, entry)
        self.refresh_dynamic_od()
        return abort

    def _send(self, cob_id, data, desc=""):
        with self.tx_lock:
            ok = send_frame(self.handle, self.channel, cob_id, data)
        if ok:
            self.log_tx(cob_id, data, desc)
            if self.local_dispatch is not None:
                frame = {
                    "id": cob_id,
                    "len": len(data),
                    "data": list(data[:8]),
                    "is_ext": False,
                    "is_error": False,
                    "is_remote": False,
                    "is_tx": True,
                    "sdk_channel": self.channel,
                    "raw_remote_flag": 0x80,
                    "raw_extern_flag": 0x00,
                    "timestamp": 0,
                    "synthetic": True,
                    "source_node_id": self.node_id,
                }
                try:
                    self.local_dispatch(self, frame)
                except Exception as exc:
                    self.emit(f"[ERR] local dispatch failed: {exc}")
        else:
            self.emit(f"[ERR] CAN send failed: id=0x{cob_id:03X}")
        return ok

    def send_address_claim(self, reason=ADDR_CLAIM_REASON_STARTUP, flags=0):
        if self.logic_addr == 0:
            return False
        publish_cob_id = device_publish_cob_id(self.device_type_code & 0xFF)
        data = encode_address_claim(
            logic_addr=self.logic_addr,
            device_type=self.device_type_code & 0xFF,
            source_node=self.node_id,
            publish_cob_id=publish_cob_id,
            reason=reason,
            flags=flags,
        )
        return self._send(
            address_claim_cob_id(self.node_id),
            data,
            f"addr claim logic=0x{self.logic_addr:02X} type={device_type_name(self.device_type_code & 0xFF)} publish=0x{publish_cob_id:03X}",
        )

    def handle_address_claim(self, data):
        payload = decode_address_claim(data)
        if payload["source_node"] in (0x00, self.node_id):
            return
        if self.logic_addr == 0 or payload["logic_addr"] == 0:
            return
        if payload["logic_addr"] != self.logic_addr:
            return

        local_type = self.device_type_code & 0xFF
        remote_type = payload["device_type"] & 0xFF
        if logic_addr_share_allowed(local_type, remote_type):
            return

        conflict_key = (
            min(self.node_id, payload["source_node"]),
            max(self.node_id, payload["source_node"]),
            self.logic_addr,
            min(local_type, remote_type),
            max(local_type, remote_type),
        )
        if conflict_key in self.reported_addr_conflicts:
            return
        self.reported_addr_conflicts.add(conflict_key)

        same_publish = (
            payload["publish_cob_id"] != 0 and
            device_publish_cob_id(local_type) == payload["publish_cob_id"]
        )
        risk_text = (
            "same publish COB-ID may collide on CAN if both nodes transmit together"
            if same_publish else
            "duplicate logic address may cause ambiguous device linkage"
        )
        self.emit(
            f"[WARN] logic address conflict: addr=0x{self.logic_addr:02X} "
            f"local={device_type_name(local_type)}(node=0x{self.node_id:02X}) "
            f"remote={device_type_name(remote_type)}(node=0x{payload['source_node']:02X}) "
            f"{risk_text}"
        )

    def handle_discovery_request(self, data):
        payload = decode_discovery_request(data)
        target_logic_addr = payload["target_logic_addr"] & 0xFF
        target_node_id = payload["target_node_id"] & 0x7F
        target_device_type = payload["target_device_type"] & 0xFF

        if target_logic_addr not in (0x00, self.logic_addr):
            return
        if target_node_id not in (0x00, self.node_id):
            return
        if target_device_type not in (0x00, self.device_type_code & 0xFF):
            return

        self.send_address_claim(reason=ADDR_CLAIM_REASON_DISCOVERY, flags=payload["flags"])

    def send_bootup(self):
        self._send(self.heartbeat_cob_id, [0x00], "boot-up")
        self.bootup_sent = True

    def send_heartbeat(self):
        self._send(self.heartbeat_cob_id, [self.nmt_state], f"heartbeat state=0x{self.nmt_state:02X}")

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
        self._send(self.sdo_tx_cob_id, data, f"sdo abort 0x{abort_code:08X}")

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
        self._send(self.sdo_tx_cob_id, data, f"sdo upload 0x{index:04X}:{subindex:02X}")

    def send_sdo_download_response(self, index, subindex):
        data = [SDO_SCS_DOWNLOAD_RSP, index & 0xFF, (index >> 8) & 0xFF, subindex, 0, 0, 0, 0]
        self._send(self.sdo_tx_cob_id, data, f"sdo download ack 0x{index:04X}:{subindex:02X}")

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

    def handle_nmt(self, data):
        if len(data) < 2:
            return
        command = data[0]
        target = data[1]
        self.log_rx(NMT_COB_ID, data, f"nmt target=0x{target:02X}")
        if target in (0x00, self.node_id):
            self.apply_nmt_command(command)
        else:
            self.emit(f"[INFO] nmt ignored, target node=0x{target:02X}, local node=0x{self.node_id:02X}")

    def handle_sdo(self, data):
        if len(data) != 8:
            return
        cmd = data[0]
        index = data[1] | (data[2] << 8)
        subindex = data[3]

        self.log_rx(self.sdo_rx_cob_id, data, f"sdo 0x{index:04X}:{subindex:02X}")

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

    def handle_app_frame(self, cob_id, data):
        return False

    def process_frame(self, frame):
        if frame.get("is_error"):
            self.emit("[WARN] CAN error frame received from USB-CAN adapter, ignored")
            return

        if frame.get("is_remote"):
            if self.verbose_rx:
                self.emit(f"[INFO] remote frame ignored, id=0x{frame['id']:03X}")
            return

        cob_id = self._effective_cob_id(frame["id"], frame.get("is_ext", False))
        data = frame["data"]

        if cob_id == NMT_COB_ID:
            self.handle_nmt(data)
        elif cob_id == self.sdo_rx_cob_id:
            self.handle_sdo(data)
        elif decode_address_claim_cob_id(cob_id) is not None:
            self.handle_address_claim(data)
        elif cob_id == APP_DISCOVERY_REQUEST_COB_ID:
            self.handle_discovery_request(data)
        else:
            self.handle_app_frame(cob_id, data)

    def receiver_loop(self):
        while self.running:
            try:
                frames = recv_frames(self.handle, self.channel)
                for frame in frames:
                    if self.verbose_rx:
                        self.log_raw_frame(frame)
                    self.process_frame(frame)
            except Exception as exc:
                self.emit(f"[ERR] receiver loop: {exc}")
            sleep(0.01)

    def print_status(self):
        dump_can_status(self.handle, self.channel, prefix="[APP]", logger=self.logger)
        self.emit(
            f"{self._log_prefix('APP')} node_id=0x{self.node_id:02X} "
            f"logic_addr=0x{self.logic_addr:02X} "
            f"nmt_state=0x{self.nmt_state:02X} "
            f"heartbeat={self._heartbeat_summary()} "
            f"heartbeat_log={'hidden' if self.hide_heartbeat_log else 'shown'} "
            f"verbose_rx={'on' if self.verbose_rx else 'off'}"
        )

    def timer_loop(self):
        self.reset_communication()
        if self.auto_operational and self.running:
            self.apply_nmt_command(NMT_CMD_START)
        self.send_address_claim(reason=ADDR_CLAIM_REASON_STARTUP)
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

    def menu_lines(self):
        return []

    def handle_command(self, cmd):
        return False

    def print_menu(self):
        lines = "\n".join(self.menu_lines())
        self.emit(
            f"""
============================================================
{self.product_name} simulator running
node_id          : {self.node_id}
logic addr       : {self.logic_addr}
usbcan channel   : {self.channel} ({channel_name(self.channel)})
heartbeat period : {self._heartbeat_summary()}
heartbeat log    : {'hidden' if self.hide_heartbeat_log else 'shown'}
heartbeat cob-id : 0x{self.heartbeat_cob_id:03X}
sdo rx cob-id    : 0x{self.sdo_rx_cob_id:03X}
sdo tx cob-id    : 0x{self.sdo_tx_cob_id:03X}
------------------------------------------------------------
Commands:
  m            show menu
  q            quit
  status       show CAN controller status
  hb <n>       set heartbeat period in ms
  hb log on/off
{lines}
============================================================
"""
        )

    def keyboard_loop(self):
        self.print_menu()
        while self.running:
            try:
                cmd = input().strip()
            except EOFError:
                break
            except KeyboardInterrupt:
                break

            self.execute_command(cmd)

    def execute_command(self, cmd):
        cmd = (cmd or "").strip()
        if not cmd:
            return True
        if cmd == "q":
            self.running = False
            return True
        if cmd == "m":
            self.print_menu()
            return True
        if cmd == "status":
            self.print_status()
            return True
        if cmd == "hb log on":
            self.hide_heartbeat_log = False
            self.emit("[APP] heartbeat log enabled")
            return True
        if cmd == "hb log off":
            self.hide_heartbeat_log = True
            self.emit("[APP] heartbeat log hidden")
            return True
        if cmd.startswith("hb "):
            try:
                hb = int(cmd.split()[1], 0)
                self.heartbeat_period_ms = hb
                self.refresh_dynamic_od()
                self.emit(f"[APP] heartbeat={hb} ms")
            except Exception:
                self.emit("[APP] usage: hb <ms>")
            return True
        if self.handle_command(cmd):
            return True
        self.emit("[APP] unknown command, type m for menu")
        return False

    def start_threads(self, with_receiver=True, with_timer=True):
        self.running = True
        if with_receiver and (self.rx_thread is None or not self.rx_thread.is_alive()):
            self.rx_thread = threading.Thread(target=self.receiver_loop, daemon=True)
            self.rx_thread.start()
        if with_timer and (self.tm_thread is None or not self.tm_thread.is_alive()):
            self.tm_thread = threading.Thread(target=self.timer_loop, daemon=True)
            self.tm_thread.start()

    def stop(self):
        self.running = False

    def run(self):
        self.start_threads(with_receiver=True, with_timer=True)
        self.keyboard_loop()


def close_device(handle, channel, logger=None):
    if handle is None:
        return
    try:
        CAN_StopGetMsg(handle, channel)
    except Exception:
        pass
    try:
        USB_CloseDevice(handle)
    except Exception:
        pass
    emit_log(logger, "[USB] device closed")
