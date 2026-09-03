# Karen Uhlenbeck handoff — Clipboard applet C2 production shell hosting

- Time: `2026-09-03T10:46:08-06:00`
- Candidate commit: `a069842659e3e96dd93d0f27a66049d9d3ff03c8`
- Candidate tree: `8035b067fdc585161cb042596e830c529facdb77`
- Exact base: `07861e19f52a3754f4f5e19587c1a63142abbcca`
- Branch: `worker/clipboard-composition`
- Requested next action: **independent exact review then manager integration**.

## Outcome

The production shell now owns a public Clipboard1 transport/client and borrows
its scoped Settings1 client to enforce exact `user-overrides` consent before
projecting history through the applet's narrow controller seam. Audited
read/write grants gate observation and mutation, owner loss purges stale truth,
privacy denial and the valid generation-ceiling purge preserve the C1 applet
phases, and Clipboard1-v1 pin requests fail closed.

The eighth compiled built-in is rendered in the shell and preview through a
keyboard-capable `Popup.Window` with accessible state, Tab/Return/Space parity,
Escape closure, and a mandatory dispatcher test import stub. Every stock
profile family places one Clipboard utility slot. `ClipboardAppletRuntime`
includes its relocatable runtime closure, every shell-bearing install component
stages the module, and DesktopVirtual receives the same module through the
owning install helper.

## Changed paths

