# Qualia ESP32-S3 physics demo

A bouncing ball on the [Adafruit Qualia ESP32-S3](https://www.adafruit.com/product/5792)
with a [480x480 round display](https://www.adafruit.com/product/5800), using Box2D and LVGL.
Requires 16 MB flash and 8 MB octal PSRAM.

Connect an [LSM6DSOX + LIS3MDL breakout](https://www.adafruit.com/product/4517)
to STEMMA QT to steer the ball by tilting the screen. Without it, gravity points
down. The up/down buttons turn the backlight on/off.
The 2D ball coasts freely away from the circular border; friction and rolling
resistance act only during contact with the border.

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
If `build/midi` was previously configured for MIDI 1.0 only, remove its
`build/midi/sdkconfig` and reconfigure before building the updated MIDI profile,
or choose a fresh build directory.

## Optional profiles

```sh
# USB MIDI 2.0 with MIDI 1.0 fallback (external synth required)
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
For the first MIDI flash or recovery, hold BOOT while pressing/releasing RESET,
then release BOOT and select the downloader's serial port.

### MIDI (experimental MIDI 2.0)

Route **Qualia MIDI2** to an external synth, DAW, or VCV Rack. The single MIDI
profile provides a bidirectional Function Block named **Bounce**, Group 1 and
channel 1 (both index 0 on the wire).
Bounces send 100 ms notes on channel 1. Pitch follows 15 equal wall sectors,
ascending clockwise from C3 at the top to C5 in C major (MIDI 48-72).
Velocity follows perpendicular impact speed; glancing hits are softer.

To reflash without buttons, send this SysEx to the device's MIDI input:

```text
F0 7D 51 55 41 4C 49 41 01 F7
```

On Linux, use `amidi -l` to find the port, then:

```sh
amidi -p hw:CARD,DEVICE,SUBDEVICE -S 'F0 7D 51 55 41 4C 49 41 01 F7'
```

Wait for the downloader serial port, then flash the MIDI profile without `monitor`.
Any connected MIDI sender can trigger this reboot; download-disabling eFuses
prevent it.

Native MIDI 2.0 Note On messages use 16-bit velocity derived directly from impact
strength; MIDI 1.0 output retains the 7-bit mapping.

The host selects USB MIDI 1.0 event packets on alternate setting 0 or UMP on
alternate setting 1. UMP initially uses MIDI 2.0; Stream Configuration can select
MIDI 1.0 UMP instead. UMP transport does not by itself imply MIDI 2.0 notes.
Endpoint/Function Block discovery is implemented; MIDI-CI, SysEx8, JR timestamps,
and per-note expression are not advertised. Input logging is rate-limited without
skipping command parsing. Session changes discard queued bounces and partial
commands, with note cleanup before accepting new bounces.

| Host | Native path | Qualification |
| --- | --- | --- |
| Linux | [ALSA USB MIDI 2.0](https://docs.kernel.org/sound/designs/midi-2.0.html) (`CONFIG_SND_USB_AUDIO_MIDI_V2`), native UMP and legacy compatibility ports | Native notes, discovery, negotiation, legacy conversion, and recovery exercised on kernel `7.2.9-200.fc44.x86_64`, ALSA library `1.2.16.1` and tools `1.2.16`; limitations below |
| Windows | Updated Windows 11 25H2 or later with [Windows MIDI Services](https://microsoft.github.io/MIDI/); verify the actual driver/service and app | Untested |
| macOS | [CoreMIDI MIDI 2.0 event lists](https://developer.apple.com/documentation/coremidi/incorporating-midi-2-into-your-apps) and a compatible USB driver/app; conversion may serve legacy apps | Untested |

The board was flashed and qualified through ALSA's native raw UMP API, without
host-side note conversion. Captured notes included `40903900 38460000` (pitch 57,
velocity 14406), with matching Note Off and approximately 101-103 ms durations
measured at the host. Discovery reported both protocols and the expected endpoint,
identity, and Function Block. MIDI 1.0 UMP output, active-note cleanup during both
protocol changes, repeated discovery alongside bounce output, and rejection of
unsupported protocol/JR requests were exercised. Legacy `amidi` received converted
notes and triggered recovery; native SysEx7 also entered the ROM downloader.
Malformed/wrong-Group/timed-out commands were rejected, and discovery remained
responsive after sustained mixed input covering all UMP message-type lengths.
Native recovery while a note sounded delivered Note Off before disconnecting.
Direct USB tests also verified fragmented UMP discovery requests, including
12 retained bytes followed by a full 64-byte OUT transfer.
Reflashing restored the endpoint.

To monitor without requesting conversion, find the sequencer client with
`aseqdump -l`, then use `aseqdump -u 2 -r -p CLIENT:0`. The raw UMP node is
`/dev/snd/umpC*D*`; `amidi` uses a separate legacy compatibility port. Receiving
converted legacy notes does **not** prove USB alternate setting 0 was selected.

Direct libusb testing selected physical alt 0 in the new profile and verified
the bootloader command as four-byte USB MIDI 1.0 packets. Assisted tilt capture
also verified 41 complete alt-0 bounce note pairs, with channel 1, scale pitches,
7-bit velocity, and matching zero-velocity Note Off. These tests isolated
both interfaces of the Qualia audio function and restored native UMP without
unloading or reconfiguring the shared audio driver. Board-only USB permission
does not survive re-enumeration. Physical unplug/replug and board-only runtime
suspend/resume were also exercised: discovery and native notes recovered, and
an incomplete bootloader command could not be completed across either session
change. DAW playback remains unqualified.
Firmware builds and host tests cover codecs, descriptors, backpressure, and
session handling, including all three transport states. The USB tests compile
the production adapter and pinned TinyUSB class with Espressif's pinned FIFO
helpers and simulated USB endpoints, not physical USB. Full UMP specification
review is pending: the public USB transport specification and Microsoft's Stream
builders were checked, but the MIDI Association's full UMP download requires
member login. These checks are not certification.

For legacy-only Linux testing, the `snd_usb_audio` module's `midi2_enable=0`
selects the MIDI 1.0 alternate setting (a module-wide setting that can affect
other USB audio/MIDI devices; do not reload a shared driver just to test this board).
The `amidi` command above is a legacy raw-MIDI workflow, not a native UMP sender.
On a native UMP path send
the same command as SysEx7 payload, without F0/F7 delimiters:
`30167D51 55414C49`, then `30324101 00000000`. BOOT/RESET recovery remains available.

The profile fetches TinyUSB commit `3ab3f9209a27eac15c464e31caec355c7b06b5cb`
using a SHA256-verified archive in `cmake/Midi2.cmake`. Only the MIDI 2.0 class is
substituted into Espressif's pinned USB component; the USB core/controller stay
with that component. The shared build helper generates a class source copy with
a checked `SET_INTERFACE` fix that closes/reopens the endpoints, canceling
in-flight transfers and resetting data toggles on alternate selection and resume.
The fetched source is unchanged. Host USB tests fetch the matching Espressif core:
`894ea01409e0407a7dbe0ee29b2d58a4691f9046`, also SHA256-verified. Firmware and tests
use the same 128-byte RX and 64-byte TX FIFOs, so partial UMP input can re-arm OUT.
Do not edit downloaded sources. The development identity
uses Espressif VID `303A` and the upstream MIDI 2.0 example PID `4062`, with a
stable MAC-derived serial. Espressif documents
[TinyUSB default PID usage](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_vid_pid.html);
this is not USB certification or a claim of an exclusive commercial identity.

### Trace capture

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
Tests compile production physics, sensor, application scene, and view code.
Minimal SDK/LVGL stand-ins cover initialization failures, resource cleanup,
injected time, and trace output without hardware.

```sh
cmake -S tests -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUALIA_ASAN=ON
cmake --build build/host --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/host --output-on-failure
```

Trace analyzer tests require NumPy:

```sh
python -m unittest discover -s tests -p test_trace.py
```

## VS Code and clangd

Install the workspace's recommended extensions: **ESP-IDF**, **clangd**, and
**C/C++**. Clangd provides C/C++ completion, navigation, diagnostics, and
formatting using `.clang-format`; Microsoft IntelliSense is disabled, but its
extension is retained for the GDB debugger.

Configure the ESP-IDF extension for your installation. The workspace uses
Espressif's **esp-clangd**, not a host LLVM clangd, so it can parse the Xtensa
target. Install `esp-clangd` and `esp-clang` with ESP-IDF's tool manager if needed:

```sh
python "$IDF_PATH/tools/idf_tools.py" install esp-clangd esp-clang
```

On Windows PowerShell, use `$env:IDF_PATH` instead of `$IDF_PATH`. The
`clangd.path` and `--resource-dir` entries in `.vscode/settings.json` match the
default ESP-IDF 6.1 tool installation under `~/.espressif`. Adjust both for a
different tools directory or version. The resource directory is the output of
the installed **esp-clang** executable's `-print-resource-dir` command; it
supplies builtin headers that the standalone clangd package does not include.

Generate the firmware compilation database from an activated ESP-IDF terminal:

```sh
idf.py -B build/demo -D SDKCONFIG=build/demo/sdkconfig reconfigure
```

A normal build also generates `build/demo/compile_commands.json`. Clangd reads
the real include paths, defines, and language standards from this database and
queries the ESP32-S3 GCC driver for toolchain headers. `.clangd` removes only
GCC flags unsupported by clangd; it does not change firmware build flags.
Run **clangd: Restart language server** after changing its executable or arguments.

VS Code defaults to `build/demo`. When switching profiles, update
`idf.buildPath`, clangd's `--compile-commands-dir`, the fallback
`compileCommands` path in `.vscode/c_cpp_properties.json`, and the debug ELF path
in `.vscode/launch.json` together, then reconfigure the selected profile.

## Code ownership

The single `main` component separates SDK-independent algorithms in `core/`,
hardware access in `drivers/`, passive subsystems in `modules/`, LVGL views in
`gui/`, and application wiring in `app/`. Hardware drivers are static board
resources; modules outlive the tasks that step them. External C library APIs
stay at their integration boundaries.

The UI task owns non-copyable `app::Simulation` and optional `app::ImuOverlay`
objects. It passes monotonic timestamps and complete newest IMU data into them.
The simulation's first step anchors its fixed-step clock. Scene initialization
rolls back partial failures, and scoped views release their LVGL objects and
draw buffers. Create, update, and destroy views on the UI task while their active
screen is valid; destroy ball bodies before their physics world.
