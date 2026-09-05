# Validation

This document records evidence about Tavernkeep behavior. Its purpose is to
keep the existence of code distinct from demonstrated correctness.

Use the following status terms where appropriate:

- **PLANNED** — the behavior or validation method has been identified.
- **IMPLEMENTED** — supporting implementation exists, without implying that it
  has been tested successfully.
- **HOST TESTED** — the recorded behavior has been exercised successfully in a
  host-side test environment.
- **HARDWARE VALIDATED** — the recorded behavior has been demonstrated on the
  identified target hardware and under the stated conditions.

These statuses are descriptive, not a mandatory progression. A feature need
not pass through every status.

## Validation Philosophy / Method

Evidence is chosen to match the risk. Protocol logic, arithmetic, state
machines and error recovery are validated on the host, where a card can be made
to behave in ways a real one rarely does on demand. Anything physical —
electrical timing, pad state, real card behaviour, DMA and PIO cancellation,
power — is not claimed until it has been demonstrated on target hardware.

Host evidence is kept honest three ways. Simulated time advances with modelled
bus work rather than with poll counts, so a timeout assertion is about elapsed
time and not about how often a function was called. Mutation testing measures
whether the suite would notice the code being wrong, rather than whether it
merely executed. And behaviour the driver does not yet have is recorded as a
failing, deliberately disabled regression rather than removed, so a green run
is never mistaken for completeness.

Coverage is used to find unexercised paths, not as an acceptance criterion.

Repeatability: every randomised test is seeded and prints its seed, every
result below names the toolchain that produced it, and the diagnostics are
scripted in `tests/tools/run_diagnostics.sh`.

### Validation record format

Use a concise record for each meaningful behavior or claim:

<!--
### VAL-NNN — Feature or behavior

- Test method:
- Environment / hardware:
- Expected result:
- Observed result:
- Status: PLANNED | IMPLEMENTED | HOST TESTED | HARDWARE VALIDATED
- Relevant commit / test / reference:
-->

## Validation Environment

All records below are host-side. No target hardware has been used.

- Host suite: `tests/`, built with CMake 3.16 or newer against small Pico SDK
  fakes; no SDK, card or firmware build required.
- Toolchains exercised: GCC 13.3.0 and Clang 18.1.3 on Linux x86-64;
  GCC 15.2.0 (MSYS2 MinGW) on Windows.
- Simulated time is derived from SPI byte duration at the programmed baud rate
  rather than from poll counts. It bounds how long a loop takes; it is not a
  throughput or latency measurement.
- Randomised tests are seeded and print their seed; `--seed N` replays a run.
- Diagnostics are scripted in `tests/tools/run_diagnostics.sh`.

## Host-Side Tests

### VAL-001 — SD SPI bring-up across supported card generations

- Test method: `sd_protocol_host_tests`, `sd_spi_host_tests`
- Environment: host suite, GCC 13.3.0 / Linux and GCC 15.2 / MinGW
- Expected result: v1 SDSC, v2 SDSC, SDHC and SDXC initialize with the correct
  addressing mode and capacity; SDUC (CSD v3) and an MMC-like card are refused
  without leaving hardware configured; the command sequence and both SPI clock
  rates are as designed
- Observed result: as expected
- Status: HOST TESTED
- Reference: `tests/test_sd_protocol.c`, `tests/PROTOCOL.md` P-01 to P-11

### VAL-002 — CSD capacity parsing against registers from real cards

- Test method: `sd_protocol_host_tests`, `sd_property_host_tests`
- Environment: host suite
- Expected result: five CSD registers captured from real cards decode to their
  marketed capacities; every CSD v1 field encoding and the CSD v2 C_SIZE range
  produce the capacity the specification formula gives; invalid structures and
  READ_BL_LEN values are refused
- Observed result: as expected. Each vector's own CRC7 is verified before use;
  one candidate vector was discarded because its CRC7 did not check out
- Status: HOST TESTED
- Reference: `tests/PROTOCOL.md` P-06

### VAL-003 — block and byte addressing, and the address conversion boundaries

- Test method: `sd_protocol_host_tests`, `sd_property_host_tests`
- Environment: host suite
- Expected result: high-capacity cards receive block addresses and
  standard-capacity cards receive byte addresses; the largest CSD v1 card's last
  block address (0xFFFFFE00) and the largest high-capacity card's last LBA
  (0xFFFFFFFF) reach the card untruncated; out-of-range and straddling requests
  are refused before any bus activity
- Observed result: as expected
- Status: HOST TESTED
- Reference: `tests/PROTOCOL.md` P-10

### VAL-004 — error recognition is immediate, not a timeout

