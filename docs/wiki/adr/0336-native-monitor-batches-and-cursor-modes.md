# ADR-0336: Native monitor batches and cursor modes

- Status: Proposed; focused/native candidate gates pending
- Date: 2026-10-02
- Supersedes: only the single-monitor/Hidden limitations in [ADR-0324](0324-native-portal-capture-boundaries.md)

## Decision

Extend the existing owned-FD Wayland producer with explicit selections of one
to sixteen offered monitor IDs and one standard Hidden1, Embedded2 or Metadata4
cursor mode. Validate the complete selection before sending any producer request.
Keep the existing single-ID overload and Hidden default for existing consumers.
One failed source, deadline, output removal or authority loss closes every stream
in the batch; protocol listener storage is released after dispatch.

The portal freezes the user's exact source selection before the consent grant.
It returns distinct real PipeWire nodes only after every selected producer is ready.
Standard session/owner/lock loss retires the entire batch. Single-selection sessions
cannot publish multiple nodes. Window/virtual/restore capabilities remain separate.

Stream helper frames add typed multiple/cursor_mode fields; legacy eight-field
frames retain single/Hidden semantics. Screenshot/color frames stay unchanged.
Batch result parsing bounds the count, rejects duplicate nodes and malformed or
nested results atomically, and preserves the standard a(ua{sv}) public wire.

## Verification and consequences

Focused policy tests cover transport round-trip, malformed flags and atomic batch
publication. Actual frontend/native input/PipeWire gates exercise all cursor modes
and a two-monitor selection with session-close cleanup. The two-output compositor
fixture is explicitly opt-in and noninstallable; it does not widen production test
authorization. Metadata routing changes only after actual family acceptance.

See [Capture contracts](../reference/portal-capture.md) and
[CompositorCapture](../architecture/compositor-capture.md).
