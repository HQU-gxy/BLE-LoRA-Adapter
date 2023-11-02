meta:
  id: hr_data
  title: Heart Rate Data
  imports:
    - common
  endian: be

doc: |
  `hr_data` represents heart rate data transmitted by Repeater.

seq:
  - id: magic_0x63
    contents: [0x63]
    doc: a magic number (0x63)
  - id: key
    type: common::name_map_key
  - id: hr
    type: u1
    doc: |
      The heart rate in beats per minute.
