# Gertrude Blanch — independent portal/interop exact-candidate review

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `c33b4908f99cb1dfac04287383441d028ab8f25b`
- Tree SHA: `bfd3464afaa46af6b2b4f4a3cce1a603008b4c3d`
- Sole parent SHA: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Base SHA: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/portal-p1-selection-k3-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Contract questions and attack evidence

### 1. Selection is real and exclusive

Accepted. `src/services/portal/data/qindaqt.portal:1` advertises exactly
`org.freedesktop.impl.portal.Settings`; `src/services/portal/data/qindaqt-portals.conf:1`
uses `default=none`, selects Settings only from `qindaqt`, explicitly orders the
reviewed fallback families as `kde;gtk;lxqt`, and closes Background with `none`.
The installed frontend is exactly `xdg-desktop-portal 1.20.4`.

The real-frontend Debug and Release selectors each passed 9/9. In particular,
`tests/services/portal/tst_portal_frontend_integration.cpp:135` checks the exact
three-key QindaQt projection through frontend `ReadAll` and every frontend
`Read`; line 208 observes live `SettingChanged`; line 225 proves FileChooser
reaches the injected KDE fallback; line 240 requires Background failure; and
line 248 restarts the frontend as `other-desktop` and requires Settings failure.

The requested hostile scratch mutation was placed only under the assigned build
root. It added FileChooser to QindaQt's staged `.portal` and changed the staged
selector to `FileChooser=qindaqt`. The exact metadata checker rejected it with
exit 1 at `tests/services/portal/check_boundary.cmake:54`. Running the real
frontend selection executable against both hostile files also exited 1 with
`FileChooser did not resolve to the declared KDE fallback`, exercising
`tst_portal_frontend_integration.cpp:233`. Thus both the static package contract
and the live routing proof catch an attempted QindaQt FileChooser selection.

The host's KDE declaration additionally advertises Account, Clipboard,
DynamicLauncher, GlobalShortcuts, InputCapture, USB, and Wallpaper. These are
not among ADR-0057's reviewed fallback families and are intentionally closed by
`default=none`; the reference table states the same rule for every unlisted
family. QindaQt gains no implementation authority from any fallback entry.

### 2. Toolkit reaction is executable evidence

Accepted. `tests/services/portal/portal_toolkit_probe.cpp:49` reads
`QStyleHints::colorScheme()`, line 52 derives/applies the initial palette, line
58 observes the live hint signal and re-derives the palette, and lines 69-94
require Dark followed by live Light with contrasting palettes. The harness sets
`QT_QPA_PLATFORM=offscreen` and `QT_QPA_PLATFORMTHEME=xdgdesktopportal` at
`tst_portal_frontend_integration.cpp:286` and joins the frontend live change to
normal probe exit at lines 303-338.

The ordinary row passed in both profiles. With
`QINDAQT_TEST_TOOLKIT_PROBE=/usr/bin/false`, the registered row did not skip: it
failed 0/1 with CTest exit 8 after reporting that the portal-theme probe did not
reach the live-change result. The normal configure cache resolved the frontend
to `/usr/libexec/xdg-desktop-portal`, and `ctest -N` listed both frontend rows.
A separate configure with
`-DQINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE:FILEPATH=` emitted the candidate's
"Portal P1 frontend rows are not registered" status and `ctest -N` listed only
the seven non-frontend portal rows; neither helper target existed. This matches
the configure-time condition at `tests/services/portal/CMakeLists.txt:85`.

### 3. Fallback routing is fail-closed

Accepted. A normalized extraction/diff of the 12 interface rows in
`docs/wiki/reference/portal-settings-backend-v1.md:24` against the selector's
12 interface assignments produced no diff; the separate unlisted-family row
matches `default=none`. The live FileChooser fallback and Background failure
passed in Debug, Release, the poisoned-outer-bus replay, and both staged-package
replays. The QindaQt `.portal` stays Settings-only, so the explicit fallback
table does not extend this module's D-Bus surface.

### 4. Packaging remains singleton and mutation-sensitive

Accepted. Both staged-package rows passed as part of the 9/9 selector and each
replayed selection plus toolkit against installed artifacts. The singleton
guard is at `tests/services/portal/run_staged_package.cmake:69`; installed
frontend replays are at lines 126-157; installed metadata/private-header poison
controls are at lines 175-232. The CTest dependency joins the staged package to
both new rows at `tests/services/portal/CMakeLists.txt:215`.

An independent Debug component install under the assigned build root found
exactly one each of `org.freedesktop.impl.portal.desktop.qindaqt.service`,
`xdg-desktop-portal-qindaqt.service`, `qindaqt.portal`, and
`qindaqt-portals.conf`. `cmp -s` returned 0 for both the installed `.portal`
against source and the installed selector against source. Exactly five public
portal headers were installed and no private adapter header was present.

### 5. Documentation and ADR scope are truthful

Accepted. README names `xdg-desktop-portal` 1.20+ and at least one explicitly
routed fallback provider. ADR-0057 explicitly grounds the implementation-only
scope in ADR-0054. The architecture and harness pages describe the two real
frontend rows and staged replays while expressly excluding GTK/GSettings,
Flatpak, an installed/host session, a real chooser, and all real non-Settings
implementation claims. The module-boundary update preserves Settings-only
authority.

