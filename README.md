# Qualia ESP32-S3 physics demo

A bouncing ball on the [Adafruit Qualia ESP32-S3](https://www.adafruit.com/product/5792)
with a [480x480 round display](https://www.adafruit.com/product/5800), using Box2D and LVGL.
Requires 16 MB flash and 8 MB octal PSRAM.

Connect an [LSM6DSOX + LIS3MDL breakout](https://www.adafruit.com/product/4517)
to STEMMA QT to steer the ball by tilting the screen. Without it, gravity points
down. The up/down buttons turn the backlight on/off.

## Build and flash

Install [ESP-IDF 6.1.x](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/)
and activate its environment in your terminal. CI uses `v6.1`.
For an EIM installation, use its generated activation script; for a conventional
installation, source `esp-idf/export.sh` (or `export.ps1` on Windows).

```sh
git clone https://github.com/Segfault1602/qualia-esp32s3.git
cd qualia-esp32s3
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig build
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig -p /dev/ttyACM0 flash monitor
```

Replace the port with your board's device (`COMn` on Windows). Dependencies
download automatically on the first build; no project submodules are needed.

Board and LVGL settings live in `sdkconfig.defaults`. Use the same build arguments
with `menuconfig` to change settings. Keep a separate build directory for each
profile: defaults do not overwrite an existing generated `sdkconfig`.

## Optional profiles

```sh
# USB MIDI receiver and packet logger (not a synthesizer)
idf.py -B build/midi -D SDKCONFIG=build/midi/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.midi" build

# Simulation timing over native JTAG
idf.py -B build/trace -D SDKCONFIG=build/trace/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.trace" build

# Raw IMU readings on screen and in the console
idf.py -B build/imu -D SDKCONFIG=build/imu/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.imu" build

# LVGL rendering statistics in the console
idf.py -B build/demo-perf -D SDKCONFIG=build/demo-perf/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.perf" build
```

Use the matching build directory and `SDKCONFIG` path when flashing.

**MIDI and native JTAG cannot run together** because they share the USB PHY.
MIDI takes over native USB; its console uses UART0 at 115200 baud and needs a
separate serial adapter. Flash MIDI over native USB without `monitor`.
To flash again, hold BOOT, press and release RESET, then release BOOT and select
the ROM downloader's serial port.

For trace capture, run Espressif's OpenOCD:

```sh
openocd -f board/esp32s3-builtin.cfg
```

In another terminal, connect with `telnet localhost 4444`, then capture for
at least 10 seconds:

```text
esp apptrace start file:///absolute/path/trace.txt 1 -1 -1 0 0
esp apptrace stop
```

Analyze the capture in a separate Python environment:

```sh
python3 -m venv build/trace-analysis
build/trace-analysis/bin/python -m pip install -r requirements-trace.txt
build/trace-analysis/bin/python parse_trace.py trace.txt
```

On Windows, use `python` and `build/trace-analysis/Scripts/python.exe`.

## Host tests

No board or ESP-IDF needed. Requires C17/C++23 compilers, CMake 3.24+, and Ninja.
ASan requires GCC or Clang on a non-Windows host; omit that option on Windows.

```sh
cmake -S tests -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUALIA_ASAN=ON
cmake --build build/host --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/host --output-on-failure
```

The tests cover physics, display mapping, sensor drivers, gravity estimation,
and button debouncing. With NumPy installed, run the trace analyzer tests:

```sh
python -m unittest discover -s tests -p test_trace.py
```

VS Code defaults to `build/demo`. Configure the ESP-IDF extension for your
installation; when switching profiles, update the build, compile-command,
and debug ELF paths together.
