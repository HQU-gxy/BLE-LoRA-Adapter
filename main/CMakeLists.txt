idf_component_register(SRCS src/app_main.cpp src/utils.cpp src/wlan_manager.cpp src/esp_hal.cpp
        INCLUDE_DIRS include)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/cplusplus.html
# C++23
target_compile_options(${COMPONENT_LIB} PUBLIC -std=gnu++23)
