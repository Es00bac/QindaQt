# Native permission-list repair candidate handoff

- Worker: `/root/native_permission_repair`
- Observed: 2026-10-04T09:54:59-06:00
- Exact candidate: `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`
- Exact parent/base: installed fork `dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698`
- Isolated worktree: `/home/cabewse/work_space/qindaqt-kwin-lock-permissions`
- Branch preserved on qinda hub: `fix/native-permission-lists-20261004`
- Original hub-base candidate `5697fc871c3c047e08831776c0f79ea0e2473098` is separately pushed at `fix/native-permission-lists-base55-20261004`; use the corrected candidate above for review/integration. Rebase had no conflict.

## Outcome and boundary

The native Wayland and D-Bus permission fields now share KDE `KDesktopFile`/`readXdgListEntry` decoding in the existing exported service utility header. A standard trailing semicolon no longer belongs to the interface name. Process/executable, desktop-ID and actual PID lookups, the private locker check and authorized capture launch checks use that boundary. All native names, root-managed file/executable checks, exact command/list checks, ptrace/environment checks and private locker connection ownership remain intact. The exported header has no new source-relative include or installed-file dependency. Deployed shortcut-release/introspection code/tests are byte-identical to dd74.

## Changed paths

- `qindaqt/CMakeLists.txt`
- `qindaqt/README.md`
- `qindaqt/capture-authority/README.md`
- `qindaqt/capture-authority/capturelaunchpolicy.cpp`
- `qindaqt/service-permissions/tests/CMakeLists.txt`
- `qindaqt/service-permissions/tests/data/org.qindaqt.Lock.desktop`
- `qindaqt/service-permissions/tests/data/org.qindaqt.PortalCapture.desktop`
- `qindaqt/service-permissions/tests/data/org.qindaqt.PortalCaptureBackend.desktop`
- `qindaqt/service-permissions/tests/permission_list_test.cpp`
- `qindaqt/service-permissions/tests/service_lookup_test.cpp`
- `qindaqt/session-lock/README.md`
- `qindaqt/session-lock/lockerlauncher.cpp`
- `src/utils/serviceutils.h`

## Executable evidence

Commands ran in the isolated fork worktree on qinda-top using configured Portage `MAKEOPTS` (`-j32 -l16`).

| Gate | Result |
| --- | --- |
| `cmake -S qindaqt/service-permissions/tests -B build-native-permissions -G Ninja -DCMAKE_BUILD_TYPE=Debug` | exit 0 |
| `nice -n12 ionice -c3 cmake --build build-native-permissions -- -j32 -l16` | exit 0; real production locker object and capture policy compile, real production PID resolver linked |
| `ctest --test-dir build-native-permissions --output-on-failure --no-tests=error -V` | exit 0; 3/3 CTests, 21 metadata + 7 real service/PID + 4 capture trust/environment Qt checks, including setup/cleanup; 0 failures/skips, 0.06 seconds |
| `dbus-run-session -- build-native-permissions/negative-build/negativeServiceLookup consumerLookups:native` against unchanged dd74 serviceutils header | expected exit 1; Qt 2 pass/1 fail/0 skips at actual desktop-ID lookup: one literal list item versus two expected entries. The same unchanged-55d1 control also failed this row. |
| `qindaqt/tools/rename-identity --check` | exit 0; all rename counts zero |
| `git diff dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698 HEAD --check` | exit 0 |
| `clang-format --style=file:/usr/share/ECM/kde-modules/clang-format.cmake --dry-run --Werror src/utils/serviceutils.h qindaqt/service-permissions/tests/permission_list_test.cpp qindaqt/service-permissions/tests/service_lookup_test.cpp` | exit 0 |

Focused coverage includes both native keys, single/multiple values, optional trailing separator, comma as literal, KDE-escaped separator/backslash round trip, empty/absent fields, absent/removed source, exact original packaged lock metadata, packaged-style capture helper/broker metadata, and stock-only/empty-native-with-stock refusal through the actual consumers. Real child images and PID resolution are used; service menu/cache and D-Bus are isolated. Logs and negative-control copies remain ignored under `build-native-permissions/` (`final-dd74-tests.log`, `final-dd74-identity.log`, `negative-control-dd74.log`), with earlier 55d1 logs preserved.

## Bounded caveats and requested next action

This is source/focused verification, not a full compositor link/native socket/PAM/physical-lock or installed Portage qualification. Positive root-managed launch admission is not exercised by the metadata fixture; existing trust/environment negative guards are exercised and preserved. No system files, live desktop, hub main or system settings were changed by this worker. No install rules/installed paths changed, so there is no new staged-install boundary in this candidate. Container-wm incident/wiki/task reconciliation and board preservation remain manager work.

Request a different worker's exact review of this immutable candidate, then manager integration preserving deployed dd74 and rerun the affected combined/Portage/native gates before delivery.
