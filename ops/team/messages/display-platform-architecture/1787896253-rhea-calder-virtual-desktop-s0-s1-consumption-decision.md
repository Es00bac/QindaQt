# Rhea Calder — ADR-0015 implementation-consumption decision

- Timestamp: `2026-08-28T05:50:53Z`
- Inputs: Elara Finch `1787895545`; accepted ADR-0015; current TASK_LIST/testing harness; exact D0 `f38453393ef2d10aaac1af27a4209b998fa8546e`; exact D1 series ending `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`; Notification Live blocker `1787895783`; current Text Editor S1 reviews.
- Activity: analysis only. No product/Git edit, build, test, nested session, process, display, input, or host-state access.

## Decision

Assign **VD-1: contained integrated boot at 1080p** as one vertical candidate combining Elara **S0 + S1**. S0 alone is useful unit infrastructure but not an executable desktop outcome; S1 alone is unsafe because its teardown/host-containment claim depends on S0. VD-1 ends when one staged production session boots at `1920x1080@1`, proves the resident graph plus one Settings application, records D0 output/input and shell-surface truth, measures resident PSS/CPU, and tears down with zero survivors.

Do not include interaction, screenshots/OpenGL, DPI/theme variants, multi-output mutation, Text Editor, or D1 transaction execution. Those begin after VD-1 is stable.

## Integration/dependency order

1. Integrate independently accepted D0 and D1 first. VD-1 consumes D0's `Compositor1.Outputs`, `InputCapabilities`, and output-generation equality with `ShellVisibilitySnapshot`. D1 is an ordering/regression dependency only: its pure transaction model has no runtime service and VD-1 must not invent one.
2. Give Soren the already-requested private lane, obtain an exact Notification Live commit/review, and integrate it before VD-1 runtime. VD-1 consumes its private locker policy, exact process-group fallback, owner/PID helpers, and development `DevelopmentShellSurfaces` method. Do not copy uncommitted candidate code.
3. Text Editor S1 is **not** a VD-1 dependency. Use already integrated `qindaqt-settings` as the first application. After Text Editor commits/integrates, its installed `qindaqt-editor --report-startup` seam becomes an S2 application row; VD-1 does not touch Linnea's paths.
4. Settings1 and Audio1 are hard resident dependencies already present in the integrated platform base. Start their exact staged executables explicitly; do not rely on their configure-time `/usr/bin` activation descriptors.

## Exact VD-1 ownership

New harness paths:

```text
tests/session/desktop_session_sandbox.py
tests/session/desktop_session_measure.py
tests/session/test_desktop_session_sandbox_unit.py
tests/session/test_desktop_session_nested.py
tests/session/desktopsessionprobe.cpp
tests/session/DesktopSessionTests.cmake
tests/session/fixtures/desktop_session/smaps_rollup.txt
tests/session/fixtures/desktop_session/proc_stat.txt
```

Small coordinated integration edits:

```text
tests/session/CMakeLists.txt
src/compositor/kwin/kwincontrolendpoint.cpp
docs/wiki/reference/compositor-control-v1.md
docs/wiki/architecture/compositor-session.md
docs/wiki/development/testing-harness.md
docs/wiki/adr/0023-contain-virtual-desktop-qualification.md
docs/wiki/adr/index.md
mkdocs.yml
```

`tests/session/CMakeLists.txt` only discovers pinned `/usr/bin/{python3,dbus-run-session,bwrap,busctl}`, builds `qindaqt-desktop-session-probe`, and includes `DesktopSessionTests.cmake`. The CMake include owns both rows and sets `RUN_SERIAL TRUE`, timeout, labels, skip code, required targets, stage arguments, and the shared private-lane resource lock.

The only compositor change broadens the already development-gated Notification method to return mapped/committed `scope == "dock"` layer surfaces as well as notification scopes. Production remains byte-identical `control-disabled`. This is required to prove the production shell rendered in the same whole-session run; `ShellVisibilitySnapshot` and `Windows` do not describe panel layer-surface mapping. Do not edit D0 inventory/seam files or any D1 path.

Public test interfaces:

- `desktop_session_sandbox.py`: typed spec → deterministic bwrap argv; `--print-command-json`; explicit configured process-group fallback only.
- `test_desktop_session_nested.py`: `--outer` stages and owns containment/cleanup; `--inner` starts exact staged Settings1/Audio1, compositor/session, probe, and Settings app; one `QINDAQT_DESKTOP_SESSION_EVIDENCE=<json>` marker plus mirrored artifact.
- `qindaqt-desktop-session-probe`: public D-Bus only, reusing `compositorprobeclient.{h,cpp}`; reports service owners, D0 outputs/generation, input devices, one mapped `dock` surface on `Virtual-0`, and the Settings window. It never imports KWin private headers.
- Evidence schema is Elara section 7 narrowed to boot fields; `interaction.batches=0` and `screenshots=[]` are explicit, not omitted.

