# -*- coding: utf-8 -*-
"""
panel_module.py

Normal panel simulator:
1. Shares logic_addr with the lighting module it belongs to.
2. Receives lighting state publish frames and updates local application state.
3. Actively publishes key events when the user presses a panel key.
"""

from building_can_def import *
from canopen_sim_core import *


class PanelModuleSimulator(CANopenNodeSimulator):
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
        self.last_light_state = 0x0000
        self.last_change_mask = 0x0000
        self.last_light_source = 0x00
        self.last_reason = 0x00
        self.last_key_code = 0x00
        self.last_key_event = 0x00
        self.key_value = 0x01
        super().__init__(
            handle=handle,
            channel=channel,
            node_id=node_id,
            logic_addr=logic_addr,
            device_type_code=DEVICE_TYPE_PANEL,
            product_name="PanelModulePC",
            verbose_rx=verbose_rx,
            hide_heartbeat_log=hide_heartbeat_log,
            logger=logger,
            shared_tx_lock=shared_tx_lock,
            local_dispatch=local_dispatch,
            auto_operational=auto_operational,
        )

    def build_device_od(self):
        return {
            (0x2100, 0x00): ODEntry(self.last_light_state, 2, "ro"),
            (0x2101, 0x00): ODEntry(self.last_change_mask, 2, "ro"),
            (0x2102, 0x00): ODEntry(self.last_reason, 1, "ro"),
            (0x2103, 0x00): ODEntry(self.last_light_source, 1, "ro"),
            (0x2200, 0x00): ODEntry(self.last_key_code, 1, "ro"),
            (0x2201, 0x00): ODEntry(self.last_key_event, 1, "ro"),
            (0x2202, 0x00): ODEntry(self.key_value, 1, "rw"),
        }

    def refresh_dynamic_od(self):
        super().refresh_dynamic_od()
        self.od[(0x2100, 0x00)].value = self.last_light_state
        self.od[(0x2101, 0x00)].value = self.last_change_mask
        self.od[(0x2102, 0x00)].value = self.last_reason
        self.od[(0x2103, 0x00)].value = self.last_light_source
        self.od[(0x2200, 0x00)].value = self.last_key_code
        self.od[(0x2201, 0x00)].value = self.last_key_event
        self.od[(0x2202, 0x00)].value = self.key_value

    def handle_device_write_od(self, index, subindex, raw_value, size, entry):
        if index == 0x2202 and subindex == 0x00:
            self.key_value = raw_value & 0xFF
            entry.value = self.key_value
            return 0
        entry.value = raw_value
        return 0

    def _update_light_state(self, payload):
        self.last_light_state = payload["state_word"] & 0xFFFF
        self.last_change_mask = payload["change_mask"] & 0xFFFF
        self.last_light_source = payload["source_node"] & 0xFF
        self.last_reason = payload["reason"] & 0xFF
        self.refresh_dynamic_od()
        self.emit(
            f"[APP] panel linked state -> addr=0x{self.logic_addr:02X}, "
            f"state=0x{self.last_light_state:04X}, change=0x{self.last_change_mask:04X}, "
            f"reason={light_reason_name(self.last_reason)}"
        )

    def _send_key(self, key_code, key_event):
        self.last_key_code = key_code & 0xFF
        self.last_key_event = key_event & 0xFF
        self.refresh_dynamic_od()

        data = encode_panel_key(
            self.logic_addr,
            key_code,
            key_event,
            self.key_value,
            self.node_id,
            0x00,
        )
        self._send(
            APP_PANEL_KEY_COB_ID,
            data,
            f"panel key addr=0x{self.logic_addr:02X} key={key_code} event={key_event_name(key_event)}",
        )

    def handle_app_frame(self, cob_id, data):
        if cob_id != APP_LIGHT_STATUS_COB_ID:
            return False

        payload = decode_light_status(data)
        if payload["logic_addr"] != self.logic_addr:
            return True

        self.log_rx(
            APP_LIGHT_STATUS_COB_ID,
            data,
            f"light state addr=0x{payload['logic_addr']:02X} state=0x{payload['state_word']:04X} reason={light_reason_name(payload['reason'])}",
        )
        self._update_light_state(payload)
        return True

    def menu_lines(self):
        return [
            "  key <n>      publish one key press",
            "  release <n>  publish one key release",
            "  long <n>     publish one long press",
            "  logic <addr> set logic address/group",
            "  value <n>    set key value byte",
        ]

    def handle_command(self, cmd):
        parts = cmd.split()
        if not parts:
            return True

        if parts[0] == "key" and len(parts) == 2:
            self._send_key(int(parts[1], 0), KEY_EVENT_PRESS)
            return True
        if parts[0] == "release" and len(parts) == 2:
            self._send_key(int(parts[1], 0), KEY_EVENT_RELEASE)
            return True
        if parts[0] == "long" and len(parts) == 2:
            self._send_key(int(parts[1], 0), KEY_EVENT_LONG)
            return True
        if parts[0] == "logic" and len(parts) == 2:
            self.set_logic_addr(int(parts[1], 0), announce=True)
            self.emit(f"[APP] logic_addr=0x{self.logic_addr:02X}")
            return True
        if parts[0] == "value" and len(parts) == 2:
            self.key_value = int(parts[1], 0) & 0xFF
            self.refresh_dynamic_od()
            self.emit(f"[APP] key_value=0x{self.key_value:02X}")
            return True
        return False


def main():
    parser = build_common_arg_parser(
        description="Normal panel simulator based on USB2XXX SDK",
        env_prefix="PANEL",
        default_channel=0,
        default_node_id=0x31,
        default_logic_addr=0x01,
    )
    args = parser.parse_args()

    handle = None
    try:
        handle = init_device()
        init_can(handle, args.channel)
        print(
            f"[APP] panel simulator node=0x{args.node_id:02X} "
            f"logic_addr=0x{args.logic_addr:02X} "
            f"usbcan={channel_name(args.channel)}(index={args.channel}) "
            f"verbose_rx={'on' if args.verbose_rx else 'off'} "
            f"heartbeat_log={'hidden' if args.hide_heartbeat_log else 'shown'}"
        )
        sim = PanelModuleSimulator(
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
