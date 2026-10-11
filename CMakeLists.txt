# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.24)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(qualia-esp32s3)

if(CONFIG_QUALIA_MODE_MIDI)
    include(cmake/Midi2.cmake)
    idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
    idf_component_get_property(wrapper_lib espressif__esp_tinyusb COMPONENT_LIB)
    get_target_property(tusb_sources ${tusb_lib} SOURCES)
    list(LENGTH tusb_sources source_count)
    list(FILTER tusb_sources EXCLUDE REGEX "(^|/)src/class/midi/midi2_device\\.c$")
    list(LENGTH tusb_sources filtered_count)
    math(EXPR removed_count "${source_count} - ${filtered_count}")
    if(NOT removed_count EQUAL 1)
        message(FATAL_ERROR "MIDI 2.0 requires exactly one replaceable TinyUSB MIDI 2.0 class source.")
    endif()
    set_property(TARGET ${tusb_lib} PROPERTY SOURCES "${tusb_sources}")
    target_sources(${tusb_lib} PRIVATE "${QUALIA_MIDI2_CLASS_SOURCE}")
    target_include_directories(${tusb_lib} PUBLIC "${tinyusb_midi2_SOURCE_DIR}/src/class/midi")
    target_compile_definitions(${tusb_lib} PUBLIC CFG_TUD_MIDI2=1 CFG_TUD_MIDI2_BLOCK_STRIDX=4
        CFG_TUD_MIDI2_RX_BUFSIZE=128 CFG_TUD_MIDI2_TX_BUFSIZE=64)
    target_compile_definitions(${wrapper_lib} PRIVATE CFG_TUD_MIDI2=1)
endif()
