cmake_minimum_required(VERSION 3.24)
project(qualia_host_tests LANGUAGES C CXX)

include(CTest)
option(QUALIA_ASAN "Enable host AddressSanitizer" OFF)
if(QUALIA_ASAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR WIN32)
        message(FATAL_ERROR "AddressSanitizer tests require GCC or Clang on a non-Windows host.")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/Box2D.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/StMems.cmake")

foreach(test_name IN ITEMS display_tests fixed_step_tests lcd_data_tests)
    add_executable(${test_name} ${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/core"
        "${CMAKE_CURRENT_LIST_DIR}/../main/drivers")
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

add_executable(physics_tests physics_tests.cpp ../main/app/physics_world.cpp)
target_include_directories(physics_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/app")
target_link_libraries(physics_tests PRIVATE box2d)
set_target_properties(physics_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(physics_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(imu_driver_tests imu_driver_tests.cpp ../main/drivers/lsm6dsox_device.cpp ../main/drivers/lis3mdl_device.cpp)
target_include_directories(imu_driver_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/drivers"
    "${CMAKE_CURRENT_LIST_DIR}/../main/core")
target_link_libraries(imu_driver_tests PRIVATE st_mems)
set_target_properties(imu_driver_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imu_driver_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(imu_core_tests imu_core_tests.cpp)
target_include_directories(imu_core_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/core")
find_package(Threads REQUIRED)
target_link_libraries(imu_core_tests PRIVATE Threads::Threads)
set_target_properties(imu_core_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imu_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Module tests build modules/ headers against minimal ESP-IDF stand-ins in host_shims/.
foreach(test_name IN ITEMS imu_module_tests inputs_module_tests midi_module_tests)
    add_executable(${test_name} ${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/host_shims"
        "${CMAKE_CURRENT_LIST_DIR}/../main" "${CMAKE_CURRENT_LIST_DIR}/../main/core"
        "${CMAKE_CURRENT_LIST_DIR}/../main/app" "${CMAKE_CURRENT_LIST_DIR}/../main/drivers")
    target_link_libraries(${test_name} PRIVATE Threads::Threads)
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

add_executable(tilt_tests tilt_tests.cpp)
target_include_directories(tilt_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/app"
    "${CMAKE_CURRENT_LIST_DIR}/../main/core")
target_link_libraries(tilt_tests PRIVATE box2d)
set_target_properties(tilt_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tilt_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME display_helpers COMMAND display_tests)
add_test(NAME fixed_step_clock COMMAND fixed_step_tests)
add_test(NAME lcd_data COMMAND lcd_data_tests)
add_test(NAME physics_scene COMMAND physics_tests)
add_test(NAME imu_drivers COMMAND imu_driver_tests)
add_test(NAME imu_core COMMAND imu_core_tests)
add_test(NAME imu_module COMMAND imu_module_tests)
add_test(NAME inputs_module COMMAND inputs_module_tests)
add_test(NAME midi_module COMMAND midi_module_tests)
add_test(NAME tilt COMMAND tilt_tests)

add_executable(midi_mapping_tests midi_mapping_tests.cpp)
target_include_directories(midi_mapping_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/app"
    "${CMAKE_CURRENT_LIST_DIR}/../main/core")
target_link_libraries(midi_mapping_tests PRIVATE box2d)
set_target_properties(midi_mapping_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(midi_mapping_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME midi_mapping COMMAND midi_mapping_tests)

add_executable(midi_ump_tests midi_ump_tests.cpp)
target_include_directories(midi_ump_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../main/core")
set_target_properties(midi_ump_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(midi_ump_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME midi_ump COMMAND midi_ump_tests)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/Midi2.cmake")
FetchContent_Declare(tinyusb_core
    URL https://codeload.github.com/espressif/tinyusb/tar.gz/894ea01409e0407a7dbe0ee29b2d58a4691f9046
    URL_HASH SHA256=9a7c17a931fbd3eeafc760bb8909f873e7d8f3358f236796d76b89566360a9f3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR qualia-unused)
FetchContent_MakeAvailable(tinyusb_core)
add_library(midi2_host_stack STATIC
    "${QUALIA_MIDI2_CLASS_SOURCE}"
    "${tinyusb_core_SOURCE_DIR}/src/common/tusb_fifo.c"
    "${tinyusb_core_SOURCE_DIR}/src/tusb.c")
target_include_directories(midi2_host_stack PUBLIC "${CMAKE_CURRENT_LIST_DIR}/usb_shims")
target_include_directories(midi2_host_stack SYSTEM PUBLIC "${tinyusb_core_SOURCE_DIR}/src"
    "${tinyusb_midi2_SOURCE_DIR}/src/class/midi")
set_target_properties(midi2_host_stack PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

add_executable(midi_usb_tests midi_usb_tests.cpp ../main/drivers/midi_usb.cpp)
target_include_directories(midi_usb_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/host_shims"
    "${CMAKE_CURRENT_LIST_DIR}/../main" "${CMAKE_CURRENT_LIST_DIR}/../main/core"
    "${CMAKE_CURRENT_LIST_DIR}/../main/drivers")
target_link_libraries(midi_usb_tests PRIVATE midi2_host_stack Threads::Threads)
set_target_properties(midi_usb_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(midi_usb_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME midi_usb COMMAND midi_usb_tests)

add_executable(gui_view_tests gui_view_tests.cpp ../main/gui/ball_view.cpp
    ../main/gui/border_view.cpp ../main/gui/imu_readout.cpp)
target_include_directories(gui_view_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/host_shims"
    "${CMAKE_CURRENT_LIST_DIR}/../main" "${CMAKE_CURRENT_LIST_DIR}/../main/core")
set_target_properties(gui_view_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gui_view_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME gui_views COMMAND gui_view_tests)

foreach(test_name IN ITEMS app_scene_tests app_scene_trace_tests)
    add_executable(${test_name} app_scene_tests.cpp ../main/app/ball.cpp ../main/app/simulation.cpp
        ../main/app/physics_world.cpp ../main/app/imu_overlay.cpp
        ../main/gui/ball_view.cpp ../main/gui/border_view.cpp ../main/gui/imu_readout.cpp)
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/host_shims"
        "${CMAKE_CURRENT_LIST_DIR}/../main" "${CMAKE_CURRENT_LIST_DIR}/../main/core"
        "${CMAKE_CURRENT_LIST_DIR}/../main/app" "${CMAKE_CURRENT_LIST_DIR}/../main/drivers")
    target_link_libraries(${test_name} PRIVATE box2d Threads::Threads)
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
target_compile_definitions(app_scene_trace_tests PRIVATE CONFIG_QUALIA_MODE_TRACE=1)
add_test(NAME app_scene COMMAND app_scene_tests)
add_test(NAME app_scene_trace COMMAND app_scene_trace_tests)

add_executable(pca9554_tests pca9554_tests.cpp ../main/drivers/pca9554_controller.cpp)
target_include_directories(pca9554_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/host_shims"
    "${CMAKE_CURRENT_LIST_DIR}/../main/drivers" "${CMAKE_CURRENT_LIST_DIR}/../main/core")
set_target_properties(pca9554_tests PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pca9554_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME pca9554_init COMMAND pca9554_tests 0)
foreach(failure RANGE 1 7)
    add_test(NAME pca9554_rollback_${failure} COMMAND pca9554_tests ${failure})
endforeach()
