# Qualia ESP32-S3

Box2D/LVGL physics demo for the Adafruit Qualia with a 480x480 round display,
16 MB flash, and 8 MB octal PSRAM. The optional LSM6DSOX/LIS3MDL breakout supplies
tilt control; without it, the demo keeps fixed downward gravity.

## Build and test

Run commands from the repository root. Firmware needs an activated ESP-IDF
6.1.x environment (`main/idf_component.yml` requires >=6.1.0,<6.2.0).
Use the EIM-generated activation script or the SDK's `export.sh`/`export.ps1`.

```sh
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig build
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig menuconfig
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig -p /dev/ttyACM0 flash monitor
```

Choose the actual serial port; Windows uses `COMn`. Board/LVGL defaults are in
`sdkconfig.defaults`, and application options are in `main/Kconfig.projbuild`.
Always keep generated configuration inside its build directory. Defaults do
not overwrite an existing `sdkconfig`; after changing them, use a fresh build
directory or remove only that profile's generated configuration and reconfigure.
Previous `build/midi/sdkconfig` files from the legacy-only profile must be
reconfigured because MIDI now requires `CONFIG_TINYUSB_MIDI_COUNT=0`.

```sh
idf.py -B build/midi -D SDKCONFIG=build/midi/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.midi" build
idf.py -B build/trace -D SDKCONFIG=build/trace/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.trace" build
idf.py -B build/imu -D SDKCONFIG=build/imu/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.imu" build
idf.py -B build/demo-perf -D SDKCONFIG=build/demo-perf/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.perf" build
```

MIDI and native JTAG share the internal USB PHY and are mutually exclusive.
MIDI sends bounce notes, logs input, and accepts the bootloader SysEx; it replaces the USB serial console with UART0 at
115200 baud. Use native USB `flash` without `monitor` for MIDI. Re-enter the ROM
downloader with BOOT held while pressing/releasing RESET before reflashing.
Trace capture uses Espressif's OpenOCD; capture/analysis commands are in README.md.

MIDI uses an experimental MIDI 2.0 class, with legacy packets on USB alt 0
and MIDI 1.0/2.0 UMP on alt 1. It retains normalized impact strength through the
handoff for independent 7-bit/16-bit velocity encoding. The pinned class archive
in `cmake/Midi2.cmake` is shared by firmware and the host USB harness. Linux
qualification includes native notes/discovery/negotiation, fragmented USB UMP,
physical alt-0 bounce output and recovery, and legacy conversion.
Physical unplug/replug and board-only runtime suspend/resume are exercised.
DAW playback, Windows, and macOS remain unqualified; see README.md. The shared
class helper generates the checked SET_INTERFACE endpoint-reset fix without
editing fetched sources. Do not infer native support
from enumeration or legacy conversion, or reload a shared audio driver for testing.
Keep MIDI 2.0 class API calls on the TinyUSB task, invalidate bounces/partial SysEx
on session changes, and repeat an identical asynchronous write until acknowledged.
The public driver API uses complete `midi::Message` values and `midi::State`.

Host tests need CMake 3.24+, Ninja, and C17/C++23 compilers, not ESP-IDF:

```sh
cmake -S tests -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUALIA_ASAN=ON
cmake --build build/host --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/host --output-on-failure

# Build and run one suite; CMake target names and CTest names differ.
cmake --build build/host --target imu_module_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/host -R '^imu_module$' --output-on-failure
ctest --test-dir build/host -N

# Trace analyzer tests, with NumPy from requirements-trace.txt installed.
python3 -m unittest discover -s tests -p test_trace.py
python3 tests/test_trace.py TraceTests.test_simulation_records
```

ASan requires GCC/Clang on a non-Windows host. On Windows omit `QUALIA_ASAN`
and the `ASAN_OPTIONS` prefix, and use `python` instead of `python3`.
C++ suites are standalone executables using `CHECK`, not a test framework with
per-case filtering. Their GCC/Clang builds use `-Wall -Wextra -Wpedantic -Werror`.
Format changed C++ files with `clang-format -i <files>` using `.clang-format`.

## Architecture