- Test method: `sd_protocol_host_tests`, `sd_faults_host_tests`
- Environment: host suite, simulated time driven by bus work
- Expected result: all fifteen data error tokens are recognised on the byte they
  arrive on both read paths, in under a microsecond and under forty bus bytes;
  all 128 R1 values are rejected except 0x00; a card that answers R1 and then
  never produces a data token costs exactly one 100 ms wait and no retry
- Observed result: as expected
- Status: HOST TESTED
- Reference: `tests/PROTOCOL.md` P-05

### VAL-005 — failure, cancellation and recovery

- Test method: `sd_faults_host_tests`, `sd_property_host_tests`
- Environment: host suite
- Expected result: for every fault kind at every read phase, the operation
  terminates inside the driver's declared budgets, releases chip select, writes
  nothing outside the destination, leaves the device usable, and releases
  hardware exactly once on teardown. Removal at any phase fails with
  INVALID_DEVICE, sends no CMD12 to an absent card, performs no teardown in the
  interrupt handler, suppresses repeat edges, and requires a fresh
  initialization before the card can be used again
- Observed result: as expected, after the two production fixes recorded under
  Validation Issues below
- Status: HOST TESTED
- Reference: `tests/KNOWN_GAPS.md`

### VAL-006 — the suite detects deliberate defects

- Test method: `python3 tests/tools/mutate.py`
- Environment: host suite, GCC 13.3.0 Release; one mutation additionally under
  ASan/UBSan
- Expected result: representative incorrect implementations are caught
- Observed result: 47 of 50 detected, 2 documented equivalent mutants, 1
  survivor caught only under the sanitizer configuration
- Status: HOST TESTED
- Reference: `tests/MUTATION.md`

### VAL-007 — memory safety and defined behaviour under the host suite

- Test method: AddressSanitizer and UndefinedBehaviorSanitizer, plus a strict
  warning set treated as errors, plus `gcc -fanalyzer`
- Environment: GCC 13.3.0, `-fno-sanitize-recover=all`, `detect_leaks=1`
- Expected result: no reports
- Observed result: no reports; 22/22 cases pass under sanitizers
- Status: HOST TESTED
- Reference: `tests/VALIDATION_RESULTS.md`

## Hardware Validation

<!--
Record demonstrations performed on target hardware. Identify the hardware and
conditions precisely enough to understand the scope of each result.
-->

## Measurements / Performance Characterization

<!--
Record measured timing, throughput, memory, power, signal, or other quantitative
results. Include method, equipment, configuration, uncertainty or limitations,
and expected limits where applicable.
-->

## Known Untested or Partially Tested Behavior

Enumerated with consequences and proposed closure in
`tests/RESIDUAL_RISK.md`. In summary:

- Nothing here has been demonstrated on target hardware. No bus capture, no
  real card, no power measurement.
- Interrupts can only be injected between SPI bytes or at a named transaction
  phase, never between two machine instructions.
- The interrupt fake models one executing core at a time, so the multicore
  memory ordering the architecture anticipates for the removal latch is
  untested.
- The card model is a reading of the specification. A card that is out of
  specification is not represented; two such deviations are already known from
  field reports and are recorded as SD-004 and SD-005.
- Read data CRC is not validated by the driver, so payload corruption is
  reported as success (SD-003). This bears directly on NFR-003.
- Writes, FatFs, DMA, PIO, USB mass storage and power management are not
  implemented, so nothing is claimed about them.

## Validation Issues / Failures

Three contract gaps are open. Each has a regression that asserts the intended
behavior and therefore fails against the current source; they are registered in
CTest and disabled by default so a passing default run is not read as evidence
that they are resolved. Enable with `-DTAVERNKEEP_TEST_KNOWN_GAPS=ON`.

- **SD-003** — the read data CRC is discarded, so a card returning corrupt data
  produces a successful read with wrong contents. Impact: silent corruption
  rather than a detected error, against NFR-003.
- **SD-004** — CMD12 can mistake read data still in flight for its own
  response, turning a read whose data all arrived correctly into an I/O error.
  Impact: false negatives whose outcome depends on the contents of the next
  block.
- **SD-005** — the R1 wait tolerates seven filler bytes where the specified
  window allows eight, and real cards have been reported needing twelve.
  Impact: bring-up or a read failing with a generic I/O error on a slow card.

Details, measurements and candidate fixes: `tests/KNOWN_GAPS.md`.

Two defects found during the same review were fixed, each with a regression
written first and now part of the enabled suite:

- **SD-001** — a removal edge landing on a read's final release clock left the
  read returning success while cancellation was already latched, against this
  document's own requirement that a late completion never override a
  cancellation.
- **SD-002** — teardown of a device that was never initialized, or already torn
  down, released the SPI peripheral again, against the exactly-once teardown
  the architecture requires.

