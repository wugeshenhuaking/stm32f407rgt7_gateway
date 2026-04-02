# -*- coding: utf-8 -*-
"""
test_demo.py

Single-process multi-node demo:
1. Open one USB2XXX CAN channel only once.
2. Run multiple simulated nodes on the same shared bus.
3. Give each node its own console window so logs do not mix together.
"""

import queue
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

from building_can_def import (
    APP_DISCOVERY_REQUEST_COB_ID,
    APP_LIGHT_STATUS_COB_ID,
    APP_PANEL_KEY_COB_ID,
    APP_SCENE_COMMAND_COB_ID,
    DEVICE_TYPE_LIGHTING,
    DEVICE_TYPE_PANEL,
    DEVICE_TYPE_RADAR,
    DEVICE_TYPE_SCENE_PANEL,
    KEY_EVENT_PRESS,
    LIGHT_REASON_GATEWAY,
    LOGIC_ADDR_BROADCAST,
    SCENE_ACTION_OFF,
    SCENE_ACTION_ON,
    SCENE_ACTION_TOGGLE,
    SCENE_SOURCE_GATEWAY,
    decode_address_claim_cob_id,
    decode_address_claim,
    decode_light_status,
    decode_panel_key,
    decode_scene_command,
    device_publish_cob_id,
    device_type_name,
    encode_discovery_request,
    encode_panel_key,
    encode_scene_command,
    key_event_name,
    light_reason_name,
    logic_addr_share_allowed,
    scene_action_name,
)
from canopen_sim_core import *
from lighting_module import LightingModuleSimulator
from panel_module import PanelModuleSimulator
from scene_panel_module import ScenePanelModuleSimulator
from radar_scene_sensor import RadarSceneSensorSimulator


NODE_KIND_CONFIG = {
    "lighting": {
        "title": "Lighting Module",
        "device_type": DEVICE_TYPE_LIGHTING,
        "node_id_start": 0x11,
        "node_id_end": 0x1F,
        "hint": "Set Modbus-style address 1..247. Lighting may share address with panel.",
    },
    "panel": {
        "title": "Normal Panel",
        "device_type": DEVICE_TYPE_PANEL,
        "node_id_start": 0x31,
        "node_id_end": 0x4F,
        "hint": "Set Modbus-style address 1..247. Panels may share address with lighting.",
    },
    "scene": {
        "title": "Scene Panel",
        "device_type": DEVICE_TYPE_SCENE_PANEL,
        "node_id_start": 0x51,
        "node_id_end": 0x67,
        "hint": "Set unique Modbus-style address 1..247 before enabling.",
    },
    "radar": {
        "title": "Radar Sensor",
        "device_type": DEVICE_TYPE_RADAR,
        "node_id_start": 0x68,
        "node_id_end": 0x7D,
        "hint": "Set unique Modbus-style address 1..247 before enabling.",
    },
}

DISCOVERY_INTERVAL_MS = 3000


def safe_int(value, default=0):
    try:
        return int(str(value).strip(), 0)
    except Exception:
        return default


def format_hex(value, size=2):
    if value is None:
        return "--"
    width = max(2, int(size) * 2)
    return f"0x{int(value) & ((1 << (width * 4)) - 1):0{width}X}"


def nmt_state_name(state):
    mapping = {
        0x00: "boot",
        0x04: "stopped",
        0x05: "operational",
        0x7F: "pre-op",
    }
    return mapping.get(state, format_hex(state, 1))


def now_age_text(last_seen):
    if not last_seen:
        return "--"
    delta = max(0.0, time() - last_seen)
    if delta < 1.0:
        return f"{delta * 1000.0:.0f} ms"
    return f"{delta:.1f} s"


