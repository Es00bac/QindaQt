# gap-wave handoff: gap-portals

- **Candidate commit:** `331eff9698043bca1b5695d3449a62d80f3c452e`
- **Base commit:** `7dad9e78f117d7fb492d381d631d7cec637ce1e5`
- **Branch:** `gap/portals` (worktree `/home/cabewse/work_SPaC3/container-wm-workers/gap-portals`)
- **Date:** 2026-09-11T11:30-06:00

## Outcome delivered

Every portal family advertised by the installed kde, gtk, lxqt, and
gnome-keyring `.portal` metadata has an explicit, tested routing decision
(ADR-0133), the selector carries it, and a private-bus ctest proof drives the
real `xdg-desktop-portal` 1.20.4 frontend with fake `kde`/`gnome-keyring`
backends to show FileChooser, Screenshot, ScreenCast, RemoteDesktop,
InputCapture, and Secret requests reaching the routed backend while Wallpaper
and Background stay unexported. Screen sharing, screenshots, file dialogs, and
Flatpak/GTK password storage now reach working backends in a QindaQt session;
the Secret routing row serves the gnome-keyring provider adopted by the
gap-keyring lane (its ADR-0135 owns the provider choice, this lane owns the
row).

## Changed paths (`git diff --name-only base..HEAD`)

```
docs/wiki/adr/0133-route-every-portal-family.md   (new)
docs/wiki/adr/index.md                            (ADR-0133 line after ADR-0131)
docs/wiki/architecture/module-boundaries.md       (portal row text, as permitted)
docs/wiki/architecture/portal-service.md
docs/wiki/development/gabbee-interop-evidence.md  (append-only smoke notes)
docs/wiki/reference/portal-settings-backend-v1.md (routing table)
mkdocs.yml                                        (nav line after ADR-0131)
src/services/portal/data/qindaqt-portals.conf
tests/services/portal/CMakeLists.txt
tests/services/portal/check_boundary.cmake
tests/services/portal/check_boundary_negative.cmake
tests/services/portal/portal_frontend_test_support.cpp
tests/services/portal/portal_frontend_test_support.h
tests/services/portal/proof/debug-routing.sh      (new, diagnostic driver)
tests/services/portal/proof/private-portal-proof.sh (new, real-backend smoke)
tests/services/portal/run_staged_package.cmake
tests/services/portal/tst_portal_frontend_integration.cpp (helper dedup)
tests/services/portal/tst_portal_frontend_routing.cpp (new)
```

Commits, oldest first: `48a10554` (ADR-0133), `79805805` (routing change),
`a3b1c34d` (routing proof), `331eff96` (smoke + docs).

## Verification evidence (commands actually run)

| Command | Result |
| --- | --- |
| `qq-build gap-portals <7 portal targets>` (two rounds, plus rebuilds) | exit 0 |
| `qq-reconfigure gap-portals` | exit 0 |
| `qq-test gap-portals 'qindaqt\.portal'` (final) | exit 0 — **12/12 passed, 0 failed** (`qindaqt.portal-appearance-policy, -settings1-source, -service, -process-lifecycle, -frontend-selection, -frontend-toolkit, -frontend-routing, -frontend-routing-negative, -source-boundary, -source-boundary-negative, -staged-package, -kde-compat`) |
| `qq-test gap-portals 'qindaqt\.portal'` (mid-development) | exit 1 → routing-proof defects found and repaired (missing `session_handle_token`; wrong InputCapture caller signature), then 12/12 |
| `ctest --test-dir .../gap-portals/debug -N` | 12 `qindaqt.portal-*` rows registered (2 new) |
| `qq-private gap-portals tests/services/portal/proof/debug-routing.sh` | exit 0 (routing mode under G_MESSAGES_DEBUG) |
| `qq-private gap-portals tests/services/portal/proof/private-portal-proof.sh` | exit 0; evidence in `/home/cabewse/work_SPaC3/builds/qindaqt/gap-portals/proof` (`summary.md`, `logs/`) |
| `./tools/validate-docs` | exit 0 — 238 documents validated |
| `mkdocs build --strict --site-dir .../gap-portals/site` | exit 0 |
| `./tools/check-source-shape` | exit 1 from **13 pre-existing errors in files this lane does not own** (audio_service, shell runtime/task_list, settings audio, bluetooth, panelgeometry, gabbee py warning); none in `src/services/portal`, `tests/services/portal`, or docs. New/changed files: largest is `portal_frontend_test_support.cpp` at 483 non-blank lines (< 500 review threshold) |
| `git diff --check 7dad9e78` | exit 0 |
| `python3 -m json.tool` | not applicable — no JSON changed |

