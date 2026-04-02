# -*- coding: utf-8 -*-
"""
scene_panel_module.py

Scene panel simulator:
1. Has its own unique node_id for gateway management.
2. Actively publishes scene execution broadcasts when a key is pressed.
3. Lighting modules decide whether to act according to their own scene table.
"""

from building_can_def import *
from canopen_sim_core import *


class ScenePanelModuleSimulator(CANopenNodeSimulator):
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
        self.target_addr = LOGIC_ADDR_BROADCAST
        self.last_scene_index = 1
        self.last_action = SCENE_ACTION_ON
        self.source_flags = 0x00
        self.scene_active = {index: False for index in range(1, 13)}
        super().__init__(
            handle=handle,
            channel=channel,
            node_id=node_id,
            logic_addr=logic_addr,
            device_type_code=DEVICE_TYPE_SCENE_PANEL,
            product_name="ScenePanelPC",
            verbose_rx=verbose_rx,
            hide_heartbeat_log=hide_heartbeat_log,
            logger=logger,
            shared_tx_lock=shared_tx_lock,
            local_dispatch=local_dispatch,
            auto_operational=auto_operational,
        )

    def build_device_od(self):
        return {
            (0x2100, 0x00): ODEntry(self.target_addr, 1, "rw"),
            (0x2101, 0x00): ODEntry(self.last_scene_index, 2, "rw"),
            (0x2102, 0x00): ODEntry(self.last_action, 2, "rw"),
            (0x2103, 0x00): ODEntry(self.source_flags, 1, "rw"),
        }

    def refresh_dynamic_od(self):
        super().refresh_dynamic_od()
        self.od[(0x2100, 0x00)].value = self.target_addr
        self.od[(0x2101, 0x00)].value = self.last_scene_index
        self.od[(0x2102, 0x00)].value = self.last_action
        self.od[(0x2103, 0x00)].value = self.source_flags

    def handle_device_write_od(self, index, subindex, raw_value, size, entry):
        if index == 0x2100 and subindex == 0x00:
            self.target_addr = raw_value & 0xFF
            entry.value = self.target_addr
            return 0
        if index == 0x2101 and subindex == 0x00:
            self.last_scene_index = raw_value & 0xFFFF
            entry.value = self.last_scene_index
            return 0
        if index == 0x2102 and subindex == 0x00:
            self.last_action = raw_value & 0xFFFF
            entry.value = self.last_action
            return 0
        if index == 0x2103 and subindex == 0x00:
            self.source_flags = raw_value & 0xFF
            entry.value = self.source_flags
            return 0
        entry.value = raw_value
        return 0

    def _set_scene_state(self, scene_index, action):
        if scene_index <= 0:
            return
        current = bool(self.scene_active.get(scene_index, False))
        if action == SCENE_ACTION_ON:
            current = True
        elif action == SCENE_ACTION_OFF:
            current = False
        elif action == SCENE_ACTION_TOGGLE:
            current = not current
        self.scene_active[scene_index] = current

    def send_scene_command(self, scene_index, action):
        self.last_scene_index = scene_index & 0xFFFF
        self.last_action = action & 0xFFFF
        self._set_scene_state(self.last_scene_index, self.last_action)
        self.refresh_dynamic_od()

        data = encode_scene_command(
            target_addr=self.target_addr,
            source_type=SCENE_SOURCE_PANEL,
            scene_index=self.last_scene_index,
            action=self.last_action,
            source_node=self.node_id,
            flags=self.source_flags,
        )
        self._send(
            APP_SCENE_COMMAND_COB_ID,
            data,
            f"scene target=0x{self.target_addr:02X} scene={self.last_scene_index} action={scene_action_name(self.last_action)}",
        )

    def handle_app_frame(self, cob_id, data):
        if cob_id != APP_SCENE_COMMAND_COB_ID:
            return False

        payload = decode_scene_command(data)
        if payload["target_addr"] not in (LOGIC_ADDR_BROADCAST, self.target_addr):
            return True

        self.log_rx(
            APP_SCENE_COMMAND_COB_ID,
            data,
            f"scene target=0x{payload['target_addr']:02X} scene={payload['scene_index']} "
            f"action={scene_action_name(payload['action'])} src={source_type_name(payload['source_type'])}",
        )
        self.last_scene_index = payload["scene_index"]
        self.last_action = payload["action"]
        self._set_scene_state(payload["scene_index"], payload["action"])
        self.refresh_dynamic_od()
        return True

    def menu_lines(self):
        return [
            "  scene on <n>     publish one scene-on command",
            "  scene off <n>    publish one scene-off command",
            "  scene toggle <n> publish one scene-toggle command",
            "  logic <addr>     set local logic address/group",
            "  target <addr>    set target logic address, 0 means broadcast",
            "  flags <hex>      set source flag byte",
        ]

    def handle_command(self, cmd):
        parts = cmd.split()
        if not parts:
            return True

        if len(parts) == 3 and parts[0] == "scene":
            scene_index = int(parts[2], 0)
            if parts[1] == "on":
                self.send_scene_command(scene_index, SCENE_ACTION_ON)
                return True
            if parts[1] == "off":
                self.send_scene_command(scene_index, SCENE_ACTION_OFF)
                return True
            if parts[1] == "toggle":
                self.send_scene_command(scene_index, SCENE_ACTION_TOGGLE)
                return True
        if parts[0] == "target" and len(parts) == 2:
            self.target_addr = int(parts[1], 0) & 0xFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] target_addr=0x{self.target_addr:02X}")
            return True
        if parts[0] == "logic" and len(parts) == 2:
            self.set_logic_addr(int(parts[1], 0), announce=True)
            self.emit(f"[APP] logic_addr=0x{self.logic_addr:02X}")
            return True
        if parts[0] == "flags" and len(parts) == 2:
            self.source_flags = int(parts[1], 0) & 0xFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] flags=0x{self.source_flags:02X}")
            return True
        return False


def main():
    parser = build_common_arg_parser(
        description="Scene panel simulator based on USB2XXX SDK",
        env_prefix="SCENE_PANEL",
        default_channel=0,
        default_node_id=0x41,
        default_logic_addr=0x21,
    )
    args = parser.parse_args()

    handle = None
    try:
        handle = init_device()
        init_can(handle, args.channel)
        print(
            f"[APP] scene panel simulator node=0x{args.node_id:02X} "
            f"logic_addr=0x{args.logic_addr:02X} "
            f"usbcan={channel_name(args.channel)}(index={args.channel}) "
            f"verbose_rx={'on' if args.verbose_rx else 'off'} "
            f"heartbeat_log={'hidden' if args.hide_heartbeat_log else 'shown'}"
        )
        sim = ScenePanelModuleSimulator(
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