class NodeConsoleWindow:
    def __init__(self, app, node_key, node, title, kind):
        self.app = app
        self.node_key = node_key
        self.node = node
        self.kind = kind
        self.closed = False
        self.refresh_job = None
        self.light_labels = []
        self.panel_buttons = []
        self.scene_buttons = []
        self.radar_button = None
        self.radar_clear_job = None
        self.info_var = tk.StringVar(value="")
        self.window = tk.Toplevel(app.root)
        self.window.title(title)
        self.window.geometry("900x700")
        self.window.protocol("WM_DELETE_WINDOW", self.on_close)

        header = ttk.Frame(self.window)
        header.pack(fill=tk.X, padx=8, pady=6)

        self.status_var = tk.StringVar(value=title)
        ttk.Label(header, textvariable=self.status_var).pack(side=tk.LEFT)
        ttk.Button(header, text="Clear", command=self.clear_log).pack(side=tk.RIGHT, padx=4)
        ttk.Button(header, text="Menu", command=lambda: self.submit_command("m")).pack(side=tk.RIGHT, padx=4)
        ttk.Button(header, text="Status", command=lambda: self.submit_command("status")).pack(side=tk.RIGHT, padx=4)

        info = ttk.Frame(self.window)
        info.pack(fill=tk.X, padx=8, pady=(0, 4))
        ttk.Label(info, textvariable=self.info_var).pack(side=tk.LEFT)

        self.custom_frame = ttk.LabelFrame(self.window, text="Quick Controls", padding=8)
        self.custom_frame.pack(fill=tk.X, padx=8, pady=(0, 8))
        self._build_custom_ui()

        self.text = scrolledtext.ScrolledText(self.window, wrap=tk.WORD, height=24)
        self.text.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        self.text.configure(state=tk.DISABLED)

        footer = ttk.Frame(self.window)
        footer.pack(fill=tk.X, padx=8, pady=(0, 8))

        self.entry = ttk.Entry(footer)
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.entry.bind("<Return>", self._on_enter)
        ttk.Button(footer, text="Send", command=self._send_from_entry).pack(side=tk.RIGHT, padx=(8, 0))
        self._schedule_refresh()

    def _build_custom_ui(self):
        if self.kind == "lighting":
            self._build_lighting_ui()
        elif self.kind == "panel":
            self._build_panel_ui()
        elif self.kind == "scene":
            self._build_scene_ui()
        elif self.kind == "radar":
            self._build_radar_ui()
        else:
            ttk.Label(self.custom_frame, text="No quick controls for this node").pack(anchor=tk.W)

    def _build_lighting_ui(self):
        ttk.Label(
            self.custom_frame,
            text="Lighting manual control: click one lamp to toggle it and publish the full relay state",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.custom_frame)
        grid.pack(anchor=tk.W)
        for idx in range(16):
            label = tk.Button(
                grid,
                text=f"L{idx + 1}",
                width=6,
                height=2,
                relief=tk.RIDGE,
                bg="#d9d9d9",
                command=lambda channel_index=idx + 1: self._toggle_light_channel(channel_index),
            )
            label.grid(row=0 if idx < 8 else 1, column=idx % 8, padx=4, pady=4)
            self.light_labels.append(label)

    def _build_panel_ui(self):
        ttk.Label(
            self.custom_frame,
            text="Normal panel quick keys: click to send key press, buttons follow linked lighting state",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.custom_frame)
        grid.pack(anchor=tk.W)
        for idx in range(16):
            btn = tk.Button(
                grid,
                text=f"K{idx + 1}",
                width=7,
                command=lambda key_index=idx + 1: self.submit_command(f"key {key_index}", require_addr=True),
            )
            btn.grid(row=0 if idx < 8 else 1, column=idx % 8, padx=4, pady=4)
            self.panel_buttons.append(btn)

    def _build_scene_ui(self):
        ttk.Label(
            self.custom_frame,
            text="Scene panel quick keys: 12 buttons, first click sends ON, next click sends OFF",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.custom_frame)
        grid.pack(anchor=tk.W)
        for idx in range(12):
            btn = tk.Button(
                grid,
                text=f"S{idx + 1}",
                width=8,
                command=lambda scene_index=idx + 1: self._toggle_scene(scene_index),
            )
            btn.grid(row=idx // 4, column=idx % 4, padx=4, pady=4)
            self.scene_buttons.append(btn)

    def _build_radar_ui(self):
        ttk.Label(
            self.custom_frame,
            text="Radar quick control: detect person, then auto clear after 3 seconds",
        ).pack(anchor=tk.W, pady=(0, 6))
        self.radar_button = tk.Button(
            self.custom_frame,
            text="Detect Person",
            width=18,
            command=self._radar_detect_once,
        )
        self.radar_button.pack(anchor=tk.W, pady=4)

    def _toggle_scene(self, scene_index):
        active = bool(getattr(self.node, "scene_active", {}).get(scene_index, False))
        cmd = f"scene off {scene_index}" if active else f"scene on {scene_index}"
        self.submit_command(cmd, require_addr=True)

    def _toggle_light_channel(self, channel_index):
        self.submit_command(f"toggle {channel_index}", require_addr=True)

    def _radar_detect_once(self):
        self._cancel_radar_clear()
        self.submit_command("detect", require_addr=True)
        if not self.closed:
            self.radar_clear_job = self.window.after(3000, self._auto_clear_radar)

    def _auto_clear_radar(self):
        self.radar_clear_job = None
        if self.closed:
            return
        self.submit_command("clear", require_addr=True)

    def _cancel_radar_clear(self):
        if self.radar_clear_job is None:
            return
        try:
            self.window.after_cancel(self.radar_clear_job)
        except Exception:
            pass
        self.radar_clear_job = None

    def append_log(self, message):
        self.text.configure(state=tk.NORMAL)
        self.text.insert(tk.END, message.rstrip() + "\n")
        self.text.see(tk.END)
        self.text.configure(state=tk.DISABLED)

    def clear_log(self):
        self.text.configure(state=tk.NORMAL)
        self.text.delete("1.0", tk.END)
        self.text.configure(state=tk.DISABLED)

    def set_status(self, text):
        self.status_var.set(text)

    def _on_enter(self, _event):
        self._send_from_entry()

    def _send_from_entry(self):
        cmd = self.entry.get().strip()
        if not cmd:
            return
        self.entry.delete(0, tk.END)
        self.submit_command(cmd, require_addr=self._command_requires_logic_addr(cmd))

    @staticmethod
    def _command_requires_logic_addr(cmd):
        parts = (cmd or "").strip().split()
        if not parts:
            return False
        if parts[0] in ("q", "m", "status", "logic"):
            return False
        if parts[0] == "hb":
            return False
        return True

    def _ensure_logic_addr(self):
        if int(getattr(self.node, "logic_addr", 0)) != 0:
            return True
        warning = "Please set a non-zero Modbus-style logic address before enabling this node."
        self.append_log(f"[WARN] {warning}")
        messagebox.showwarning("Address Required", warning, parent=self.window)
        return False

    def submit_command(self, cmd, require_addr=False):
        if require_addr and not self._ensure_logic_addr():
            return
        self.append_log(f">>> {cmd}")
        if cmd.strip() == "q":
            self.app.remove_node(self.node_key)
            return
        threading.Thread(target=self.node.execute_command, args=(cmd,), daemon=True).start()

    def _schedule_refresh(self):
        if self.closed:
            return
        self.refresh_view()
        self.refresh_job = self.window.after(250, self._schedule_refresh)

    def refresh_view(self):
        node = self.node
        summary = (
            f"node=0x{node.node_id:02X}  logic=0x{node.logic_addr:02X}  "
            f"state=0x{node.nmt_state:02X}  heartbeat={node._heartbeat_summary()}"
        )
        self.status_var.set(f"{self.kind} {summary}")

        if self.kind == "lighting":
            self.info_var.set(
                f"relay_state=0x{node.relay_state:04X}  last_change=0x{node.last_change_mask:04X}  "
                f"scene1=0x{node.scene_on_masks.get(1, 0):04X}"
            )
            for idx, label in enumerate(self.light_labels):
                on = bool(node.relay_state & (1 << idx))
                label.configure(bg="#ffd34d" if on else "#cfcfcf")
        elif self.kind == "panel":
            self.info_var.set(
                f"linked_light=0x{node.last_light_state:04X}  last_key={node.last_key_code}  "
                f"last_event={key_event_name(node.last_key_event)}"
            )
            for idx, btn in enumerate(self.panel_buttons):
                on = bool(node.last_light_state & (1 << idx))
                btn.configure(bg="#9be39b" if on else "#f0f0f0")
        elif self.kind == "scene":
            active_list = [
                str(scene_index)
                for scene_index, active in sorted(getattr(node, "scene_active", {}).items())
                if active
            ]
            self.info_var.set(
                f"target=0x{node.target_addr:02X}  last_scene={node.last_scene_index}  "
                f"last_action={scene_action_name(node.last_action)}  "
                f"active={','.join(active_list) if active_list else '(none)'}"
            )
            for idx, btn in enumerate(self.scene_buttons):
                active = bool(getattr(node, "scene_active", {}).get(idx + 1, False))
                btn.configure(bg="#ffd34d" if active else "#f0f0f0")
        elif self.kind == "radar":
            self.info_var.set(
                f"occupied={node.occupied}  detect_scene={node.detect_scene}  clear_scene={node.clear_scene}"
            )
            if self.radar_button is not None:
                occupied = bool(node.occupied)
                self.radar_button.configure(
                    text="Detect Person (ON)" if occupied else "Detect Person",
                    bg="#ff9a76" if occupied else "#f0f0f0",
                )

    def on_close(self):
        self.app.remove_node(self.node_key)

    def destroy(self):
        self.closed = True
        self._cancel_radar_clear()
        if self.refresh_job is not None:
            try:
                self.window.after_cancel(self.refresh_job)
            except Exception:
                pass
        try:
            self.window.destroy()
        except Exception:
            pass


class GatewayDeviceWindow:
    def __init__(self, manager, node_id):
        self.manager = manager
        self.app = manager.app
        self.node_id = node_id & 0x7F
        self.closed = False
        self.refresh_job = None
        self.scene_buttons = []
        self.light_buttons = []
        self.panel_buttons = []
        self.radar_button = None
        self.radar_clear_job = None
        self.info_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="")

        snapshot = self.manager.get_device_snapshot(self.node_id)
        self.window = tk.Toplevel(self.app.root)
        self.window.geometry("880x640")
        self.window.protocol("WM_DELETE_WINDOW", self.close)
        self.window.title(self._window_title(snapshot))

        header = ttk.Frame(self.window)
        header.pack(fill=tk.X, padx=8, pady=6)
        ttk.Label(header, textvariable=self.status_var).pack(side=tk.LEFT)
        ttk.Button(header, text="Refresh", command=self.refresh_remote_state).pack(side=tk.RIGHT, padx=4)
        ttk.Button(header, text="Clear", command=self.clear_log).pack(side=tk.RIGHT, padx=4)

        info = ttk.Frame(self.window)
        info.pack(fill=tk.X, padx=8, pady=(0, 4))
        ttk.Label(info, textvariable=self.info_var).pack(side=tk.LEFT)

        self.control_frame = ttk.LabelFrame(self.window, text="Remote Controls", padding=8)
        self.control_frame.pack(fill=tk.X, padx=8, pady=(0, 8))
        self._build_controls(snapshot)

        self.text = scrolledtext.ScrolledText(self.window, wrap=tk.WORD, height=18)
        self.text.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        self.text.configure(state=tk.DISABLED)

        footer = ttk.Frame(self.window)
        footer.pack(fill=tk.X, padx=8, pady=(0, 8))
        self.entry = ttk.Entry(footer)
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.entry.bind("<Return>", self._run_custom_range_query)
        ttk.Button(footer, text="Read Range", command=self._run_custom_range_query).pack(side=tk.RIGHT, padx=(8, 0))
        self.entry.insert(0, "0x2000 8 0x00")

        self._schedule_refresh()
        self.refresh_remote_state()

    def _window_title(self, snapshot):
        device_label = device_type_name(snapshot.get("device_type", 0))
        return f"Gateway Control - {device_label} node=0x{self.node_id:02X}"

    def _build_controls(self, snapshot):
        device_type = snapshot.get("device_type", 0)
        if device_type == DEVICE_TYPE_LIGHTING:
            self._build_lighting_controls()
        elif device_type == DEVICE_TYPE_PANEL:
            self._build_panel_controls()
        elif device_type == DEVICE_TYPE_SCENE_PANEL:
            self._build_scene_controls()
        elif device_type == DEVICE_TYPE_RADAR:
            self._build_radar_controls()
        else:
            ttk.Label(
                self.control_frame,
                text="Unknown device type. Use the range query box below to inspect OD registers.",
            ).pack(anchor=tk.W)

    def _build_lighting_controls(self):
        ttk.Label(
            self.control_frame,
            text="Lighting control: click a lamp to toggle the remote relay state through SDO write 0x2400:00",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.control_frame)
        grid.pack(anchor=tk.W)
        for index in range(16):
            btn = tk.Button(
                grid,
                text=f"L{index + 1}",
                width=6,
                height=2,
                command=lambda channel_index=index + 1: self._toggle_lighting_channel(channel_index),
            )
            btn.grid(row=0 if index < 8 else 1, column=index % 8, padx=4, pady=4)
            self.light_buttons.append(btn)

    def _build_panel_controls(self):
        ttk.Label(
            self.control_frame,
            text="Panel control: click a key to inject the same panel-key publish frame from the gateway tool",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.control_frame)
        grid.pack(anchor=tk.W)
        for index in range(16):
            btn = tk.Button(
                grid,
                text=f"K{index + 1}",
                width=7,
                command=lambda key_code=index + 1: self._send_panel_key(key_code),
            )
            btn.grid(row=0 if index < 8 else 1, column=index % 8, padx=4, pady=4)
            self.panel_buttons.append(btn)

    def _build_scene_controls(self):
        ttk.Label(
            self.control_frame,
            text="Scene control: click a scene to send ON/OFF through the gateway, using the remote target_addr",
        ).pack(anchor=tk.W, pady=(0, 6))
        grid = ttk.Frame(self.control_frame)
        grid.pack(anchor=tk.W)
        for index in range(12):
            btn = tk.Button(
                grid,
                text=f"S{index + 1}",
                width=8,
                command=lambda scene_index=index + 1: self._toggle_scene(scene_index),
            )
            btn.grid(row=index // 4, column=index % 4, padx=4, pady=4)
            self.scene_buttons.append(btn)

    def _build_radar_controls(self):
        ttk.Label(
            self.control_frame,
            text="Radar control: click detect once, gateway sends scene-on immediately and scene-off 3 seconds later",
        ).pack(anchor=tk.W, pady=(0, 6))
        self.radar_button = tk.Button(
            self.control_frame,
            text="Detect Person",
            width=18,
            command=self._trigger_radar_cycle,
        )
        self.radar_button.pack(anchor=tk.W, pady=4)

    def _schedule_refresh(self):
        if self.closed:
            return
        self.refresh_view()
        self.refresh_job = self.window.after(300, self._schedule_refresh)

    def refresh_view(self):
        snapshot = self.manager.get_device_snapshot(self.node_id)
        self.window.title(self._window_title(snapshot))
        self.status_var.set(
            f"node=0x{self.node_id:02X}  type={device_type_name(snapshot.get('device_type', 0))}  "
            f"logic={format_hex(snapshot.get('logic_addr', 0), 1)}  "
            f"nmt={nmt_state_name(snapshot.get('nmt_state'))}  "
            f"last_seen={now_age_text(snapshot.get('last_seen'))}"
        )

        device_type = snapshot.get("device_type", 0)
        if device_type == DEVICE_TYPE_LIGHTING:
            self.info_var.set(
                f"relay={format_hex(snapshot.get('relay_state', 0), 2)}  "
                f"change={format_hex(snapshot.get('last_change_mask', 0), 2)}  "
                f"reason={light_reason_name(snapshot.get('last_reason', 0))}"
            )
            relay_state = snapshot.get("relay_state", 0)
            for index, btn in enumerate(self.light_buttons):
                on = bool(relay_state & (1 << index))
                btn.configure(bg="#ffd34d" if on else "#d6d6d6")
        elif device_type == DEVICE_TYPE_PANEL:
            self.info_var.set(
                f"linked_light={format_hex(snapshot.get('linked_light_state', 0), 2)}  "
                f"last_key={snapshot.get('last_key_code', 0)}  "
                f"event={key_event_name(snapshot.get('last_key_event', 0))}"
            )
            light_state = snapshot.get("linked_light_state", 0)
            for index, btn in enumerate(self.panel_buttons):
                on = bool(light_state & (1 << index))
                btn.configure(bg="#9be39b" if on else "#f0f0f0")
        elif device_type == DEVICE_TYPE_SCENE_PANEL:
            active_scenes = snapshot.get("active_scenes", set())
            self.info_var.set(
                f"target_addr={format_hex(snapshot.get('target_addr', 0), 1)}  "
                f"last_scene={snapshot.get('last_scene_index', 0)}  "
                f"last_action={scene_action_name(snapshot.get('last_scene_action', 0))}"
            )
            for index, btn in enumerate(self.scene_buttons):
                active = (index + 1) in active_scenes
                btn.configure(bg="#ffd34d" if active else "#f0f0f0")
        elif device_type == DEVICE_TYPE_RADAR:
            occupied = bool(snapshot.get("occupied", 0))
            self.info_var.set(
                f"occupied={int(occupied)}  detect_scene={snapshot.get('detect_scene', 0)}  "
                f"clear_scene={snapshot.get('clear_scene', 0)}"
            )
            if self.radar_button is not None:
                self.radar_button.configure(
                    text="Detect Person (ON)" if occupied else "Detect Person",
                    bg="#ff9a76" if occupied else "#f0f0f0",
                )
        else:
            self.info_var.set("Unknown device type. Use range query below.")

    def append_log(self, message):
        self.text.configure(state=tk.NORMAL)
        self.text.insert(tk.END, str(message).rstrip() + "\n")
        self.text.see(tk.END)
        self.text.configure(state=tk.DISABLED)

    def clear_log(self):
        self.text.configure(state=tk.NORMAL)
        self.text.delete("1.0", tk.END)
        self.text.configure(state=tk.DISABLED)

    def _run_async(self, fn, description):
        self.append_log(f">>> {description}")

        def worker():
            try:
                fn()
            except Exception as exc:
                self.window.after(0, lambda: self.append_log(f"[ERR] {exc}"))

        threading.Thread(target=worker, daemon=True).start()

    def _toggle_lighting_channel(self, channel_index):
        def action():
            snapshot = self.manager.get_device_snapshot(self.node_id)
            current_state = snapshot.get("relay_state", 0) & 0xFFFF
            new_state = current_state ^ (1 << (channel_index - 1))
            self.manager.sdo_download(self.node_id, 0x2400, 0x00, new_state, 2)
            self.manager.update_device_info(
                self.node_id,
                relay_state=new_state,
                last_reason=LIGHT_REASON_GATEWAY,
                last_seen=time(),
            )
            self.window.after(0, self.refresh_remote_state)

        self._run_async(action, f"toggle lighting channel {channel_index}")

    def _send_panel_key(self, key_code):
        self._run_async(lambda: self.manager.send_panel_key_for_device(self.node_id, key_code), f"send panel key {key_code}")

    def _toggle_scene(self, scene_index):
        snapshot = self.manager.get_device_snapshot(self.node_id)
        active_scenes = snapshot.get("active_scenes", set())
        action_code = SCENE_ACTION_OFF if scene_index in active_scenes else SCENE_ACTION_ON

        def action():
            target_addr = self.manager.get_device_snapshot(self.node_id).get("target_addr", LOGIC_ADDR_BROADCAST)
            self.manager.send_scene_command(target_addr=target_addr, scene_index=scene_index, action=action_code)

        self._run_async(action, f"scene {scene_action_name(action_code)} {scene_index}")

    def _trigger_radar_cycle(self):
        self._cancel_radar_clear()

        def action():
            snapshot = self.manager.get_device_snapshot(self.node_id)
            self.manager.send_scene_command(
                target_addr=snapshot.get("target_addr", LOGIC_ADDR_BROADCAST),
                scene_index=snapshot.get("detect_scene", 1),
                action=snapshot.get("detect_action", SCENE_ACTION_ON),
            )
            self.manager.update_device_info(self.node_id, occupied=1, last_seen=time())

        self._run_async(action, "radar detect")
        if not self.closed:
            self.radar_clear_job = self.window.after(3000, self._auto_clear_radar)

    def _auto_clear_radar(self):
        self.radar_clear_job = None
        if self.closed:
            return

        def action():
            snapshot = self.manager.get_device_snapshot(self.node_id)
            self.manager.send_scene_command(
                target_addr=snapshot.get("target_addr", LOGIC_ADDR_BROADCAST),
                scene_index=snapshot.get("clear_scene", 1),
                action=snapshot.get("clear_action", SCENE_ACTION_OFF),
            )
            self.manager.update_device_info(self.node_id, occupied=0, last_seen=time())

        self._run_async(action, "radar clear")

    def _cancel_radar_clear(self):
        if self.radar_clear_job is None:
            return
        try:
            self.window.after_cancel(self.radar_clear_job)
        except Exception:
            pass
        self.radar_clear_job = None

    def _run_custom_range_query(self, _event=None):
        raw = self.entry.get().strip()
        if not raw:
            return
        parts = raw.split()
        if len(parts) not in (2, 3):
            self.append_log("[ERR] usage: <start_index> <count> [subindex]")
            return
        start_index = safe_int(parts[0], 0x2000)
        count = max(1, safe_int(parts[1], 1))
        subindex = safe_int(parts[2], 0x00) if len(parts) == 3 else 0x00

        def action():
            results = self.manager.read_range(self.node_id, start_index, count, subindex)
            for line in results:
                self.window.after(0, lambda text=line: self.append_log(text))

        self._run_async(action, f"read range start={format_hex(start_index, 2)} count={count} sub={format_hex(subindex, 1)}")

    def refresh_remote_state(self):
        def action():
            snapshot = self.manager.get_device_snapshot(self.node_id)
            device_type = snapshot.get("device_type", 0)
            self.manager.query_common_info(self.node_id)
            if device_type == DEVICE_TYPE_LIGHTING:
                relay_state, _ = self.manager.sdo_upload(self.node_id, 0x2100, 0x00)
                change_mask, _ = self.manager.sdo_upload(self.node_id, 0x2101, 0x00)
                reason, _ = self.manager.sdo_upload(self.node_id, 0x2102, 0x00)
                source_node, _ = self.manager.sdo_upload(self.node_id, 0x2103, 0x00)
                self.manager.update_device_info(
                    self.node_id,
                    relay_state=relay_state,
                    last_change_mask=change_mask,
                    last_reason=reason,
                    last_source_node=source_node,
                )
            elif device_type == DEVICE_TYPE_PANEL:
                linked_state, _ = self.manager.sdo_upload(self.node_id, 0x2100, 0x00)
                change_mask, _ = self.manager.sdo_upload(self.node_id, 0x2101, 0x00)
                reason, _ = self.manager.sdo_upload(self.node_id, 0x2102, 0x00)
                source_node, _ = self.manager.sdo_upload(self.node_id, 0x2103, 0x00)
                self.manager.update_device_info(
                    self.node_id,
                    linked_light_state=linked_state,
                    linked_change_mask=change_mask,
                    linked_reason=reason,
                    linked_source_node=source_node,
                )
            elif device_type == DEVICE_TYPE_SCENE_PANEL:
                target_addr, _ = self.manager.sdo_upload(self.node_id, 0x2100, 0x00)
                last_scene, _ = self.manager.sdo_upload(self.node_id, 0x2101, 0x00)
                last_action, _ = self.manager.sdo_upload(self.node_id, 0x2102, 0x00)
                self.manager.update_device_info(
                    self.node_id,
                    target_addr=target_addr,
                    last_scene_index=last_scene,
                    last_scene_action=last_action,
                )
            elif device_type == DEVICE_TYPE_RADAR:
                target_addr, _ = self.manager.sdo_upload(self.node_id, 0x2100, 0x00)
                detect_scene, _ = self.manager.sdo_upload(self.node_id, 0x2101, 0x00)
                clear_scene, _ = self.manager.sdo_upload(self.node_id, 0x2102, 0x00)
                detect_action, _ = self.manager.sdo_upload(self.node_id, 0x2103, 0x00)
                clear_action, _ = self.manager.sdo_upload(self.node_id, 0x2104, 0x00)
                occupied, _ = self.manager.sdo_upload(self.node_id, 0x2105, 0x00)
                self.manager.update_device_info(
                    self.node_id,
                    target_addr=target_addr,
                    detect_scene=detect_scene,
                    clear_scene=clear_scene,
                    detect_action=detect_action,
                    clear_action=clear_action,
                    occupied=occupied,
                )
            self.window.after(0, lambda: self.append_log("[APP] remote state refreshed"))

        self._run_async(action, "refresh remote state")

    def focus(self):
        try:
            self.window.deiconify()
            self.window.lift()
            self.window.focus_force()
        except Exception:
            pass

    def close(self):
        self.closed = True
        self._cancel_radar_clear()
        if self.refresh_job is not None:
            try:
                self.window.after_cancel(self.refresh_job)
            except Exception:
                pass
        self.manager.unregister_device_window(self.node_id)
        try:
            self.window.destroy()
        except Exception:
            pass


class GatewayConsoleWindow:
    def __init__(self, manager):
        self.manager = manager
        self.app = manager.app
        self.closed = False
        self.refresh_job = None
        self.auto_job = None
        self.auto_discovery_var = tk.BooleanVar(value=True)
        self.discovery_interval_var = tk.StringVar(value=str(DISCOVERY_INTERVAL_MS))
        self.range_node_var = tk.StringVar(value="")
        self.range_start_var = tk.StringVar(value="0x2000")
        self.range_count_var = tk.StringVar(value="8")
        self.range_subindex_var = tk.StringVar(value="0x00")

        self.window = tk.Toplevel(self.app.root)
        self.window.title("Shared Gateway Console")
        self.window.geometry("1180x760")
        self.window.protocol("WM_DELETE_WINDOW", self.close)

        top = ttk.Frame(self.window, padding=8)
        top.pack(fill=tk.X)
        ttk.Button(top, text="Discover Now", command=self.manager.discover_now).pack(side=tk.LEFT)
        ttk.Checkbutton(top, text="Auto Discover", variable=self.auto_discovery_var).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Label(top, text="Interval ms").pack(side=tk.LEFT, padx=(10, 4))
        ttk.Entry(top, textvariable=self.discovery_interval_var, width=8).pack(side=tk.LEFT)
        ttk.Button(top, text="Bus Status", command=self.app.print_bus_status).pack(side=tk.LEFT, padx=6)
        ttk.Button(top, text="Clear", command=self.clear_log).pack(side=tk.LEFT, padx=6)

        note = ttk.LabelFrame(self.window, text="Gateway Notes", padding=8)
        note.pack(fill=tk.X, padx=8, pady=(0, 8))
        ttk.Label(
            note,
            text="This window shares the same USB-CAN instance with all simulator nodes in test_demo.py.",
        ).pack(anchor=tk.W)
        ttk.Label(
            note,
            text="Burst register query uses repeated SDO uploads to emulate a Modbus-style multi-register read.",
        ).pack(anchor=tk.W)

        query = ttk.LabelFrame(self.window, text="Burst Register Query", padding=8)
        query.pack(fill=tk.X, padx=8, pady=(0, 8))
        ttk.Label(query, text="Node").pack(side=tk.LEFT)
        ttk.Entry(query, textvariable=self.range_node_var, width=10).pack(side=tk.LEFT, padx=(4, 10))
        ttk.Label(query, text="Start Index").pack(side=tk.LEFT)
        ttk.Entry(query, textvariable=self.range_start_var, width=10).pack(side=tk.LEFT, padx=(4, 10))
        ttk.Label(query, text="Count").pack(side=tk.LEFT)
        ttk.Entry(query, textvariable=self.range_count_var, width=8).pack(side=tk.LEFT, padx=(4, 10))
        ttk.Label(query, text="Subindex").pack(side=tk.LEFT)
        ttk.Entry(query, textvariable=self.range_subindex_var, width=8).pack(side=tk.LEFT, padx=(4, 10))
        ttk.Button(query, text="Read Range", command=self.run_range_query).pack(side=tk.LEFT, padx=6)
        ttk.Button(query, text="Refresh Node", command=self.refresh_selected_node).pack(side=tk.LEFT, padx=6)

        devices = ttk.LabelFrame(self.window, text="Discovered Devices", padding=8)
        devices.pack(fill=tk.BOTH, expand=False, padx=8, pady=(0, 8))
        self.device_canvas = tk.Canvas(devices, height=250)
        self.device_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        device_scroll = ttk.Scrollbar(devices, orient=tk.VERTICAL, command=self.device_canvas.yview)
        device_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.device_canvas.configure(yscrollcommand=device_scroll.set)
        self.device_table = ttk.Frame(self.device_canvas)
        self.device_table.bind(
            "<Configure>",
            lambda _event: self.device_canvas.configure(scrollregion=self.device_canvas.bbox("all")),
        )
        self.device_canvas.create_window((0, 0), window=self.device_table, anchor="nw")

        log_frame = ttk.LabelFrame(self.window, text="Gateway Log", padding=8)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        self.text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, height=20)
        self.text.pack(fill=tk.BOTH, expand=True)
        self.text.configure(state=tk.DISABLED)

        self.app.register_log_sink("gateway", self)
        self._schedule_refresh()
        self._schedule_auto_discovery()

    def append_log(self, message):
        self.text.configure(state=tk.NORMAL)
        self.text.insert(tk.END, str(message).rstrip() + "\n")
        self.text.see(tk.END)
        self.text.configure(state=tk.DISABLED)

    def clear_log(self):
        self.text.configure(state=tk.NORMAL)
        self.text.delete("1.0", tk.END)
        self.text.configure(state=tk.DISABLED)

    def _schedule_refresh(self):
        if self.closed:
            return
        self.refresh_table()
        self.refresh_job = self.window.after(500, self._schedule_refresh)

    def _schedule_auto_discovery(self):
        if self.closed:
            return
        interval_ms = max(1000, safe_int(self.discovery_interval_var.get(), DISCOVERY_INTERVAL_MS))
        if self.auto_discovery_var.get():
            self.manager.discover_now()
        self.auto_job = self.window.after(interval_ms, self._schedule_auto_discovery)

    def refresh_table(self):
        rows = self.manager.get_all_device_snapshots()
        for child in self.device_table.winfo_children():
            child.destroy()

        headers = ["Node", "Type", "Logic", "NMT", "Last Seen", "Summary", "Actions"]
        for column, title in enumerate(headers):
            ttk.Label(self.device_table, text=title).grid(row=0, column=column, sticky="w", padx=6, pady=2)

        for row_index, info in enumerate(rows, start=1):
            summary = self.manager.device_summary(info)
            ttk.Label(self.device_table, text=f"0x{info['node_id']:02X}").grid(row=row_index, column=0, sticky="w", padx=6, pady=2)
            ttk.Label(self.device_table, text=device_type_name(info.get("device_type", 0))).grid(row=row_index, column=1, sticky="w", padx=6, pady=2)
            ttk.Label(self.device_table, text=format_hex(info.get("logic_addr", 0), 1)).grid(row=row_index, column=2, sticky="w", padx=6, pady=2)
            ttk.Label(self.device_table, text=nmt_state_name(info.get("nmt_state"))).grid(row=row_index, column=3, sticky="w", padx=6, pady=2)
            ttk.Label(self.device_table, text=now_age_text(info.get("last_seen"))).grid(row=row_index, column=4, sticky="w", padx=6, pady=2)
            ttk.Label(self.device_table, text=summary).grid(row=row_index, column=5, sticky="w", padx=6, pady=2)

            action_cell = ttk.Frame(self.device_table)
            action_cell.grid(row=row_index, column=6, sticky="w", padx=6, pady=2)
            ttk.Button(
                action_cell,
                text="Open",
                command=lambda current_node=info["node_id"]: self.manager.open_device_window(current_node),
            ).pack(side=tk.LEFT)
            ttk.Button(
                action_cell,
                text="Use",
                command=lambda current_node=info["node_id"]: self.range_node_var.set(f"0x{current_node:02X}"),
            ).pack(side=tk.LEFT, padx=4)
            ttk.Button(
                action_cell,
                text="Refresh",
                command=lambda current_node=info["node_id"]: self.manager.refresh_node_async(current_node),
            ).pack(side=tk.LEFT)

    def run_range_query(self):
        node_id = safe_int(self.range_node_var.get(), 0)
        start_index = safe_int(self.range_start_var.get(), 0x2000)
        count = max(1, safe_int(self.range_count_var.get(), 1))
        subindex = safe_int(self.range_subindex_var.get(), 0x00)
        if node_id <= 0:
            messagebox.showwarning("Node Required", "Please choose a discovered node ID first.", parent=self.window)
            return

        def worker():
            try:
                lines = self.manager.read_range(node_id, start_index, count, subindex)
                for line in lines:
                    self.app.enqueue_log("gateway", line)
            except Exception as exc:
                self.app.enqueue_log("gateway", f"[ERR] range query failed: {exc}")

        threading.Thread(target=worker, daemon=True).start()

    def refresh_selected_node(self):
        node_id = safe_int(self.range_node_var.get(), 0)
        if node_id <= 0:
            messagebox.showwarning("Node Required", "Please choose a discovered node ID first.", parent=self.window)
            return
        self.manager.refresh_node_async(node_id)

    def focus(self):
        try:
            self.window.deiconify()
            self.window.lift()
            self.window.focus_force()
        except Exception:
            pass

    def close(self):
        self.closed = True
        if self.refresh_job is not None:
            try:
                self.window.after_cancel(self.refresh_job)
            except Exception:
                pass
        if self.auto_job is not None:
            try:
                self.window.after_cancel(self.auto_job)
            except Exception:
                pass
        self.app.unregister_log_sink("gateway")
        self.manager.unregister_main_window()
        try:
            self.window.destroy()
        except Exception:
            pass