## Acceptance matrix and commands

| Row | Required proof | Failure conditions |
| --- | --- | --- |
| `desktop.virtual.sandbox-unit` | exact environment allow-list; deterministic argv; private `/run`, `/tmp`, `/dev`; no input/uinput; cross-worktree lock; smaps/stat parsers; timeout/cleanup paths | broad writable bind, host socket/env source, implicit fallback, malformed proc data accepted, or unbounded cleanup |
| `desktop.virtual.boot.1080p` | staged KWin/QindaQt plugin + `qindaqt-session`; exact Settings1/Audio1 owner PID/exe; notification host + shell resident; Settings window exposed; D0 one `1920x1080@1` output and equal nonzero output/visibility generation; exactly one development input device; mapped/committed `dock` surface on `Virtual-0`; aggregate resident PSS ≤1,024 MiB and measured idle CPU; bounded exit and zero survivors | missing/wrong process, output/device/surface mismatch, PSS over ceiling, host reachability, timeout, survivor, or missing bounded artifact/log/evidence |

```sh
cmake --preset dev
cmake --build --preset dev --parallel 1 --target qindaqt-desktop-session-probe qindaqt-wm qindaqt-session qindaqt-shell qindaqt-notification-host qindaqt-settings-service qindaqt-audio-service qindaqt-settings qindaqt_compositor qindaqt_decoration

# Safe unit gate; no private runtime allocation needed.
ctest --test-dir build/dev --parallel 1 --output-on-failure \
  -R '^desktop\.virtual\.sandbox-unit$'

# Manager-allocated single private lane only.
ctest --test-dir build/dev --parallel 1 --output-on-failure \
  -R '^desktop\.virtual\.boot\.1080p$'

# Same-tree regression boundary.
ctest --test-dir build/dev --parallel 1 --output-on-failure \
  -R '^(session\.virtual-output\.|compositor\.(kwin-plugin-nested|production-control-read-only)|qindaqt\.display-|shell\.production-surface\.(1080p|wuxga|1440p))'
./tools/validate-docs
./tools/check-source-shape --largest 40
git diff --check
```

## Private-lane containment

- Source/unit work may proceed without the lane. The boot row requires an explicit manager allocation, `RUN_SERIAL TRUE`, and a per-user cross-worktree advisory lock; a build-local `desktop-session-runs/*/lock` is insufficient.
- Primary mode is bwrap PID/network/IPC isolation with `--die-with-parent`, `--new-session`, `--clearenv`, private `/run`, `/tmp`, `/dev`, and only exact read-only system/stage/source binds plus one writable run root. No `/dev/input`, `/dev/uinput`, host runtime, host home/config, host Wayland/X11/bus/PipeWire socket, or render node is visible in VD-1.
- Process-group mode is an explicit configure choice, never automatic. It reuses the integrated Notification helper and records the containment downgrade in evidence.
- Every phase deadline enters the same outer `finally` cleanup. Post-exit host audit matches the unpredictable run-id and exact executable identities, without connecting to the host compositor/session.

## P0–P2 corrections to Elara's proposal

- **P0:** `--dev-bind / /` is a broad writable host-root exposure and cannot land. Use explicit read-only binds and private writable roots only.
- **P0:** staged Settings1 and Audio1 descriptors expand to absolute `/usr/bin/...`; merely prepending staged `XDG_DATA_DIRS` does not activate staged binaries. VD-1 explicitly starts both staged executables and authenticates owner PID/exe.
- **P0:** `ShellVisibilitySnapshot`/`Windows` cannot prove panel layer surfaces mapped. The integrated Notification method currently filters to notification scopes; VD-1 must add development-only `dock` evidence before claiming a rendered shell.
- **P1:** Elara recommends S1 first although G3/S0 is a P0 prerequisite. Consume them as one candidate, with S0 unit green before any S1 runtime.
- **P1:** the proposed grep rejects safe command text because the argv necessarily names private `/run/user` and `.X11-unix` destinations. Validate structured argv sources, bind modes, and environment values instead.
- **P1:** a lock beneath one build's run root does not serialize other worktrees. Use the manager allocation plus a per-user cross-worktree lock.
- **P2:** do not defer `--unshare-ipc` until S5; it is cheap structural containment and belongs in S0.
- **P2:** a new sandbox dependency/process boundary requires ADR-0023 in VD-1 under repository documentation policy; it cannot wait until S5.
- **P2:** KWin/session graceful exit remains an inference. VD-1 records exact exit modes and requires bounded zero-survivor containment; it must not claim graceful child shutdown until the supervisor SIGTERM bridge is implemented and proven.

## Manager next action

Preserve D0 unchanged, complete independent D0/D1 review/integration, allocate Notification Live's pending runtime lane, then branch one dedicated harness implementer from that integrated base for VD-1 above. Elara can exact-review the resulting S0+S1 commit. No Text Editor or D1 runtime owner is needed for this slice.
