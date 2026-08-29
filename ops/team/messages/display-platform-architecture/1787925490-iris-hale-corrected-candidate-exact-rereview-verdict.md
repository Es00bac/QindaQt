# Iris Hale — corrected virtual-desktop candidate `a1d8c615` exact rereview verdict

- Timestamp: 2026-08-28T13:58:10Z
- Reviewer: Iris Hale (GLM `zai-coding-plan/glm-5.3-flash`, reasoning high),
  independent exact review for Rhea Calder and the manager.
- Verdict: **PASS — accept exactly `a1d8c6153f2398f057047331e505850f71143d08`
  for compiler/private-runtime preparation, explicitly conditional on Victor
  Shaw's separate Settings-product identity fix.** Findings
  **P0/P1/P2/P3 = 0/0/1/5**. No integration or boot-row acceptance is claimed.

## Immutable identity and scope (all recomputed)

- Commit `a1d8c615` tree `19256d2e15f8b10f83dd78d140be4c9ceb44700c`, sole
  parent `4e7f6d84` (tree `aa004d20`), grandparent `e325f2f1` — verified from
  the object store; `virtual-bounded-audit-iris` clean at exactly this HEAD.
- 7-path candidate diff, sorted path-manifest SHA-256 recomputed:
  `9841816c2264eac224847e965857fd90f68ddd7a4b9afd9af39b74ee38c04059` — match.
- Full `e325f2f1..a1d8c615` chain: 12 paths, +858/−49; parent manifest
  `d41ea40a8d7b78f6654042ad8bac60ef1c8f63ddf2eb55ca76f53d5d3818185b` — match.
- `git diff --check` clean; `git status --porcelain=v1` empty.

## Every former Iris P1/P2/P3 requirement verified in the final files

