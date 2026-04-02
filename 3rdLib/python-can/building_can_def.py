# -*- coding: utf-8 -*-
"""
Application-layer CAN definitions for the building devices.

Design notes:
1. Keep CANopen-like node_id unique for gateway management and register access.
2. Keep logic_addr as the business/group address so a lighting module and a
   normal panel can share the same logical target without colliding on CAN IDs.
3. Use fixed application COB-IDs and carry logic_addr in payload to avoid
   exhausting the 11-bit standard CAN ID space.
"""

LOGIC_ADDR_BROADCAST = 0x00

DEVICE_TYPE_LIGHTING = 0x01
DEVICE_TYPE_PANEL = 0x02
DEVICE_TYPE_SCENE_PANEL = 0x03
DEVICE_TYPE_RADAR = 0x04

# Application-layer CAN IDs. 0x780..0x7FF are outside the common CiA301
# predefined connection set and work well with the CANopen-like management
# frames already used in the project.
APP_SCENE_COMMAND_COB_ID = 0x780
APP_LIGHT_STATUS_COB_ID = 0x781
APP_PANEL_KEY_COB_ID = 0x782
APP_BROADCAST_WRITE_COB_ID = 0x783
APP_DISCOVERY_REQUEST_COB_ID = 0x785
APP_ADDRESS_CLAIM_BASE_COB_ID = 0x480

SCENE_SOURCE_GATEWAY = 0x01
SCENE_SOURCE_PANEL = 0x02
SCENE_SOURCE_RADAR = 0x03

SCENE_ACTION_ON = 0x0001
SCENE_ACTION_OFF = 0x0002
SCENE_ACTION_TOGGLE = 0x0003

LIGHT_REASON_MANUAL = 0x01
LIGHT_REASON_PANEL_KEY = 0x02
LIGHT_REASON_SCENE = 0x03
LIGHT_REASON_GATEWAY = 0x04

KEY_EVENT_PRESS = 0x01
KEY_EVENT_RELEASE = 0x02
KEY_EVENT_LONG = 0x03

BCAST_WRITE_TIME_SYNC = 0x01
BCAST_WRITE_COMMON_AREA = 0x02

ADDR_CLAIM_REASON_STARTUP = 0x01
ADDR_CLAIM_REASON_UPDATE = 0x02
ADDR_CLAIM_REASON_DISCOVERY = 0x03


def clamp_u8(value):
    return int(value) & 0xFF


def clamp_u16(value):
    return int(value) & 0xFFFF


def encode_scene_command(target_addr, source_type, scene_index, action, source_node=0, flags=0):
    scene_index = clamp_u16(scene_index)
    action = clamp_u16(action)
    return [
        clamp_u8(target_addr),
        clamp_u8(source_type),
        (scene_index >> 8) & 0xFF,
        scene_index & 0xFF,
        (action >> 8) & 0xFF,
        action & 0xFF,
        clamp_u8(source_node),
        clamp_u8(flags),
    ]


def decode_scene_command(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "target_addr": payload[0],
        "source_type": payload[1],
        "scene_index": (payload[2] << 8) | payload[3],
        "action": (payload[4] << 8) | payload[5],
        "source_node": payload[6],
        "flags": payload[7],
    }


def encode_light_status(logic_addr, state_word, change_mask=0, reason=LIGHT_REASON_MANUAL, source_node=0, sequence=0):
    state_word = clamp_u16(state_word)
    change_mask = clamp_u16(change_mask)
    return [
        clamp_u8(logic_addr),
        clamp_u8(reason),
        state_word & 0xFF,
        (state_word >> 8) & 0xFF,
        change_mask & 0xFF,
        (change_mask >> 8) & 0xFF,
        clamp_u8(source_node),
        clamp_u8(sequence),
    ]


def decode_light_status(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "logic_addr": payload[0],
        "reason": payload[1],
        "state_word": payload[2] | (payload[3] << 8),
        "change_mask": payload[4] | (payload[5] << 8),
        "source_node": payload[6],
        "sequence": payload[7],
    }


def encode_panel_key(logic_addr, key_code, key_event=KEY_EVENT_PRESS, value=1, source_node=0, flags=0):
    return [
        clamp_u8(logic_addr),
        clamp_u8(key_code),
        clamp_u8(key_event),
        clamp_u8(value),
        clamp_u8(source_node),
        clamp_u8(flags),
        0x00,
        0x00,
    ]


def decode_panel_key(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "logic_addr": payload[0],
        "key_code": payload[1],
        "key_event": payload[2],
        "value": payload[3],
        "source_node": payload[4],
        "flags": payload[5],
    }


