# Actual native notification popup candidate handoff

- Worker: pf-native-popup-sol-20261001
- Timestamp: 2026-10-01T13:28:10Z
- Exact candidate: `6181566a68703cc2a1bb5b693c729cd2a89190a1`
- Exact base: `3dfe915bcbd8d62afa0401686471b4c9197324a6`
- Hub branch: `worker/pf-native-popup-20261001`
- Laptop worktree: `.cache/pf-native-popup-20261001`
- Qinda worktree: `/home/cabewse/work_SPaC3/container-wm.worktrees/pf-native-popup-20261001` (clean detached mirror)
- Status: executable qualification passed; requires a different-worker exact review
- Requested manager action: independently review exact candidate and its reuse evidence, then integrate and rerun the focused native gate on the combined tree. No implementer ACCEPT claimed.

## Outcome and boundary

Actual unchanged production Qt shell and resident notification host run through public TokenizedProcessLauncher descriptor provisioning. Test-owned real Session1 connections select the actual fork through daemon UID/PID and ordinary socket/PIDFD. The real qualified fork is the sole NativeLock1 producer; public native transport/monitor consume current targeted nonce receipts. No result decision, backend, receipt, QML view or production observer is injected/relaxed.

The actual compositor inventory records one committed/mapped production popup before selected Session1 owner loss and before native locking. Each transition retires all mapped notification surfaces and clears active/popup/history models while the actual shell/host remain alive; critical Notify calls during denial remain cleared. Owner loss immediately resolves the public monitor to Unknown. A fresh shell/selection then observes current actual Locked/Protected receipt after RequestLock. The fixed locker is absent, so launch refusal is asserted and real clientless black protection is observed; no authentication/unlock admission is claimed.

Actual evidence: compositor PID2684627/owner:1.0; first selected owner:1.6/shell2684653, second owner:1.11/shell2684677. Raw final summary reports mappedPopupCount1 and retiredMappedCount0 in both rows, model counts0, live shell/host true; nonce counts1 and3. Production source directories reused from immutable root faaaf74646f638bc5074163e45e950faa2cea230 have zero delta from base for shell, host, Settings, launcher/session, supervisor, attachment and lock-state. Qualified fork is690c0112d13ca7d861e070865c9054d657946b75; its actual production build defines fixed /usr/bin/qindaqt-lock and /usr prefix. No product-source bytes changed by this candidate.

## Changed paths

- `tests/session/native_notification_live/{CMakeLists.txt,native_popup_fixture.cpp,native_popup_fixture.h,run_native_popup.py,tst_native_notification_live.cpp}`
- One additive helper subdirectory line in `tests/session/CMakeLists.txt`
- Native notification section in `docs/wiki/development/testing-harness.md`
- Own worker board and timestamped claim/material/gate/handoff messages only

Legacy test_notification_live_nested.py, notification_live_stage.py and notificationlivelock.cpp were inspected and remain unchanged. Their KScreenLocker/password-free-unlock/global-shortcut assumptions do not supply native privacy evidence; the new focused native gate is explicitly separate. No shell/model/service/fork/overlay/shared manager source edit.

## Exact build and executable gates

All compile/private execution ran in the own qinda worktree above, under explicit manager grants; MAKEOPTS was not altered.

```sh
cmake -S . -B build/native-popup -G Ninja \
 -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
 -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
 -DCMAKE_PREFIX_PATH=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-knighttime-gate/stage/usr \
 -DQINDAQT_KWIN_WAYLAND=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-knighttime-gate/stage/usr/bin/qindaqt-kwin \
 -DQINDAQT_NATIVE_POPUP_PRODUCTION_BUILD=/home/cabewse/work_SPaC3/container-wm.worktrees/pf-combined-20261001/build/combined
cmake --build build/native-popup --target qindaqt_native_notification_live_tests -- -j24 -l24
env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u DISPLAY \
 -u DBUS_SESSION_BUS_ADDRESS -u DBUS_SYSTEM_BUS_ADDRESS \
 ctest --test-dir build/native-popup --output-on-failure -V \
 -R '^shell\.notification-native-popup$' -j1
```

