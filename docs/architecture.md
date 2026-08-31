# Architecture

This is the living description of Tavernkeep's current firmware architecture.
Update it as the design changes, and preserve concise rationale for choices
that materially affect implementation or future work.

## System Overview

Tavernkeep is firmware for the RP2350-based Tome e-reader and MP3 player.

The firmware is divided into platform-level hardware support and higher-level subsystems responsible for storage, filesystems, book handling, audio playback, display output, physical input, USB functionality, and system operation.

Development is currently proceeding from the lowest-level hardware interfaces upward. Removable storage is the first substantial subsystem under implementation. Higher-level filesystem, EPUB, display, audio, USB, input, and power-management functionality remains planned or only scaffolded unless otherwise documented.

Tome's final hardware uses two e-ink displays and removable SD storage. The exact physical control scheme remains unresolved and must be settled before significant hardware development proceeds.

## Design Principles

**Implement hardware functionality from documented interfaces**

Firmware is developed from hardware datasheets, interface specifications, the Pico SDK, and general embedded-system principles rather than by adapting existing e-reader firmware implementations.

**Prefer simple implementations before optimization**

Subsystems should first be implemented in a straightforward blocking or polling form where practical. More complex mechanisms such as DMA, asynchronous operation, and PIO implementations should be introduced after basic functionality has been validated.

For storage, this means proving SD operation through conventional SPI before introducing DMA or the planned 4-bit SD interface.

**Preserve clear subsystem ownership**

Low-level hardware behavior should remain within the subsystem responsible for that hardware. Higher layers should operate on meaningful subsystem interfaces rather than depending on protocol-specific details where such separation is useful.

For example, SD-specific response values remain within the SD implementation while the generic block-device interface exposes generic storage results.

## System / Subsystem Boundaries

**Platform**

The platform layer owns board-level initialization and low-level facilities shared by other Tavernkeep subsystems.

Current scaffolded responsibilities include board support and debugging/logging support.

The exact long-term boundary of this layer has not yet been finalized.

**Storage**

The storage subsystem owns access to removable SD storage.

Its responsibilities include:

- initializing and deinitializing the storage interface

- issuing SD commands

- transferring fixed-size storage blocks

- handling SD addressing differences

- representing storage failures through the block-device interface

- exposing block-level access to higher layers

The current implementation uses SPI.

A separate 4-bit SD backend using the RP2350's programmable I/O hardware is planned for the final hardware.

Filesystem interpretation does not belong to the block-device backend.

**Filesystem**

The filesystem layer will operate above the block-device interface and provide file-level access to removable storage.

FatFs is planned for this layer but has not yet been integrated into the current Tavernkeep implementation.

The filesystem layer should not depend on whether the underlying SD implementation uses SPI or the planned 4-bit interface.

**EPUB/Book**

The book subsystem will be responsible for opening and navigating supported book content.

EPUB and plain-text files are required book formats.

The internal architecture of EPUB parsing, text layout, caching, and rendering has not yet been defined.

M4B audiobook support is a stretch feature and therefore does not currently form part of the required architecture.

**Audio**

The audio subsystem will be responsible for MP3 playback and audio output.

The final hardware is planned around a PCM5102A DAC and PAM8908 headphone amplifier.

The detailed software architecture for decoding, buffering, scheduling, and audio output has not yet been defined.

**Display**

The display subsystem will control Tome's two e-ink displays.

The dual-display configuration is fixed and defining hardware for Tome.

The detailed display-driver, framebuffer, update scheduling, and rendering architecture has not yet been defined.

**Input**

The input subsystem will translate Tome's physical controls into firmware input events or equivalent higher-level actions.

The final physical control scheme has not yet been selected. Two rotary encoders with push input have been explored, but the control mechanism remains an open design decision.

**USB**

USB mass-storage access to Tome's removable storage is a required feature.

The detailed USB architecture, including coordination between local filesystem access and USB host access to the same storage medium, has not yet been defined.

**Power / System Operation**

Tavernkeep will ultimately manage hardware initialization, shutdown, peripheral power state, and other system-level behavior.

Detailed power-management architecture has not yet been implemented.

## Major Interfaces and Abstractions

**Block Device**

