set(srcs app/main.cpp app/tasks.cpp app/simulation.cpp app/ball.cpp app/physics_world.cpp
         gui/port.cpp gui/ball_view.cpp gui/border_view.cpp
         drivers/i2c_bus.cpp drivers/pca9554_controller.cpp drivers/lcd_controller.cpp
         drivers/esp_i2c_register_bus.cpp drivers/lsm6dsox.cpp drivers/lsm6dsox_device.cpp
         drivers/lis3mdl.cpp drivers/lis3mdl_device.cpp)
if(CONFIG_QUALIA_MODE_MIDI)
    list(APPEND srcs drivers/midi_usb.cpp)
endif()
if(CONFIG_QUALIA_IMU_DEBUG)
    list(APPEND srcs app/imu_overlay.cpp gui/imu_readout.cpp)
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "." "drivers" "core" "app"
    PRIV_REQUIRES box2d st_mems esp_driver_gpio esp_driver_i2c esp_lcd esp_mm esp_timer esp_trace
                  espressif__esp_io_expander_tca9554 espressif__esp_lcd_st7701
                  espressif__esp_lcd_panel_io_additions espressif__esp_tinyusb lvgl__lvgl efuse)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-std=c++23>")

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "The Qualia demo requires IDF_TARGET=esp32s3.")
endif()
if(NOT CONFIG_SPIRAM_MODE_OCT OR NOT CONFIG_SPIRAM OR
   NOT CONFIG_ESPTOOLPY_FLASHSIZE_16MB OR NOT CONFIG_PARTITION_TABLE_CUSTOM)
    message(FATAL_ERROR "Use the Qualia board settings in sdkconfig.defaults: octal PSRAM, 16 MB flash, and custom partitions.")
endif()
if(CONFIG_FREERTOS_HZ LESS 1000)
    message(FATAL_ERROR "The millisecond task timing requires CONFIG_FREERTOS_HZ=1000.")
endif()
if(NOT CONFIG_LV_CONF_SKIP OR NOT CONFIG_LV_OS_NONE OR
   NOT CONFIG_LV_USE_CANVAS OR NOT CONFIG_LV_DRAW_SW_COMPLEX OR
   NOT CONFIG_LV_DRAW_SW_SUPPORT_RGB565 OR NOT CONFIG_LV_DRAW_SW_SUPPORT_ARGB8888)
    message(FATAL_ERROR "Use the SDK LVGL settings in sdkconfig.defaults (single-task software rendering, RGB565, ARGB8888, canvas, and rounded shapes).")
endif()
if(CONFIG_QUALIA_MODE_MIDI)
    if(NOT CONFIG_TINYUSB_MIDI_COUNT EQUAL 1 OR CONFIG_ESP_TRACE_ENABLE OR
       CONFIG_ESP_TRACE_TRANSPORT_APPTRACE OR CONFIG_ESP_TRACE_TRANSPORT_USB_SERIAL_JTAG OR
       CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG OR CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG)
        message(FATAL_ERROR "MIDI mode requires one MIDI interface, tracing disabled, and a non-USB-Serial/JTAG console. Use sdkconfig.defaults.midi.")
    endif()
elseif(CONFIG_TINYUSB_MIDI_COUNT GREATER 0)
    message(FATAL_ERROR "MIDI interfaces must be disabled outside MIDI mode.")
endif()
if(CONFIG_QUALIA_MODE_TRACE)
    if(NOT CONFIG_ESP_TRACE_LIB_NONE OR NOT CONFIG_ESP_TRACE_TRANSPORT_APPTRACE OR
       NOT CONFIG_APPTRACE_DEST_JTAG)
        message(FATAL_ERROR "Tracing mode requires the enabled apptrace JTAG transport. Use sdkconfig.defaults.trace.")
    endif()
elseif(CONFIG_ESP_TRACE_ENABLE OR CONFIG_ESP_TRACE_TRANSPORT_APPTRACE OR
       CONFIG_ESP_TRACE_TRANSPORT_USB_SERIAL_JTAG)
    message(FATAL_ERROR "Application tracing must be disabled outside tracing mode.")
endif()