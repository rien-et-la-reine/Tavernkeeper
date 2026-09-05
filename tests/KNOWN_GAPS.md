# Production contract gaps

Gaps that are still open, and the two that were fixed. Each open gap has a
registered regression that asserts the behaviour the driver *should* have and
therefore fails against the current source. They are disabled by default, so a
green default run is not evidence that they are resolved.

```sh
cmake -S tests -B tests/build -DTAVERNKEEP_TEST_KNOWN_GAPS=ON
cmake --build tests/build
ctest --test-dir tests/build -L known-gap --output-on-failure
```

All three currently fail. `-DTAVERNKEEP_TEST_KNOWN_GAPS=OFF` restores the
default; `ctest -L host` selects the enabled coverage alone.

---

## Open

### SD-003 — read data CRC is discarded, so corruption is reported as success

Affected code: `sd_spi_device_read_blocks()` and `sd_spi_read_csd()` in
`src/storage/sd_spi.c`, both marked TODO in the source.

```sh
ctest --test-dir tests/build -R sd_gap_data-crc --output-on-failure
```

Every data packet carries a CRC16-CCITT over the payload. The driver clocks the
two bytes and throws them away, so a card that returns corrupt data produces a
successful read with wrong contents. For NFR-003 that is silent corruption
rather than a detected error.

The card model computes a real CRC16 over every block it sends, so the
regression is already expressible: corrupt one payload byte and a validating
driver would reject the read. `test_read_data_crc_is_not_validated` in
`test_sd_faults.c` pins the current behaviour and proves the corrupted byte
reaches the caller; `sd_gap_data-crc` asserts the desired behaviour and fails.

The fault sweep in `test_sd_faults.c` had to be given an explicit exclusion for
payload-corrupting faults because of this gap. When CRC validation lands, remove
that exclusion and the sweep tightens automatically.

**Cost to fix:** a CRC16 routine and a comparison at two call sites. The
question worth settling first is what to do on a mismatch — fail, or retry a
bounded number of times, which is what the card's own error-recovery model
expects.

### SD-004 — CMD12 can mistake in-flight read data for its own response

Affected code: `sd_spi_stop_transmission()` in `src/storage/sd_spi.c`.

```sh
ctest --test-dir tests/build -R sd_gap_stop-residual --output-on-failure
```

During a CMD18 stream the card keeps sending until it decodes CMD12, so
residual read data can still be on the bus while the host is already looking for
the response. `sd_spi_stop_transmission()` discards exactly one stuff byte —
which is correct as far as it goes — and then treats the first byte with bit 7
clear as R1. A residual data byte with bit 7 clear satisfies that.

Measured against the model, with all three blocks of a read arriving intact:

| Residual bytes in flight when CMD12 is decoded | Result |
| --- | --- |
| 0 to 7 | `OK` |
| 8 or more | `IO_ERROR` |

Which way it goes above the threshold depends on the byte values of the *next*
block, so the same read can pass or fail depending on the card's contents. This
is a false negative, not a detected error: the data was correct.

