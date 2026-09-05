# Repository assessment and validation plan

Reviewed 2026-09-04 and revised 2026-09-05 after the test-infrastructure
overhaul: production sources, build configuration, README,
`docs/requirements.md`, `docs/architecture.md`, `docs/validation.md`, and tests.
No future product subsystem is implemented by this suite.

This document is forward-looking: it records what evidence each planned feature
will owe. For what the current harness can and cannot represent see
[HARNESS.md](HARNESS.md) and [RESIDUAL_RISK.md](RESIDUAL_RISK.md); for what has
actually been demonstrated see [VALIDATION_RESULTS.md](VALIDATION_RESULTS.md).

## Current implementation and evidence

Source determines implementation status; the root README predates current
storage work. Requirements/architecture describe intentions, not proof.

| Area | Implementation | Host evidence |
| --- | --- | --- |
| Boot/system | Board, logging and IRQ initialization; cooperative heartbeat | Real loop for 1.1 seconds of fake time, including initialization failures; separate board/logging compile-time variants |
| GPIO dispatcher | One SDK callback on the owning core, per-pin handlers and masks | Validation, wrong-core operations, two GPIO users, callback filtering/self-removal and nested critical sections; real SD integration |
| Block-device API | Validation and backend dispatch | Every wrapper and missing callback; context/buffer/LBA/count forwarding including 64-bit extremes; all eight result categories |
| SD SPI | Debounced active-low availability; rollback; legacy/v2 initialization; CSD v1/v2 capacity; single/multiple 512-byte reads; bounded waits; removal latch and cleanup | Four suites against a stateful card model: card-variant matrix, command ordering, CRC7 and application-command enforcement, the R1 poll boundary, all 128 R1 values, all 15 error tokens with immediacy bounds, multiple-block reads of arbitrary length, addressing and capacity boundaries, CSD registers from real cards, phase-based fault injection with recovery assertions, and seeded property and fuzz tests |
| SD info/writes | Internal capacity parsed; public info/writes remain stubs | `NOT_IMPLEMENTED`, preconditions and no SPI activity; no write success claim |
| Filesystem | Prepare/bind state; mount/unmount stubs | Validation, state preservation, rebinding, repeat stub calls and no backend invocation |
| Other product subsystems | Planned | Acceptance matrix below; no passing feature placeholders |

Data is compared with expected bytes only after successful reads. After failure
the entire requested destination is unspecified, so the tests assert how much of
it was populated rather than what it contains. Outside-buffer canaries remain
valid checks even on interrupted transfers.

Removal coverage is both byte-based and phase-based: every SPI byte of a
single- and two-block read and of successful SDHC initialization, plus an
injected ejection at each named transaction phase including the final release
clock. Each verifies bus release, immediate rejection of new operations, no
CMD12 to an absent card, no teardown inside the interrupt handler, interrupt
suppression, exactly-once hardware release, and that reinsertion requires a
fresh initialization. Discrete injection is not exhaustive testing of
machine-instruction races or of multicore execution.

The three disabled regressions in [KNOWN_GAPS.md](KNOWN_GAPS.md) are failed
contract evidence and must accompany any report of a passing enabled suite.
Read data CRC validation, writes and public capacity reporting remain
explicitly unimplemented.

## Future acceptance matrix

Add executable tests with each real public API as it is implemented. Use small,
deterministic fixtures under `tests/fixtures`, recording provenance; avoid full
books/music or large disk images. Do not implement mock product subsystems just
to make this matrix executable.

