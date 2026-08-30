# Architecture

This is the living description of Tavernkeep's current firmware architecture.
Update it as the design changes, and preserve concise rationale for choices
that materially affect implementation or future work.

## System Overview

<!--
Summarize the current system at a level that orients a new contributor. Name
the major responsibilities without repeating detailed subsystem descriptions.
-->

## Design Principles

<!--
List principles that actively guide engineering tradeoffs. Include the practical
consequence of each principle so it can inform future decisions.
-->

## System / Subsystem Boundaries

<!--
Identify firmware subsystems and adjacent hardware or software systems. State
what each owns, what it exposes, and what is deliberately outside its boundary.
-->

## Major Interfaces and Abstractions

<!--
Describe important interfaces between subsystems and the abstractions intended
to remain stable. Link to source definitions when that is more precise than
duplicating them here.
-->

## Data Flow

<!--
Describe the important paths taken by data and control through the system.
Include direction, buffering, transformation, and persistence where relevant.
-->

## Resource Ownership

<!--
State which subsystem owns shared or finite resources such as peripherals,
GPIO, DMA channels, buffers, storage media, and power domains. Document
acquisition, release, and transfer-of-ownership rules.
-->

## Error Handling Philosophy

<!--
Describe how errors are represented, propagated, logged, retried, and recovered
from. Distinguish recoverable failures from conditions requiring subsystem or
device restart where applicable.
-->

## Concurrency / Execution Model

<!--
Describe foreground work, interrupts, DMA, asynchronous state machines, and any
scheduling model. Record synchronization rules and contexts in which APIs may
or may not be called.
-->

## Important Design Decisions and Rationale

Keep consequential decisions here unless the project eventually becomes large
enough to justify a separate decision-record system.

<!--
Suggested entry:

### Decision — Short title

- Context:
- Decision:
- Rationale:
- Alternatives considered:
- Consequences and tradeoffs:
-->

## Hardware Assumptions Relevant to Firmware

<!--
Record hardware properties on which firmware correctness depends, including
electrical behavior, pin assignments, peripheral capabilities, timing, memory,
and power behavior. Reference authoritative hardware material where useful.
-->

## Known Architectural Limitations

<!--
Record current design limitations that affect correctness, extensibility,
performance, or supported use cases. Link to requirements or validation records
where relevant.
-->

## Planned / Future Architecture

<!--
Describe credible architectural changes that are not part of the current
design. Clearly separate plans from implemented structure and state the trigger
or requirement motivating each change.
-->

