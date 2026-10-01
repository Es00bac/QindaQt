# Independent exact native runtime review — ACCEPT

- Reviewer: pf-portals-sol-20261001
- Time: 2026-10-01T03:20:04-06:00
- Immutable reviewed candidate: 891092f66029fdffa50258e98b6d52d3ed61007b
- Base: ab7640944d67b49f5fcbd198a96ac6c70cad193c
- Verdict: ACCEPT for the bounded native display-off and per-source Settings source boundary. Blocking findings: none.
- Requested next action: manager integration and affected integrated-tree gates; installed/physical qualification remains separate.

## Source review

Reviewed all50 changed paths, concentrating on IdlePolicy private KWayland controller/public port, DisplayOffStage, NativeLockComposition, Settings source model/QML/composition, Settings migration/import tests, desktop-controls removal and normative wiki/ADR0319. Candidate891 differs from tested ancestorc7e8fc3a only in desktop-controls documentation; reviewer still built the exact immutable891 source independently. No source edits or shared checkout/build mutations occurred.

- DPMS opens only the admission supplier's transferred connected FD after live checks. No environment-path reconnect exists; ordinary On/Off require live lineage. Revocation makes a final On only through the retained peer. Blocking worker flush has a bounded250ms writable wait, followed by proxy/queue teardown before connection destruction. Normal composition teardown explicitly calls restoreAndStop; source never relies on the queued stage.stop On alone. Connection death permits local cleanup. The actual private Wayland fixture proves revoked final On and exactly one supplier call, asynchronous output capability/removal, reconnect and repeated teardown with Qt warnings fatal.
- Display-off owns a separate admitted idle observer and six-key Settings client; it cannot overwrite the lock timer. Settings must be Ready with an exact current-owner snapshot, valid epoch/revision and exact bounded typed values. Degraded retained snapshots are unusable. Missing preferences restore/disarm without the old fallback. Power source selection consumes validated current Ready/Degraded Power1 state; malformed/Unavailable snapshots disarm. A present Power1 owner without a current authenticated inhibitor receipt suppresses, and DisplayOff leases suppress/restore. Power1 continues advertising zero consumed scopes because no complete idle-suspend consumer exists.
- Per-source Settings writes remain serialized and exact-owner/epoch; Applied replies are acknowledgements until fresh matching readback at or above revisionAfter. Missing/refused/mismatched/readback-timeout/replaced authority has bounded error/uncertainty paths without replay; Refresh is read-only. Unavailable authority disables edits while the model can retain previously read values behind its unavailable status. Existing four model cases directly prove positive fresh readback, source transitions and admission/bounds; not every uncertainty branch has a dedicated new model test, so no broader hostile-UI coverage claim is made.
- V1 migration copies only explicitly stored legacy global minutes, converts bounded positive values to source seconds, preserves disabled state, and leaves absent choices for ordinary schema defaults. Native explicit imports remain authoritative. Desktop-controls no longer binds the old idle preference to PowerDevil. ADR/wiki explicitly retain the optional PowerDevil child and its independently stored legacy policy until retirement; this is not single production display-power authority or full inhibitor compatibility.

## Independent executable evidence

Fresh isolated sources:

- Laptop: /home/cabewse/work_space/container-wm/.cache/pf-runtime-review-20261001, detached891.
- Qinda: /home/cabewse/work_SPaC3/container-wm.worktrees/pf-runtime-review-20261001, detached891; private build/review.

Read qinda /home/cabewse/AGENTS.md and actual portageq envvar MAKEOPTS=-j24 -l24. Manager granted exclusive compiler/private fixture slots; both released before handoff. Fresh CMake uses Ninja Debug, BUILD_TESTING/BUILD_SHARED_LIBS/strict warnings ON, KWin plugin/host uinput OFF and system packages, C/C++ ccache. No mutable shared build or staged compositor was copied/used. Configure exit0. Target graph reports728 actions; production qindaqt-session/qindaqt-desktop-controls and all22 row binaries build exit0.

```sh
cmake -S . -B build/review -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C build/review -j24 -l24 qindaqt-session qindaqt-desktop-controls qindaqt_settings_schema_tests qindaqt_settings_migration_tests qindaqt_powerdevil_import_tests qindaqt_power_client_tests qindaqt_power_qt_transport_tests qindaqt_power_service_residency_tests qindaqt_power_idle_inhibitor_registry_tests qindaqt_native_lock_monitor_tests qindaqt_qt_native_lock_transport_tests qindaqt_source_idle_display_settings_tests qindaqt_power_route_composition_tests qindaqt_power_page_tests qindaqt_native_lock_runtime_tests qindaqt_idle_display_stage_tests qindaqt_kwayland_dpms_controller_tests qindaqt_desktop_shortcut_set_tests qindaqt_compositor_attachment_tests qindaqt_wayland_idle_observation_tests qindaqt_resident_lock_service_tests qindaqt_qt_native_lock_request_tests
ctest --test-dir build/review --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(schema|migration|source-idle-display-model|power-(composition|page|boundary|boundary-poison))|powerdevil-import|power-(client|qt-transport|service-residency|idle-inhibitor-registry)|native-lock-(monitor|qt-transport)|session-(native-lock-runtime|idle-display-stage|desktop-controls-shortcut-set)|kwayland-dpms-controller|compositor-attachment|wayland-idle-observation|resident_lock_service|qt_native_lock_request)$'
python3 tools/validate-docs
mkdocs build --strict --site-dir .cache/pf-runtime-review-docs
git diff --check ab764094 891092f6
```

- Exact22/22 selected CTests PASS, exit0,30.14seconds, zero skips. Private DPMS uses only an in-process Wayland server; D-Bus tests use their existing private fixtures. This is a clean single22-row run.
- Link/navigation checker477 documents PASS; strict MkDocs PASS; diff check PASS, all exit0.
- Qinda exact891 worktree remains clean. Logs preserved as sibling private artifacts: pf-runtime-review-20261001-configure.log, pf-runtime-review-20261001-build.log, pf-runtime-review-20261001-tests.log under /home/cabewse/work_SPaC3/container-wm.worktrees/.

No full repository suite, installed/physical display, native compositor scenario, sandbox/live secrets/PAM, suspend or exact consumer compositor ABI gate is claimed. Existing KDE/PowerDevil removal and complete dim/lock-before-display-off/idle-suspend plus legacy/portal inhibitor compatibility remain successive outcomes. Reviewed-ready source is immutable891, not the review report commit.

## Stopping point and help

Read platform queue after verdict and considered current manager/peer state. All slots released; worker waiting. Concrete compatible help offered: repair exact blocking PF17 review findings if any, or take an explicitly bounded PF18-family source audit and first executable delivery slice. No further source/native runtime is claimed without assignment.
