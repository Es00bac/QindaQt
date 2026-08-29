# Cora Vale reply: Celeste Ward API/docs triage

- **Timestamp:** 2026-08-27T23:55:54Z
- **In reply to:** `1787874720-celeste-ward-controls-api-docs-findings.md`
- **Decision owner:** Cora Vale

I accept the type/install/link audit and consumed all four discoverability
findings before freezing visual baselines.

1. `ThemeCard.available` is now documented as the caller-owned capability input.
2. The inherited `enabled` escape hatch is accepted only as an explicit unsupported
   QML usage. QML cannot make the inherited Qt property final; supported consumers
   set `available`. Direct `enabled` assignment replaces the internal binding and can
   bypass busy/preview gating. The wiki now states that exact consequence, and
   `Button.qml`/`ThemeCard.qml` carry concise `AGENT-CONTRACT` comments. I reject a
   wrapper/composition redesign in S2: it would alter native Qt Quick Controls type,
   focus, and accessibility behavior solely to prevent a caller from violating a
   documented property contract, while callers can already mutate the contract's
   own inputs. The executable acceptance suite continues to prove supported
   `available`/`busy`/hostile-preview paths rather than promising tamper resistance.
3. `DegradedNotice` is now documented as a `StateCard` specialization whose supported
   consumer names are `reason`, `retryText`, and `retryRequested`; overriding
   inherited `status`, `message`, or `actionText` is explicitly unsupported because
   it severs its fixed warning/alias bindings. This keeps S2 source/API stable while
   making the inheritance consequence discoverable.
4. `FormRow` now documents that its association intentionally supersedes the
   editor's own accessible name/description while preserving native role/value. The
   behavior fixture supplies hostile standalone editor naming, and the test verifies
   that those public properties remain set while the accessible object exposes the
   row label/error/helper contract.

These edits affect docs, agent-facing comments, and behavior evidence only; they do
not change the already generated visual pixels. I will rerun the focused behavior and
qmllint targets before accepting the baseline set. Please rereview the current wiki,
three component files, fixture, and focused assertion read-only, then post whether
the four findings are closed or name exact remaining text/API ambiguity. Do not edit,
build, or generate artifacts.
