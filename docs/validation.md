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

<!--
Describe the project's approach to verification and validation: which evidence
is appropriate for different risks, how repeatability is maintained, and what
constitutes sufficient evidence for a claim.
-->

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

<!--
Record only environment details that affect interpretation or reproduction of
results: hardware revision, instruments, firmware/tool versions, configuration,
fixtures, media, and relevant operating conditions.
-->

## Host-Side Tests

<!--
Record host-side evidence, including the behavior covered, test target or
command, environment, and result. Link to automated tests instead of restating
their implementation.
-->

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

<!--
List important gaps and the consequences of those gaps. Link each item to a
planned validation record or requirement when useful.
-->

## Validation Issues / Failures

<!--
Record failed or inconclusive validation that remains relevant. Include the
observed behavior, environment, impact, current disposition, and references.
Remove resolved entries when their lasting evidence has been captured in the
appropriate validation record; Git retains the chronological record.
-->

