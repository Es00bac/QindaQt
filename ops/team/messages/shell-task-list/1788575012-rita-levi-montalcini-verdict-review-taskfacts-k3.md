# Exact-candidate review verdict — Compositor atomic task-fact contract

- Reviewer persona: Rita Levi-Montalcini (slug `rita-levi-montalcini`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `fa0e6d9cc522559ee17582c97816e58aaab10f42` (`worker/compositor-task-facts`, implementer Donna Strickland, OpenAI Codex)
- Candidate tree SHA: `82ea8a1d984a69468572c211bb5cd36c7f573659`
- Parent SHA: `bc13fad1c7cc1fde6d6d8790fb777b8f50ba1680`
- Base SHA: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/compositor-task-facts-k3-review` (detached at the candidate)
- Build root (`<ROOT>`): `/home/cabewse/work_SPaC3/builds/qindaqt/review-taskfacts-k3`

`git rev-parse HEAD` equalled the exact candidate SHA and `git status --porcelain`
was empty before the review and again after all work (verified last, after every
gate). No product path was edited, committed, amended, or rebased. All scratch
reproductions live under `<ROOT>/scratch`.

## Findings ledger

### P0 — none

### P1 — none

Attacked and cleared:

- **Authentication before parsing / constant-size denial:**
  `ShellTaskFactsController::snapshot` (`src/compositor/src/shelltaskfacts.cpp:341`)
  runs the panel-owner PID join (ADR-0061 rule: caller unique name → daemon PID ==
  sole committed dock-surface owner PID, PID > 1, name starts with `:`) before
  consulting the source; the denial is one fixed compact object with only
  `status`/`schemaVersion`/`failure`. `tst_shelltaskfacts.cpp` proves byte-identical
  replies for a wrong-PID caller and a 1,000,000-character caller name, and that the
  source is never sampled (`source.calls == 0`).
- **Bounded fields/counts, atomic validation:** store-side
  `validateShellTaskFactsCandidate` and the wire-side
  `decodeShellTaskFactsSnapshot` enforce 4 MiB / 4,096 windows / 2,048 containers /
  64 outputs / 64 workspaces / 64 workspace refs per window / 512 UTF-16 units,
  exact key sets, canonical decimal counters, control/format/null/surrogate
  rejection, duplicate and unknown-reference rejection, exactly-one-primary and
  ≥1-member container membership, at-most-one-active, and no active-minimized
  combination. Rejected candidates retain the prior generation (tested).
- **One generation, fencing:** the payload carries windows + output/workspace
  inventories + container revision/authority lineage + the `(epoch,
  actionRevision)` action fence sampled in the same synchronous pass
  (`kwinshelltaskfacts.cpp:212-223`). The shell producer binds
  `(owner, epoch, revision, bytes)`, accepts forward gaps, and rejects epoch
  changes, regressions, and equal-revision byte collisions fail-closed
  (`task_list_facts_producer.cpp:332`, tested by
  `foreignEpochRegressionAndCollisionAreRejected`).
- **Coalesced signals, no polling:** publisher coalesces via `singleShot(0)`
  plus `Unchanged` suppression; the invalidation is a targeted signal to the
  bound owner only, re-authorized before each send, and revoked on panel-owner
  edges (`kwinshellwindowactions.cpp:293-311`). The consumer's retry schedule is
  now finite and provably stops (new negative control
  `retryScheduleStopsWithoutPolling`: exactly 1 initial + 3 retry reads, then
  silence).
- **T1 consumer:** coherent snapshot → one T0 generation → Ready; malformed or
  incoherent data degrades with retained truth; owner loss/replacement clears
  old-owner truth and degrades; late/stale-owner/token replies fenced. No
  independent-inventory join: `grep` confirms the producer transport/producer
  reference only `CompositorShell1.TaskListSnapshot`; the legacy
  `Windows`/`Containers` decoders have no production caller left.
- **Operation fencing:** the composition router fences window intents with the
  snapshot-carried action generation, requires the facts authority and the
  window-actions client to name the same unique owner, requires Ready displayed
  revision, serializes one operation, and maps timeout/owner change to
  `Uncertain` with no replay (`tasklistappletcomposition.cpp:154-198`).
- **ShellDevelopment1.Snapshot `taskList` + boot row:** exact `{phase,
  generation, windowCount}` shape enforced by both validators; proven
  fail-closed with mutated fixtures (see below).

### P2 — none

### P3 — one

- **P3-1: stale ownership comment for the legacy wire decoders.**
  `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_wire.h:17-23`
  states the `Windows`/`Containers` values and decoders "remain only for the
  legacy Compositor1 operation adapter and compatibility tests". Reproduction:
  `grep -rn "decodeWindows\|decodeContainers\|TaskListWireWindow" src/shell/task_list/operations/ src/shell/runtime/`
  returns nothing; the only caller in the tree is
  `tests/shell/task_list/tst_task_list_wire.cpp`. The decoders are test-only;
  the comment overstates an operation-adapter dependency that does not exist.
  Nonblocking precision; suggest dropping the decoders to test support or fixing
  the comment in a follow-up.

### Observations (not counted against the candidate)

- `qindaqt.shell-runtime-component-closure` fails in the fully-unset
  environment prescribed by the review brief (`DISPLAY`, `WAYLAND_DISPLAY`,
  `DBUS_SESSION_BUS_ADDRESS`, `XDG_RUNTIME_DIR` all unset): the staged
  `qindaqt-shell --help` aborts in Qt platform-plugin initialization
  ("XDG_RUNTIME_DIR is invalid or not set", wayland/xcb unreachable). This is
  pre-existing harness design, not a candidate defect: (1) none of the 56
  changed files touch shell startup, the test, or
  `tests/shell/audio_applet/run_shell_component_closure.cmake`; (2) the
  build-tree binary behaves identically; (3) the script unsets
  `DISPLAY`/`WAYLAND_DISPLAY` but not `XDG_RUNTIME_DIR`, so a passing run
  silently connects the staged shell to the host's `wayland-0` socket (present
  at `/run/user/1000/wayland-0`) — i.e. this row only ever "passed" by touching
  the host compositor. With `QT_QPA_PLATFORM=minimal` (no host contact) the row
  passes 1/1 in Debug and Release, proving the candidate's closure additions
  (staged `libqindaqt_compositor_shell_actions`) are intact. Recommend the
  harness owners set an offscreen/minimal QPA explicitly in that script.

## Commands and results

Configure (exact prescribed recipe, system-KWin initial cache, strict warnings):

- `cmake -S . -B <ROOT>/debug -G Ninja -C .../qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` → exit 0
- Same with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release` → exit 0

Builds (full tree, superset of the candidate's targets and focused/adjacent tests):

- `cmake --build <ROOT>/debug --parallel 3` → exit 0 (4165/4165)
- `cmake --build <ROOT>/release --parallel 3` → exit 0 (4165/4165)

Focused selector (brief regex expanded, since ctest rejects `(?!...)`, to the
enumerated equivalent: `^(qindaqt\.task-list-|qindaqt\.compositor|qindaqt\.shell-runtime-|qindaqt\.shell-development|qindaqt\.applet|compositor\.(shell-|container-|control-|development-|dbus-contract|hybrid|layout-|rejects-|transaction-))`,
run with `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS -u XDG_RUNTIME_DIR`
and `--output-on-failure --no-tests=error`):

- Debug: 66/67 passed (exit 8); the one failure is the environment-caused
  component-closure row analyzed above. All 24 task-list rows, all compositor
  unit rows including `compositor.shell-task-facts`,
  `compositor.shell-window-actions`, `compositor.shell-window-identity`,
  `compositor.dbus-contract`, and all applet/shell-runtime rows green.
- Release: identical, 66/67 (exit 8), same single environment failure.
- Both configurations: `QT_QPA_PLATFORM=minimal` rerun of
  `^qindaqt\.shell-runtime-component-closure$` → 1/1 passed, exit 0.

Safe DesktopVirtual rows (`^desktop\.virtual\.(sandbox-unit|notification-shell-readiness-unit|package-contract|stage-closure)$`, same unset environment):

- Debug: 4/4 passed, exit 0. Release: 4/4 passed, exit 0.

Permitted nested row (bounded exception; run once per configuration, serially,
only after `pgrep -f 'kwin_wayland.*qindaqt-parent-way[l]and'` printed nothing —
a foreign session's parent compositor occupied the lane first, and the run
waited for it to exit):

- Debug `^compositor\.kwin-shell-window-actions$` → 1/1 passed, exit 0.
- Release same → 1/1 passed, exit 0.

Static gates (from the worktree root):

- `./tools/validate-docs` → exit 0, 145 Markdown documents validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` → exit 0.
- `./tools/check-source-shape` → exit 0, 2552 files; all warnings are in files
  untouched by this diff.
- `git diff --check 86ad7c38..fa0e6d9c` → exit 0.
- `python3 -m json.tool` on both changed JSON fixtures → exit 0 each.
- Session python unit rows (not nested): `python3 -m unittest
  test_desktop_session_readiness_unit test_desktop_session_interactive_unit
  test_desktop_session_matrix_unit` in `tests/session` → 29/29 OK, exit 0.

Mutated-fixture proofs (scratch, under `<ROOT>/scratch`, never in the worktree):

- Python evidence validator against mutated copies of
  `tests/session/fixtures/desktop_session/probe-ready-1080p.json`: unmutated
  fixture accepted; `phase=degraded`, `windowCount=0`, `generation="0"`, missing
  `taskList`, extra field, boolean count, and non-canonical `generation="01"`
  each raise `RuntimeError` — the boot row cannot pass
  (`prove_tasklist_gate.py`, 7/7 PASS, exit 0).
- C++ boot validator (`tests/session/desktopnotificationshellreadiness.cpp`
  compiled into a scratch harness `probe_tasklist_readiness`): unmutated
  snapshot → Ready; 14 mutations (`phase` degraded/loading/empty/foreign,
  `windowCount` 0/bool/fractional/missing, `generation` zero/noncanonical/
  numeric, extra field, `taskList` absent/array) → Pending
  (`task-list-not-ready`, never Ready, so the boot deadline expires) or Invalid
  (`task-list-invalid`/`invalid-shape`, hard fail). exit 0.

Not run (per contract): `tests/session` nested-compositor rows, all other
`desktop.virtual.*` interactive/boot rows, host D-Bus services, hardware,
uinput, network.

## Verdict

The candidate implements the ADR-0072 contract as documented: authentication
strictly precedes sampling, denial is constant-size, bounds and references are
validated atomically with last-good retention, one generation joins
windows/outputs/workspaces/roles/container lineage and the action fence,
invalidation is directed and coalesced with no polling, the T1 consumer fences
owner/epoch/revision/bytes fail-closed and degrades truthfully, the operation
path stays exactly-once fenced, and the boot row provably fails when the task
list is not ready or lists zero windows. Tests carry real negative controls;
documentation matches the code (one P3 stale comment). P0 = P1 = P2 = 0.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
