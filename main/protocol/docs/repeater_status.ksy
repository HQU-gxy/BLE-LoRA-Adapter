meta:
  id: repeater_status
  title: Query by MAC Response
  imports:
    - common
  endian: be

doc: |
  `query_device_by_mac_r` is the response to `query_device_by_mac`.
   It contains the Bluetooth LE address of the repeater,
   if the repeater is connected to a Bluetooth LE heart rate monitor.
   The name and the Bluetooth LE address of the heart rate monitor
   and a key map to the name of device also included.

seq:
  - id: magic_0x47
    contents: [0x47]
    doc: a magic number (0x47)
  - id: repeater_addr
    type: common::ble_addr
  - id: key
    type: common::name_map_key
  - id: reserved
    type: b7
    doc: reserved
  - id: is_connected
    type: b1
    doc: |
      1 if the repeater is connected to a Bluetooth LE heart rate monitor,
      0 otherwise.
  - id: device
    type: hr_device
    if: is_connected == true

types:
  hr_device:
    seq:
    - id: addr
      type: b6
      doc: |
        The Bluetooth LE address of the heart rate monitor.
    - id: name
      # As this is a very common thing,
      # there’s a shortcut for type: str and terminator: 0.
      type: strz
      encoding: utf-8
      doc: |
        The name of the heart rate monitor.

