meta:
  id: common
  title: Common type definitions
  endian: be

seq:
  - id: ignore_me
    type: strz
    encoding: utf-8

types:
  name_map_key:
    seq:
      - id: key
        type: u1
        doc: |
          A key (uint8_t) to the name of the device. This key is used to represent
          the device name in messages from the HR monitor to the repeater, in order
          to reduce the size of the message. This is necessary because bandwidth is
          limited for LoRA.
  ble_addr:
    seq:
      - id: addr
        size: 6
        doc: |
          The Bluetooth LE address of the device.
          The all 1 address (FF:FF:FF:FF:FF:FF) is reserved for broadcast 
          (i.e. query all devices).
