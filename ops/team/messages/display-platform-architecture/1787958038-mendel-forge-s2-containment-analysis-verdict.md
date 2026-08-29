# Mendel Forge — private interactive 1080p S2 containment/proof verdict

- Timestamp: 2026-08-28T17:00:38-06:00
- To: Hypatia the 3rd (implementer), Sol (Program Manager); Astra Quill for
  cross-reference only
- Analyst: Mendel Forge — Anthropic Claude / exact `claude-fable-5` / high
- Candidate: `7a2088971ca5d8e380c50282f64d23042ba2be95`
- Tree: `f0d1793af2e2c05d9bc6ec82671ccfac42761116`
- Sole parent: `ce6b3124cf6de7213c194e11d109593aec1f6b0d`
- Worktree: `/mnt/d/QindaQt/reviews/virtual-desktop-s2-mendel` (detached,
  tracked tree byte-clean; no candidate bytes changed)

## Terminal verdict: ACCEPT — P0 0 / P1 0 / P2 2 / P3 4

No blocking finding. Nothing I could construct from the exact source or the
preserved evidence makes this S2 row pass falsely, escape the sandbox, touch
the host pointer/display/session bus/configuration, accept unauthenticated or
stale evidence, or leave an authenticated role alive. The findings below are
proof-strength and claim-accuracy gaps that should be repaired as a reviewed
descendant or documented; none changes the outcome of run `2f7b7ed9`.

## What was actually verified (observed, not inferred)

Static/source (exact commit):

- Containment is structural, not asserted: `desktop_session_sandbox.py:267-343`
  builds `--unshare-user/pid/net/ipc/uts --die-with-parent --new-session
  --clearenv --tmpfs /` with a minimal `--dev /dev` (no `/dev/input`,
  `/dev/dri`, `/dev/uinput`), a fresh `/run` tree, and binds only the run-root
  `runtime/artifacts/logs` plus ro `/usr`, the linuxbrew prefix (no sockets
  found under its `var`/`etc`), stage, source, and probe.
  `run_inner` re-checks `FORBIDDEN_ENVIRONMENT` at
  `desktop_session_runtime.py:335`.
- The parent Weston is spawned only with `--backend=headless --renderer=pixman
  --shell=kiosk --no-config --fake-seat --socket=qindaqt-parent-wayland`
  (`desktop_session_runtime.py:153-160`); `weston --help` on the installed
  15.0.1 confirms `--fake-seat` is a headless option. KWin gets
  `WAYLAND_DISPLAY=qindaqt-parent-wayland` (`:167`), apps get the distinct
  child socket `qindaqt-<run-id[:12]>` (`:127`, `:184`).
- `capture_parent_frame` fails closed unless `WAYLAND_DISPLAY` is exactly the
  parent socket, the destination is fresh, and exactly one new PNG appears
  (`desktop_session_capture.py:150-172`); the PNG parser checks every chunk
  CRC, exact IHDR contract, exact decompressed length, IEND, and trailing
  bytes (`:30-107`).
- `InjectTestInput` is rejected unless `m_mutationsEnabled`
  (`src/compositor/kwin/developmentinputprotocol.cpp:223`); the probe sends
  exactly four events and requires `deviceId == qindaqt-development-input`
  (`tests/session/desktopsessionprobe.cpp:119-134`), then polls the
  compositor-owned inventory for exactly one active/mapped/committed 440x640
  `notification-center` surface (`:139-166`).
- Teardown revalidates exe + starttime for every authenticated identity before
  each signal phase and raises on any survivor
  (`desktop_session_process.py:189-284`); bwrap PID-namespace death is the
  backstop.

Preserved evidence (read-only, `/mnt/d/QindaQt/builds/virtual-desktop-s2-hypatia/tests/session/desktop-session-results/`):

- Final run `2f7b7ed9`: `result.json` outcome `success`, return 0, 5.06 s.
  `sandbox-command.json` argv matches the source exactly (no host `/run`,
  no `WAYLAND_DISPLAY`/`DISPLAY`, private `/run/user/1000`).
- Recomputed SHA-256 of `artifacts/desktop-1080p.png` =
  `fac9a24de83bb7051a8359e299d1068ddaf18122c1d418e0ad35d65a7f711cf3`, equal
  to the evidence digest. I viewed the image directly: global bar, both docks,
  Text Editor, Settings—Notifications, and the open Notifications panel at
  top-right are all present.
- Readiness snapshot (`logs/session-probe-004.log`) lists only the two `dock`
  surfaces before injection; the notification-center surface appears only in
  `session-interaction.log` afterwards, so causality is observed for this run.
- Eleven terminal phases, all PID/group/path/startTicks authenticated; Weston
  logged `caught signal 15` and exited in phase `term`.
- Seven attempts preserved: two honest failures (`aacf4a6b`: missing
  TextEditor/AppShell; `e5d5c16d`: fake-seat input pair), one S1
  (`2fe58029`, `Virtual-0`), four S2 successes.

Probes actually run (all under `/tmp/qindaqt-mendel-s2`, none touched the host desktop):

