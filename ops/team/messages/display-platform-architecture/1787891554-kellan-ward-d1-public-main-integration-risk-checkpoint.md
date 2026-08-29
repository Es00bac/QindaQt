# Display D1 source-only public-main integration-risk checkpoint

- **Timestamp:** 2026-08-28T04:32:34Z
- **From:** Kellan Ward, Display D1 transaction implementer and lead
- **Compared trees:** public main `2c52c985f846b083c2aebb7a08f04aa8318a2912`,
  failed immutable D1 candidate `0e38fa726af69e34be3cacdd6b71d40350ac8092`,
  common parent/public base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`,
  and the preserved 15-path uncommitted repair over the candidate
- **Execution boundary:** source-read-only comparison only; zero product edits,
  merges, rebases, stashes, commits, configures, builds, tests, binaries,
  runtimes, installs, displays/sessions, or host actions

## Exact path and hunk comparison

Public main is one commit over the common base and changes 6 paths
(`+104/-13`):

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/panel-surfaces.md`
- `tests/session/CMakeLists.txt`
- `tests/session/fixtures/qindaqt-surface-proof.json`
- `tests/session/shellsurfaceprobe.cpp`
- `tests/session/test_shell_surface_nested.py`

The D1 candidate changes 66 paths from the same base. Their exact path
intersection is **one path only**:
`docs/wiki/development/testing-harness.md`. The hunks are non-overlapping:

- public main edits the production-surface explanation at base lines 204-245
  (`@@ -204,11 +204,18` and `@@ -232,10 +239,10`), binding that nested proof
  to the schema-valid never-hidden `qindaqt-surface-proof` fixture;
- D1 adds the deterministic model/Q-det section after base line 676
  (`@@ -676,4 +676,22`), explicitly denying runtime, nested-protocol, and
  hardware claims.

A read-only three-way `git merge-file --object-id -p` over those three blobs
returned exit 0, no conflict markers, and retained all four distinguishing
terms: `qindaqt-surface-proof`, `deliberately never-hidden`,
`D1 deterministic display model`, and `Q-det`.

The other five public-main paths are outside D1. Conversely, every one of the
15 dirty repair paths is absent on both the common base and public main because
each was created by the D1 candidate; the exact repair/public overlap is zero
paths. The preserved repair remains `+245/-26` over `0e38fa72`:

- `docs/wiki/adr/0015-display1-transaction-authority.md`
- `docs/wiki/architecture/display-service.md`
- `src/services/display_protocol/include/qindaqt/services/display_protocol/display_limits.h`
- `src/services/display_protocol/src/display_validation.cpp`
- `src/services/display_topology/src/topology_fingerprint.cpp`
- `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_machine.h`
- `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_ports.h`
- `src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_types.h`
- `src/services/display_transaction/src/transaction_machine_events.cpp`
- `src/services/display_transaction/src/transaction_machine_revert.cpp`
- `tests/services/display_protocol/tst_display_protocol_codec.cpp`
- `tests/services/display_topology/tst_topology_candidate.cpp`
- `tests/services/display_transaction/tst_transaction_adversarial.cpp`
- `tests/services/display_transaction/tst_transaction_recovery.cpp`
- `tests/services/display_transaction/tst_transaction_state.cpp`

## Shared registries and semantic drift

Public main is byte-identical to the common base at all seven D1 additive
coordination registries: `src/CMakeLists.txt`, `tests/CMakeLists.txt`,
`mkdocs.yml`, `docs/wiki/index.md`, `docs/wiki/adr/index.md`,
`docs/wiki/architecture/overview.md`, and
`docs/wiki/architecture/module-boundaries.md`. D1's changes there remain
strict additions: four module registrations, four test registrations, two wiki
pages, two ADRs, and matching overview/boundary links. There is no public-main
build registration or public Display contract drift.

The sole shared testing-harness page has compatible semantics: public main
narrows a shell nested-surface proof to deterministic initial publication;
D1 separately labels its pure model evidence and forbids upgrading it to a
nested/runtime claim. Integration must retain both sections. Public main also
changes the session test fixture/command without adding a D1 selector; future
integrated broad-test counts must be discovered from the integrated tree rather
than copied from the base-era D1 handoff.

## Safe integration order

1. Keep public main `2c52c985` as the target and first finish/qualify the
   non-amended D1 repair branch under the serial compiler lane.
2. Obtain exact independent rereview of the new repair commit/tree. Do not
   expose failed `0e38fa72` alone as an integration milestone.
3. Manager applies the accepted two-commit D1 series (`0e38fa72`, then its new
   repair commit) atomically onto public main, retaining both non-overlapping
   testing-harness hunks and all public surface-proof changes.
4. Run integrated focused Display, docs/navigation, source/static, and broad
   proportional gates on the resulting tree; report newly discovered broad
   counts. D0 and D1 remain prerequisite foundations; D3b/D2 follow, and D7
   must define class-B policy values/methods before Power brightness binds.

## Power class-B boundary help

I reread Priya Nair's complete midpoint/handoff and the newest Display/Power
threads. The compatible direction is Power/brightness -> a future public
Display client; Display1 never depends on Power1. D1 already supplies stable
identity, ambiguity, mirror-source identity, and closed confirmation classes.

The proposed dependency on brightness/auto-brightness fields in Display
snapshots is not yet real: D1's fixed v1 `Output`/`Snapshot` structs contain no
brightness, dimming, SDR, DDC-CI, or auto-brightness values. D2 does not add
them; the accepted sequence assigns typed class-B policy to D7. Widening v1
now would change tested D-Bus/canonical signatures, and including such values
in topology fingerprints/preimages would corrupt transaction semantics.

I posted exact, non-blocking cross-lane help at
`platform-power-brightness/1787891463-kellan-ward-d1-class-b-boundary-help.md`:
pre-D7 pure Power work may use a brightness-owned injected fixture keyed by D1
stable/mirror IDs; D7 must then make the separately reviewed additive/versioned
policy-value decision before display-dependent binding. No Power edit is
requested. Elara's latest Power review stopped at its provider limit with no
accepted verdict, so the handoff remains a proposal pending resumed review and
manager routing.

## Preserved state

HEAD is still `0e38fa726af69e34be3cacdd6b71d40350ac8092` with exactly
15 tracked repair paths at `+245/-26` and the sole external untracked
`ops/team/workers/kai-mercer.md`. No static gates were repeated because this
read-only comparison found no product change; the prior source-static pass
remains recorded at `1787891180-kellan-ward-display-d1-source-ready-compiler-wait.md`.
Kellan is now waiting/not live until the compiler lane is explicitly assigned.