block_device_t is the generic block-storage interface used to separate higher storage layers from the physical SD transport.

A block device contains a generic void *context used by a concrete backend to access its device-specific state.

The SPI SD implementation interprets this context as an already-created and configured sd_spi_t.

The abstraction exposes generic block-device results rather than SD protocol responses. SD-specific response interpretation remains within the SD backend.

Current block-device result categories include:

- success

- invalid argument

- not initialized

- out of range

- I/O error

- invalid device

- not implemented

INVALID_DEVICE represents the state in which no usable writable SD card is available.

**SPI SD Device State**

sd_spi_t contains both the fixed configuration required to communicate with an SD card and runtime state learned while communicating with the card.

Static configuration includes the SPI peripheral and relevant GPIO configuration.

Runtime state currently includes:

- initialization state

- whether the card follows the legacy SD initialization path

- whether the card uses SDHC/SDXC block addressing

Card type is maintained per device rather than globally.

A non-legacy SD card is not assumed to be SDHC/SDXC; version and addressing mode are treated as separate properties.

*Other subsystem interfaces*

Interfaces between the filesystem, EPUB, display, input, audio, USB, and power-management subsystems have not yet been finalized and should be documented here as they become concrete.

## Data Flow

**Storage reads**

The intended storage read path is:

SD card
→ physical SD transport
→ block-device interface
→ filesystem
→ consuming subsystem

Consuming subsystems will include at least the book and audio systems.

The block-device layer operates in logical 512-byte blocks.

For SDHC/SDXC cards, logical block addresses are transmitted directly to applicable SD commands.

For SDSC cards, logical block addresses are converted to byte addresses before being sent to the card.

**Book content**

The intended book-content path is:

removable storage
→ block device
→ filesystem
→ EPUB/book subsystem
→ rendering
→ dual-display output

The detailed buffering and rendering stages have not yet been designed.

**Audio**

The intended music path is:

removable storage
→ block device
→ filesystem
→ MP3 decoding/playback
→ digital audio output
→ PCM5102A
→ PAM8908
→ headphones

Detailed buffering and concurrency behavior remain to be designed.

**USB storage**

USB mass-storage operation will expose Tome's removable storage to an attached USB host.

Ownership and coordination rules between USB mass storage and Tavernkeep's own filesystem access remain unresolved.

## Resource Ownership

Detailed system-wide resource ownership has not yet been assigned.

Current storage ownership is more clearly defined:

- the SPI SD backend owns the SPI peripheral and GPIO resources configured for its SD interface while that device is initialized

- the SD device structure retains its static configuration across initialization and deinitialization

- successful SD deinitialization releases the SPI peripheral and associated GPIO configuration

- a failed clean deinitialization may deliberately leave the SD device initialized and its hardware resources configured so that shutdown can be retried or handled by a separate forced-shutdown path

Future ownership rules will be required for:

- DMA channels

- PIO state machines

- shared buffers

- display resources

- audio buffers and peripherals

- removable-storage ownership during USB mass-storage operation

- switchable peripheral power domains

These have not yet been assigned.

## Error Handling Philosophy

Subsystem-specific errors should be interpreted as close as practical to the subsystem that understands them.

The SD backend therefore interprets SD protocol responses internally and maps them onto the generic block-device result type before returning errors to higher storage layers.

Protocol-specific diagnostic information may be logged internally without becoming part of the generic block-device API.

Errors that can be confidently represented generically, such as an out-of-range block request, may be returned using the corresponding block-device result. Other command, response, transfer, or timeout failures normally collapse to an I/O error at the generic storage boundary.

Storage operations use finite timeouts rather than waiting indefinitely for hardware responses.

Once an SD transaction has asserted chip select, the implementation is responsible for restoring the bus to its released state on all completed transaction paths.

A normal device shutdown is distinguished from a forced shutdown. The current SD deinitialization behavior attempts to leave the device operational if the card cannot be cleanly brought to a ready state rather than partially tearing down the peripheral and losing the ability to retry.

Broader error-handling and recovery policy for other Tavernkeep subsystems remains to be defined as those subsystems are implemented.

## Concurrency / Execution Model

Tavernkeep does not use an RTOS.