class GatewayStateManager:
    def __init__(self, app):
        self.app = app
        self.discovered = {}
        self.device_lock = threading.Lock()
        self.pending_sdo = {}
        self.pending_lock = threading.Lock()
        self.sdo_lock = threading.Lock()
        self.main_window = None
        self.device_windows = {}

    def register_main_window(self, window):
        self.main_window = window

    def unregister_main_window(self):
        self.main_window = None

    def log(self, message):
        if self.main_window is not None and not self.main_window.closed:
            self.app.enqueue_log("gateway", message)
        else:
            self.app.system_log(message)

    @staticmethod
    def default_device_info(node_id):
        return {
            "node_id": node_id & 0x7F,
            "logic_addr": 0,
            "device_type": 0,
            "publish_cob_id": 0,
            "last_claim_reason": 0,
            "last_seen": 0.0,
            "nmt_state": None,
            "heartbeat_ms": None,
            "relay_state": 0,
            "last_change_mask": 0,
            "last_reason": 0,
            "last_source_node": 0,
            "linked_light_state": 0,
            "linked_change_mask": 0,
            "linked_reason": 0,
            "linked_source_node": 0,
            "last_key_code": 0,
            "last_key_event": 0,
            "target_addr": LOGIC_ADDR_BROADCAST,
            "last_scene_index": 0,
            "last_scene_action": 0,
            "active_scenes": set(),
            "detect_scene": 1,
            "clear_scene": 1,
            "detect_action": SCENE_ACTION_ON,
            "clear_action": SCENE_ACTION_OFF,
            "occupied": 0,
        }

    def update_device_info(self, node_id, **kwargs):
        node_id &= 0x7F
        with self.device_lock:
            info = self.discovered.get(node_id)
            if info is None:
                info = self.default_device_info(node_id)
                self.discovered[node_id] = info
            for key, value in kwargs.items():
                if key == "active_scenes" and value is not None:
                    info[key] = set(value)
                else:
                    info[key] = value
            info["last_seen"] = kwargs.get("last_seen", time())

    def get_device_snapshot(self, node_id):
        node_id &= 0x7F
        with self.device_lock:
            info = self.discovered.get(node_id)
            if info is None:
                return self.default_device_info(node_id)
            snapshot = dict(info)
            snapshot["active_scenes"] = set(info.get("active_scenes", set()))
            return snapshot

    def get_all_device_snapshots(self):
        with self.device_lock:
            snapshots = []
            for node_id in sorted(self.discovered.keys()):
                info = self.discovered[node_id]
                snapshot = dict(info)
                snapshot["active_scenes"] = set(info.get("active_scenes", set()))
                snapshots.append(snapshot)
            return snapshots

    @staticmethod
    def device_summary(info):
        device_type = info.get("device_type", 0)
        if device_type == DEVICE_TYPE_LIGHTING:
            return f"relay={format_hex(info.get('relay_state', 0), 2)}"
        if device_type == DEVICE_TYPE_PANEL:
            return f"linked={format_hex(info.get('linked_light_state', 0), 2)}"
        if device_type == DEVICE_TYPE_SCENE_PANEL:
            active = sorted(info.get("active_scenes", set()))
            return f"active={','.join(str(item) for item in active) if active else '(none)'}"
        if device_type == DEVICE_TYPE_RADAR:
            return f"occupied={int(bool(info.get('occupied', 0)))}"
        return f"publish=0x{info.get('publish_cob_id', 0):03X}"

    def open_main_window(self):
        if self.main_window is not None and not self.main_window.closed:
            self.main_window.focus()
            return
        window = GatewayConsoleWindow(self)
        self.register_main_window(window)

    def open_device_window(self, node_id):
        node_id &= 0x7F
        existing = self.device_windows.get(node_id)
        if existing is not None and not existing.closed:
            existing.focus()
            return
        self.device_windows[node_id] = GatewayDeviceWindow(self, node_id)

    def unregister_device_window(self, node_id):
        self.device_windows.pop(node_id & 0x7F, None)

    def forget_node(self, node_id):
        node_id &= 0x7F
        with self.device_lock:
            self.discovered.pop(node_id, None)
        window = self.device_windows.pop(node_id, None)
        if window is not None and not window.closed:
            window.close()

    def handle_frame(self, frame):
        cob_id = CANopenNodeSimulator._effective_cob_id(frame["id"], frame.get("is_ext", False))
        data = frame["data"]
        if decode_address_claim_cob_id(cob_id) is not None:
            self.handle_address_claim_frame(cob_id, data)
        elif cob_id == APP_LIGHT_STATUS_COB_ID:
            self.handle_light_status_frame(data)
        elif cob_id == APP_PANEL_KEY_COB_ID:
            self.handle_panel_key_frame(data)
        elif cob_id == APP_SCENE_COMMAND_COB_ID:
            self.handle_scene_command_frame(data)
        elif 0x580 <= cob_id <= 0x5FF:
            self._handle_sdo_response_frame(cob_id, data)
        elif 0x700 <= cob_id <= 0x77F:
            self.handle_heartbeat_frame(cob_id, data)

    def handle_address_claim_frame(self, cob_id, data):
        payload = decode_address_claim(data)
        node_id = payload["source_node"] & 0x7F
        fallback_node_id = decode_address_claim_cob_id(cob_id)
        if node_id == 0:
            node_id = fallback_node_id or 0
        if node_id == 0:
            return
        previous = self.get_device_snapshot(node_id)
        self.update_device_info(
            node_id,
            logic_addr=payload["logic_addr"],
            device_type=payload["device_type"],
            publish_cob_id=payload["publish_cob_id"],
            last_claim_reason=payload["reason"],
        )
        changed = (
            previous.get("device_type", 0) == 0 or
            previous.get("logic_addr") != payload["logic_addr"] or
            previous.get("device_type") != payload["device_type"] or
            previous.get("publish_cob_id") != payload["publish_cob_id"]
        )
        if changed:
            self.log(
                f"[BUS] addr-claim node=0x{node_id:02X} type={device_type_name(payload['device_type'])} "
                f"logic={format_hex(payload['logic_addr'], 1)} publish=0x{payload['publish_cob_id']:03X} "
                f"reason={format_hex(payload['reason'], 1)}"
            )
        if previous.get("device_type", 0) == 0:
            self.refresh_node_async(node_id)

    def handle_heartbeat_frame(self, cob_id, data):
        node_id = cob_id - 0x700
        state = data[0] if data else None
        self.update_device_info(node_id, nmt_state=state)

    def handle_light_status_frame(self, data):
        payload = decode_light_status(data)
        source_node = payload["source_node"] & 0x7F
        if source_node != 0:
            self.update_device_info(
                source_node,
                logic_addr=payload["logic_addr"],
                relay_state=payload["state_word"],
                last_change_mask=payload["change_mask"],
                last_reason=payload["reason"],
                last_source_node=source_node,
            )

        for info in self.get_all_device_snapshots():
            if info.get("device_type") != DEVICE_TYPE_PANEL:
                continue
            if info.get("logic_addr") != payload["logic_addr"]:
                continue
            self.update_device_info(
                info["node_id"],
                linked_light_state=payload["state_word"],
                linked_change_mask=payload["change_mask"],
                linked_reason=payload["reason"],
                linked_source_node=source_node,
            )

    def handle_panel_key_frame(self, data):
        payload = decode_panel_key(data)
        source_node = payload["source_node"] & 0x7F
        if source_node == 0:
            return
        self.update_device_info(
            source_node,
            logic_addr=payload["logic_addr"],
            last_key_code=payload["key_code"],
            last_key_event=payload["key_event"],
        )

    def handle_scene_command_frame(self, data):
        payload = decode_scene_command(data)
        source_node = payload["source_node"] & 0x7F
        if source_node != 0:
            self.update_device_info(
                source_node,
                last_scene_index=payload["scene_index"],
                last_scene_action=payload["action"],
            )

        for info in self.get_all_device_snapshots():
            if info.get("device_type") != DEVICE_TYPE_SCENE_PANEL:
                continue
            target_addr = info.get("target_addr", LOGIC_ADDR_BROADCAST)
            if payload["target_addr"] not in (LOGIC_ADDR_BROADCAST, target_addr):
                continue
            active = set(info.get("active_scenes", set()))
            if payload["action"] == SCENE_ACTION_ON:
                active.add(payload["scene_index"])
            elif payload["action"] == SCENE_ACTION_OFF:
                active.discard(payload["scene_index"])
            elif payload["action"] == SCENE_ACTION_TOGGLE:
                if payload["scene_index"] in active:
                    active.discard(payload["scene_index"])
                else:
                    active.add(payload["scene_index"])
            self.update_device_info(info["node_id"], active_scenes=active)

        if source_node != 0:
            source_snapshot = self.get_device_snapshot(source_node)
            if source_snapshot.get("device_type") == DEVICE_TYPE_RADAR:
                occupied = source_snapshot.get("occupied", 0)
                if payload["scene_index"] == source_snapshot.get("detect_scene", 1) and payload["action"] == source_snapshot.get("detect_action", SCENE_ACTION_ON):
                    occupied = 1
                if payload["scene_index"] == source_snapshot.get("clear_scene", 1) and payload["action"] == source_snapshot.get("clear_action", SCENE_ACTION_OFF):
                    occupied = 0
                self.update_device_info(source_node, occupied=occupied)

    def _handle_sdo_response_frame(self, cob_id, data):
        node_id = cob_id - 0x580
        with self.pending_lock:
            waiter = self.pending_sdo.get(node_id)
        if waiter is None or len(data) != 8:
            return
        if data[0] == SDO_ABORT or (
            (data[1] | (data[2] << 8)) == waiter["index"] and
            data[3] == waiter["subindex"]
        ):
            waiter["response"] = list(data)
            waiter["event"].set()

    def ensure_bus_open(self):
        if self.app.handle is not None:
            return
        self.app.open_bus(safe_int(self.app.channel_var.get(), 1))

    def send_can(self, cob_id, data, desc=""):
        self.ensure_bus_open()
        self.app.send_gateway_frame(cob_id, data, desc=desc, logger=self.log)

    def discover_now(self):
        def worker():
            try:
                self.send_discovery_request()
            except Exception as exc:
                self.log(f"[ERR] discovery failed: {exc}")

        threading.Thread(target=worker, daemon=True).start()

    def send_discovery_request(self, target_logic_addr=0, target_node_id=0, target_device_type=0):
        data = encode_discovery_request(target_logic_addr, target_node_id, target_device_type, 0x00)
        self.send_can(
            APP_DISCOVERY_REQUEST_COB_ID,
            data,
            f"discover logic={format_hex(target_logic_addr, 1)} node={format_hex(target_node_id, 1)} type={format_hex(target_device_type, 1)}",
        )

    def sdo_upload(self, node_id, index, subindex=0x00, timeout=0.8):
        node_id &= 0x7F
        event = threading.Event()
        waiter = {
            "index": index & 0xFFFF,
            "subindex": subindex & 0xFF,
            "event": event,
            "response": None,
        }
        with self.sdo_lock:
            with self.pending_lock:
                self.pending_sdo[node_id] = waiter
            try:
                request = [SDO_CCS_UPLOAD_REQ, index & 0xFF, (index >> 8) & 0xFF, subindex & 0xFF, 0, 0, 0, 0]
                self.send_can(0x600 + node_id, request, f"sdo upload 0x{index:04X}:{subindex:02X}")
                if not event.wait(timeout):
                    raise TimeoutError(f"SDO timeout node=0x{node_id:02X} index=0x{index:04X}:{subindex:02X}")
                response = waiter["response"]
            finally:
                with self.pending_lock:
                    current = self.pending_sdo.get(node_id)
                    if current is waiter:
                        self.pending_sdo.pop(node_id, None)

        if response is None:
            raise RuntimeError("empty SDO response")
        if response[0] == SDO_ABORT:
            abort_code = response[4] | (response[5] << 8) | (response[6] << 16) | (response[7] << 24)
            raise RuntimeError(f"SDO abort 0x{abort_code:08X} at 0x{index:04X}:{subindex:02X}")

        size_map = {
            SDO_SCS_UPLOAD_1B: 1,
            SDO_SCS_UPLOAD_2B: 2,
            SDO_SCS_UPLOAD_4B: 4,
        }
        if response[0] not in size_map:
            raise RuntimeError(f"unsupported SDO upload response cmd 0x{response[0]:02X}")
        size = size_map[response[0]]
        value = 0
        for offset in range(size):
            value |= response[4 + offset] << (8 * offset)
        return value, size

    def sdo_download(self, node_id, index, subindex, value, size, timeout=0.8):
        node_id &= 0x7F
        if size not in (1, 2, 4):
            raise ValueError("SDO expedited download supports only 1, 2 or 4 bytes")
        cmd = {
            1: SDO_CCS_DOWNLOAD_1B,
            2: SDO_CCS_DOWNLOAD_2B,
            4: SDO_CCS_DOWNLOAD_4B,
        }[size]
        payload = [cmd, index & 0xFF, (index >> 8) & 0xFF, subindex & 0xFF, 0, 0, 0, 0]
        for offset in range(size):
            payload[4 + offset] = (value >> (8 * offset)) & 0xFF

        event = threading.Event()
        waiter = {
            "index": index & 0xFFFF,
            "subindex": subindex & 0xFF,
            "event": event,
            "response": None,
        }
        with self.sdo_lock:
            with self.pending_lock:
                self.pending_sdo[node_id] = waiter
            try:
                self.send_can(0x600 + node_id, payload, f"sdo download 0x{index:04X}:{subindex:02X} value={format_hex(value, size)}")
                if not event.wait(timeout):
                    raise TimeoutError(f"SDO timeout node=0x{node_id:02X} index=0x{index:04X}:{subindex:02X}")
                response = waiter["response"]
            finally:
                with self.pending_lock:
                    current = self.pending_sdo.get(node_id)
                    if current is waiter:
                        self.pending_sdo.pop(node_id, None)

        if response is None:
            raise RuntimeError("empty SDO response")
        if response[0] == SDO_ABORT:
            abort_code = response[4] | (response[5] << 8) | (response[6] << 16) | (response[7] << 24)
            raise RuntimeError(f"SDO abort 0x{abort_code:08X} at 0x{index:04X}:{subindex:02X}")
        if response[0] != SDO_SCS_DOWNLOAD_RSP:
            raise RuntimeError(f"unexpected SDO download response cmd 0x{response[0]:02X}")
        return True

    def query_common_info(self, node_id):
        logic_addr, _ = self.sdo_upload(node_id, 0x2000, 0x00)
        device_type, _ = self.sdo_upload(node_id, 0x2001, 0x00)
        heartbeat_ms, _ = self.sdo_upload(node_id, 0x1017, 0x00)
        self.update_device_info(node_id, logic_addr=logic_addr, device_type=device_type, heartbeat_ms=heartbeat_ms)

    def read_range(self, node_id, start_index, count, subindex=0x00):
        node_id &= 0x7F
        start_index &= 0xFFFF
        count = max(1, min(int(count), 1024))
        subindex &= 0xFF
        results = []
        for offset in range(count):
            index = (start_index + offset) & 0xFFFF
            try:
                value, size = self.sdo_upload(node_id, index, subindex)
                results.append(f"[RANGE] node=0x{node_id:02X} 0x{index:04X}:{subindex:02X} = {format_hex(value, size)} ({size}B)")
            except Exception as exc:
                results.append(f"[RANGE] node=0x{node_id:02X} 0x{index:04X}:{subindex:02X} -> {exc}")
        return results

    def refresh_node_async(self, node_id):
        def worker():
            try:
                self.query_common_info(node_id)
                snapshot = self.get_device_snapshot(node_id)
                device_type = snapshot.get("device_type", 0)
                if device_type == DEVICE_TYPE_LIGHTING:
                    relay_state, _ = self.sdo_upload(node_id, 0x2100, 0x00)
                    self.update_device_info(node_id, relay_state=relay_state)
                elif device_type == DEVICE_TYPE_PANEL:
                    linked_state, _ = self.sdo_upload(node_id, 0x2100, 0x00)
                    self.update_device_info(node_id, linked_light_state=linked_state)
                elif device_type == DEVICE_TYPE_SCENE_PANEL:
                    target_addr, _ = self.sdo_upload(node_id, 0x2100, 0x00)
                    self.update_device_info(node_id, target_addr=target_addr)
                elif device_type == DEVICE_TYPE_RADAR:
                    target_addr, _ = self.sdo_upload(node_id, 0x2100, 0x00)
                    occupied, _ = self.sdo_upload(node_id, 0x2105, 0x00)
                    self.update_device_info(node_id, target_addr=target_addr, occupied=occupied)
                self.log(f"[APP] refreshed node 0x{node_id:02X}")
            except Exception as exc:
                self.log(f"[ERR] refresh node 0x{node_id:02X} failed: {exc}")

        threading.Thread(target=worker, daemon=True).start()

    def send_panel_key_for_device(self, node_id, key_code, key_event=KEY_EVENT_PRESS, value=1):
        snapshot = self.get_device_snapshot(node_id)
        logic_addr = snapshot.get("logic_addr", 0)
        if logic_addr == 0:
            raise RuntimeError("remote panel logic address is 0, cannot publish key frame")
        data = encode_panel_key(
            logic_addr=logic_addr,
            key_code=key_code,
            key_event=key_event,
            value=value,
            source_node=node_id,
            flags=0x00,
        )
        self.send_can(
            APP_PANEL_KEY_COB_ID,
            data,
            f"gateway injected panel key node=0x{node_id:02X} addr={format_hex(logic_addr, 1)} key={key_code} event={key_event_name(key_event)}",
        )
        self.update_device_info(node_id, last_key_code=key_code, last_key_event=key_event)

    def send_scene_command(self, target_addr, scene_index, action, flags=0):
        data = encode_scene_command(
            target_addr=target_addr,
            source_type=SCENE_SOURCE_GATEWAY,
            scene_index=scene_index,
            action=action,
            source_node=0x00,
            flags=flags,
        )
        self.send_can(
            APP_SCENE_COMMAND_COB_ID,
            data,
            f"gateway scene target={format_hex(target_addr, 1)} scene={scene_index} action={scene_action_name(action)}",
        )
        self.handle_scene_command_frame(data)


