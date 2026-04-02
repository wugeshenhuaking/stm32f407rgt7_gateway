# -*- coding: utf-8 -*-
"""
lighting_module.py

Lighting module simulator for the custom CAN scheme:
1. Gateway manages it through unique node_id (NMT + SDO-like access).
2. Business binding uses logic_addr, which can be shared with a normal panel.
3. Relay state changes are published actively on the bus.
4. Scene broadcast frames from scene panels or radar sensors are executed here.
"""

from building_can_def import *
from canopen_sim_core import *


class LightingModuleSimulator(CANopenNodeSimulator):
    def __init__(
        self,
        handle,
        channel,
        node_id,
        logic_addr,
        verbose_rx=False,
        hide_heartbeat_log=True,
        logger=None,
        shared_tx_lock=None,
        local_dispatch=None,
        auto_operational=False,
    ):
        self.publish_enabled = True
        self.auto_key_link = True
        self.relay_state = 0x0000
        self.last_change_mask = 0x0000
        self.last_reason = LIGHT_REASON_MANUAL
        self.last_source_node = 0x00
        self.scene_on_masks = {
            1: 0x5A5A,
            2: 0x0003,
            3: 0x00FF,
            4: 0xFFFF,
        }
        self.scene_off_masks = {
            1: 0x0000,
            2: 0x0000,
            3: 0x0000,
            4: 0x0000,
        }
        super().__init__(
            handle=handle,
            channel=channel,
            node_id=node_id,
            logic_addr=logic_addr,
            device_type_code=DEVICE_TYPE_LIGHTING,
            product_name="LightingModulePC",
            verbose_rx=verbose_rx,
            hide_heartbeat_log=hide_heartbeat_log,
            logger=logger,
            shared_tx_lock=shared_tx_lock,
            local_dispatch=local_dispatch,
            auto_operational=auto_operational,
        )

    def build_device_od(self):
        return {
            (0x2100, 0x00): ODEntry(self.relay_state, 2, "ro"),
            (0x2101, 0x00): ODEntry(self.last_change_mask, 2, "ro"),
            (0x2102, 0x00): ODEntry(self.last_reason, 1, "ro"),
            (0x2103, 0x00): ODEntry(self.last_source_node, 1, "ro"),
            (0x2200, 0x00): ODEntry(1, 1, "rw"),
            (0x2201, 0x00): ODEntry(1, 1, "rw"),
            (0x2400, 0x00): ODEntry(self.relay_state, 2, "rw"),
            (0x2300, 0x00): ODEntry(self.scene_on_masks[1], 2, "rw"),
            (0x2301, 0x00): ODEntry(self.scene_off_masks[1], 2, "rw"),
            (0x2302, 0x00): ODEntry(self.scene_on_masks[2], 2, "rw"),
            (0x2303, 0x00): ODEntry(self.scene_off_masks[2], 2, "rw"),
            (0x2304, 0x00): ODEntry(self.scene_on_masks[3], 2, "rw"),
            (0x2305, 0x00): ODEntry(self.scene_off_masks[3], 2, "rw"),
            (0x2306, 0x00): ODEntry(self.scene_on_masks[4], 2, "rw"),
            (0x2307, 0x00): ODEntry(self.scene_off_masks[4], 2, "rw"),
        }

    def refresh_dynamic_od(self):
        super().refresh_dynamic_od()
        self.od[(0x2100, 0x00)].value = self.relay_state
        self.od[(0x2101, 0x00)].value = self.last_change_mask
        self.od[(0x2102, 0x00)].value = self.last_reason
        self.od[(0x2103, 0x00)].value = self.last_source_node
        self.od[(0x2200, 0x00)].value = 1 if self.publish_enabled else 0
        self.od[(0x2201, 0x00)].value = 1 if self.auto_key_link else 0
        self.od[(0x2400, 0x00)].value = self.relay_state
        self.od[(0x2300, 0x00)].value = self.scene_on_masks.get(1, 0)
        self.od[(0x2301, 0x00)].value = self.scene_off_masks.get(1, 0)
        self.od[(0x2302, 0x00)].value = self.scene_on_masks.get(2, 0)
        self.od[(0x2303, 0x00)].value = self.scene_off_masks.get(2, 0)
        self.od[(0x2304, 0x00)].value = self.scene_on_masks.get(3, 0)
        self.od[(0x2305, 0x00)].value = self.scene_off_masks.get(3, 0)
        self.od[(0x2306, 0x00)].value = self.scene_on_masks.get(4, 0)
        self.od[(0x2307, 0x00)].value = self.scene_off_masks.get(4, 0)

    def handle_device_write_od(self, index, subindex, raw_value, size, entry):
        if index == 0x2200 and subindex == 0x00:
            self.publish_enabled = bool(raw_value & 0x01)
            entry.value = 1 if self.publish_enabled else 0
            return 0
        if index == 0x2201 and subindex == 0x00:
            self.auto_key_link = bool(raw_value & 0x01)
            entry.value = 1 if self.auto_key_link else 0
            return 0
        if index == 0x2400 and subindex == 0x00:
            self.apply_relay_state(raw_value, LIGHT_REASON_GATEWAY, self.node_id)
            entry.value = self.relay_state
            return 0
        if 0x2300 <= index <= 0x2307 and subindex == 0x00:
            scene_index = ((index - 0x2300) // 2) + 1
            if (index - 0x2300) % 2 == 0:
                self.scene_on_masks[scene_index] = raw_value & 0xFFFF
            else:
                self.scene_off_masks[scene_index] = raw_value & 0xFFFF
            entry.value = raw_value & 0xFFFF
            return 0
        entry.value = raw_value
        return 0

    def _state_summary(self):
        return f"0x{self.relay_state:04X}"

    def _send_light_status(self):
        if not self.publish_enabled:
            return
        if self.nmt_state != CO_NMT_OPERATIONAL:
            self.emit("[INFO] relay state changed, but node is not operational, publish suppressed")
            return

        self.sequence = (self.sequence + 1) & 0xFF
        data = encode_light_status(
            self.logic_addr,
            self.relay_state,
            self.last_change_mask,
            self.last_reason,
            self.node_id,
            self.sequence,
        )
        self._send(
            APP_LIGHT_STATUS_COB_ID,
            data,
            f"light state addr=0x{self.logic_addr:02X} state=0x{self.relay_state:04X} reason={light_reason_name(self.last_reason)}",
        )

    def apply_relay_state(self, new_state, reason, source_node=0):
        new_state &= 0xFFFF
        change_mask = (self.relay_state ^ new_state) & 0xFFFF
        if change_mask == 0:
            self.last_reason = reason
            self.last_source_node = source_node & 0xFF
            return

        self.relay_state = new_state
        self.last_change_mask = change_mask
        self.last_reason = reason
        self.last_source_node = source_node & 0xFF
        self.refresh_dynamic_od()
        self.emit(
            f"[APP] relay state changed -> 0x{self.relay_state:04X}, "
            f"change_mask=0x{self.last_change_mask:04X}, reason={light_reason_name(reason)}"
        )
        self._send_light_status()

    def _set_channel(self, channel_index, on):
        if not 1 <= channel_index <= 16:
            self.emit("[APP] channel index must be 1..16")
            return
        mask = 1 << (channel_index - 1)
        if on:
            self.apply_relay_state(self.relay_state | mask, LIGHT_REASON_MANUAL, self.node_id)
        else:
            self.apply_relay_state(self.relay_state & (~mask & 0xFFFF), LIGHT_REASON_MANUAL, self.node_id)

    def _toggle_channel(self, channel_index, reason=LIGHT_REASON_MANUAL, source_node=0):
        if not 1 <= channel_index <= 16:
            self.emit("[APP] channel index must be 1..16")
            return
        mask = 1 << (channel_index - 1)
        self.apply_relay_state(self.relay_state ^ mask, reason, source_node)

    def _scene_masks(self, scene_index):
        on_mask = self.scene_on_masks.get(scene_index, 0x0000) & 0xFFFF
        off_mask = self.scene_off_masks.get(scene_index, 0x0000) & 0xFFFF
        return on_mask, off_mask

    def _apply_scene(self, scene_index, action, source_node=0):
        on_mask, off_mask = self._scene_masks(scene_index)
        control_mask = (on_mask | off_mask) & 0xFFFF
        if action == SCENE_ACTION_ON:
            if control_mask == 0:
                self.emit(f"[APP] no binding for scene {scene_index}")
                return
            target_state = (self.relay_state | on_mask) & (~off_mask & 0xFFFF)
        elif action == SCENE_ACTION_OFF:
            if control_mask == 0:
                self.emit(f"[APP] no OFF binding for scene {scene_index}")
                return
            target_state = self.relay_state & (~control_mask & 0xFFFF)
        elif action == SCENE_ACTION_TOGGLE:
            if control_mask == 0:
                self.emit(f"[APP] no TOGGLE binding for scene {scene_index}")
                return
            is_active = ((self.relay_state & on_mask) == on_mask) and ((self.relay_state & off_mask) == 0)
            if is_active:
                target_state = self.relay_state & (~control_mask & 0xFFFF)
            else:
                target_state = (self.relay_state | on_mask) & (~off_mask & 0xFFFF)
        else:
            self.emit(f"[APP] unsupported scene action 0x{action:04X}")
            return
        self.apply_relay_state(target_state, LIGHT_REASON_SCENE, source_node)

    def _handle_panel_key(self, data):
        payload = decode_panel_key(data)
        if payload["logic_addr"] != self.logic_addr:
            return True

        self.log_rx(
            APP_PANEL_KEY_COB_ID,
            data,
            f"panel key addr=0x{payload['logic_addr']:02X} key={payload['key_code']} event={key_event_name(payload['key_event'])}",
        )

        if self.auto_key_link and payload["key_event"] == KEY_EVENT_PRESS and 1 <= payload["key_code"] <= 16:
            self._toggle_channel(payload["key_code"], LIGHT_REASON_PANEL_KEY, payload["source_node"])
        return True

    def _handle_scene_command(self, data):
        payload = decode_scene_command(data)
        if payload["target_addr"] not in (LOGIC_ADDR_BROADCAST, self.logic_addr):
            return True

        self.log_rx(
            APP_SCENE_COMMAND_COB_ID,
            data,
            f"scene target=0x{payload['target_addr']:02X} scene={payload['scene_index']} action={scene_action_name(payload['action'])} src={source_type_name(payload['source_type'])}",
        )
        self._apply_scene(payload["scene_index"], payload["action"], payload["source_node"])
        return True

    def handle_app_frame(self, cob_id, data):
        if cob_id == APP_PANEL_KEY_COB_ID:
            return self._handle_panel_key(data)
        if cob_id == APP_SCENE_COMMAND_COB_ID:
            return self._handle_scene_command(data)
        return False

    def menu_lines(self):
        return [
            "  on <1..16>   switch one relay on",
            "  off <1..16>  switch one relay off",
            "  toggle <n>   toggle one relay",
            "  state <hex>  set all 16 relay bits directly, e.g. state 0x00FF",
            "  send         publish current relay state once",
            "  logic <addr> set logic address/group",
            "  publish on/off",
            "  keylink on/off",
            "  scene on <scene> <mask>",
            "  scene off <scene> <mask>",
        ]

    def handle_command(self, cmd):
        parts = cmd.split()
        if not parts:
            return True

        if parts[0] == "on" and len(parts) == 2:
            self._set_channel(int(parts[1], 0), True)
            return True
        if parts[0] == "off" and len(parts) == 2:
            self._set_channel(int(parts[1], 0), False)
            return True
        if parts[0] == "toggle" and len(parts) == 2:
            self._toggle_channel(int(parts[1], 0))
            return True
        if parts[0] == "state" and len(parts) == 2:
            self.apply_relay_state(int(parts[1], 0), LIGHT_REASON_MANUAL, self.node_id)
            return True
        if parts[0] == "send" and len(parts) == 1:
            self._send_light_status()
            return True
        if parts[0] == "logic" and len(parts) == 2:
            self.set_logic_addr(int(parts[1], 0), announce=True)
            self.emit(f"[APP] logic_addr=0x{self.logic_addr:02X}")
            return True
        if parts[0] == "publish" and len(parts) == 2:
            self.publish_enabled = (parts[1].lower() == "on")
            self.refresh_dynamic_od()
            self.emit(f"[APP] publish={'on' if self.publish_enabled else 'off'}")
            return True
        if parts[0] == "keylink" and len(parts) == 2:
            self.auto_key_link = (parts[1].lower() == "on")
            self.refresh_dynamic_od()
            self.emit(f"[APP] keylink={'on' if self.auto_key_link else 'off'}")
            return True
        if len(parts) == 4 and parts[0] == "scene" and parts[1] in ("on", "off"):
            scene_index = int(parts[2], 0)
            mask = int(parts[3], 0) & 0xFFFF
            if parts[1] == "on":
                self.scene_on_masks[scene_index] = mask
            else:
                self.scene_off_masks[scene_index] = mask
            self.refresh_dynamic_od()
            self.emit(f"[APP] scene {scene_index} {parts[1]} mask=0x{mask:04X}")
            return True
        return False


def main():
    parser = build_common_arg_parser(
        description="Lighting module simulator based on USB2XXX SDK",
        env_prefix="LIGHTING",
        default_channel=0,
        default_node_id=0x11,
        default_logic_addr=0x01,
    )
    args = parser.parse_args()

    handle = None
    try:
        handle = init_device()
        init_can(handle, args.channel)
        print(
            f"[APP] lighting simulator node=0x{args.node_id:02X} "
            f"logic_addr=0x{args.logic_addr:02X} "
            f"usbcan={channel_name(args.channel)}(index={args.channel}) "
            f"verbose_rx={'on' if args.verbose_rx else 'off'} "
            f"heartbeat_log={'hidden' if args.hide_heartbeat_log else 'shown'}"
        )
        sim = LightingModuleSimulator(
            handle=handle,
            channel=args.channel,
            node_id=args.node_id,
            logic_addr=args.logic_addr,
            verbose_rx=args.verbose_rx,
            hide_heartbeat_log=args.hide_heartbeat_log,
        )
        sim.run()
    except KeyboardInterrupt:
        print("\n[APP] interrupted")
    except Exception as exc:
        print(f"[APP] exception: {exc}")
    finally:
        close_device(handle, args.channel)


if __name__ == "__main__":
    main()