Configure exit0. Initial293 planned actions failed in two own fixture translation units (undeclared legacy event waits and const QtDBus connection connect); first affected repair still failed the qualifier. Final affected rebuild exit0, raw log displays3 actual actions (MOC, C++ compile, link). No claim that initial293 actions all passed. Compiler was released before runtime. Actual native final CTest exit0,1/1 in3.42s; raw Qt4passed/0failed/0skipped (init, two behavior cases, cleanup). Native helper/production children inherit QT_FATAL_WARNINGS=1; compositor startup warnings remain normal. Actual prior umask022 and final socket0700/UID1000 recorded.

The runtime-tested source e7018ba915412f8010a7ca67c2858314eb600979 and exact candidate differ only in the relevant qualification documentation; `git diff --quiet e7018ba9 6181566a -- src tests` exit0. Helper binary comes from actual final own strict build, and unchanged production binaries are reused from root faaaf746; no fresh shell/module build claimed. Candidate's final documentation gates rerun after the documentation addition.

Laptop own static/adjacent commands: `tools/validate-docs` exit0,483 Markdown documents/navigation; `mkdocs build --strict --site-dir build/native-popup-static/site` exit0; `tools/check-source-shape --root tests/session/native_notification_live --config tools/source-shape.json --warnings-as-errors` exit0,5 files/largest fixture210 nonblank lines; exact base-to-candidate `git diff --check` exit0; `python3 -B tests/session/test_notification_live_unit.py` exit0,11/11 in0.071s.

## Preserved failures and isolation correction

Raw qinda logs remain `build/native-popup/{configure.log,build-first.log,build-repair.log,build-final.log,tests-first.log,tests-short-socket.log,tests-name-ready.log,tests-empty-session.log,tests-canonical-peer.log,tests-private-mode.log}`.

Initial runtime failed before compositor start because AF_UNIX bus path under long build directory exceeded its limit. Short root then exposed authority-before-name readiness. A name-ready run uncovered the omitted launcher --session boundary: installed qindaqt-session actually started on the disposable bus, took Session1 and attempted optional installed children, including nm-applet/tray entries in logs. This is a real fixture isolation defect in the failed predecessor, not a product finding or acceptable caveat. It was repaired with explicit --session '' and --no-xwayland; corrected runs contain no default supervisor/optional launch. No blanket 'only source binaries ever executed' claim is made. Fixed numeric socket and private077 umask then conformed the fixture to existing strict ordinary admission; none of those admissions were weakened.

Direct follow-up `build/native-popup/isolation-process-audit-complete.json` examined47 same-UID processes, found zero exact accidental runtime/HOME/unique socket command matches, and confirmed known installed nm-applet PID2680778 absent. Four limited environment reads were identified as host systemd/sd-pam/sshd processes, untouched. No actual host mutation evidence appeared. Final success `final-lifetime-audit.json` confirms exact fork/shell PIDs gone and no final run-root process matches.

Final evidence directory: `build/native-popup/tests/session/native_notification_live/evidence/qindaqt-native-popup-g_368zmv-evidence/` includes raw fixture/compositor/production-child logs, process.json, independently parsed summary.json and final-lifetime-audit.json. Failed evidence directories alongside it are preserved. Empty broker activation entries, unavailable systembus, disposable HOME/XDG, disabled OSK discovery, no Xwayland/input and compiled native-locker absence guard remain explicit. Compositor stops before broker.

## Caveats and release

Qualification is actual private virtual1920x1080/100% mapping/privacy metadata with the qualified production fork/module. No pixel capture, physical display/output, installed deployment, keyboard/shortcut/settings/restart matrix, PAM/credential/authentication or unlock proof. Clientless protection is distinct from successful locker admission. The earlier accidental installed-child launch and bounded process-read limitations remain disclosed above; no unrelated host service was stopped or cleaned up.

Compiler and private-runtime slots released immediately after their final gates. Own board is waiting. Read Platform queue and related peer handoffs; concrete bounded help offer: reproduce/repair one exact reviewer finding inside these owned focused fixture paths after a fresh resource grant. No additional product outcome claimed.
