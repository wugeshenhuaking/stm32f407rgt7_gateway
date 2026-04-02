# -*- coding: utf-8 -*-
"""
radar_scene_sensor.py

Radar scene sensor simulator:
1. Unique node_id for gateway management.
2. When occupancy changes, it publishes the same scene command frame used by the
   scene panel, so lighting modules can act without polling.
"""

from building_can_def import *
from canopen_sim_core import *


class RadarSceneSensorSimulator(CANopenNodeSimulator):
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
        self.detect_scene = 1
        self.clear_scene = 1
        self.detect_action = SCENE_ACTION_ON
        self.clear_action = SCENE_ACTION_OFF
        self.occupied = 0
        self.source_flags = 0x00
        super().__init__(
            handle=handle,
            channel=channel,
            node_id=node_id,
            logic_addr=logic_addr,
            device_type_code=DEVICE_TYPE_RADAR,
            product_name="RadarSceneSensorPC",
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
            (0x2101, 0x00): ODEntry(self.detect_scene, 2, "rw"),
            (0x2102, 0x00): ODEntry(self.clear_scene, 2, "rw"),
            (0x2103, 0x00): ODEntry(self.detect_action, 2, "rw"),
            (0x2104, 0x00): ODEntry(self.clear_action, 2, "rw"),
            (0x2105, 0x00): ODEntry(self.occupied, 1, "ro"),
            (0x2106, 0x00): ODEntry(self.source_flags, 1, "rw"),
        }

    def refresh_dynamic_od(self):
        super().refresh_dynamic_od()
        self.od[(0x2100, 0x00)].value = self.target_addr
        self.od[(0x2101, 0x00)].value = self.detect_scene
        self.od[(0x2102, 0x00)].value = self.clear_scene
        self.od[(0x2103, 0x00)].value = self.detect_action
        self.od[(0x2104, 0x00)].value = self.clear_action
        self.od[(0x2105, 0x00)].value = self.occupied
        self.od[(0x2106, 0x00)].value = self.source_flags

    def handle_device_write_od(self, index, subindex, raw_value, size, entry):
        if index == 0x2100 and subindex == 0x00:
            self.target_addr = raw_value & 0xFF
            entry.value = self.target_addr
            return 0
        if index == 0x2101 and subindex == 0x00:
            self.detect_scene = raw_value & 0xFFFF
            entry.value = self.detect_scene
            return 0
        if index == 0x2102 and subindex == 0x00:
            self.clear_scene = raw_value & 0xFFFF
            entry.value = self.clear_scene
            return 0
        if index == 0x2103 and subindex == 0x00:
            self.detect_action = raw_value & 0xFFFF
            entry.value = self.detect_action
            return 0
        if index == 0x2104 and subindex == 0x00:
            self.clear_action = raw_value & 0xFFFF
            entry.value = self.clear_action
            return 0
        if index == 0x2106 and subindex == 0x00:
            self.source_flags = raw_value & 0xFF
            entry.value = self.source_flags
            return 0
        entry.value = raw_value
        return 0

    def _send_scene(self, scene_index, action):
        data = encode_scene_command(
            target_addr=self.target_addr,
            source_type=SCENE_SOURCE_RADAR,
            scene_index=scene_index,
            action=action,
            source_node=self.node_id,
            flags=self.source_flags,
        )
        self._send(
            APP_SCENE_COMMAND_COB_ID,
            data,
            f"radar scene target=0x{self.target_addr:02X} scene={scene_index} action={scene_action_name(action)} occupied={self.occupied}",
        )

    def detect(self):
        self.occupied = 1
        self.refresh_dynamic_od()
        self._send_scene(self.detect_scene, self.detect_action)

    def clear(self):
        self.occupied = 0
        self.refresh_dynamic_od()
        self._send_scene(self.clear_scene, self.clear_action)

    def menu_lines(self):
        return [
            "  detect             publish occupancy-detected scene command",
            "  clear              publish occupancy-clear scene command",
            "  logic <addr>       set local logic address/group",
            "  target <addr>      set target logic address, 0 means broadcast",
            "  detect scene <n>   set scene number for detect",
            "  clear scene <n>    set scene number for clear",
            "  detect action <n>  set action for detect, e.g. 1 on, 2 off",
            "  clear action <n>   set action for clear",
        ]

    def handle_command(self, cmd):
        parts = cmd.split()
        if not parts:
            return True

        if parts[0] == "detect" and len(parts) == 1:
            self.detect()
            return True
        if parts[0] == "clear" and len(parts) == 1:
            self.clear()
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
        if len(parts) == 3 and parts[0] == "detect" and parts[1] == "scene":
            self.detect_scene = int(parts[2], 0) & 0xFFFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] detect_scene={self.detect_scene}")
            return True
        if len(parts) == 3 and parts[0] == "clear" and parts[1] == "scene":
            self.clear_scene = int(parts[2], 0) & 0xFFFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] clear_scene={self.clear_scene}")
            return True
        if len(parts) == 3 and parts[0] == "detect" and parts[1] == "action":
            self.detect_action = int(parts[2], 0) & 0xFFFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] detect_action=0x{self.detect_action:04X}")
            return True
        if len(parts) == 3 and parts[0] == "clear" and parts[1] == "action":
            self.clear_action = int(parts[2], 0) & 0xFFFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] clear_action=0x{self.clear_action:04X}")
            return True
        return False


def main():
    parser = build_common_arg_parser(
        description="Radar scene sensor simulator based on USB2XXX SDK",
        env_prefix="RADAR_SCENE",
        default_channel=0,
        default_node_id=0x21,
        default_logic_addr=0x00,
    )
    args = parser.parse_args()

    handle = None
    try:
        handle = init_device()
        init_can(handle, args.channel)
        print(
            f"[APP] radar scene simulator node=0x{args.node_id:02X} "
            f"logic_addr=0x{args.logic_addr:02X} "
            f"usbcan={channel_name(args.channel)}(index={args.channel}) "
            f"verbose_rx={'on' if args.verbose_rx else 'off'} "
            f"heartbeat_log={'hidden' if args.hide_heartbeat_log else 'shown'}"
        )
        sim = RadarSceneSensorSimulator(
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