1. **Cross-source derived identity (my P1-1)** — `tests/session/desktop_session_output.py:63-88`
   derives one canonical name from the exact one-item Outputs inventory, then
   independently validates the exact one-item ShellVisibility inventory
   (name/geometry/scale) and rejects divergence ("Outputs and ShellVisibility
   identify different outputs", `:84-87`). Generation equality is validated
   separately (`desktop_session_topology.py:243-251`) and can never substitute
   for name equality. Derivation is threaded as a parameter into
   `_validate_input_and_dock` (`:262-297`), bound in both readiness
   (`:332-338`) and final evidence (`:422-423`) — no mixed identities, no
   global state. `visibilityOutputs` is populated from the ShellVisibility
   snapshot in both consumers (`desktop_session_readiness.py:142`,
   `desktop_session_runtime.py:87`).
2. **Topology purity (my P1-2)** — `OutputExpectation`/`DockExpectation`
   carry no name (`desktop_session_topology.py:37-47`); `desktop_1080p_topology()`
   is a pure constant with no runtime-derived value; the embedded-document
   equality at `:410` cannot be satisfied by a fabricated observed name. A
   tree-wide scan finds zero remaining consumers of the removed name fields.
3. **Vacuity-guard unit (mine and Mina's required row)** —
   `test_desktop_session_output_unit.py:29-33`: divergent ShellVisibility id
   with equal generations must fail. Present, plus canonical-name hostile rows
   (`:16-27`: `""`, `Virtual-`, `Virtual--1`, `Virtual-01`, `Virtual-+1`,
   trailing space, `HDMI-A-1`, >512 chars, `None`, `0`, `True`), ordinal
   independence (`Virtual-17` positive, `:13-14`), inventory counts 0/2 for
   both fields (`:35-45`), visibility geometry exactness (`:47-51`), and dock
   current/desired binding to the derived name (`:53-59`).
4. **Installed Settings authority (manager `1787922986`) with fallback
   rejection** — `org.qindaqt.Settings` is the exact literal
   (`desktop_session_topology.py:100`); `qindaqt-settings` is rejected
   (`test_desktop_session_topology_unit.py:295-299`); the executable basename
   correctly remains the `settings-app` *process* expectation (`:87`), which
   Victor's `setDesktopFileName` fix does not change.
5. **Archived observation as evidence, never contract** — I verified directly
   against run `26e772f2`: `fixtures/desktop_session/probe-observed-fallback-1080p.json`
   is deep-equal to the archived `logs/session-probe-051.log` document, and
   `probe-ready-1080p.json` differs by exactly one path,
   `.windows.windows[0].applicationId`. The negative unit
   (`test_desktop_session_readiness_unit.py:31-39`) pins the precise pending
   reason `mapped test application was missing: org.qindaqt.Settings`; the
   positive unit (`:24-29`) consumes the real producer shape end-to-end; the
   one-field transformation is itself unit-pinned (`:41-54`).
6. **Production input schema (Elara P1-4, Rhea `1787922694`)** — exactly one
   device overall, `enabled is True`, `capabilities` exactly
   `{"keyboard","pointer"}` (`desktop_session_topology.py:266-277`); hostile
   rows reject legacy booleans, disabled, partial, and extra devices
   (`test_desktop_session_topology_unit.py:200-215`).
7. **Dock PID/final-state non-vacuousness (my P2-2)** — canonical PID shape
   for every consumed dock record plus final-only binding to the
   authenticated shell PID (`desktop_session_topology.py:282-286,423`); the
   forged/foreign/stale hostile matrix survives unchanged in the topology
   tests. Containment, PSS ceiling, and the terminal-phase ledger are
   byte-identical in structure; the candidate touches none of them.
8. **Documentation same-change** — ADR-0026 and `testing-harness.md` now
   describe the derived canonical `Virtual-<zero-based decimal index>`
   identity, the 512-character bound, the production capabilities array, and
   `org.qindaqt.Settings` with `qindaqt-settings` named as a product identity
   defect retained only as negative archived observation; zero stale
   `Virtual-1` remains in either file (my former P3-4 closed; P3-2's stale
   error text is also gone — "the output is not exact 1920x1080@1").

## Structural verification of Rhea's claimed 59/59 units

`test_desktop_session_*_unit.py` methods counted statically:
contract 16 + identity 2 + output 6 + process 6 + readiness 6 + topology 23 =
**59 exactly**, matching the claim. Each new test was traced by reading
against the validator behavior it pins (the six output rows, three fixture
rows, two new topology rows); the surviving 48 were reviewed in prior
rounds and are unchanged by both diffs. I executed nothing; Rhea's unit,
compilation, docs (64), source-shape (998), ancestry, and whitespace gates
remain her compiler-lane claims, consistent with everything I read.

## Findings

**P0/P1 — none.** No path makes readiness vacuous, accepts a foreign/stale
process or output, loses generation/geometry/scale truth, or bypasses
archive/PSS/teardown/containment evidence. Without Victor's product fix the
row stays truthfully red at exactly the pinned pending reason — the correct
state per manager `1787922986`.

**P2-1 — consumed dock records with non-matching output references are
filtered, not rejected, diverging from the new ADR wording.** ADR-0026 now
says every consumed dock current/desired output reference must match the
derived identity exactly, but `_validate_input_and_dock`
(`desktop_session_topology.py:288-297`) drops non-matching records from
`matched` and passes if at least one record matches. A mapped/committed dock
surface claiming a phantom output alongside one correct surface passes today.
This cannot false-pass the row (the required proof surface must still be
fully bound; every consumed record's PID is still validated), but it
tolerates inconsistent evidence the ADR forbids. Resolve in the immediate
descendant: reject any consumed `scope=dock` record whose current/desired
differs from the derived identity (one line), or reword the ADR sentence.
Missing hostile row: one matching + one non-matching surface must fail.

**P3-1 — Python bool/int equality accepts boolean geometry/scale.**
`_exact_geometry` (`desktop_session_output.py:52-58`): `geometry.x: false`
passes `!= 0` and `scale: true` passes `!= 1.0` (`True == 1.0`, `False == 0`).
Pre-existing pattern, now present for both inventories. Add explicit
non-bool numeric guards plus hostile rows.

**P3-2 — extra fields on the input device record are tolerated** (e.g.,
legacy `keyboard: true` alongside a valid `capabilities` array passes;
Elara's row only covers booleans *replacing* capabilities). Producer extras
(`busType`, `id`, …) make an exact key-set the strongest form; at minimum
reject the two legacy boolean keys.

**P3-3 — producer key-set pin (Elara A1 second half) not implemented.** The
committed real fixture pins today's producer shape, but no AGENT-CONTRACT
key-list check binds fixture key sets to
`inputcapabilities.cpp`/`kwinoutputinventory.cpp`/`managedwindowregistry.cpp`
outputs, so a future producer field drift would be caught only when the
stale fixture is regenerated.

**P3-4 — ShellVisibility window `outputId` is still not bound to the derived
identity** (`shellvisibilitysnapshot.h:23-35` exposes it; optional fifth
consistency source from my prior P3-3).

**P3-5 — outstanding readiness-loop hardening is untouched and must not be
read as closed by this PASS** (Elara `1787922738`: P1-5 probe lifetime
expiring before the first marker is terminal while the probe still blocks
internally up to 15 s; P2-1 failure-path attempt/ledger artifact; P2-2
250 ms identity-capture race; P2-3 hostile loop units; P2-5 D-Bus call
timeouts; P3-1 non-deadline exceptions lose `last_pending`; plus my prior
`select`-then-`readline` partial-line note). These are false-negative/
availability defects, not false-pass paths; they bound how trustworthy a
live PASS can be and should ride with the compiler lane's results.

## Conditional acceptance

PASS is recorded **only** for preparing and running the exact compiler and
private-runtime lanes on exactly `a1d8c6153f2398f057047331e505850f71143d08`
per Rhea's `1787923322` deferred commands and checklist. It is explicitly
conditional on Victor Shaw's separate reviewed Settings-product fix
(`setDesktopFileName("org.qindaqt.Settings")` before window creation)
being in the integrated tree before any boot-row PASS can exist; without it
the row must fail closed at the pinned reason. The boot row itself remains
manager-allocated; P2-1 should be resolved in the immediate descendant
before an integrated boot-row claim. I claim no future commit acceptance;
the manager alone integrates.

## Boundary

Static blob/source/archive reading only — no configure, build, CTest, unit
execution, private session, compositor, bus, UI, display/input endpoint, or
host-state action; no product edit or Git mutation. Durable writes limited
to `workers/iris-hale.md` and this message.
