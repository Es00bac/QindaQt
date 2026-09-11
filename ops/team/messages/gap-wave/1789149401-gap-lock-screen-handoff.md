# gap-lock-screen handoff

- **Candidate commit:** `d0af6528` (branch `gap/lock-screen`; last deliverable commit `a67faab0`, this message commit is the collection point)
- **Base commit:** `7dad9e78f117d7fb492d381d631d7cec637ce1e5`
- **Worker:** GLM 5.3 Flash in kimi-code, lane `gap-lock-screen`
- **Date:** 2026-09-11T11:56:41-06:00

## Outcome delivered

Settings → Power gained a complete lock story, PowerDevil lid/power-button
policy, and one Meta+L owner (ADR-0132):

1. Screen lock rows: **Lock after waking from sleep** (`LockOnResume`) and
   **unlock grace** (`LockGrace`: immediately/5 s/30 s/1 min/5 min) beside the
   existing idle lock. The ADR-0091 strictness is preserved and tightened:
   write set is exactly the four keys, unchanged values are never written (a
   no-op save leaves the file byte-identical), `configure` is requested once
   per mutation, unreadable configs fail closed.
2. New module `src/session/powerdevil_lid` (`PowerDevilLidAdapter`): writes
   only `[AC|Battery|LowBattery][SuspendAndShutdown] LidAction`,
   `InhibitLidActionWhenExternalMonitorPresent`, `PowerButtonAction`, then
   reloads via `org.kde.Solid.PowerManagement.refreshStatus` with the same
   owner fencing/error truth as `powerdevil_idle`. Offered actions are the six
   supported `PowerDevil::PowerButtonAction` values (0/1/2/8/32/64); 16 and
   128 exist upstream but are not offered. Values cited from upstream tag
   `v6.6.6` (`daemon/powerdevilenums.h`, `PowerDevilProfileSettings.kcfg`,
   `daemon/actions/bundled/handlebuttonevents.cpp:203-206,172-187`,
   `daemon/actions/bundled/suspendsession.cpp:111-141`), which matches the
   installed `libpowerdevilcore.so.6.6.6`; moc enum names in the installed .so
   corroborate. `powerdevil_idle`-equivalent current-value display properties
   were added to the adapter (read fallback NoAction/true).
3. Lid presence: Power1 **already carried** `SourceTruth.lidPresent` end to
   end (codec, service assembly, logind-derived `m_lidProven`). The additive
   piece is the route model's `lidPresent` property behind the shared
   admission predicate plus a QML-visible `lidPolicy` port (injected opaque
   `PowerButtonLidPolicyPort`, same pattern as the session-actions facade).
4. Power page: new icon-led rows — power button action, lid action, external
   monitor switch (both lid rows hidden without an admitted lid), resume-lock
   switch, grace selector (out-of-set stored values shown as an extra entry,
   never silently rewritten). Tooltips, accessible names/descriptions,
   keyboard operable; `firstFocusTarget` chain extended through the new
   section. QML files remain under the 275-line review threshold.
5. Meta+L: the shell's dead `qindaqt_lock_session` registration is removed;
   KWin's ksmserver "Lock Session" component is the sole owner.
   `check_runtime_boundary.cmake` now FAILS if the composition reintroduces
   `qindaqt_lock_session`, `KGlobalAccelShortcutRegistrar`, or `Qt::Key_L`
   (mutation-sensitive poison included), and docs no longer claim shell
   ownership.
6. Private proof: `tools/lock-proof/private-lock-proof.sh` (run via
   `qq-private gap-lock-screen tools/lock-proof/private-lock-proof.sh`).
   Result on this machine: **lock engaged** — `Lock` accepted and
   `GetActive` returns `true` in a KWin virtual session on a private bus
   (`PROOF OK`). Recorded limitations (exact evidence in the proof dir):
   `kscreenlocker_greet` is not spawned because KWin reports "Could not load
   a session backend…" in the logind-less virtual session; ScreenShot2
   refuses with `org.kde.KWin.ScreenShot2.Error.NoAuthorized`. Unlocking
   requires the session password and cannot be automated.

