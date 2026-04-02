# Gateway CAN Scheme

## 1. Core decision

Do not carry raw Modbus RTU frames directly on classic CAN.

Reasons:

1. Classic CAN only has 8 data bytes per frame, but Modbus `03` and `10` often need longer payloads.
2. CAN already gives us arbitration and broadcast, so it is better to split the design into:
   - a management/configuration plane
   - an application/event plane
3. Your "normal panel and lighting module share the same address" requirement conflicts with a pure node-id design, so we need two address layers.

## 2. Two address layers

Each device has two identifiers:

1. `node_id`
   - Unique on the CAN bus.
   - Used by the gateway for management, register read/write, heartbeat, and diagnostics.
   - Recommended range: `1..127`.

2. `logic_addr`
   - Business/group address.
   - Can be shared by a lighting module and one or more normal panels.
   - Used only by application-layer frames.
   - `0x00` means broadcast.

This solves the conflict:

- Gateway still talks to each physical device uniquely through `node_id`.
- A normal panel can still "belong to" the same lighting group by sharing `logic_addr`.

## 3. Management plane

Use the same CANopen-like management model already used by the current Python radar script:

- `0x000` NMT
- `0x600 + node_id` SDO RX
- `0x580 + node_id` SDO TX
- `0x700 + node_id` heartbeat

### Mapping from old Modbus usage

Old RTU semantic:

- `01 03 00 00 00 xx` read registers
- `01 10 00 00 00 xx ...` write registers
- `00 10 ...` broadcast write

New CAN semantic:

1. Unicast register read
   - Gateway uses SDO upload to unique `node_id`.
   - Equivalent to old `addr + 03`.

2. Unicast register write
   - Gateway uses SDO download to unique `node_id`.
   - Equivalent to old `addr + 10`.

3. Broadcast action
   - Use dedicated application broadcast frames instead of SDO.
   - Equivalent to old `00 + 10`.

This avoids designing fragmentation for every Modbus frame.

## 4. Recommended register layout

Keep a common register block across all devices, but expose it through SDO.

Recommended convention:

- `0x2000:00` logic address
- `0x2001:00` device type
- `0x2100...` live state area
- `0x2200...` control switches / local behavior enables
- `0x2300...` scene binding area
- `0x2400...` direct output / command mirror area

If you want a Modbus-like view on STM32, the gateway can internally map:

- Modbus register `0x0000` -> SDO object `0x2000:00`
- Modbus register `0x0001` -> SDO object `0x2001:00`
- Modbus register `0x0100` -> SDO object `0x2100:00`

So the upper application still thinks in "registers", while the CAN transport uses SDO.

## 5. Application plane

Use fixed standard CAN IDs and put `logic_addr` into payload.

This keeps IDs stable and avoids running out of 11-bit space.

### Application IDs

- `0x780` scene command broadcast
- `0x781` lighting state publish
- `0x782` normal panel key publish
- `0x783` reserved for gateway broadcast write / time sync
- `0x785` discovery request broadcast

Address-claim response:

- `0x480 + node_id`

Why the address-claim response is not a single fixed ID:

- a discovery request may trigger multiple nodes to answer at almost the same time
- if all of them replied on the same CAN ID, classic CAN could not arbitrate different payloads safely
- using `0x480 + node_id` gives each node a unique response ID and avoids that collision risk

These IDs are outside the common CiA301 predefined connection set and do not clash with the SDO/heartbeat IDs above.

## 6. Payload definitions

### 6.1 Scene command `0x780`

Used by:

- scene panel
- radar module
- gateway broadcast actions if needed

Payload:

- Byte0: `target_logic_addr`, `0x00` means broadcast
- Byte1: source type
  - `0x01` gateway
  - `0x02` scene panel
  - `0x03` radar
- Byte2..3: scene index
- Byte4..5: action
  - `0x0001` scene on
  - `0x0002` scene off
  - `0x0003` scene toggle
- Byte6: source node id
- Byte7: flags

Example:

