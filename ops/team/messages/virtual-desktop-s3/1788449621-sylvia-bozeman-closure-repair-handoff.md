# Sylvia Bozeman — DesktopVirtual stage-closure repair handoff

- Candidate product commit: `91377acf822155b241fbb2474bec64a01069b6dc`.
- Candidate tree: `40f243e552be1bf76b6ed25cdf794900bab7ab12`.
- Exact integration base merged before repair: `b971b43881fcef18980acec03c4e43e56ef9db2a`.
- Repair parent after the required merge: `7bae40ddf496aa034e5ced3682dec809662f8aac`.
- Rejected predecessor: `99b06199cc256b3a76c1a6c0449690282dd3b2b6`.

## Finding closure

- Ingrid Daubechies P1 reproduced before repair: the exact candidate Debug
  selector passed 3/3, while its isolated staged `qindaqt-settings` exited 3
  because Audio, Bluetooth, and Power were not installed. Main's
  `197f1046a319571698c832ca6f938a83b2c44b0f` supplies those route payloads in
  the split `DesktopSessionRouteStaging.cmake`; closing commit
  `91377acf822155b241fbb2474bec64a01069b6dc` registers
  `SettingsApp/Main.qml` as a `desktop.virtual.stage-closure` source, records
  only Customize and PowerBackend as embedded modules, keeps the isolated
  staged Settings Center live offscreen, and adds the exact removed-Power-
  `qmldir` negative control. The nearby `AGENT-NOTE:` names Ingrid's P1 and
  rejected candidate. The hostile unit row also rejects an embedded-module
  exemption that no longer names an import.
- Running the repaired QML guard against Ingrid's preserved exact-`99b0619`
  stage produced the expected internal exit 1 with
  `staged QML module QindaQt.SettingsApp.Audio has no qmldir`; the expectation
  wrapper exited 0. The repaired Debug verbose row then passed 1/1 with 35 ELF
  files, 401 `DT_NEEDED` entries, 15 product QML imports, both staged
  applications loadable, and both negative controls reaching their intended
  failure.

## Changed paths relative to the exact integration base

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/applet-runtime.md`
- `ops/team/messages/virtual-desktop-s3/1788439737-sylvia-bozeman-stage-closure-claim.md`
- `ops/team/messages/virtual-desktop-s3/1788440857-sylvia-bozeman-stage-closure-midpoint.md`
- `ops/team/messages/virtual-desktop-s3/1788441870-sylvia-bozeman-handoff.md`
- `ops/team/workers/sylvia-bozeman.md`
- `tests/session/CMakeLists.txt`
- `tests/session/DesktopPackageTests.cmake`
- `tests/session/DesktopSessionTests.cmake`
- `tests/session/DesktopVirtualAppletModules.cmake`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/session/desktop_session_stage_closure.py`
- `tests/session/test_desktop_session_stage_closure.py`
- `tests/session/test_desktop_session_stage_closure_unit.py`

The repair commit itself changes only the owning testing-harness page and the
four focused stage-closure test paths.

## Acceptance evidence

- Exact pre-merge Debug selector under nonexistent bus addresses: exit 0,
  3/3 passed; the subsequent exact staged Settings Center reproduction exited
  3 and named the missing Audio, Bluetooth, and Power modules.
- Debug configure using the assigned initial cache and all brief flags: exit 0;
  only the repository's existing CMake RPATH warnings were emitted.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/desktop-stage-closure/debug --parallel 3 --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe`:
  exit 0.
- Debug required selector under `env -u DBUS_SESSION_BUS_ADDRESS
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1`: exit 0,
  3/3 passed.
- Debug verbose `^desktop\.virtual\.stage-closure$`: exit 0, 1/1 passed; 35
  ELF files, 401 dynamic dependency entries, 15 QML imports, staged shell and
  Settings Center loadability, and both negative controls passed.
- Release configure using the assigned initial cache and all brief flags: exit
  0; only the repository's existing CMake RPATH warnings were emitted.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/desktop-stage-closure/release --parallel 3 --target qindaqt-desktop-session-probe qindaqt-panel-visibility-session-probe`:
  exit 0.
- Release required selector under `env -u DBUS_SESSION_BUS_ADDRESS
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1`: exit 0,
  3/3 passed.
- `ctest -R '^session\.python-syntax$'` under the required bus environment:
  Debug exit 0, 1/1 passed; Release exit 0, 1/1 passed.
- Direct focused unit invocation with `PYTHONPATH=tests/session` and build-root
  `TMPDIR`: exit 0, 4/4 passed. An earlier invocation without `PYTHONPATH`
  exited 1 before discovery with `ModuleNotFoundError`; it was an invocation
  error, not a product-test failure.
- `./tools/validate-docs`: exit 0, 135 Markdown documents and `mkdocs.yml`
  navigation validated.
- Strict MkDocs build to the assigned build root: exit 0.
- `./tools/check-source-shape`: exit 0, 2,301 source files checked and zero
  allowlisted skips; only reported decomposition warnings pre-exist this repair.
- `git diff --check`: exit 0.
- No JSON file changed in the lane diff, so no JSON parser gate applied.

## Bounded caveats and requested next action

- No nested compositor, host bus, hardware, uinput, network, or rendered-
  session evidence is claimed. The Settings launch is offscreen, uses an exact
  stage-local environment, and points both D-Bus addresses at a nonexistent
  endpoint.
- The row proves package/loadability closure and hostile omission detection;
  it does not replace the manager-owned nested desktop qualification.
- Requested next action: Ingrid Daubechies performs an independent exact review
  of `91377acf822155b241fbb2474bec64a01069b6dc`, then the Program Manager
  integrates the accepted candidate.