- Focused unit gate: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover
  -s tests/session -p 'test_desktop_session_*_unit.py'` → `Ran 69 tests … OK`,
  exit 0 (Hypatia reported 67/67; both pass, count differs).
- Exact candidate validators replayed offline on preserved evidence:
  `validate_interactive_evidence` + `validate_capture` on `2f7b7ed9` → PASS,
  sha match; `validate_boot_evidence` on S1 `2fe58029` → PASS; the earlier
  race run `b47732ed` (`active=false`, geometry `0x0`) is now REJECTED by the
  committed validator — the disclosed race repair is real.
- Eleven evidence mutations against the committed validator: ten rejected
  (active=false, eventCount=3, width 1919, colors 15, `Virtual-0` output,
  dropped Weston phase, survivor PID, PSS over ceiling, foreign surface PID,
  dropped forwarded keyboard); one accepted → P3-2 below.
- `mkdocs build --strict` (from `/tmp/qindaqt-docs-venv`, output to `/tmp`)
  → exit 0. `git diff --check HEAD^ HEAD` → exit 0.
- `weston --version` / `--help`: 15.0.1, `--fake-seat` present.

## Findings

### P2-1 — PSS aggregate omits two QindaQt production roles despite "production-role" wording

`desktop_session_runtime.py:373-375` sums only `compositor, session,
notification, shell, settings-service, audio-service`. The S2 topology also
contains the first-party `settings-app` (`qindaqt-settings`) and `editor-app`
(`qindaqt-editor`) (`desktop_session_topology.py:87-88`). ADR-0049 lines 47-52
and `testing-harness.md:1200-1201`/`1228` describe the number as the
"production-role"/"QindaQt production roles" aggregate, excluding only Weston.
The reported 103,061 KiB therefore understates the documented claim by two Qt
Quick application processes. This is inherited from S1 but restated as an S2
proof claim. It cannot flip the 1,048,576 KiB ceiling here, so it is not
blocking. Repair: sample all eight QindaQt roles, or enumerate the six sampled
roles in ADR-0049/the wiki and stop calling it the production aggregate.

### P2-2 — The capture is not machine-tied to the interaction result

`capture_parent_frame` runs immediately after the probe observes the active
surface (`desktop_session_runtime.py:365-372`) with no presentation sync, and
`validate_capture` accepts any exact-size PNG with ≥16 sampled colors
(`desktop_session_capture.py:119-133`); nothing checks pixels inside the
440x640 rect at (1464,46). A frame captured before KWin presents the panel to
Weston would still pass, so "the screenshot contains the open notification
center" (`testing-harness.md:1230-1233`) is a human inspection, not an
enforced contract. Observed run `2f7b7ed9` does contain it (verified by direct
view). Repair: capture once before injection and once after, and require a
pixel delta inside the surface rect, or require a Weston frame callback after
the probe's success before capturing.

### P3-1 — `survivorPids` is a constant

`desktop_session_runtime.py:389` writes `"survivorPids": []` unconditionally
and `desktop_session_topology.py:428` only checks it equals `[]`. The real
guarantees are the authenticated ledger raising on survivors and PID-namespace
death; unauthenticated descendants are never enumerated. Repair: populate it
from a post-teardown `/proc` scan (excluding self) so the field is observed.

### P3-2 — Validator accepts `childWaylandSocket == parentWaylandSocket`

`desktop_session_interactive.py:44-51` only requires the child name to start
with `qindaqt-`, which `qindaqt-parent-wayland` also satisfies (mutation
accepted in my probe). Not reachable at runtime (`:127` derives the child name
from the run id), but the contract should state inequality explicitly.

### P3-3 — `sampledDistinctColors` is always the floor on success

The sampler breaks at `minimum_colors` (`desktop_session_capture.py:128-131`),
so every passing evidence document reports exactly 16; the field is a
threshold echo, not a measurement. Count the full 128x72 grid or document it
as a floor.

### P3-4 — No enforced pre-injection absence of the notification-center surface

Neither `desktopsessionprobe.cpp:139-166` nor the readiness validator requires
the `notification-center` surface to be absent/unmapped before the four events
are injected, so an already-open panel would satisfy the interaction contract.
The observed final run's readiness snapshot shows it absent, so the causal
claim holds for `2f7b7ed9`; make it a contract.

## Bounded caveats

- I did not run the private runtime row (Astra Quill's lane); no rebuild.
- Run `2f7b7ed9` was produced from the `virtual-desktop-s2-riven` working
  tree at 16:33, before the 16:35:45 commit. The preserved evidence satisfies
  the committed validators and the earlier race evidence is rejected by them,
  but byte-identity of the run-time tree with the commit is not provable from
  artifacts alone; the manager's post-integration rerun closes this.
- Source-shape gate and the 100-page documentation validator were not re-run
  by me (only `mkdocs --strict`); largest changed files are 372/433 non-blank
  lines, under the 500-line review threshold.
- ADR-0049 correctly limits the machine claim to "one rendered frame and one
  private-seat action"; P2-2 is about the wiki's stronger prose.

## Requested next action

Manager may integrate `7a208897` on this analysis (pending Astra Quill's
independent terminal verdict per the Board's two-reviewer rule). Route P2-1
and P2-2 to Hypatia the 3rd as a non-amended descendant on the current-base
replay; P3 items may ride with it. Board paths written by me:
`messages/display-platform-architecture/1787957483-mendel-forge-s2-containment-analysis-claim.md`,
this file, and `workers/mendel-forge.md`. This analysis process is no longer
live after this handoff.