- Scene panel requests scene 3 ON for all lighting groups:
  - COB-ID `0x780`
  - DATA `[00 02 00 03 00 01 41 00]`

This is the CAN replacement for your old broadcast scene execution idea.

### 6.2 Lighting state publish `0x781`

Used by lighting modules to push relay state changes.

Payload:

- Byte0: `logic_addr`
- Byte1: reason
  - `0x01` manual/local
  - `0x02` panel key
  - `0x03` scene
  - `0x04` gateway
- Byte2..3: relay state word, 16 bits
- Byte4..5: change mask
- Byte6: source node id
- Byte7: sequence

Example:

- Lighting group `0x01` changes to `0xFFFF`:
  - COB-ID `0x781`
  - DATA `[01 03 FF FF FF FF 11 05]`

Normal panels that share `logic_addr=0x01` consume this frame and update UI/application logic.

### 6.3 Panel key publish `0x782`

Used by normal panels.

Payload:

- Byte0: `logic_addr`
- Byte1: key code
- Byte2: key event
  - `0x01` press
  - `0x02` release
  - `0x03` long press
- Byte3: key value / optional argument
- Byte4: source node id
- Byte5: flags
- Byte6..7: reserved

Example:

- Normal panel of group `0x01` sends key 1 press:
  - COB-ID `0x782`
  - DATA `[01 01 01 01 31 00 00 00]`

Lighting modules that share `logic_addr=0x01` may consume it directly.

### 6.4 Gateway broadcast write `0x783`

Reserve this for compact common-area broadcasts such as:

- time sync
- mode sync
- common scene area writes

Recommended payload:

- Byte0: command
  - `0x01` time sync
  - `0x02` common-area write
- Byte1..2: index
- Byte3..4: value
- Byte5: aux
- Byte6: source node id
- Byte7: reserved

For time sync, it is also acceptable to define a dedicated time payload later if you need year/month/day/hour/minute/second in one transfer.

### 6.5 Discovery request `0x785`

Used by the gateway to periodically ask all modules on the bus to identify themselves.

Payload:

- Byte0: target logic address, `0x00` means all
- Byte1: target node id, `0x00` means all
- Byte2: target device type, `0x00` means all
- Byte3: flags
- Byte4..7: reserved

After receiving this request, a module replies with its address-claim frame on:

- `0x480 + node_id`

Address-claim payload:

- Byte0: `logic_addr`
- Byte1: `device_type`
- Byte2: `source_node`
- Byte3: reason
  - `0x01` startup
  - `0x02` address update
  - `0x03` discovery response
- Byte4..5: main publish COB-ID of this device
- Byte6: flags
- Byte7: reserved

## 7. Device behavior rules

### Gateway

- Uses unique `node_id` for SDO read/write.
- Uses `0x780` or `0x783` for broadcast actions.
- Does not need a `logic_addr`.

### Lighting module

- Has unique `node_id`.
- Has `logic_addr` for business grouping.
- Executes scene broadcasts only when local scene table contains the scene.
- Publishes relay state changes on `0x781`.

### Normal panel

- Has unique `node_id`.
- Shares `logic_addr` with the lighting module it is bound to.
- Consumes `0x781` for UI/application updates.
- Publishes key events on `0x782`.

### Scene panel

- Has unique `node_id`.
- Its own `logic_addr` does not need to match the lighting module.
- Publishes scene commands on `0x780`.

### Radar module

- Has unique `node_id`.
- Publishes the same scene command frame `0x780` when occupancy changes.
- Lighting modules treat it exactly like a scene panel source.

## 8. Why this is better than RS485 half duplex

1. No TX direction switching problem.
2. No bus fight when multiple devices need to report state.
3. Active publish is natural on CAN because arbitration is built in.
4. Broadcast scene execution no longer needs polling.

## 9. Python simulators added in this repo

- `lighting_module.py`
- `panel_module.py`
- `scene_panel_module.py`
- `radar_scene_sensor.py`

Shared files:

- `building_can_def.py`
- `canopen_sim_core.py`
