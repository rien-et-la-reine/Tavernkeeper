# Host validation record — 2026-09-05

Environment: Linux x86-64, GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, Unix
Makefiles. The suite is also maintained for GCC 15.2 / MSYS2 MinGW on Windows,
which is the toolchain the previous record used; the Windows numbers below are
from that earlier run and are marked as such.

## Baseline, before this work

Recorded on the tree as it stood, with the previous harness:

| Run | Result |
| --- | --- |
| Enabled suite | 15/15 CTest cases passed; SD executable reported 60 groups passed |
| Registered gap cases (disabled by default) | 3/3 failed as documented |

## Final

| Run | Result |
| --- | --- |
| Enabled suite, Release | **22/22 CTest cases passed** |
| `sd_spi_host_tests`, bus-time clock | 60/60 groups passed |
| `sd_spi_host_tests`, poll-tick clock | 60/60 groups passed |
| `sd_protocol_host_tests` | 22/22 cases, 3231 checks |
| `sd_faults_host_tests` | 11/11 cases, 2052 checks |
| `sd_property_host_tests` | 6/6 cases, 12217 checks (default seed) |
| `sd_property_host_tests`, seeds 1, 7, 42, 999, 20260905 | passed at every seed |
| Strict warnings (`-Werror`, `-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wcast-qual`, `-Wmissing-prototypes`, `-Wformat=2` and more) | clean, production and test sources |
| ASan + UBSan, Debug, `-fno-sanitize-recover=all`, `detect_leaks=1` | 22/22 passed, no reports |
| Clang 18 Release build | 22/22 passed; only `-Wformat-nonliteral` from the harness's own `vsnprintf` and a pre-existing `REQUIRE(!"...")` idiom |
| `gcc -fanalyzer` on `sd_spi.c` and `gpio_irq.c` | no diagnostics |
| Registered gap cases (disabled by default) | 3/3 failed as documented |

The three failing gap cases are the point, not a defect in the run: they assert
the behaviour the driver should have and are enabled explicitly. See
[KNOWN_GAPS.md](KNOWN_GAPS.md).

Clang's sanitizers were **not** exercised: this environment lacks the
compiler-rt runtime (`libclang-rt-18-dev`), so the clang sanitizer link fails.
GCC's ASan and UBSan were used instead, and clang was used as a second static
analysis pass.

## Mutation testing

The primary evidence that the suite detects bugs rather than merely executing
code. 53 deliberate incorrect implementations across `sd_spi.c` and
`gpio_irq.c`, spanning wrong constants, reversed conditions, removed
validation, incorrect bit masks, truncated integer widths, off-by-one range
checks, removed cleanup, skipped state transitions, ignored error codes,
incorrect device-type handling, wrong addressing conversion, premature success,
delayed error recognition and eliminated timeout behaviour.

| Outcome | Count |
| --- | --- |
| Detected by the enabled suite | 49 |
| Documented equivalent mutants | 2 |
| Out of this harness' reach, with the reason recorded | 1 |
| Survived the Release suite | 1 (caught under the sanitizer configuration) |

Full table, the arguments for the equivalent mutants, and the reason the
unreachable one cannot be closed on the host: [MUTATION.md](MUTATION.md).

Running the mutation suite against the *finished* work, not only against the
starting point, is what surfaced the last of these: a removal check in
`sd_spi_stop_transmission()` that guards a window this harness cannot reach.
It also caught a bug in the harness itself — the card model's capacity guard
rejected the smallest legal CSD v1 card, because it treated a `C_SIZE` of zero
as a failed search.

## Coverage

GCC coverage, per target. Reports that share a basename overwrite each other,
so these must not be summed; each row is one executable's view of one source.

| Source / measuring target | Lines executed | Branch outcomes taken |
| --- | --- | --- |
| `sd_spi.c` / `sd_spi_host_tests` | 96.81% of 439 | 92.05% of 302 |
| `sd_spi.c` / `sd_protocol_host_tests` | 76.77% of 439 | 61.26% of 302 |
| `sd_spi.c` / `sd_faults_host_tests` | 75.63% of 439 | 59.93% of 302 |
| `sd_spi.c` / `sd_property_host_tests` | 71.53% of 439 | 57.95% of 302 |
| `gpio_irq.c` / `gpio_irq_host_tests` | 100% of 64 | 100% of 40 |
| `block_device.h` / `block_device_host_tests` | 100% of 30 | 100% of 36 |
| `filesystem.c` / `filesystem_host_tests` | 100% of 17 | 100% of 14 |
| `main.c` / all main variants | 100% of 15 | 100% of 8 |

The four SD rows overlap heavily; the new suites are not attempts to raise the
number but to test different questions about the same code, which is why each
alone covers less than the original suite does. Percentages refer to
compiler-instrumented lines and branch outcomes, not to requirements. High line
coverage does not establish real-time bounds or validate a card protocol
physically, which is why the mutation results above carry more weight here.

## Production changes made

Two defects, each with a regression written first. Both were previously
reproduced and documented but left unfixed.

| Defect | Change | Regression |
| --- | --- | --- |
| SD-001: removal during the release clock published a successful read | recheck the removal latch after `sd_spi_release_bus()` on both read success paths | `sd_removal_single-release_host_tests`, `sd_removal_multi-release_host_tests`, `test_removal_during_the_release_clock` |
| SD-002: teardown of an uninitialised device released hardware again | `deinit` returns `OK` without touching hardware when nothing was acquired | `sd_removal_repeated-teardown_host_tests`, `sd_fx_check_removed_and_teardown_once`, the operation-sequence fuzz |

Both are guarded by mutations (`release-race-unchecked-single`,
`deinit-releases-twice`) that the enabled suite catches, so a reintroduction
fails the default run.

Documentation corrections: the root `README.md` no longer claims the storage
layer is unimplemented, and `docs/validation.md` carries a validation record
instead of empty templates.

## Not performed

No Pico SDK cross-build, no target flashing, no physical card test, no bus
capture, no power measurement, no hardware validation of any kind. Everything
above is host-side evidence about host-side behaviour of the production
sources. What that cannot establish is enumerated in
[RESIDUAL_RISK.md](RESIDUAL_RISK.md); acceptance criteria for functionality not
yet written are in [VALIDATION_PLAN.md](VALIDATION_PLAN.md).