## Changed files

`git diff --name-only 7dad9e78f117d7fb492d381d631d7cec637ce1e5..HEAD`
(41 files, +2473/−67):

docs/wiki/adr/0132-finish-session-locking.md, docs/wiki/adr/index.md,
docs/wiki/apps/power-settings.md, docs/wiki/architecture/compositor-session.md,
docs/wiki/architecture/module-boundaries.md,
docs/wiki/architecture/powerdevil-idle.md,
docs/wiki/handbook/catalog/features.md, docs/wiki/shell/applet-runtime.md,
docs/wiki/shell/power-applet.md, mkdocs.yml, src/CMakeLists.txt,
src/apps/settings/power/CMakeLists.txt,
src/apps/settings/power/include/qindaqt/apps/settings_power/lid_power_button_settings.h,
src/apps/settings/power/include/qindaqt/apps/settings_power/power_settings_model.h,
src/apps/settings/power/include/qindaqt/apps/settings_power/screen_lock_settings.h,
src/apps/settings/power/lid_power_button_settings.cpp,
src/apps/settings/power/power_route_composition.cpp,
src/apps/settings/power/power_settings_model.cpp,
src/apps/settings/power/qml/PowerLidPowerButtonSection.qml,
src/apps/settings/power/qml/PowerPage.qml,
src/apps/settings/power/qml/PowerScreenLockSection.qml,
src/apps/settings/power/qt_powerdevil_lid_port.cpp,
src/apps/settings/power/qt_powerdevil_lid_port.h,
src/apps/settings/power/screen_lock_settings.cpp,
src/session/powerdevil_lid/CMakeLists.txt,
src/session/powerdevil_lid/include/qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h,
src/session/powerdevil_lid/src/powerdevil_lid_adapter.cpp,
src/shell/runtime/powerappletcomposition.cpp,
src/shell/runtime/powerappletcomposition.h, tests/CMakeLists.txt,
tests/apps/settings/power/CMakeLists.txt,
tests/apps/settings/power/check_boundary.cmake,
tests/apps/settings/power/check_installed_route.cmake,
tests/apps/settings/power/stub_power_settings_model.h,
tests/apps/settings/power/tst_power_lid_presence.cpp,
tests/apps/settings/power/tst_power_page.cpp,
tests/apps/settings/power/tst_screen_lock_resume_settings.cpp,
tests/session/powerdevil_lid/CMakeLists.txt,
tests/session/powerdevil_lid/tst_powerdevil_lid_adapter.cpp,
tests/shell/power_applet/check_runtime_boundary.cmake,
tools/lock-proof/private-lock-proof.sh

## Verification commands (all run through the lane helpers)

Final combined suite (exit 8 due to one environmental row below):

```
qq-test gap-lock-screen 'qindaqt\.(settings-power|settings-screen-lock|session-powerdevil|power-applet|power-service|power-client|power-protocol|session-lock)'
→ 37 rows: 36 passed, 1 failed
```

Focused rows all PASS (each verified individually during the lane):

- `qindaqt.settings-screen-lock-model` PASS (0.07 s)
- `qindaqt.settings-screen-lock-resume` PASS (new, 7 rows)
- `qindaqt.settings-power-lid-presence` PASS (new, 4 rows)
- `qindaqt.settings-power-model` PASS; `qindaqt.settings-power-slider-admission-debounce` PASS
- `qindaqt.settings-power-page` PASS (11 rows incl. new resume/grace, lid-policy, focus-chain rows)
- `qindaqt.settings-power-boundary` + `-poison` PASS (composition-only rule for the PowerDevil lid adapter, new QML file token)
- `qindaqt.session-powerdevil-lid` PASS (new, 7 rows incl. busy rejection, unsupported-action rejection ×3 signatures, refresh-failure, absent-owner recovery)
- `qindaqt.session-powerdevil-idle` PASS (unrelated-key preservation unchanged)
- `qindaqt.session-powerdevil-lifetime` PASS
- `qindaqt.power-applet-*` (controller, controls, presentation, request-state, qml/offscreen, boundary, runtime-boundary, installed-package) PASS
- `qindaqt.power-service-*`, `power-client`, `power-protocol-*`, `session-lock-*` PASS
- `qindaqt.session-lock-authentication/-transitions/-qt-transport` PASS