- `data/profiles/gnome-inspired.json`
- `data/profiles/macos-inspired.json`
- `data/profiles/mate-inspired.json`
- `data/profiles/minimal.json`
- `data/profiles/nextstep-inspired.json`
- `data/profiles/qindaqt.json`
- `data/profiles/unity-inspired.json`
- `data/profiles/windows-classic.json`
- `data/profiles/windows-modern.json`
- `data/profiles/xfce-inspired.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/clipboard-applet.md`
- `src/shell/CMakeLists.txt`
- `src/shell/clipboard_applet/CMakeLists.txt`
- `src/shell/clipboard_applet/qml/ClipboardApplet.qml`
- `src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml`
- `src/shell/qml/BuiltinAppletContent.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/qml/PanelContent.qml`
- `src/shell/qml/RuntimePanel.qml`
- `src/shell/runtime/clipboardappletcomposition.cpp`
- `src/shell/runtime/clipboardappletcomposition.h`
- `src/shell/runtime/runtimepanelwindowfactory.cpp`
- `src/shell/runtime/runtimepanelwindowfactory.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/applet_runtime/tst_applet_instance_resolver.cpp`
- `tests/applets/tst_catalog.cpp`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`
- `tests/shell/clipboard_applet/CMakeLists.txt`
- `tests/shell/clipboard_applet/check_clipboard_applet_boundary.cmake`
- `tests/shell/clipboard_applet/qml/FakeClipboardController.qml`
- `tests/shell/clipboard_applet/qml/tst_ClipboardProductionPanelKeyboard.qml`
- `tests/shell/clipboard_applet/qml/tst_ClipboardProductionPanelReturn.qml`
- `tests/shell/clipboard_applet/qml_interactive_main.cpp`
- `tests/shell/clipboard_applet/run_installed_clipboard_applet.cmake`
- `tests/shell/clipboard_applet/run_installed_clipboard_runtime.cmake`
- `tests/shell/clipboard_applet/tst_clipboard_applet_composition_private_bus.cpp`
- `tests/shell/launcher/check_launcher_contract_text.cmake`
- `tests/shell/qml/imports/QindaQt/Shell/ClipboardApplet/ClipboardPanelApplet.qml`
- `tests/shell/qml/imports/QindaQt/Shell/ClipboardApplet/qmldir`

## Acceptance evidence

All commands below ran from the lane worktree. Final CTest acceptance commands
used `env -u DBUS_SESSION_BUS_ADDRESS -u DBUS_SYSTEM_BUS_ADDRESS -u DISPLAY -u
WAYLAND_DISPLAY`.

- Exact prescribed Debug configure command — exit `0`.
- Exact prescribed Release configure command — exit `0`.
- Focused Debug build of `qindaqt-shell`, `qindaqt-shell-preview`, manifest,
  catalog, resolver, Clipboard model/controller/fencing/admission/snapshot/seam,
  private-bus composition, QML, and runtime-options targets — exit `0`.
- The same focused Release build — exit `0`, `811/811` actions.
- `cmake --build <debug> --parallel 3 --target
  qindaqt_applet_host_policy_tests qindaqt_applet_host_handshake_tests
  qindaqt_applet_host_lifecycle_tests` — exit `0`, `12/12` actions.
- The same applet-host build in Release — exit `0`, `12/12` actions.
- `cmake --build <debug> --parallel 3 --target
  qindaqt-desktop-session-probe` — exit `0`, `761/761` actions.
- The same DesktopVirtual package-probe build in Release — exit `0`, `761/761`
  actions.
- `ctest --test-dir <debug> -R '^qindaqt\.clipboard-applet-'
  --output-on-failure --no-tests=error` under host-unset isolation — exit `0`,
  `16/16` passed.
- The same Clipboard selector in Release under host-unset isolation — exit `0`,
  `16/16` passed.
- `ctest --test-dir <debug> -R
  '^(qindaqt\.(applet-(manifest|catalog|runtime-resolution|host-policy|host-handshake|host-lifecycle)|global-menu-installed-package|audio-applet-installed-package|bluetooth-applet-installed-package|power-applet-installed-package|shell-runtime-(options|catalog|component-closure)|launcher-(panel-dispatcher|contract-text)|notification-center-applet-offscreen))$'
  --output-on-failure --no-tests=error` under host-unset isolation — exit `0`,
  `16/16` passed.
- The same adjacent selector in Release under host-unset isolation — exit `0`,
  `16/16` passed.
- `ctest --test-dir <debug> -R '^desktop\.virtual\.package-contract$'
  --output-on-failure --no-tests=error` under host-unset isolation — exit `0`,
  `1/1` passed.
- The same DesktopVirtual package-contract selector in Release — exit `0`,
  `1/1` passed.
- Standalone notification-center offscreen recheck after the path-ownership
  cleanup — Debug exit `0`, `1/1`; Release exit `0`, `1/1`.
- `./tools/validate-docs` — exit `0`, `135` Markdown documents plus
  `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir
  /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-composition/site` — exit `0`.
- `./tools/check-source-shape` — exit `0`, `2305` checked, `0` allowlisted.
  It reports existing review-threshold warnings; touched
  `shellruntimeapplication.cpp` is `499` non-blank lines and every new source
  remains below its enforced ceiling.
- `git diff --check` and `git diff --cached --check` — exit `0`.
- `python3 -m json.tool` over all ten changed profile JSON files — exit `0`.

Repair-cycle diagnostics were not used as acceptance evidence: early popup
runs exposed and then repaired missing static QML plugin registration, font
pinning, popup focus traversal, and Return-order assumptions; the first full
Clipboard run was `15/16` because the new composition poison regex missed a
path form; and the first adjacent run was `12/13` because the runtime-options
executable had not yet been built. Each was repaired and replaced by the final
green isolated selectors above. A mistaken `^qindaqt\.desktop...$` package
selector exited `8` with no tests found; the registered
`^desktop\.virtual\.package-contract$` selector then passed in both builds.

## Bounded caveats

- The non-negotiable common-contract rule forbids executing `tests/session`
  nested-compositor rows or touching the host desktop. Therefore
  `desktop.virtual.boot.1080p` and both
  `desktop.virtual.panel-visibility.*` rows were deliberately **not run** in
  this worker lane, despite their later lane mention. This candidate claims
  only the static DesktopVirtual package contract; the manager/reviewer must
  schedule any authorized nested qualification separately.
- No live host clipboard, ambient session/system bus, hardware, network,
  Wayland selection, or nested capture-proof behavior was exercised or is
  claimed.
- Clipboard1 version 1 exposes no pin operation. The production adapter keeps
  the visible applet action fail-closed with a precise feedback message rather
  than reaching into service internals.