Current SD communication is synchronous and blocking. Individual SPI transfers and SD commands complete in the caller's execution context.

The initial storage implementation intentionally uses polling rather than DMA.

DMA is planned for bulk storage transfers after the polling implementation has been validated.

Interrupt-driven or asynchronous operation will be introduced where required by later subsystems, but a complete system-wide execution and scheduling model has not yet been defined.

Audio playback is expected to impose stronger real-time data-flow requirements than the current storage work, but the mechanism by which audio, display updates, storage access, user input, and other work will coexist has not yet been selected.

## Important Design Decisions and Rationale

Keep consequential decisions here unless the project eventually becomes large
enough to justify a separate decision-record system.

### Decision — Develop directly on the Pico SDK without an RTOS

- Context: Tavernkeep is being developed as firmware specifically for Tome's RP2350 platform.

- Decision: Use the Raspberry Pi Pico SDK directly and do not introduce an RTOS.

- Rationale: The project is intended to implement and understand the underlying embedded mechanisms directly rather than placing system behavior behind an operating-system abstraction.

- Alternatives considered: RTOS-based development has not been selected.

- Consequences and tradeoffs: Tavernkeep must explicitly manage execution, asynchronous work, interrupts, peripheral ownership, and any required scheduling behavior.

### Decision — Implement SD access through SPI before the final 4-bit interface

- Context: The current development hardware provides an SPI-accessible microSD breakout, while Tome's final hardware is intended to use a full-size SD card and eventually a wider 4-bit native SD interface.

- Decision: Implement and validate the SD stack over SPI first.

- Rationale: SPI provides a simpler path for proving command handling, addressing, block transfers, filesystem integration, and higher storage layers before introducing the additional complexity of a PIO-based interface.

- Alternatives considered: Implementing the 4-bit interface first.

- Consequences and tradeoffs: Tavernkeep temporarily contains an SD transport that is not intended to be the final high-performance implementation, making a transport-independent block-device boundary useful.

### Decision — Use a block-device abstraction for storage

- Context: Tavernkeep is expected to operate with both the current SPI SD implementation and a planned 4-bit SD implementation.

- Decision: Higher storage layers operate through block_device_t rather than directly through an SD transport implementation.

- Rationale: The filesystem and higher-level content systems should not need to be rewritten when the SD transport changes.

- Alternatives considered: Allow the filesystem layer to call the SD SPI implementation directly.

- Consequences and tradeoffs: A small abstraction boundary is introduced, but portability to arbitrary unrelated hardware is not itself a Tavernkeep design requirement.

### Decision — Begin with polling before introducing DMA

- Context: Reliable SD command and block-transfer behavior must be established before optimizing transfer mechanisms.

- Decision: Implement the initial SD driver synchronously using polling.

- Rationale: This keeps protocol correctness and hardware bring-up easier to observe and debug.

- Alternatives considered: Implement DMA as part of initial SD bring-up.

- Consequences and tradeoffs: Initial transfer performance is not representative of the eventual optimized implementation. DMA will be introduced later without changing the block-level behavior exposed to higher layers.

### Decision — Treat card absence and hardware write protection as the same unavailable-device state

- Context: Tome's final full-size SD socket uses card-detect and write-protect switch information through the same GPIO in order to conserve GPIO resources. Tome requires writable storage for persistent device state.

- Decision: Firmware does not distinguish between an absent card and a write-protected card. Either condition means that a usable storage device is unavailable.

- Rationale: Read-only operation is not a required Tome operating mode, so distinguishing the two states provides no required functionality.

- Alternatives considered: Allocate separate GPIO inputs for card detection and write protection.

- Consequences and tradeoffs: Tavernkeep cannot inform the user which of the two physical conditions caused the device to become unavailable.

### Decision — Dual displays are fixed, the physical control scheme is not

- Context: Tome's two-display format is a defining product characteristic. The originally explored dual-encoder input system may not necessarily provide the best user experience.

- Decision: Design Tavernkeep around two e-ink displays while leaving the physical control interface open until it is deliberately selected.

- Rationale: Display count is fixed by the product concept, while changing the controls remains possible if another scheme materially improves interaction.

- Alternatives considered: Treating both display and control hardware as fixed.