def encode_broadcast_write(command, index, value, source_node=0, aux=0):
    index = clamp_u16(index)
    value = clamp_u16(value)
    return [
        clamp_u8(command),
        (index >> 8) & 0xFF,
        index & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF,
        clamp_u8(aux),
        clamp_u8(source_node),
        0x00,
    ]


def decode_broadcast_write(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "command": payload[0],
        "index": (payload[1] << 8) | payload[2],
        "value": (payload[3] << 8) | payload[4],
        "aux": payload[5],
        "source_node": payload[6],
    }


def encode_address_claim(logic_addr, device_type, source_node=0, publish_cob_id=0, reason=ADDR_CLAIM_REASON_STARTUP, flags=0):
    publish_cob_id = int(publish_cob_id) & 0x7FF
    return [
        clamp_u8(logic_addr),
        clamp_u8(device_type),
        clamp_u8(source_node),
        clamp_u8(reason),
        publish_cob_id & 0xFF,
        (publish_cob_id >> 8) & 0xFF,
        clamp_u8(flags),
        0x00,
    ]


def decode_address_claim(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "logic_addr": payload[0],
        "device_type": payload[1],
        "source_node": payload[2],
        "reason": payload[3],
        "publish_cob_id": payload[4] | (payload[5] << 8),
        "flags": payload[6],
    }


def address_claim_cob_id(node_id):
    return APP_ADDRESS_CLAIM_BASE_COB_ID + (clamp_u8(node_id) & 0x7F)


def decode_address_claim_cob_id(cob_id):
    cob_id = int(cob_id) & 0x7FF
    if APP_ADDRESS_CLAIM_BASE_COB_ID <= cob_id <= APP_ADDRESS_CLAIM_BASE_COB_ID + 0x7F:
        node_id = cob_id - APP_ADDRESS_CLAIM_BASE_COB_ID
        if node_id != 0:
            return node_id
    return None


def encode_discovery_request(target_logic_addr=0, target_node_id=0, target_device_type=0, flags=0):
    return [
        clamp_u8(target_logic_addr),
        clamp_u8(target_node_id),
        clamp_u8(target_device_type),
        clamp_u8(flags),
        0x00,
        0x00,
        0x00,
        0x00,
    ]


def decode_discovery_request(data):
    payload = list(data[:8]) + [0x00] * max(0, 8 - len(data))
    return {
        "target_logic_addr": payload[0],
        "target_node_id": payload[1],
        "target_device_type": payload[2],
        "flags": payload[3],
    }


def scene_action_name(action):
    return {
        SCENE_ACTION_ON: "on",
        SCENE_ACTION_OFF: "off",
        SCENE_ACTION_TOGGLE: "toggle",
    }.get(action, f"0x{clamp_u16(action):04X}")


def key_event_name(key_event):
    return {
        KEY_EVENT_PRESS: "press",
        KEY_EVENT_RELEASE: "release",
        KEY_EVENT_LONG: "long",
    }.get(key_event, f"0x{clamp_u8(key_event):02X}")


def light_reason_name(reason):
    return {
        LIGHT_REASON_MANUAL: "manual",
        LIGHT_REASON_PANEL_KEY: "panel_key",
        LIGHT_REASON_SCENE: "scene",
        LIGHT_REASON_GATEWAY: "gateway",
    }.get(reason, f"0x{clamp_u8(reason):02X}")


def source_type_name(source_type):
    return {
        SCENE_SOURCE_GATEWAY: "gateway",
        SCENE_SOURCE_PANEL: "scene_panel",
        SCENE_SOURCE_RADAR: "radar",
    }.get(source_type, f"0x{clamp_u8(source_type):02X}")


def device_type_name(device_type):
    return {
        DEVICE_TYPE_LIGHTING: "lighting",
        DEVICE_TYPE_PANEL: "panel",
        DEVICE_TYPE_SCENE_PANEL: "scene_panel",
        DEVICE_TYPE_RADAR: "radar",
    }.get(device_type, f"0x{clamp_u8(device_type):02X}")


def device_publish_cob_id(device_type):
    return {
        DEVICE_TYPE_LIGHTING: APP_LIGHT_STATUS_COB_ID,
        DEVICE_TYPE_PANEL: APP_PANEL_KEY_COB_ID,
        DEVICE_TYPE_SCENE_PANEL: APP_SCENE_COMMAND_COB_ID,
        DEVICE_TYPE_RADAR: APP_SCENE_COMMAND_COB_ID,
    }.get(device_type, 0)


def logic_addr_share_allowed(device_type_a, device_type_b):
    allowed_pairs = {
        frozenset((DEVICE_TYPE_PANEL, DEVICE_TYPE_PANEL)),
        frozenset((DEVICE_TYPE_PANEL, DEVICE_TYPE_LIGHTING)),
    }
    return frozenset((clamp_u8(device_type_a), clamp_u8(device_type_b))) in allowed_pairs
