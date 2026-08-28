# ADR-0021: Isolate every Controls visual row in its own process

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Controls and testing working groups
- **Supersedes:** None
- **Superseded by:** None

## Context

The Controls visual matrix renders 25 exact theme, scale, and width-profile
combinations through Qt Quick's offscreen software backend. A combined QtTest
process made row identity and renderer/window lifetime implicit while captured
Dusk and Qinda macOS compact pixels could retain a one-character text clip even
though the live QML object graph reported correct card, content, text, and
action geometry. Reusing one view also allowed animated resize state to cross
rows. Process isolation alone did not settle pixels: a frame diagnostic proved
that capture also had to follow the fixture's published QST motion boundary.
The two requirements address separate concerns—one exact row lifetime and one
semantically derived capture boundary.

The owning test contract is [Development and testing harness](../development/testing-harness.md#current-reusable-controls-proof).

## Decision

Each of the 25 Controls visual combinations runs as one named CTest and one
fresh process. Every CTest invokes the same compiled visual-test executable
with exactly one validated QtTest data selector. The wrapper rejects a missing
or scale-incompatible selector, executes no other data row, and verifies that
the requested row is the sole tagged visual pass.

The stable focused prefix remains `qindaqt.controls-`. Visual test names encode
scale, theme, and width profile. Baseline generation and normal comparison use
the same process boundary, environment, DPR and pixel-size assertions, source
paths, and reviewed PNG locations. Each row waits through the fixture control's
published transition duration before requesting the frames used for capture;
reduced-motion duration comes from QST rather than a test override.

## Consequences

- Captures cannot inherit renderer, font-node, window-size, or control state
  from another theme/profile row.
- Failures identify the exact theme, scale, and width profile at the CTest
  boundary rather than only inside combined output.
- The Controls selector discovers 29 tests: behavior, 25 visual rows, source
  policy, staged installed import, and PSS measurement.
- Visual execution pays process startup once per row. That bounded cost is
  accepted for deterministic review evidence and remains serial by default.
- The source-policy gate must prove rejection of missing and unknown selectors;
  every registered visual row must prove exactly one matching QtTest pass.
- Combining rows in one visual process or reusing a view across rows violates
  this decision even when object-property assertions pass.

## Revisit when

Reconsider only when a replacement runner proves the same exact row identity,
lifetime isolation, QST-derived motion boundary, DPR/pixel assertions,
generate-versus-compare parity, and deterministic reviewed baselines.