This is a documented real-world hazard. Linux's `mmc_spi` driver carries a fix
for a related collision, where an out-of-range error token arriving as CMD12 was
sent left some cards rejecting every subsequent command until reset
([patch](https://lkml.iu.edu/hypermail/linux/kernel/1403.0/00865.html)).

**Two candidate fixes, both design decisions:**

1. *Synchronise before stopping*, as Linux does: clock 0xFF until the next data
   token appears, then send CMD12. Deterministic, but costs up to a block time
   of extra latency on every multiple-block read and needs its own bounded wait.
2. *Do not gate success on CMD12's response.* The data has already been received
   and, once SD-003 is fixed, validated; CMD12 only ends the transfer. Still
   wait for the busy period so the bus is quiescent, but do not turn a good read
   into an error because the response byte could not be located.

Not chosen here: this changes the driver's error semantics and wants validation
against real cards before it is settled.

### SD-005 — the R1 wait is shorter than the specified response window

Affected code: `sd_spi_command()` in `src/storage/sd_spi.c`.

```sh
ctest --test-dir tests/build -R sd_gap_r1-tolerance --output-on-failure
```

The loop reads at most eight bytes while waiting for a byte with bit 7 clear, so
it tolerates at most **seven** filler bytes before R1. The response window for
an SD card in SPI mode is quoted as 0 to 8 bytes, which under the strictest
reading puts a worst-case card's response one byte out of reach. Independently,
Linux's `mmc_spi` driver raised its own limit from 8 to 16 after observing real
cards that needed 12
([patch](https://lkml.iu.edu/hypermail/linux/kernel/0903.1/01387.html)).

The failure mode is a generic `IO_ERROR` during bring-up or a read, with nothing
to distinguish "this card is slow" from "this card is broken".

**Cost to fix:** one constant. The reason it is not changed here is that
widening a tolerance is the owner's call and wants bus captures from real cards
to size properly. `test_response_latency_boundary` pins the current boundary
exactly from both sides, so whatever is chosen has to be chosen deliberately.

---

## Fixed during this work

Both were previously reproduced and documented but left unfixed. They now have
regressions in the **enabled** suite, so reintroducing either fails the default
run; mutations `release-race-unchecked-single` and `deinit-releases-twice` in
`tools/mutations.txt` confirm that.

### SD-001 — removal during the release clock published a successful read

`sd_spi_device_read_blocks()` checked the removal latch around every data and
CRC transfer, but not after `sd_spi_release_bus()` clocked its final byte. A
card-detect edge landing on that byte left the read returning `OK` with
`removal_latched` already true.

`docs/architecture.md` requires that "a transaction interrupted by even an
unconfirmed removal edge must fail and must not resume" and that "a late
transfer-completion event must never change a cancelled request into success".
Initialisation already rechecked after its own release helper; the read paths
did not.

**Fix:** recheck the latch after `sd_spi_release_bus()` on both read success
paths and return `INVALID_DEVICE`. Four lines, symmetric with what
initialisation already did.

**Regressions:** `sd_removal_single-release_host_tests`,
`sd_removal_multi-release_host_tests`, and
`test_removal_during_the_release_clock` in `test_sd_faults.c`, which expresses
the same case against the release *phase* rather than a byte index, so it stays
valid if the driver's byte layout changes.

Scope note: only the success paths were changed. A removal edge landing on the
release clock of a path that was already failing still returns that path's
error rather than `INVALID_DEVICE`. That is arguably less precise, but the
operation fails either way, and narrowing the change kept it reviewable.

### SD-002 — teardown of an uninitialised device released hardware again

`sd_spi_device_deinit()` unconditionally called `spi_deinit()` and
`gpio_deinit()` on all five pins, even when the device was never initialised or
had already been torn down. Two consecutive teardowns released the SPI
peripheral twice, against the architecture's requirement that teardown happen
exactly once, and a device that was only ever configured would release a
peripheral it had never acquired — taking it from whatever owns it now.

**Fix:** return `OK` immediately when `sd->initialized` is false. This is safe
because every initialisation path that fails releases what it acquired, so an
uninitialised device owns nothing; the removal teardown path still runs in full,
because the interrupt handler does not clear `initialized`.

**Regressions:** `sd_removal_repeated-teardown_host_tests`, the
exactly-once assertions inside `sd_fx_check_removed_and_teardown_once()` which
every removal case uses, and the operation-sequence fuzz, which asserts one
hardware release per initialisation across random lifecycles.

---

## Other limits recorded during this review

Not reproduced failures; scope and unfinished work.

- Public `get_info()` still returns `NOT_IMPLEMENTED` even though the CSD is
  parsed and the block count is known internally.
- Writes and filesystem mount/unmount are stubs. The card model implements the
  full write path (CMD24/CMD25, data tokens, data-response tokens, stop-tran,
  programming busy) so the tests are ready when the driver is; no speculative
  production code was added.
- `sd_spi_configure()` does not validate GPIO numbers or reject duplicate pin
  assignments. See [RESIDUAL_RISK.md](RESIDUAL_RISK.md) section 3.1.
- Argument validation order differs between the read and write backends. See
  RESIDUAL_RISK.md section 3.2.
- Foreground removal coordination, handle invalidation, USB media ownership,
  PIO and DMA cancellation, and low-power operation are not implemented.
- Host fakes cannot validate physical SPI completion, GPIO pad state,
  electrical removal, RP2350 errata, multicore memory ordering or real card
  compatibility. See RESIDUAL_RISK.md.