- Consequences and tradeoffs: Input architecture should not become unnecessarily coupled to an unfinalized physical control arrangement during early development.

## Hardware Assumptions Relevant to Firmware

**Processor**

Tome is based on the RP2350.

Firmware is developed using the Raspberry Pi Pico SDK.

Current development uses a Raspberry Pi Pico 2.

**Removable storage**

The final Tome hardware uses a full-size SD card socket.

Current development uses a microSD breakout accessed through SPI.

The current SD implementation begins communication at approximately 400 kHz and increases SPI speed only after successful card initialization.

Tome operates SD cards from a 3.3 V interface.

The final socket's write-protect switching is used as one firmware-visible availability signal that also incorporates card-detect (this decision was made based on the particular socket's truth table so if a hardware change happens here this may need to be reevaluated).

An external pull-up is provided for this signal.

**Displays**

Tome uses two e-ink displays.

The currently selected displays are Good Display GDEH0576T81 panels using the SSD2677 controller.

The dual-display configuration is a fixed architectural assumption.

Detailed firmware-side display timing, buffering, update strategy, and memory requirements have not yet been established.

**Audio**

The planned audio path uses:

- PCM5102A digital-to-analog conversion

- PAM8908 headphone amplification

Audio data will be supplied digitally to the DAC.

The detailed firmware timing and buffering requirements have not yet been established.

**Physical controls**

The physical control scheme remains unresolved.

Two rotary encoders with push input have been explored but are not currently a fixed architectural assumption.

**USB**

Tome provides USB connectivity and is required to support USB mass-storage access to removable storage.

The detailed firmware ownership and synchronization model for this access remains unresolved.

## Known Architectural Limitations

The current firmware architecture is incomplete because development is still concentrated on storage bring-up.

At the current stage:

- only the SPI SD transport is being actively implemented

- the planned 4-bit SD backend does not yet exist

- FatFs has not yet been integrated

- DMA-based storage transfer has not yet been implemented

- the filesystem-to-USB ownership model has not yet been defined

- the final physical input hardware has not been selected

- EPUB parsing and rendering architecture has not yet been defined

- display buffering and update scheduling have not yet been defined

- MP3 decoding and audio-buffer scheduling have not yet been defined

- the overall asynchronous/concurrency model beyond the current blocking storage implementation has not yet been defined

- system-wide power-management architecture has not yet been defined

These are development-state limitations rather than necessarily permanent restrictions.

## Planned / Future Architecture

**4-bit SD backend**

A separate SD backend using the RP2350's PIO resources is planned for the final Tome hardware.

It is intended to provide the same block-device behavior to higher layers as the SPI backend so that filesystem and application code do not depend on the physical SD transport.

SPI support is intended to remain useful for hardware bring-up even after the 4-bit backend is introduced.

**DMA-backed storage transfer**

After the polling SD implementation has been validated, bulk block transfers are planned to use DMA.

The exact asynchronous API and DMA ownership model have not yet been designed.

**Filesystem integration**

FatFs is planned above the block-device interface.

The adapter between FatFs and block_device_t has not yet been implemented.

**Input/events**

A general input/event mechanism is expected to be required as Tavernkeep gains display, audio, and physical controls, but its structure should be designed after the physical control scheme and actual subsystem needs become clearer.

**Displays**

Tavernkeep will require a driver and higher-level rendering path for both e-ink displays.

Framebuffer strategy, partial-update policy, rendering ownership, and display scheduling remain to be determined.

**EPUB**

Tavernkeep will require EPUB file access, parsing, layout, navigation, persistent reading position, and bookmark support.

The internal architecture for these functions has not yet been designed.

**Audio**

Tavernkeep will require MP3 decoding and continuous delivery of audio to the hardware audio path.

Buffering, DMA use, timing, decoder selection or implementation, and interaction with simultaneous storage/display activity remain to be determined.

**USB mass storage**

USB mass-storage access to removable storage is required.

A safe ownership model between the USB host and Tavernkeep's own filesystem access must be defined before this feature is implemented.

**Power management**

Power management will eventually coordinate processor activity, peripheral power state, display behavior, audio hardware, removable storage, and system shutdown.

The detailed architecture remains to be designed.