## Commands and results

All commands below ran from the exact detached review worktree unless the path
in the command says otherwise.

1. Identity and cleanliness:
   `git rev-parse HEAD`; `git rev-parse HEAD^{tree}`; `git rev-parse HEAD^`;
   `git status --porcelain` — exit 0; exact candidate/tree/parent above; empty
   status before review and immediately before verdict creation.
2. Host facts:
   `/usr/libexec/xdg-desktop-portal --version` — exit 0,
   `xdg-desktop-portal 1.20.4`.
   Installed `.portal` inventory was read with
   `for f in /usr/share/xdg-desktop-portal/portals/*.portal; ...`; no installed
   artifact was changed.
3. Debug configure:
   `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` — exit 0.
4. Release configure: the preceding exact command with build directory
   `/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/release` and
   `-DCMAKE_BUILD_TYPE=Release` — exit 0.
5. Focused Debug build:
   `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug --parallel 3 --target qindaqt_portal_toolkit_probe qindaqt_portal_frontend_integration_tests xdg-desktop-portal-qindaqt qindaqt_portal_appearance_policy_tests qindaqt_portal_settings_source_tests qindaqt_portal_service_tests qindaqt_portal_process_lifecycle_tests` — exit 0, 106/106 actions.
6. Focused Release build: the preceding command under `release` — exit 0,
   106/106 actions.
7. Debug selector:
   `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug -R '^qindaqt\.portal-' --output-on-failure --no-tests=error` — exit 0,
   9/9 passed.
8. Release selector: the preceding command under `release` — exit 0, 9/9
   passed.
9. Poisoned outer bus:
   `DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug -R '^qindaqt\.portal-frontend-' --output-on-failure --no-tests=error` — exit 0, 2/2 passed. The test command unsets the caller address before creating its own `dbus-run-session` bus.
10. Toolkit negative control:
    `QINDAQT_TEST_TOOLKIT_PROBE=/usr/bin/false ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug -R '^qindaqt\.portal-frontend-toolkit$' --output-on-failure --no-tests=error` — expected exit 8, 0/1 passed, explicit test failure rather than skip.
11. Hostile metadata checker:
    `cmake -DPORTAL_ROOT=/home/cabewse/work_SPaC3/container-wm-workers/portal-p1-selection-k3-review/src/services/portal -DPORTAL_METADATA_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/hostile/qindaqt.portal -DPORTAL_SELECTION_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/hostile/qindaqt-portals.conf -P tests/services/portal/check_boundary.cmake` — expected exit 1, rejected the non-Settings `.portal` addition.
12. Hostile live routing:
    `env -u DBUS_SESSION_BUS_ADDRESS -u DBUS_STARTER_ADDRESS -u DBUS_STARTER_BUS_TYPE QINDAQT_TEST_PORTAL_METADATA=/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/hostile/qindaqt.portal QINDAQT_TEST_PORTAL_SELECTION=/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/hostile/qindaqt-portals.conf dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/debug/tests/services/portal/qindaqt_portal_frontend_integration_tests selection` — expected exit 1; observed KDE fallback not reached.
13. Configure-time absence control: the exact Debug configure recipe under
    `/home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/no-xdp` plus
    `-DQINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE:FILEPATH=` — exit 0; configure
    explicitly reported the P1 rows unregistered. `ctest -N -R '^qindaqt\.portal-'`
    listed seven tests and no frontend row; Ninja target inventory contained
    neither P1 helper target.
14. `./tools/validate-docs` — exit 0; 118 Markdown documents and navigation
    validated.
15. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-portal-codex/site` — exit 0; strict build completed.
16. `./tools/check-source-shape` — exit 0; 1,793 files checked, zero skipped.
    It reported only the two known threshold warnings outside candidate-owned
    portal paths: `tests/compositor/CMakeLists.txt` at 500 and
    `tests/services/display_color_model/tst_color_model.cpp` at 539 non-blank
    lines.
17. `git diff --check`; `git diff --cached --check`; and
    `git diff --check f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9..c33b4908f99cb1dfac04287383441d028ab8f25b` — exit 0.
18. Routing documentation normalization/diff using `awk` extractions and
    `diff -u` — exit 0, no diff. The selector and reference table agree for all
    12 listed interfaces and separately agree on `default=none` for unlisted
    families.
19. `cmake --install .../debug --prefix .../manual-stage-c33b490 --component QindaQtPortalP0`, followed by `find` singleton counts and `cmp -s` — exit 0; four required runtime-discovery artifacts each count 1; both policy files byte-identical to source; five public headers.
20. Changed JSON inventory was empty, so `python3 -m json.tool` was not
    applicable.
21. Final process inspection found no build-root portal frontend, QindaQt portal
    backend, Settings service, integration-test, or `dbus-run-session` survivor.
    The pre-existing host portal PIDs observed before the review remained alive
    and were not contacted, stopped, or modified.

No `tests/session` row, host D-Bus service, hardware/uinput path, or network was
run.

## Verdict

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
