# Independent ACCEPT: KNightTime discovery retirement

- Candidate: `690c0112d13ca7d861e070865c9054d657946b75`, parent `2e4e1961169febaf3e2a1f6d6eeede9510d18ac2`.
- Changed paths: `CMakeLists.txt`, `qindaqt/README.md`; no other implementation or recipe changes.
- Review worktree on qinda: `~/work_SPaC3/qindaqt-kwin.worktrees/pf-knighttime-review`, branch `review/pf-knighttime-20261001`, clean tracked source at exact candidate.

The removed discovery is unused. Production nightlight links kwin, KF6 GlobalAccel and I18n; its sources consume the native Schedule1 client, not KNightTime. Reviewed native unique-owner subscription, nonce/cookie/revision receipt validation, frame delivery and manager consumption. Source/CMake searches find no KNightTime callsite or link remaining. Documentation accurately states this narrowly scoped retirement. KGlobalAccel remains an explicit production link and is not claimed retired.

Independent gates:

- Fresh Release production configure, BUILD_TESTING=OFF, QINDAQT_KWIN_BUILD_TESTS=ON, QINDAQT_SESSION_LOCK_TEST_AUTHORIZATION=OFF, with KNightTime/Plasma/PlasmaActivities/Breeze/Aurorae find_package disabled: exit0.
- `qindaqt/tools/rename-identity --check`: exit0, every checked identity category zero residual stock names.
- `qindaqt/tools/check-install-collisions ../pf-knighttime-gate/stage`: exit0;453 staged paths against1059 stock paths (kwin500,kwin-x11472,kdecoration87), no collision/stock names.
- `ctest --test-dir ../pf-knighttime-gate/build/production/qindaqt/helpers/terminate-process/tests --output-on-failure -V`: exit0;1/1CTest,28Qt cases passed,0failed. Reviewed fixture: only disposable child processes, no external power or installed helper action.
- readelf staged nightlight plugin and libqindaqt-kwin: exit0, no KNightTime NEEDED entry.
- Root gate HEAD independently confirmed exact690. Existing full production compile log reaches1411/1411; stage log inspected and staged artifacts directly examined. No fresh full compiler run is claimed by this review.

All independent logs remain on qinda under review `build/review/{configure,identity,collisions,helper-ctest,elf-needed}.log`; existing root logs are sibling `pf-knighttime-gate-{configure,build,stage}.log`. Production cache retains Release, /usr prefix and test authorization OFF. Actual plugin stage path is `/usr/lib64/qt6/plugins/qindaqt-kwin/plugins/nightlight.so`; data namespace identity check passes.

Caveats: BUILD_TESTING=OFF gives no top-level test registration; helper registration is in its explicit subdirectory. Root stage is untracked `stage/` output while tracked source remains exact. This accepts the two-path KNightTime build discovery removal only; no new nightlight behavior, physical display, installed rollout, full Plasma dependency closure or KGlobalAccel retirement is asserted.

Requested next action: manager integrate exact690 and rerun affected integration gates. Small review build slot released. Read Platform queue (stale older entries preserved); available for another exact dependency/recipe review, or accepted-artifact packaging when assigned. No source/install edits made during this review.