| Requirement / subsystem | Future host tests and observable outcomes | Integration / hardware acceptance |
| --- | --- | --- |
| FR-009 storage completion | Public info matches CSD and 512-byte sectors; CMD24/CMD25 framing/addressing; accepted/rejected data-response tokens; busy bounds, stop token and cleanup; known CRC vectors and corruption rejection | Disposable media; first/last-sector independent readback hashes; capture initialization and writes at configured SPI rates |
| FR-009 FatFs/diskio | Real adapter over test memory blocks; error/status mapping, sector size/capacity, multi-sector forwarding, range checks and `CTRL_SYNC`; mount/read/write/unmount/reopen a small known filesystem; corrupt/truncated media and injected I/O errors | Host-created files readable by firmware and vice versa; no false mounts/stale handles; define write completion before sync acceptance |
| FR-009/012 removal coordinator | Idle/init/wait/read/write removal; consumer cancellation, handle invalidation, discard whole failed read; exactly-once cleanup after unwinding; bounce still requires fresh init | Remove/write-protect during each phase; no blocking cleanup in ISR, no flush to absent media or CMD12 after cancellation; unrelated input/UI remains responsive |
| FR-009 PIO/4-bit and DMA | Reuse generic storage contracts; register traces for active/cancel/quiesce/completion; no late success; abort ordering and all chained channels disabled | Logic-analyzer traces; removal during chains/FIFO activity; canaries; no late memory writes or retriggering; RP2350-E5 procedure on actual SDK/silicon |
| FR-010 USB mass storage | Explicit local/USB ownership state machine; reject competing ownership; SCSI bounds/mapping; eject/disconnect/removal races; invalidate local cache/handles | Host copy/eject/reconnect on supported OSes; no simultaneous writable mounts; hashes intact after clean handoff |
| FR-001 EPUB / FR-002 text | Small Unicode/plain-text/ZIP/EPUB fixtures; line endings/empty files; malformed archives, unsupported compression/encryption, missing manifests, spine order; streaming bounds, malformed XHTML, ZIP expansion limits and deterministic navigation/layout | Representative local books with bounded memory; malformed content fails locally and returns to navigation |
| FR-003 reading state / FR-004 bookmarks | Versioned persistence round-trip, boundary positions, multiple books, create/delete; corrupt/truncated state and failure at every persistence stage | Restart/power-cycle after checkpoints; recover last committed state; removal cannot falsely commit |
| FR-005 MP3 / FR-006 controls | Decoder vectors/PCM hashes with documented rounding; malformed/truncated frames, tags/seeking; pause/resume/skip/volume transitions; buffer wrap, storage errors and underrun recovery | I2S to PCM5102A and amplifier behavior; continuous audio during storage/display/input load; underrun/seek measurements |
| FR-007 dual displays / CON-001 | Golden command/render fixtures for both panels; clipping/rotation, independent busy/error states, bounded BUSY waits and update coalescing | SSD2677/GDEH0576T81 reset/update sequencing, dual-panel output and current against selected hardware documentation |
| FR-008 input / OPEN-001 | After controls are selected: timestamped bounce, press/hold/release or quadrature traces; queue overflow/order; failures cannot steal other IRQs | Input-to-event latency during display/audio load; test selected controls without assuming dual encoders |
| FR-011 navigation | Empty/corrupt/large directories, selection bounds, removed files, book/audio transitions and unsupported-content recovery | Browse/select entirely local content; media replacement while menus are open |
| FR-012 system/power | Initialization failure matrix, cooperative progress, clean/forced shutdown order, wake events, cancellation before power-off and idempotent ownership | Repeated startup/shutdown/suspend/wake and interrupted shutdown with instrumented power rails |
| STRETCH-001 M4B | Only after scope acceptance: chapter/container fixtures, seek/resume, persistence and malformed input | Long audiobook seek/resume and playback; do not count stretch work as a required delivered feature |

FatFs low-level sector and cache completion expectations follow the official
[disk read](https://elm-chan.org/fsw/ff/doc/dread.html),
[disk ioctl](https://elm-chan.org/fsw/ff/doc/dioctl.html) and
[integration notes](https://elm-chan.org/fsw/ff/doc/appnote.html).
In particular, `CTRL_SYNC` must complete pending cached writes; tests must
reflect the eventual write-completion contract.

GPIO ownership and future DMA tests follow the
[Pico SDK hardware reference](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html):
ordinary GPIO callbacks are per core; RP2350-E5 requires clearing enable on the
aborted DMA channel and chained channels before abort. Host register-order
checks need physical validation too. For future protocol fixtures, pin an SD
Physical Layer revision from the
[SD Association archive](https://www.sdcard.org/downloads/pls/archives/)
and record the sections used for golden expectations.

## Cross-cutting requirements

| Requirement | Evidence to add |
| --- | --- |
| NFR-001 responsiveness | Input-to-action latency under supported concurrent load. Agree on a numerical limit first; fake time is not a latency measurement. |
| NFR-002 audio continuity | DMA/buffer underrun counts and audio capture during reading, display and input workloads; define duration/load. |
| NFR-003 integrity | Corruption fixtures, I/O fault/commit-boundary injection, independent hashes and disposable-media power-cut trials. |
| NFR-004 isolation | Book/audio/display/storage failures leave unrelated work progressing; current shared IRQ integration is partial evidence only. |
| NFR-005 resources | Host sanitizers where supported; target map, stack high-water marks, buffer bounds and allocation ceilings on worst supported content. Set budgets before acceptance. |
| NFR-006 power | Instrumented current/energy for defined active/idle/sleep workloads and unused peripheral inactivity. |
| NFR-007 offline | Future product flows with network absent. Today's no-download host suite alone does not prove future offline product functionality. |
| NFR-008 recovery | Fault/retry, removal/reinsertion and peripheral recovery without unnecessary full reset; ownership never leaked or released twice. |

## Hardware procedure (planned, not performed)

Record firmware revision and dirty-tree state, SDK/toolchain, board revision,
wiring/pull-ups, card model/capacity, instruments and raw traces. Capture
CS/SCK/MOSI/MISO/availability for initialization, single/multiple reads,
timeouts and removal. Verify idle clocks, command bytes, selection/release,
operating frequency and pin direction against the selected specifications.
Include v1 SDSC, v2 SDSC and SDHC/SDXC where available.

Compare reads against independent host data. Repeat with switch bounce,
removal at each phase, reinsertion and write protection. Observe that ISRs do
no blocking protocol cleanup and foreground cleanup waits for operations to
unwind. Fake byte boundaries cannot prove physical IRQ or SPI timing.

Write/corruption/power-cut trials require disposable media and a recorded
restore image once those paths exist. No physical hardware tests were performed
during this review.
