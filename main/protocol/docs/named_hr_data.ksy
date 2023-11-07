meta:
  id: named_hr_data
  title: Named Heart Rate Data
  imports:
    - common
  endian: be

doc: |
  named_hr_data transmit the HR with Bluetooth LE address.

seq:
  - id: magic_0x60
    contents: [0x60]
    doc: a magic number (0x60)
  - id: key
    type: common::name_map_key
  - id: hr
    type: u1
    doc: |
      The heart rate in beats per minute.
  - id: addr
    type: common::ble_addr
