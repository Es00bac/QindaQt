# KWin 6.6.6 panel-visibility handoff

- Worker: Erna Hoover-Codex (`erna-hoover-codex`)
- Candidate commit: `34db07da093a461c22a3779e0f5e7192d40f6e59`
- Candidate tree: `9574cf42d05b8d4afd3e44ec4ce2b24652b66aa1`
- Exact base: `aa862093993cc263c0059efca0f67282fcdb265a`
- Branch: `worker/panel-visibility-kwin666`
- Requested next action: independent exact review, then manager integration.

## Outcome and diagnosis

The KWin 6.6.5-to-6.6.6 source comparison found no layer-shell,
exclusive-zone, keyboard-interactivity, window-added, or visibility-fact
semantic change. The relevant upstream changes were confined to interactive
xdg-toplevel move geometry, FIFO fallback ownership, an unmanaged-X11 focus
guard, and scene-item lifetime repair.

The preserved and fresh failing evidence instead showed revision-4 compositor
authority with the active fullscreen proof window covering `WL-0`, while shell
surface authority had already removed the intelligent left panel. The original
probe combined authority settlement and framebuffer capture into one error, so
the later failure was mislabeled `window-overlap-hidden`.

The system-KWin sandbox deliberately excludes private Weston libraries from
its global loader path to prevent `/usr/bin/kwin_wayland` from loading the
private 6.6.5 libkwin closure. That also left private Weston 15's
`weston-screenshooter` unable to load `libweston-15.so.0`. The candidate passes
the authenticated parent-compositor loader path as a dedicated probe argument
and applies it only to the screenshot child. Authority and capture diagnostics
are now distinct, and a registered hostile unit row rejects missing, relative,
or partially empty loader paths.

KWin 6.6.6 may append `Session process has crashed` only during successful
terminal cleanup because the harness sends `SIGTERM` to the authenticated
shared KWin/session process group. Passing evidence has all eight captures
before that point, a complete `term`/`already-exited` ledger, and an independently
observed empty survivor set. The marker is therefore explained as teardown
ordering, not a hidden panel phase or compositor crash.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/panel-visibility.md`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/session/panel_visibility_capture.py`
- `tests/session/panelvisibilitysessionprobe.cpp`
- `tests/session/test_panel_visibility_capture_loader_unit.py`
- `tests/session/test_panel_visibility_nested.py`

## Verification evidence

All nested commands used
`QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`, `--parallel 1`, and
an explicit `pgrep -f '[k]win_wayland'` empty check before and after each row.

- Debug configure with
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-kwin666/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug`:
  exit 0.
- Debug focused builds with `cmake --build .../debug --parallel 3 --target`
  for the panel session/settlement probes, all visibility policy/client/producer
  targets, orchestration inventory, runtime options, `qindaqt-shell`,
  `qindaqt-shell-preview`, and all 45 compositor test executables: exit 0;
  recorded incremental batches were 1404/1404, 24/24, 57/57, 54/54, and
  264/264 actions.
- Release configure with the same cache and `-DCMAKE_BUILD_TYPE=Release`:
  exit 0.
- Release focused build of the same panel/visibility/runtime/shell targets:
  exit 0, 1506/1506 actions. Release build of all 45 compositor test
  executables: exit 0, 275/275 actions.
- `ctest --test-dir .../{debug,release} -R '^(qindaqt\.shell-(visibility|orchestration-visibility|runtime-)|desktop\.virtual\.panel-visibility\.(capture-loader-unit|validator-unit))' --output-on-failure --no-tests=error`:
  exit 0, 22/22 in Debug and 22/22 in Release.
- `ctest --test-dir .../{debug,release} --parallel 1 -R '^compositor\.' --output-on-failure --no-tests=error`:
  exit 0, 49/49 in Debug and 49/49 in Release.
- `ctest --test-dir .../{debug,release} --parallel 1 -R '^desktop\.virtual\.panel-visibility\.single-1080p$' --output-on-failure --no-tests=error`:
  two accepted passes per configuration, each 2/2 including the package fixture;
  an additional post-refactor Debug pass was 2/2.
- `ctest --test-dir .../{debug,release} --parallel 1 -R '^desktop\.virtual\.panel-visibility\.single-wuxga$' --output-on-failure --no-tests=error`:
  two accepted passes per configuration, each 2/2; an additional post-refactor
  Release pass was 2/2.
- `ctest --test-dir .../{debug,release} --parallel 1 -R '^desktop\.virtual\.boot\.1080p$' --output-on-failure --no-tests=error`:
  two passes per configuration, each 2/2.
- Latest post-refactor Debug and Release panel archives each contain eight phase
  PNGs and `survivorPids: []`; final host check found no `kwin_wayland` process.
- `./tools/validate-docs`: exit 0, 138 Markdown documents.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/panel-visibility-kwin666/site`:
  exit 0.
- `./tools/check-source-shape --root tests/session --largest 20`: exit 0,
  145 files; the modified driver is 362 non-blank lines and the C++ probe is
  408, with no modified-file shape error.
- `git diff --check`: exit 0.
- No JSON file changed, so no JSON parser gate applied.

Expected negative evidence was also recorded: the unrepaired Debug nested row
failed 1/2 after authority had hidden the panel and before creating the first
PNG; the new unit contract is absent on the exact base and fails closed without
the third loader-path argument. One initial Release acceptance attempt exited
1 after a transient private-bus portal helper overlapped topology sampling; it
left no survivor and did not recur across the subsequent accepted passes.

## Bounded caveats

- Repository-wide `./tools/check-source-shape` exits 1 because unchanged exact-base
  `src/apps/file_manager/main.cpp::main` spans 181 lines against the 180-line
  limit. `git diff aa862093993cc263c0059efca0f67282fcdb265a --
  src/apps/file_manager/main.cpp` is empty, and that path is outside this lane.
- Qualification is limited to the documented 100% 1080p and WUXGA software
  nested paths. It makes no fractional-scale, multi-output, GPU, physical-input,
  screenshot-baseline, or host-session claim.
