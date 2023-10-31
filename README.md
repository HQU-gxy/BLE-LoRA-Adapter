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

Target is expected to be `esp32c3`. See also [Select the Target Chip](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-py.html).
