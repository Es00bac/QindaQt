# Rhea Calder — virtual desktop observed-identity repair handoff

- **Timestamp:** 2026-08-28T13:14:08Z
- **State:** handoff; no work/process live
- **Exact candidate:** `4e7f6d8448fe1c9cab5ebf3b4605cacaddee008b`
- **Tree:** `aa004d20adc37ca20656321406c6901e5d0eb87e`
- **Parent:** `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`
- **Ancestry:** non-amended descendant; reviewed/public history retained
- **Exact 11-path sorted manifest SHA-256:** `d41ea40a8d7b78f6654042ad8bac60ef1c8f63ddf2eb55ca76f53d5d3818185b`

## Outcome

The topology now requires the installed Settings application's exact observed
`qindaqt-settings` identity. It derives one canonical, wire-bounded
`Virtual-<zero-based decimal index>` from the exact one-item Outputs inventory;
requires the exact one-item ShellVisibility inventory to carry the same
identity, `(0,0)`/1920x1080/scale-1 geometry, and equal canonical nonzero
generation; then requires every consumed mapped/committed production dock's
current and desired output references to equal the derived identity. The frozen
topology document contains no runtime-derived output name.

Elara's archive replay `1787922527`/terminal handoff `1787922738` exposed a
third independent readiness mismatch. The validator now requires exactly one
enabled `QindaQt Development Input` with the production
`capabilities: ["keyboard", "pointer"]` shape and rejects invented booleans,
disabled/partial devices, and extra devices. The positive readiness unit
consumes a normalized semantic-exact copy of real probe-051 from run
`26e772f23f519434ce445dca4ff51128`; a static equality gate proves it matches
that archived JSON document.

Iris's adversarial verdict `1787924840` is consumed: cross-source output
equality, topology purity, dock-name/PID separation, exact geometry/scale/
generation/count pins, hostile name forms, and ShellVisibility's 512-character
identifier bound are all present. PSS, cleanup-ledger, containment, and final
authenticated shell-PID gates remain unchanged.

## Exact path manifest

1. `docs/wiki/adr/0026-contain-virtual-desktop-qualification.md`
2. `docs/wiki/development/testing-harness.md`
3. `tests/session/CMakeLists.txt`
4. `tests/session/desktop_session_output.py`
5. `tests/session/desktop_session_readiness.py`
6. `tests/session/desktop_session_runtime.py`
7. `tests/session/desktop_session_topology.py`
8. `tests/session/fixtures/desktop_session/probe-ready-1080p.json`
9. `tests/session/test_desktop_session_output_unit.py`
10. `tests/session/test_desktop_session_readiness_unit.py`
11. `tests/session/test_desktop_session_topology_unit.py`

## Post-commit evidence

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p 'test_desktop_session_*_unit.py'`: **PASS 57/57**.
- In-memory compilation of 14 desktop-session source/test modules: **PASS**.
- Archived probe-051 versus committed normalized JSON semantic equality: **PASS**.
- `tools/validate-docs`: **PASS, 64 documents/navigation**.
- `tools/check-source-shape`: **PASS, 998 files, zero warnings/issues**.
- `git diff --check HEAD^ HEAD`: **PASS**.
- Parent ancestry, exact manifest, and clean `git status --porcelain=v1`: **PASS**.

Per assignment, Devika owned compiler capacity. No configure, build, CTest,
package row, nested/private compositor/session, UI, or host display/input/
cursor/bus/config/hardware endpoint ran.

## Bounded review decisions

Elara recommends a Settings product `setDesktopFileName` change and a literal
`Virtual-0`; both conflict with the manager's explicit assignment to treat the
installed application's current observed ID as truth and remove the brittle
ordinal literal, and Rhea does not own Settings paths. This candidate follows
the manager's exact direction and makes either future production identity
change fail closed. Elara also identified broader readiness/probe/failure-ledger
P1/P2 work beyond this bounded repair; it remains unclaimed and must not be
silently upgraded by these source-safe gates.

## Requested next action

Dorian (or another independent reviewer) should review exact immutable
`4e7f6d8448fe1c9cab5ebf3b4605cacaddee008b` against Elara `1787922738`, Iris
`1787924840`, the manager's observed-ID/derived-output direction, and the real
fixture. Return exact P0–P3 PASS/FAIL before any compiler/private-runtime lane
allocation. Manager alone integrates.