class MultiNodeDemoApp:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("USB-CAN Multi-Node Test Demo")
        self.root.geometry("960x640")
        self.root.protocol("WM_DELETE_WINDOW", self.shutdown)

        self.log_queue = queue.Queue()
        self.handle = None
        self.channel = None
        self.tx_lock = threading.Lock()
        self.receiver_running = False
        self.receiver_thread = None
        self.nodes = {}
        self.windows = {}
        self.log_sinks = {}
        self.launch_addr_vars = {}
        self.reported_bus_conflicts = set()
        self.gateway = GatewayStateManager(self)

        self._build_ui()
        self._print_help()
        self.root.after(50, self._drain_log_queue)

    def _build_ui(self):
        top = ttk.Frame(self.root, padding=8)
        top.pack(fill=tk.X)

        ttk.Label(top, text="USB-CAN channel").pack(side=tk.LEFT)
        self.channel_var = tk.StringVar(value="1")
        ttk.Entry(top, textvariable=self.channel_var, width=8).pack(side=tk.LEFT, padx=(6, 12))
        ttk.Button(top, text="Open USB-CAN", command=self.open_bus_from_ui).pack(side=tk.LEFT)
        ttk.Button(top, text="Bus Status", command=self.print_bus_status).pack(side=tk.LEFT, padx=6)
        ttk.Button(top, text="Open Gateway", command=self.open_gateway_window).pack(side=tk.LEFT, padx=6)
        ttk.Button(top, text="Clear Log", command=self.clear_system_log).pack(side=tk.LEFT, padx=6)

        launcher = ttk.LabelFrame(self.root, text="Enable Nodes", padding=8)
        launcher.pack(fill=tk.X, padx=8, pady=(0, 8))
        for kind in ("lighting", "panel", "scene", "radar"):
            cfg = NODE_KIND_CONFIG[kind]
            row = ttk.Frame(launcher)
            row.pack(fill=tk.X, pady=2)
            ttk.Label(row, text=cfg["title"], width=14).pack(side=tk.LEFT)
            ttk.Label(row, text="Addr").pack(side=tk.LEFT, padx=(4, 2))
            addr_var = tk.StringVar(value="")
            self.launch_addr_vars[kind] = addr_var
            ttk.Entry(row, textvariable=addr_var, width=10).pack(side=tk.LEFT)
            ttk.Button(
                row,
                text="Enable",
                command=lambda current_kind=kind: self.add_node_from_form(current_kind),
            ).pack(side=tk.LEFT, padx=6)
            ttk.Label(row, text=cfg["hint"]).pack(side=tk.LEFT, padx=(8, 0))

        cmd = ttk.LabelFrame(self.root, text="Main Commands", padding=8)
        cmd.pack(fill=tk.X, padx=8, pady=(0, 8))

        ttk.Label(
            cmd,
            text="Examples: usb 1 | gateway | add lighting 0x01 | add panel 0x01 | add scene 0x21 | add radar 0x31 | list | remove 0x11",
        ).pack(fill=tk.X)

        cmd_row = ttk.Frame(cmd)
        cmd_row.pack(fill=tk.X, pady=(6, 0))
        self.main_entry = ttk.Entry(cmd_row)
        self.main_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.main_entry.bind("<Return>", self._on_main_enter)
        ttk.Button(cmd_row, text="Run", command=self._run_main_command).pack(side=tk.RIGHT, padx=(8, 0))

        log_frame = ttk.LabelFrame(self.root, text="Bus / System Log", padding=8)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

        self.system_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, height=20)
        self.system_text.pack(fill=tk.BOTH, expand=True)
        self.system_text.configure(state=tk.DISABLED)

    def enqueue_log(self, target, message):
        self.log_queue.put((target, str(message)))

    def system_log(self, message):
        self.enqueue_log("system", message)

    def node_logger(self, node_key):
        return lambda message, key=node_key: self.enqueue_log(key, message)

    def register_log_sink(self, sink_key, sink):
        self.log_sinks[sink_key] = sink

    def unregister_log_sink(self, sink_key):
        self.log_sinks.pop(sink_key, None)

    def _drain_log_queue(self):
        while True:
            try:
                target, message = self.log_queue.get_nowait()
            except queue.Empty:
                break

            if target == "system":
                self.system_text.configure(state=tk.NORMAL)
                self.system_text.insert(tk.END, message.rstrip() + "\n")
                self.system_text.see(tk.END)
                self.system_text.configure(state=tk.DISABLED)
                continue

            sink = self.log_sinks.get(target)
            if sink is not None:
                sink.append_log(message)

        self.root.after(50, self._drain_log_queue)

    def _on_main_enter(self, _event):
        self._run_main_command()

    def _run_main_command(self):
        cmd = self.main_entry.get().strip()
        if not cmd:
            return
        self.main_entry.delete(0, tk.END)
        self.system_log(f">>> {cmd}")
        self.execute_main_command(cmd)

    def _print_help(self):
        self.system_log("Single USB-CAN multi-node demo ready.")
        self.system_log("Open USB-CAN once, then enter a Modbus-style logic address before enabling any node.")
        self.system_log("Panel and lighting may share one logic address. Other duplicate addresses will raise a CAN warning.")
        self.system_log("CAN arbitration only protects different frame IDs. Same publish COB-ID plus different payload can still conflict.")
        self.system_log("Gateway mode is now integrated here, so gateway and simulated nodes share one USB-CAN instance.")
        self.system_log("Each node window has its own command line, so logs stay separated.")
        self.system_log("Current UI is built with tkinter, not PyQt.")

    def clear_system_log(self):
        self.system_text.configure(state=tk.NORMAL)
        self.system_text.delete("1.0", tk.END)
        self.system_text.configure(state=tk.DISABLED)

    def _parse_logic_addr(self, text):
        value = int((text or "").strip(), 0)
        if not 1 <= value <= 247:
            raise ValueError("logic address must be in 1..247")
        return value

    def add_node_from_form(self, kind):
        addr_text = self.launch_addr_vars.get(kind, tk.StringVar(value="")).get().strip()
        if not addr_text:
            messagebox.showwarning(
                "Address Required",
                f"Please enter a Modbus-style address for {NODE_KIND_CONFIG[kind]['title']} first.",
                parent=self.root,
            )
            return
        try:
            logic_addr = self._parse_logic_addr(addr_text)
        except Exception as exc:
            messagebox.showerror("Address Error", str(exc), parent=self.root)
            return
        try:
            self.add_node(kind, logic_addr)
        except Exception as exc:
            messagebox.showerror("Enable Error", str(exc), parent=self.root)

    def allocate_node_id(self, kind):
        cfg = NODE_KIND_CONFIG[kind]
        used_ids = {node.node_id for node in self.nodes.values()}
        for node_id in range(cfg["node_id_start"], cfg["node_id_end"] + 1):
            if node_id not in used_ids:
                return node_id
        raise RuntimeError(f"no free CAN node id left for {cfg['title']}")

    def _report_logic_addr_conflict(self, local_node_id, local_type, logic_addr, remote_node_id, remote_type):
        conflict_key = (
            min(local_node_id, remote_node_id),
            max(local_node_id, remote_node_id),
            logic_addr,
            min(local_type, remote_type),
            max(local_type, remote_type),
        )
        if conflict_key in self.reported_bus_conflicts:
            return
        self.reported_bus_conflicts.add(conflict_key)
        same_publish = (
            device_publish_cob_id(local_type) != 0 and
            device_publish_cob_id(local_type) == device_publish_cob_id(remote_type)
        )
        risk_text = (
            "same publish COB-ID may collide on CAN if both nodes transmit together"
            if same_publish else
            "duplicate logic address may cause ambiguous linkage on the bus"
        )
        self.system_log(
            f"[WARN] logic address conflict: addr=0x{logic_addr:02X} "
            f"local={device_type_name(local_type)}(node=0x{local_node_id:02X}) "
            f"remote={device_type_name(remote_type)}(node=0x{remote_node_id:02X}) "
            f"{risk_text}"
        )

    def inspect_bus_frame(self, frame):
        cob_id = CANopenNodeSimulator._effective_cob_id(frame["id"], frame.get("is_ext", False))
        if decode_address_claim_cob_id(cob_id) is None:
            return

        payload = decode_address_claim(frame["data"])
        logic_addr = payload["logic_addr"]
        remote_type = payload["device_type"] & 0xFF
        remote_node_id = payload["source_node"] & 0x7F
        if logic_addr == 0 or remote_node_id == 0:
            return

        self.system_log(
            f"[BUS] addr-claim type={device_type_name(remote_type)} "
            f"node=0x{remote_node_id:02X} logic=0x{logic_addr:02X} "
            f"publish=0x{payload['publish_cob_id']:03X}"
        )
        for node in list(self.nodes.values()):
            if node.node_id == remote_node_id:
                continue
            if node.logic_addr != logic_addr:
                continue
            local_type = node.device_type_code & 0xFF
            if logic_addr_share_allowed(local_type, remote_type):
                continue
            self._report_logic_addr_conflict(node.node_id, local_type, logic_addr, remote_node_id, remote_type)

    def open_bus_from_ui(self):
        try:
            channel = int(self.channel_var.get().strip(), 0)
        except Exception:
            messagebox.showerror("Channel Error", "Channel must be a valid integer, e.g. 0 or 1.")
            return
        self.open_bus(channel)

    def open_bus(self, channel):
        if self.handle is not None:
            if self.channel != channel:
                self.system_log(f"[APP] bus already open on channel {self.channel}, restart app to switch channel")
            else:
                self.system_log(f"[APP] bus already open on channel {self.channel}")
            return

        self.handle = init_device(logger=self.system_log)
        init_can(self.handle, channel, logger=self.system_log)
        self.channel = channel
        self.receiver_running = True
        self.receiver_thread = threading.Thread(target=self.receiver_loop, daemon=True)
        self.receiver_thread.start()
        self.system_log(f"[APP] shared USB-CAN ready on {channel_name(channel)} (index={channel})")

    def open_gateway_window(self):
        self.gateway.open_main_window()

    def _make_synthetic_frame(self, cob_id, data):
        return {
            "id": cob_id,
            "len": len(data),
            "data": list(data[:8]),
            "is_ext": False,
            "is_error": False,
            "is_remote": False,
            "is_tx": True,
            "sdk_channel": self.channel if self.channel is not None else 0,
            "raw_remote_flag": 0x80,
            "raw_extern_flag": 0x00,
            "timestamp": 0,
            "synthetic": True,
        }

    def _route_frame_to_local_nodes(self, frame, exclude_node=None):
        current_nodes = list(self.nodes.values())
        for node in current_nodes:
            if node is exclude_node:
                continue
            if node.running:
                node.process_frame(dict(frame))

    def send_gateway_frame(self, cob_id, data, desc="", logger=None):
        if self.handle is None:
            channel = safe_int(self.channel_var.get(), 1)
            self.open_bus(channel)
        if self.handle is None:
            raise RuntimeError("shared USB-CAN is not open")

        with self.tx_lock:
            ok = send_frame(self.handle, self.channel, cob_id, data)
        if not ok:
            raise RuntimeError(f"CAN send failed: id=0x{cob_id:03X}")

        if logger is not None:
            suffix = f"  {desc}" if desc else ""
            logger(f"[TX] id=0x{cob_id:03X}  data=[{' '.join(f'{b:02X}' for b in data)}]{suffix}")

        frame = self._make_synthetic_frame(cob_id, data)
        self._route_frame_to_local_nodes(frame)

    def print_bus_status(self):
        if self.handle is None:
            self.system_log("[APP] bus is not open yet")
            return
        dump_can_status(self.handle, self.channel, prefix="[APP]", logger=self.system_log)
        self.system_log(f"[APP] active nodes: {', '.join(self.nodes.keys()) if self.nodes else '(none)'}")

    def receiver_loop(self):
        while self.receiver_running:
            try:
                frames = recv_frames(self.handle, self.channel)
                if not frames:
                    sleep(0.01)
                    continue
                current_nodes = list(self.nodes.values())
                for frame in frames:
                    if frame.get("is_tx"):
                        continue
                    self.gateway.handle_frame(frame)
                    self.inspect_bus_frame(frame)
                    for node in current_nodes:
                        if node.running:
                            node.process_frame(dict(frame))
            except Exception as exc:
                self.system_log(f"[ERR] shared receiver loop: {exc}")
                sleep(0.05)

    def dispatch_local_frame(self, sender, frame):
        self.gateway.handle_frame(frame)
        self.inspect_bus_frame(frame)
        self._route_frame_to_local_nodes(frame, exclude_node=sender)

    def add_default_node(self, kind):
        self.add_node_from_form(kind)

    def _make_node(self, kind, node_id, logic_addr, node_key):
        common_kwargs = {
            "handle": self.handle,
            "channel": self.channel,
            "node_id": node_id,
            "logic_addr": logic_addr,
            "verbose_rx": False,
            "hide_heartbeat_log": True,
            "logger": self.node_logger(node_key),
            "shared_tx_lock": self.tx_lock,
            "local_dispatch": self.dispatch_local_frame,
            "auto_operational": True,
        }
        if kind == "lighting":
            return LightingModuleSimulator(**common_kwargs)
        if kind == "panel":
            return PanelModuleSimulator(**common_kwargs)
        if kind == "scene":
            return ScenePanelModuleSimulator(**common_kwargs)
        if kind == "radar":
            return RadarSceneSensorSimulator(**common_kwargs)
        raise ValueError(f"unsupported node kind: {kind}")

    def add_node(self, kind, logic_addr):
        if self.handle is None:
            try:
                channel = int(self.channel_var.get().strip(), 0)
            except Exception:
                self.system_log("[APP] invalid channel value")
                return
            self.open_bus(channel)
        if self.handle is None:
            return

        node_id = self.allocate_node_id(kind)
        logic_addr &= 0xFF
        node_key = f"0x{node_id:02X}"
        if node_key in self.nodes:
            self.system_log(f"[APP] node {node_key} already exists")
            return

        node = self._make_node(kind, node_id, logic_addr, node_key)
        title = f"{NODE_KIND_CONFIG[kind]['title']} node={node_key} logic=0x{logic_addr:02X}"
        window = NodeConsoleWindow(self, node_key, node, title, kind)
        self.nodes[node_key] = node
        self.windows[node_key] = window
        self.register_log_sink(node_key, window)
        node.start_threads(with_receiver=False, with_timer=True)
        node.print_menu()
        if kind in self.launch_addr_vars:
            self.launch_addr_vars[kind].set("")
        self.system_log(f"[APP] added {title} (node id auto-assigned)")

    def remove_node(self, node_key):
        node = self.nodes.pop(node_key, None)
        window = self.windows.pop(node_key, None)
        if node is None:
            return
        node.stop()
        self.gateway.forget_node(node.node_id)
        self.unregister_log_sink(node_key)
        if window is not None:
            window.destroy()
        self.system_log(f"[APP] removed node {node_key}")

    def list_nodes(self):
        if not self.nodes:
            self.system_log("[APP] no active nodes")
            return
        for node_key, node in self.nodes.items():
            self.system_log(
                f"[APP] {node_key}: product={node.product_name}, logic_addr=0x{node.logic_addr:02X}, heartbeat={node._heartbeat_summary()}"
            )

    def execute_main_command(self, cmd):
        parts = cmd.split()
        if not parts:
            return

        try:
            if parts[0] == "usb" and len(parts) == 2:
                self.open_bus(int(parts[1], 0))
                return
            if parts[0] in ("gateway", "gw"):
                self.open_gateway_window()
                return
            if parts[0] == "add" and len(parts) >= 2:
                kind = parts[1].lower()
                if kind not in NODE_KIND_CONFIG:
                    self.system_log(f"[APP] unknown node kind: {kind}")
                    return
                if len(parts) < 3:
                    self.system_log("[APP] usage: add <lighting|panel|scene|radar> <logic_addr>")
                    return
                logic_addr = self._parse_logic_addr(parts[2])
                self.add_node(kind, logic_addr)
                return
            if parts[0] == "remove" and len(parts) == 2:
                node_key = f"0x{int(parts[1], 0) & 0x7F:02X}"
                self.remove_node(node_key)
                return
            if parts[0] == "list":
                self.list_nodes()
                return
            if parts[0] == "status":
                self.print_bus_status()
                return
            if parts[0] == "help":
                self._print_help()
                return
        except Exception as exc:
            self.system_log(f"[APP] command error: {exc}")
            return

        self.system_log("[APP] unknown command")

    def shutdown(self):
        if messagebox.askokcancel("Quit", "Close the shared USB-CAN demo and all node windows?"):
            if self.gateway.main_window is not None and not self.gateway.main_window.closed:
                self.gateway.main_window.close()
            for node_id in list(self.gateway.device_windows.keys()):
                window = self.gateway.device_windows.pop(node_id, None)
                if window is not None and not window.closed:
                    window.close()
            for node_key in list(self.nodes.keys()):
                self.remove_node(node_key)
            self.receiver_running = False
            close_device(self.handle, self.channel if self.channel is not None else 0, logger=self.system_log)
            self.handle = None
            self.root.destroy()

    def run(self):
        self.root.mainloop()


def main():
    app = MultiNodeDemoApp()
    app.run()


if __name__ == "__main__":
    main()
