# gap-night-light handoff — 2026-09-11T13:05:00-06:00

- Candidate commit: `719d3d47f6d60edc2c09542a749fce3a515181d8`)
- Base commit: `7dad9e78f117d7fb492d381d631d7cec637ce1e5`
- Branch `gap/night-light`, worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/gap-night-light`.

## Outcome delivered

Settings → Display gains a **Night light** section: on/off, schedule
(sunset to sunrise by automatic location or manual coordinates, custom
times, always on), night/day temperature sliders (1000–6500 K, 100 K
steps, live preview through KWin), transition length, and a status line
(active now, current temperature, next change). The section is fail
closed: it says so and disables everything when KWin's night light
service is unavailable, and disables only the schedule rows when
knighttimed is absent. Backed by the new `src/services/night_light`
module and ADR-0136.

## Changed files (39)

```
docs/wiki/adr/0136-night-light-through-kwin.md
docs/wiki/adr/index.md
docs/wiki/apps/display-settings.md
docs/wiki/architecture/module-boundaries.md
docs/wiki/architecture/night-light.md
mkdocs.yml
ops/team/messages/gap-wave/1781219400-gap-night-light-claim.md
src/CMakeLists.txt
src/apps/settings/display/CMakeLists.txt
src/apps/settings/display/display_night_light_model.cpp
src/apps/settings/display/display_night_light_route.cpp
src/apps/settings/display/include/qindaqt/apps/settings_display/display_night_light_model.h
src/apps/settings/display/include/qindaqt/apps/settings_display/display_night_light_route.h
src/apps/settings/display/qml/DisplayNightLightLocationRow.qml
src/apps/settings/display/qml/DisplayNightLightSection.qml
src/apps/settings/display/qml/DisplayNightLightTemperatureRow.qml
src/apps/settings/display/qml/DisplayNightLightTimesRows.qml
src/apps/settings/display/qml/DisplayPage.qml
src/services/night_light/CMakeLists.txt
src/services/night_light/include/qindaqt/services/night_light/night_light_config_port.h
src/services/night_light/include/qindaqt/services/night_light/night_light_state_port.h
src/services/night_light/include/qindaqt/services/night_light/night_light_values.h
src/services/night_light/include/qindaqt/services/night_light/night_time_schedule_monitor.h
src/services/night_light/src/night_light_values.cpp
src/services/night_light/src/night_time_schedule_monitor.cpp
src/services/night_light/src/qt_config_night_light_port.cpp
src/services/night_light/src/qt_night_light_state_port.cpp
tests/CMakeLists.txt
tests/apps/settings/display/CMakeLists.txt
tests/apps/settings/display/stub_night_light_model.h
tests/apps/settings/display/tst_display_night_light_section.cpp
tests/services/night_light/CMakeLists.txt
tests/services/night_light/check_boundary.cmake
tests/services/night_light/check_boundary_negative.cmake
tests/services/night_light/proof/night_light_proof_helper.cpp
tests/services/night_light/proof/run_night_light_proof.sh
tests/services/night_light/tst_night_light_config_port.cpp
tests/services/night_light/tst_night_light_state_port.cpp
tests/services/night_light/tst_night_light_values.cpp
```

Additive shared edits: `src/CMakeLists.txt` and `tests/CMakeLists.txt`
(`add_subdirectory(services/night_light)` after the display_color_assignment
anchor), `mkdocs.yml` (night light page nav after display-color-model;
ADR-0136 nav after ADR-0131), `docs/wiki/architecture/module-boundaries.md`
(night light row after the display_color_assignment row), `docs/wiki/adr/index.md`
(ADR-0136 after ADR-0131). `DisplayPage.qml` gained exactly one import
(the module's own URI, for the singleton) plus the section instance;
`src/apps/settings_center` and `src/shell/**` untouched.

## Commands run (all with exit status)

| Command | Result |
| --- | --- |
| `qq-test gap-night-light 'qindaqt\.night-light'` | exit 0 — 5/5 passed: values, config-port, state-port, boundary, boundary-poison |
| `qq-test gap-night-light 'qindaqt\.display-night-light-section'` | exit 0 — 1/1 passed |
| `qq-test gap-night-light 'qindaqt\.(display-settings-model\|display-settings-model-adversarial\|display-page\|display-night-light-section\|night-light)'` | exit 0 — 9/9 passed (includes the pre-existing display rows: no regressions) |
| `qq-private gap-night-light …/proof/run_night_light_proof.sh` | exit 0 — PROOF PASS (evidence under `builds/qindaqt/gap-night-light/proof/`) |
| `./tools/validate-docs` | exit 0 — 238 documents validated |
| `mkdocs build --strict --site-dir …/gap-night-light/site` | exit 0 |
| `./tools/check-source-shape` | exit 1 — 13 errors, **all pre-existing in files this lane did not touch** (audio, compositor, shell runtime, task_list, bluetooth, panelgeometry); no error for any file created or changed here |
| `git diff --check 7dad9e78…` | exit 0 |

Private proof highlights (virtual KWin 6.6.6, socket `qq-gap-night-light-0`,
private bus + private config dir):

- A config pre-seeded through the production `QtConfigNightLightPort`
  (`Active=true`, `Mode=Constant`, `NightTemperature=3400`) drives the real
  compositor at startup: `enabled=true running=true mode=0
  currentTemperature=3400 targetTemperature=3400`.
- `preview(2700)` animates the reported temperature 3400 → 2700 K over the
  bus; `stopPreview` ends it.
- `inhibit()` is connection-scoped against the real plugin: the lock holds
  while the caller lives and disappears when the caller exits without
  `uninhibit` — the basis for offering no pause in Settings (ADR-0136).
- Bounded caveat: a changed value written while the compositor ran did not
  converge in the sandbox even with an explicit KWin `reconfigure`; the
  watcher delivery could not be observed end-to-end. Recorded in ADR-0136;
  the shell quick-toggle follow-up should re-check live convergence on a
  real session.

## Deliberately left out

- No pause control in Settings: `inhibit()` is connection-scoped; a Settings
  process cannot hold a meaningful pause (ADR-0136; proved). Shell
  quick-toggle is the follow-up.
- No schedule truth of our own: sunset/sunrise stays with knighttimed;
  QindaQt writes only the schedule *choices*. `knighttimestaterc` is never
  read or written.
- No shell quick-toggle, no panel quick setting (off limits this wave).

## Requests for the Program Manager

1. **Packaging**: add `kde-plasma/knighttime` to RDEPEND (knighttimed +
   libKNightTime for scheduled night light; already requested in ADR-0136
   Consequences). The Display route CMake now calls
   `qindaqt_install_shell_icons_runtime(SettingsAppearanceRuntime)` — same
   call Audio/Customize already make, so the ebuild needs nothing new.
2. **testing-harness.md rows** (page is lane-frozen) for the new test names:
   `qindaqt.night-light-values`, `qindaqt.night-light-config-port`,
   `qindaqt.night-light-state-port`, `qindaqt.night-light-boundary`,
   `qindaqt.night-light-boundary-poison`,
   `qindaqt.display-night-light-section`.
3. **Installed-desktop checks**: open Settings → Display, confirm the Night
   light section appears (it must render unavailable-and-disabled when KWin's
   night light service is absent), toggle it, move the night temperature
   slider (screen warms after the settle, reverts on release), apply, and
   confirm the change survives a re-login. On a session with
   `plasma-knighttimed` running, confirm schedule changes take effect live.
4. The service module links `KF6::ConfigCore` (writer for the two owned
   files; `powerdevil_idle` precedent) — noted in ADR-0136 and the module
   boundary row; no other new dependency.
