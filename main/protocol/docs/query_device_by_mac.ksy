meta:
  id: query_device_by_mac
  title: Query by MAC
  imports:
    - common
  endian: be

doc: |
  `query_device_by_mac` would be sent by Hub (i.e. the TrackLane)
   to query a repeater device by its Bluetooth LE address.
   The all 1 address (FF:FF:FF:FF:FF:FF) is reserved for broadcast 
   (i.e. query all devices).

seq:
  - id: magic_0x37
    contents: [0x37]
    doc: a magic number
  - id: mac
    type: common::ble_addr
