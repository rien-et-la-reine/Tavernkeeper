# Tavernkeep RP2350 firmware

Tavernkeep is a learning and portfolio firmware project for an eventual custom
RP2350-based device. This repository currently contains only a clean bring-up
scaffold: Pico SDK initialization, lightweight logging, an optional status LED
heartbeat, and explicit storage interfaces.


## Status and roadmap

Current status: bring-up firmware plus an SPI SD block-device backend. The
firmware initializes Pico SDK stdio, prints a startup banner, blinks the Pico 2
status LED when the selected board defines one, and owns a GPIO interrupt
dispatcher. The SD backend implements card detection with debounce, SDSC/SDHC/
SDXC initialization, CSD capacity parsing, single- and multiple-block reads with
bounded timeouts, and latched hot-removal handling. Writes and `get_info` are
explicit stubs, the read data CRC is not yet validated, and the filesystem entry
points still return not-implemented. None of this has been validated on
hardware; see `tests/` for what the host suite does and does not establish.

Planned work, none of which is implemented yet:

1. SD block writes, data CRC validation and public capacity reporting
2. FatFs integration through a disk I/O adapter
3. Native 4-bit SD using PIO on the custom hardware
4. USB mass-storage support and explicit media ownership
5. E-paper display driver and update scheduling
6. Encoder/button input and event handling
7. I2S audio transfers using DMA
8. MP3 and audiobook streaming/playback
9. Streaming EPUB/ZIP/XHTML parsing and layout
10. Power-management policy and low-power states