Static gates:

- `./tools/validate-docs` → exit 0 ("Validated 238 Markdown documents and mkdocs.yml navigation")
- `mkdocs build --strict --site-dir .../gap-lock-screen/site` → exit 0
- `./tools/check-source-shape` → no report for any file this lane created or changed (overall exit 1 comes from pre-existing reports in audio/, compositor/, shellruntimeapplication.cpp, task_list/, calendar, etc.)
- `git diff --check 7dad9e78…` → clean
- JSON files changed: none

## Deliberately left out

- Upstream grace options outside the requested ladder (`-1` never-require
  password via `RequirePassword`, 900 s, free-form custom): RequirePassword
  stays outside the adapter's four-key write set (ADR-0132).
- `PromptLogoutDialog` (16) and `ToggleScreenOnOff` (128) power-button
  actions: supported by PowerDevil but outside the requested choice set; the
  adapter's supported-action table is the single place to extend.
- ADR-0070 text unchanged (ADRs are history); ADR-0132 supersedes its Meta+L
  sentence.

## Requests for the Program Manager

1. **`qindaqt.settings-power-installed-route` fails on this machine for an
   environmental reason, verified not caused by the lane:** the relocation
   poison withholds the installed Power module, but Qt's import resolution
   falls back to the system-wide QindaQt install
   (`/usr/lib64/qt6/qml/QindaQt/SettingsApp/Power`), so the relocated app
   stays alive instead of exiting 3. Evidence: `QML_IMPORT_TRACE=1` run of
   the staged binary shows
   `QindaQt.SettingsApp.Power module's qmldir found at "/usr/lib64/qt6/qml/..."`
   with the staged module withheld. This machine has QindaQt installed under
   /usr; a tree without a system install passes. I did not weaken the check.
   One real environment defect in the script was fixed within my owned
   paths: the sandbox moved from
   `<stage>/power-route-runtime` (Wayland socket path >108 bytes in deep lane
   layouts) to `<build>/pr-sandbox`.
2. Pre-existing defect fixed additively in my owned paths:
   `PowerIdleDisplaySection.qml` was missing from the route's second
   `install(FILES ...)` list; it is now installed (together with the new
   section file). Please re-verify on the integration tree.
3. Harness-page rows for the new test names: `qindaqt.settings-screen-lock-resume`,
   `qindaqt.settings-power-lid-presence`, `qindaqt.session-powerdevil-lid`
   need rows on the testing-harness page (I do not own that page).
4. `docs/wiki/architecture/hybrid-topology.md` was named in my prompt as
   carrying a Meta+L sentence; it contains none (grepped Meta, lock,
   shortcut, ksmserver). No edit made there.
5. Checks worth running on the installed desktop: Settings → Power shows the
   two new screen-lock rows and (machines with a lid) the two lid rows;
   pressing the power button still triggers PowerDevil's configured action;
   Meta+L locks via ksmserver with exactly one binding in
   `kglobalshortcutsrc`; Settings writes exactly the four
   `kscreenlockerrc [Daemon]` keys and the three
   `powerdevilrc [<profile>][SuspendAndShutdown]` keys per save.
6. Packaging: no new dependencies beyond KF6::ConfigCore/Qt6::DBus already in
   use; `src/session/powerdevil_lid` installs its public header via the
   existing QindaQtTargets export.
