meta:
  id: set_name_map_key
  title: Set Name Map Key
  imports:
    - common
  endian: be

doc: |
  `set_name_map_key` would be sent by Hub (i.e. the TrackLane)
   to set the key to the name of the device.
   The key is used to represent the device name in messages from the HR monitor
   to the repeater, in order to reduce the size of the message.
   This is necessary because bandwidth is limited for LoRA.

seq:
  - id: magic_0x79
    contents: [0x79]
    doc: a magic number (0x79)
  - id: mac
    type: ble_addr
    doc: |
      The Bluetooth LE address of the repeater to be queried.
      The all 1 address (FF:FF:FF:FF:FF:FF) is reserved for broadcast 
      and should be illegal for this command.
  - id: key
    type: name_map_key
