# Iris Hale interim re-evaluation: QtDBus decode and mirror canonicalization

- **Timestamp:** 2026-08-27T17:41:43-06:00
- **From:** Iris Hale, Display D1 adversarial audit assistant
- **To:** Display D1 lead/keeper
- **State:** audit in progress; interim findings re-evaluated against the
  current uncommitted tree at HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1`

## 1. Raw QtDBus destination mutation / unbounded demarshalling — partially
resolved; residual hardening items (severity: Low–Medium)

Destination-safety (contract 1 "no partial caller replacement") **holds within
D1**: `decodeCandidateArgument`/`decodeSnapshotArgument`/
`decodeOperationResultArgument` decode into locals and move into the
destination only after full validation
(`src/services/display_protocol/src/display_dbus.cpp:63-99`), and the byte
codec additionally enforces magic, codec version, bounded counts before
`reserve`, bounded `bytes`/`text` before allocation, strict UTF-8, and a
trailing-byte check (`src/display_codec.cpp:41-87`,
`src/display_codec_snapshot.cpp:156-218`, `src/display_codec_p.h:118-150`).

Residual items:

1. **Mid-struct decode failures can yield plausibly-valid values
   (Medium).** The raw `operator>>` implementations perform no per-element
   signature verification and QtDBus offers no per-read error signal. If a
   hostile peer's structure diverges at field N, fields 1..N-1 keep real data
   and the tail silently keeps defaults (e.g. `Output`: position (0,0), scale
   1.0, transform Normal — `display_dbus.cpp:138-161`). `Mode`,
   `CandidateOutput`, and `TransactionSummary` carry no `wireValid` member, so
   they cannot flag partial decode at all. Post-validation catches empty-id
   cases but not default-valued tails. Recommended boundary hardening before
   D2 wires this to a live peer: verify the expected static DBus signature at
   the adapter boundary before demarshalling, and add focused hostile rows for
   tail-divergent structures. Within D1's transport-free scope this is
   acceptable to defer, but it should be an explicit documented decision.
2. **`readBoundedArray` iterates the entire received array even after the
   bound is exceeded (Low).** `display_dbus.cpp:31-39` stores at most
   `maximum` elements but keeps demarshalling every remaining element. Memory
   is bounded; CPU is wire-size-bounded (session-bus message limits), not
   loop-bounded. Early exit is not safe mid-structure; the real fix is the
   same signature pre-check as (1). Document the amplification bound as
   accepted, or pre-check.

Raw `operator>>` remains public API whose partial-mutation hazard is guarded
only by the header comment (`include/.../display_dbus.h:19-21`); consider
making the wrappers the only documented entry point for adapters in the
reference page.

## 2. Replicated-output canonicalization — addressed in the current tree;
residual notes (severity: Low)

The tree now canonicalizes replicas:
`src/services/display_topology/src/topology_validation.cpp`

- Normalization origin uses non-replicated enabled outputs only
  (lines 143-146), so replica positions no longer shift `(0,0)`.
- Replica `modeId`/`transform` are forced from the live output (lines 186-192).
- A root-walk pass copies root position/scale into each replica, replaces the
  replica's geometry rect with the root's, and erases stale non-integral
  warnings for replicas (lines 241-275).

Independent re-evaluation: callers can no longer vary replica fields, so the
fingerprint/diff asymmetry I previously flagged is closed — `diff` ignores
replica Mode/Position/Scale/Transform (`topology_fingerprint.cpp:112-118`) and
the fingerprint over normalized values is now invariant to replica-input
divergence. Cycle-safe walk confirmed (cycle rejection at lines 234-240
precedes the walk; disabled replicas have sources cleared at line 173 and are
skipped at 241-242). Residual notes:

1. Replica `transform` comes from the live snapshot while its `scale` comes
   from the candidate root (lines 191 vs 268): a candidate that changes the
   root's transform leaves the replica's canonical transform stale relative to
   the root. Harmless for no-op truth (diff ignores replica transform) but
   document the mixed derivation as intentional.
2. The replica's intermediate rect (its live mode × candidate scale) can still
   trigger `CoordinateOverflow` (lines 219-224) before line 269 replaces the
   rect with the root's. A false rejection is possible for replica candidates
   whose intermediate extent overflows `kCoordinateBound` even though the
   canonical rect equals the root's passing rect. Low, but worth a hostile
   test row or moving the replica rect computation after canonicalization.

Both interim findings are preserved in this wording; the consolidated audit
message carries the remaining open findings.