All application layers belong to the single ESP-IDF `main` component; the build
does not enforce their boundaries:

- `core/` holds SDK-independent algorithms and value types.
- `drivers/` owns hardware access. Static board resources implement
  `drivers::HardwareDriver`; sensor device wrappers use `imu::RegisterBus`
  separately from the ESP-IDF bus adapter.
- `modules/` holds passive subsystems with `init()`, `is_initialized()`, and
  `step(now)`. Modules never create tasks. IMU and inputs are templates over
  small static-driver concepts; `app/board.h` selects chips and button pins.
- `gui/` owns LVGL, display presentation, and views. Its public view APIs accept
  panel pixels/plain values, not Box2D objects or module types.
- `app/` wires startup, tasks, physics, and tilt mapping through those APIs.

`app/main.cpp` initializes shared `DRIVERS` in dependency order, then all
`MODULES`, then starts tasks. Required failures abort startup; IMU failure is
optional. Module instances have namespace-scope lifetime because tasks retain
pointers after `app_main` returns.

`app/tasks.cpp` owns application scheduling: inputs every 5 ms and IMU every
10 ms on the core-1 sensors task; LVGL timers and physics on the core-0 UI task;
MIDI on its own task. Each module is stepped by one task only. The UI consumes
complete newest IMU snapshots through `LatestValue`, not shared mutable state.
After a sensors-task overrun, restart the schedule rather than replaying missed
polls and collapsing the debounce window.

Host tests compile the production physics scene and sensor device wrappers.
Module tests substitute static fake drivers and `tests/host_shims` for ESP-IDF
headers; keep clocks injected through `step(now)` so timing stays host-testable.

## Conventions and invariants

- Firmware C++ is strict C++23 with GNU extensions disabled; Box2D uses C17.
  Declare values before `if`/`switch`, rather than using initializer statements.
  Preserve each file's line endings: the repository mixes LF and CRLF.
- Add firmware sources explicitly in `main/CMakeLists.txt`. MIDI and IMU debug
  sources are conditionally compiled with their matching Kconfig options.
- Dependencies are pinned in `main/idf_component.yml`, `dependencies.lock`, and
  shared FetchContent helpers in `cmake/`. Firmware and host tests use the same
  Box2D/ST driver sources. Local source overrides are
  `FETCHCONTENT_SOURCE_DIR_BOX2D`, `FETCHCONTENT_SOURCE_DIR_LSM6DSOX_PID`, and
  `FETCHCONTENT_SOURCE_DIR_LIS3MDL_PID`; keep local checkouts at the pinned commits.
  Do not edit fetched sources in `managed_components/` or build directories.
- Driver initialization rejects duplicate calls and unmet dependencies with
  `ESP_ERR_INVALID_STATE`. Hardware APIs use `esp_err_t` or
  `std::expected<T, esp_err_t>`; SDK-independent sensor wrappers use `imu::Error`.
  Motion/button read failures skip the step and log once until reads recover.
- The expander and sensors share `drivers::I2cBus`. Startup hardware work finishes
  before tasks begin. Keep expander output changes on the sensors task through
  the button callback; transfer serialization does not make cached output
  read/modify/write sequences safe across tasks. Register transfers have a
  20 ms timeout; component-backed expander output writes use its longer timeout.
- Ordinary LVGL calls stay on the UI task. `gui::init()` must run there because
  VSYNC notifications target its caller; only the LVGL tick runs on `esp_timer`.
  The two RGB565 PSRAM framebuffers use LVGL DIRECT mode. Do not reuse the old
  scan buffer before its release VSYNC, including the extra frame for late swaps.
- Physics uses a fixed 60 Hz clock with four Box2D substeps, carrying fractional
  time and dropping backlog beyond four whole steps per UI update. Keep timing
  independent of render rate and scene constants in `app/physics_world`.
- World Y points up; panel Y points down. Use `display::world_to_panel` and signed
  centering for both ball and border. Sensor acceleration is specific force
  (up at rest, m/s^2), gyro is rad/s, and `tilt::BOARD_AXIS_MAP` maps the mounted
  breakout's -X/-Y to display right/up. Keep the ball awake so gravity changes
  move it after it comes to rest.
