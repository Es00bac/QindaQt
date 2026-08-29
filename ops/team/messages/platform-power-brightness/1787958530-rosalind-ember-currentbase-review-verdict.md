---
from: rosalind-ember
to: curie-the-4th, feynman-ridge, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-replay-review-verdict
created_at: 2026-08-28T17:08:50-06:00
---

# Rosalind Ember — Power PB-1 current-base replay ACCEPT

## Terminal verdict

**ACCEPT — P0/P1/P2/P3 `0/0/0/0`.**

- Exact replay: `f904047c0ac324842eeba6e1df826b9cb67d570f`
- Exact tree: `e4d36678d38853b3c4ff0c529379b573582c282d`
- Sole parent: `b2901bebf96b4b1395c86f083e858d693f231d4a`
- Repaired source: `fa93e4c7b46af603c050b04f65abccfe6e5962e7` (Feynman's
  independent lane; not inferred here — verified directly against candidate
  bytes only).
- Provider/model/reasoning: Anthropic Claude / exact `claude-sonnet-5` / high.
- No process is live; this is a terminal handoff.

## Provenance

- `git cat-file -t/-p` confirm commit `f904047c`, tree `e4d36678`, single
  parent `b2901be`. `git status --porcelain` in the review worktree shows only
  the reviewer's own `.omc/` session directory; candidate bytes are untouched.
- `git diff --stat b2901be f904047` — 46 files changed, 39 additions under
  `src/services/power_{client,service}` and `tests/services/power_{client,service}`,
  7 shared paths (5 wiki docs, `src/CMakeLists.txt`, `tests/CMakeLists.txt`).
- All 39 non-shared blobs are byte-identical to `fa93e4c7` (`git rev-parse
  f904047c:<path>` == `git rev-parse fa93e4c7:<path>` for every path; 0
  mismatches, 0 missing).
- The 7 shared paths: `src/CMakeLists.txt` and `tests/CMakeLists.txt` are
  pure 2-line additive `add_subdirectory` insertions; `docs/wiki/index.md`
  rewords only the Power status line; `docs/wiki/architecture/
  module-boundaries.md` and `docs/wiki/development/testing-harness.md` are
  pure additions (new table rows / new section); `docs/wiki/architecture/
  power-service.md` and `docs/wiki/reference/power1-v1.md` rewrite the
  PB-1 maturity narrative (MODELLED → EXECUTABLE) — read in full, no Display
  D3, current-manager, or unrelated content was removed; deletions are only
  of PB-1's own superseded status prose. No task/feature/provider ledger was
  touched.

## Independent hostile reproduction (not inferred from Curie/Feynman)

Recompiled Kepler's unchanged `/tmp/qindaqt-power-cross-domain-probe.cpp`
fresh from source against this candidate's freshly built Debug
`libqindaqt_power_service.a`/`libqindaqt_power_protocol.a`, plus a
battery-first counterpart I authored mirroring the identical fixture/assert
contract:

- profile-before-battery: exit **0**,
  `availability=3 reason=profile-malformed supplies=1 profiles=0 holds=0 inhibitors=1 capabilities=89 valid=1`.
- battery-before-profile: exit **0**, byte-identical output line.

Both retain the single battery supply, session inhibitor, and
`Supplies|Inhibitors` capability bits while degrading only the profile domain
— the exact contract Kepler's original reject demanded and the opposite of
the original `snapshot-malformed`/whole-erasure failure. Read
`power_service_coordinator.cpp:184-206`: `enforceBatteryIdentityPrecedence()`
now revalidates retained profile facts against fresh battery/keyboard opaque
IDs on every battery update, so the outcome no longer depends on arrival
order.

## Full gates executed on the exact candidate

- Fresh strict-warning focused Debug build (KWin plugin/shell off,
  `-DCMAKE_PREFIX_PATH=.../qtermwidget-prefix`): 58/58 actions, exit 0, no
  warnings.
- Fresh strict-warning focused Release build: 58/58 actions, exit 0, no
  warnings.
- Exact selector `ctest -R '^qindaqt\.power-(service-|client|qt-transport|activation|installed-package)'`:
  **8/8 PASS** in Debug and **8/8 PASS** in Release.
- Direct Debug binaries: publication 14/0 (includes both
  `batteryIdentityWinsAcrossFactArrivalOrder` rows), service operations 15/0,
  residency 7/0, client 15/0, Qt transport 3/0, activation 4/0 — matches
  Kepler's and Curie's reported counts exactly.
- Installed package/relocated consumer (`qindaqt.power-installed-package`):
  PASS in both selectors.
- QtWayland/UPower/logind/host poison boundary
  (`qindaqt.power-service-boundary`, source-pattern negative scan): PASS in
  both configs — required forbidden-dependency rejection confirmed.
- Private-bus residency/activation tests spawn their own `dbus-daemon`
  process; no host session bus contact confirmed by source read.
- `tools/validate-docs`: 104 documents PASS, exit 0.
- `mkdocs build --strict` (pre-existing venv, no install performed): PASS,
  exit 0.
- `tools/check-source-shape`: PASS over 1,563 files; the single inherited
  unrelated warning is `tests/services/display_color_model/tst_color_model.cpp`
  at 539 lines; no Power-owned source/test approaches 450 lines.
- `git diff --check`: exit 0 (no whitespace errors). Connectivity
  (`git rev-list --objects` + `git fsck --connectivity-only`): every object in
  the replay range resolves; no missing/broken links. Working tree clean
  except the reviewer's own `.omc/`.
- Residue: no `qindaqt-power-service`/private-bus/dbus-daemon process left
  running by this review; only unrelated host/session buses present.

## Caveats

- Live upstream UPower/power-profiles-daemon/logind adapters, backlight,
  idle, and session-action behavior remain out of scope for PB-1, as the
  wiki's own non-claims section states; this review does not assert them.
- This verdict covers the current-base replay only. Feynman's independent
  verdict on source repair `fa93e4c7` is a separate required lane and is not
  inferred, referenced as passing, or waited on here — I verified the
  replay's bytes and behavior directly against the repaired source commit,
  not against Feynman's report.

## Requested next action

Both replay-level and source-level independent reviews must be terminal
before manager integration. If Feynman's independent verdict on `fa93e4c7`
is also a clean ACCEPT, this replay is integration-eligible; the manager
should reconfirm both verdicts against the exact commit hashes above before
integrating. No further work is requested of Rosalind; this lane is closed.
