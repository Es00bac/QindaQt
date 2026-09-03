# KWin 6.6.6 panel repair handoff

- Worker: Erna Hoover-Codex (`erna-hoover-codex`)
- Candidate commit: `1d86b1349cc51c0516c269e1f9eb228fd6fb20cc`
- Candidate tree: `669f60fa8861ec646ed0613388a149dcba110f84`
- Exact repair base: `34db07da093a461c22a3779e0f5e7192d40f6e59`
- Candidate parent: `fbf0de92d0a8b4f4e68b0286032bcc291dfa092a` (coordination-only handoff commit above the repair base)
- Original integration base: `aa862093993cc263c0059efca0f67282fcdb265a`
- Branch: `worker/panel-visibility-kwin666`
- Requested next action: independent exact review then manager integration.

## Outcome

The private session now terminates the authenticated KWin PID before group
cleanup and rejects any `Session process has crashed` diagnostic. The contained
fixture disables Qt/GTK portal activation, removing the transient second
`dbus-daemon` race without weakening topology validation. Screenshot-child
loader injection is now one production C++ boundary with a compiled hostile
unit row; the pre-existing Python command contract remains independently
covered. Panel rows receive a 100-second attempt budget inside the unchanged
110-second CTest budget so all eight captures, validation, and clean shutdown
complete before the outer archive deadline.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/panel-visibility.md`
- `tests/session/CMakeLists.txt`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/session/desktop_session_process.py`
- `tests/session/desktop_session_runtime.py`
- `tests/session/desktop_session_sandbox.py`
- `tests/session/desktop_session_shutdown.py`
- `tests/session/panelvisibilitycaptureprocess.cpp`
- `tests/session/panelvisibilitycaptureprocess.h`
- `tests/session/panelvisibilitysessionprobe.cpp`
- `tests/session/test_desktop_session_contract_unit.py`
- `tests/session/test_desktop_session_nested.py`
- `tests/session/test_desktop_session_process_unit.py`
- `tests/session/test_panel_visibility_nested.py`
- `tests/session/tst_panelvisibilitycaptureprocess.cpp`

## Verification evidence

All nested rows ran serially with
`QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop` and the system-KWin
cache `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake`.

- Debug and Release configuration used the prescribed Ninja command with
  `BUILD_TESTING=ON`, all shell/plugin options enabled, host uinput disabled,
  and strict warnings enabled: exit 0 in both profiles.
- `cmake --build <profile> --parallel 3 --target` for the two panel helper
  tests, both session probes, the owned visibility/compositor/runtime unit
  targets, `qindaqt-shell`, and `qindaqt-shell-preview`: exit 0 in Debug and
  Release.
- `ctest --test-dir <profile> -R '^(qindaqt\.shell-(visibility|orchestration-visibility|runtime-)|desktop\.virtual\.panel-visibility\.(capture-loader(-cpp)?-unit|validator-unit))' --output-on-failure --no-tests=error`:
  exit 0, 23/23 in Debug and 23/23 in Release.
- `ctest --test-dir <profile> -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error`:
  exit 0, 1/1 in Debug and 1/1 in Release.
- `ctest --test-dir <profile> --parallel 1 -R '^(desktop\.virtual\.package-fixture|desktop\.virtual\.boot\.1080p|desktop\.virtual\.panel-visibility\.single-(1080p|wuxga))$' --output-on-failure --no-tests=error`:
  two independent passes in each profile, every pass 4/4. Debug totals were
  92.88 s and 92.75 s; Release totals were 92.46 s and 92.13 s.
- The twelve final live-row archives all report `outcome: success`. Exact scans
  found zero `Session process has crashed`, zero duplicate-private-bus
  diagnostics, and zero portal activation lines. A final
  `pgrep -f '[k]win_wayland'` returned exit 1 with empty output.
- `./tools/validate-docs`: exit 0, 138 documents.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-kwin666/site`:
  exit 0.
- `./tools/check-source-shape --root tests/session --largest 20`: exit 0,
  149 files; all owned sources are within the lane limits.
- Repository-wide `./tools/check-source-shape` on this historical branch:
  exit 1 solely for the inherited 181-line
  `src/apps/file_manager/main.cpp::main`. The same full gate on current `main`
  exits 0 across 2422 files; main already contains the out-of-lane repair.
- `git diff --check` and `git diff --cached --check`: exit 0.
- No source JSON changed; all twelve generated `result.json` evidence files
  were parsed successfully.

The unrepaired Release evidence identified portal activation immediately before
the second private-bus process appeared. During repair, one pre-final Debug
WUXGA run reached all eight captures but was killed at the old exact 70-second
outer deadline; that finding produced the bounded 100/110-second deadline
contract. The complete final matrices above were fresh runs, not retries.

## Bounded caveats

- Qualification covers the documented 100% 1080p and WUXGA software nested
  paths only. It makes no fractional-scale, multi-output, GPU, hardware-input,
  screenshot-baseline, host-session, or host-D-Bus claim.
- The candidate does not alter the exact topology validator and does not permit
  the former KWin crash diagnostic.