Mutation sensitivity of the new proof: `qindaqt.portal-frontend-routing`
fails if any asserted row stops resolving (each family's call must reach the
routed fake; Wallpaper/Background must stay unexported);
`qindaqt.portal-frontend-routing-negative` stages the routing file with the
Secret row stripped and proves the Secret interface disappears while the
ScreenCast control still exports; exact-byte checks fail source, staged, and
installed-poison gates on any other conf byte change; dedicated poisons cover
Secret→kde, Secret row dropped, and Wallpaper→kde.

## Deliberately left out

- Real ScreenCast pixel capture: the smoke reaches real-backend CreateSession
  and stops before SelectSources/Start, where the KDE screen chooser needs a
  human (and the virtual compositor does not advertise
  `zkde_screencast_unstable_v1` per the existing interop notes).
- Real screenshot capture: the recorded run stops at the real consent dialog;
  the headless permission-store grant could not be seeded (recorded
  `UnknownMethod`), which is the documented valid stopping point.
- Usb export assertion in the routing proof: `org.freedesktop.portal.Usb`
  export is gated on the frontend's `HAVE_GUDEV` build flag, so it is not
  deterministic across builds. The row stays pinned by exact-byte checks.
- No changes to `src/session_supervisor/**` (restart list already covers the
  KDE backend and the frontend, ADR-0094).

## Requests for the Program Manager

1. Harness page rows for the two new test names in
   `docs/wiki/development/testing-harness.md` (no lane may edit it):
   `qindaqt.portal-frontend-routing` and
   `qindaqt.portal-frontend-routing-negative` — private-bus frontend rows,
   `RUN_SERIAL`, labels `service;portal;frontend;dbus;hostile`, staging like
   the existing frontend rows plus fake backend declarations.
2. Collision note: `gap-keyring` owns the Secret provider decision
   (ADR-0135); this lane owns the routing row. If ADR-0135 lands with a
   different provider than gnome-keyring, the selector row, the exact-byte
   pins (`check_boundary.cmake`, `check_boundary_negative.cmake`), and the
   routing proof's fake `gnome-keyring` declarations must change together.
3. `mkdocs.yml`/`docs/wiki/adr/index.md`: my ADR-0133 lines sit directly
   after the ADR-0131 line as instructed; merge order with the other four
   lanes is yours.
4. Installed-desktop spot check after integration (live session, no lane may
   do it): after relogin, `busctl --user introspect org.freedesktop.portal.Desktop
   /org/freedesktop/portal/desktop` should now list `org.freedesktop.portal.Secret`
   (gnome-keyring backend), and `org.freedesktop.portal.Wallpaper` /
   `org.freedesktop.portal.Background` must stay absent. A Flatpak/GTK password
   prompt should reach gnome-keyring.
5. The smoke's permission-store seeding failure (`UnknownMethod` on
   `/org/freedesktop/impl/portal/PermissionStore` despite owning the name) is
   recorded, not fixed — if integration wants an unattended screenshot capture,
   that store interaction needs a look first.

## Outcome statement for the milestone

Screen sharing (OBS, browsers, video calls), screenshots, file dialogs from
Flatpak and GTK apps, and password storage for Flatpak apps all reach working,
explicitly routed backends in a QindaQt session; every portal family has an
explicit, tested routing decision backed by a private routing proof, and a
private real-backend smoke records exactly how far unattended flows get.
