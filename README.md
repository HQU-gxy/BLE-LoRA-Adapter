Need certain IDF version ([v5.1.1](https://github.com/espressif/esp-idf/tree/v5.1.1)) to build the project.

In your IDF directory.

```bash
cd $IDF_PATH
# I assume your origin is 
# https://github.com/espressif/esp-idf.git
git fetch origin v5.1.1
git checkout v5.1.1
git submodule update --init --recursive
./install.sh
source export.sh
```

I could make full use of the flash... See also [Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html).

```txt
$ python -m esptool -p /dev/cu.wchusbserial1130 flash_id
Chip is ESP32-D0WD-V3 (revision v3.0)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: 30:c6:f7:eb:69:44
Uploading stub...
Running stub...
Stub running...
Manufacturer: 20
Device: 4016
Detected flash size: 4MB
```
