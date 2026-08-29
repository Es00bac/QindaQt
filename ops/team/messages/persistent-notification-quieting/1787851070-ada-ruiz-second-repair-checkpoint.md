# Ada Ruiz completes second-cycle blocker implementation before broad gates

- **Timestamp:** 2026-08-27T11:17:50-06:00
- **Preserved candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **State:** uncommitted second repair; broad Debug/Release/docs/install gates remain

The two late P1 classes and the design-audit follow-ups now have modular source
and focused evidence:

- schema-private recursive canonical JSON normalizes every Object ingress to
  Nullptr/bool/signed-64/double/string/list/map, rejects invalid QVariant,
  unsigned values above `INT64_MAX`, non-finite/non-JSON values, malformed
  UTF-16, embedded NUL, and invalid object keys, and explicitly encodes QJson;
- integer/double tests assert exact metatypes and bit patterns for signed and
  unsigned boundaries, `-0.0`, denormal, maximum double, `0.1`, and both sides
  of `+/-2^63` through save/reload and a real private-bus service replacement;
- JSON null crosses Settings1 only as fixed bounded `g:"v"`; one bounded
  traversal rejects caller signatures, ordinary/opaque arrays canonicalize
  alike, every snapshot/commit outcome value map is encoded, and the public Qt
  transport defensively encodes complete operations before libdbus;
- service startup rejects persistent layers outside Settings1 wire limits,
  while rejected wide/invalid commits never mutate revision or file;
- DND owner/loss now dominates accepted-save and conflict projections, valid
  conflict intent is restored only after a fresh baseline, and confirmed
  persistence/validation/revision-exhaustion diagnostics survive automatic
  refresh until a new explicit write. Both QML surfaces project the stable
  diagnostic.

Focused Debug evidence currently passes:

- canonical schema/persistence/protocol/private-bus/service/client/transport
  matrix: **8/8**;
- client/controller/private-transport slice after test decomposition: **3/3**;
- settings-app and shell quieting offscreen surfaces: **2/2**;
- `tools/check-source-shape --largest 30`: exit 0, 767 files, no skips or
  warnings. The cohesive decoder is 423 nonblank lines; envelope and outbound
  encoding are separate 132/141-line collaborators. The prior oversized
  transport function and 571-line client test warning were eliminated.

Architecture, protocol, ADR, presentation, and test-harness documentation now
describe the canonical/null/state contracts. I am proceeding to strict docs,
full Debug and Release registries, production build/QML lint, staged install,
and isolated activation. No live desktop, real session bus, compositor, input,
pointer, or another worker's source tree was used.
