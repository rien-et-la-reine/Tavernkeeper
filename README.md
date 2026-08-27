# Tavernkeep RP2350 firmware

Tavernkeep is a learning and portfolio firmware project for an eventual custom
RP2350-based device. This repository currently contains only a clean bring-up
scaffold: Pico SDK initialization, lightweight logging, an optional status LED
heartbeat, and explicit storage interfaces. It does **not** implement product
functionality.

## Current development hardware

- Raspberry Pi Pico 2 (RP2350, Arm Cortex-M33 target)
- Raspberry Pi Debug Probe connected over SWD
- SPI microSD breakout board (driver not yet implemented)

The firmware is bare-metal and cooperative; no RTOS is used.

## Required tools

- Git
- CMake 3.13 or newer
- A build tool supported by CMake (Ninja is recommended)
- Raspberry Pi Pico SDK 2.0.0 or newer, with its submodules initialized
- Arm GNU Toolchain providing `arm-none-eabi-gcc` and `arm-none-eabi-gdb`
- An OpenOCD build containing `target/rp2350.cfg`

Raspberry Pi currently recommends its Pico VS Code extension as the easiest
way to obtain an integrated toolchain, OpenOCD, GDB, and device definitions.
VS Code is optional; all build and debug operations can be run from a terminal.

This project never fetches the SDK automatically. Set `PICO_SDK_PATH` to an
existing local SDK checkout instead. No machine-specific SDK path is committed.

PowerShell:

```powershell
$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"
```

POSIX shells:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
```

Alternatively, pass `-DPICO_SDK_PATH=/path/to/pico-sdk` while configuring.

## Build

Configure a fresh Debug build for the Pico 2 Cortex-M33 target and build it:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build --parallel
```

`PICO_BOARD` and `PICO_PLATFORM` already have those defaults, but they are shown
explicitly so the selected hardware and processor architecture are obvious.
Debug builds retain source-level information for GDB. The project does not
force an optimization level beyond the Pico SDK/CMake Debug defaults.

Expected primary outputs are:

- `build/tavernkeep.elf` for SWD flashing and debugging
- `build/tavernkeep.uf2` for BOOTSEL drag-and-drop flashing

Project sources compile with `-Wall -Wextra -Wpedantic`. Warnings are not
promoted to errors and the options are not applied to Pico SDK sources.

### Build options

- `-DTAVERNKEEP_ENABLE_LOGGING=OFF` compiles out the logging macros.
- `-DTAVERNKEEP_STDIO_UART=ON` enables UART stdio (the default).
- `-DTAVERNKEEP_STDIO_USB=ON` enables USB CDC stdio (off by default).

The Debug Probe UART bridge can capture the default Pico SDK UART stdio. SWD
debugging itself does not carry `printf` output. Keep target and probe grounds
common and follow the Debug Probe documentation for SWD/UART wiring.

## Flash

### UF2 / BOOTSEL

Hold BOOTSEL while connecting the Pico 2, then copy
`build/tavernkeep.uf2` to the mounted RP2350 boot volume.

### Debug Probe / SWD

With the target powered and SWD wired, flash the ELF through OpenOCD:

```sh
openocd -f debug/openocd.cfg -c "program build/tavernkeep.elf verify reset exit"
```

The checked-in configuration uses the Debug Probe's CMSIS-DAP interface and
the RP2350 target file. It deliberately does not use the older RP2040 target.

## Command-line debugging

Start OpenOCD in one terminal:

```sh
openocd -f debug/openocd.cfg
```

In a second terminal:

```sh
arm-none-eabi-gdb build/tavernkeep.elf
```

At the GDB prompt, a typical first session is:

```gdb
target extended-remote localhost:3333
monitor reset halt
load
monitor reset halt
break main
continue
```

Useful GDB commands include:

```gdb
next
step
print variable_name
info locals
info registers
x/16wx address
monitor reset halt
monitor reset run
continue
```

Use `Ctrl-C` to halt a running target and return to the GDB prompt. `detach`
then `quit` leaves the target running. The provided `.vscode/launch.json` and
`.vscode/tasks.json` offer the same basic flow with Cortex-Debug/OpenOCD when
those tools are available on `PATH`; VS Code is not required.

## Repository structure

```text
CMakeLists.txt              Pico SDK target and project build
cmake/                      SDK discovery glue
debug/                      Portable OpenOCD configuration
src/main.c                  Minimal cooperative bring-up loop
src/platform/               Pico board and lightweight stdio logging
src/storage/                Block-device API and unimplemented SD/FS shells
src/input/                   Reserved for input/event handling
src/display/                 Reserved for the display subsystem
src/audio/                   Reserved for audio
src/epub/                    Reserved for host-testable EPUB logic
tests/                       Future desktop-hosted tests
third_party/                 Future vendored dependencies
```

Hardware-independent modules should not include Pico SDK headers. In
particular, future ZIP/EPUB/XHTML parsing and layout code should remain usable
from a desktop test target. Physical storage backends implement
`block_device_t`, so the intended boundary is:

```text
application / FatFs
        |
FatFs diskio adapter (future: src/storage/fatfs_diskio.c)
        |
block_device_t
        |
SPI SD now, or native 4-bit PIO SD on the custom PCB later
```

When FatFs is added under `third_party/fatfs/`, its `diskio` adapter should sit
in `src/storage/` and depend only on `block_device_t`. FatFs and application code
must not include `sd_spi.h` or know which physical backend is active.

## Status and roadmap

Current status: bring-up firmware only. It initializes Pico SDK stdio, prints a
startup banner, and blinks the Pico 2 status LED when the selected board defines
one. Storage and filesystem entry points explicitly return not-implemented or
not-initialized results; no card protocol or fake filesystem behavior exists.

Planned work, none of which is implemented yet:

1. SPI SD bring-up with bounded retries/timeouts
2. FatFs integration through a disk I/O adapter
3. Native 4-bit SD using PIO on the custom hardware
4. USB mass-storage support and explicit media ownership
5. E-paper display driver and update scheduling
6. Encoder/button input and event handling
7. I2S audio transfers using DMA
8. MP3 and audiobook streaming/playback
9. Streaming EPUB/ZIP/XHTML parsing and layout
10. Power-management policy and low-power states
