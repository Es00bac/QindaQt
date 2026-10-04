# ACCEPT — parser candidate 24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae

- Reviewer: desktop-incident-review (/root/desktop_incident_review), different from implementer native-permission-repair.
- Exact final candidate: 24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae, explicitly fetched from qinda hub fix/native-permission-lists-20261004.
- Exact tree: 3a257b8d991777247970c83ce9ae8ca265448db3.
- Exact parent: deployed dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698.
- Detached independent source tree: ~/work_space/qindaqt-kwin-lock-permissions-review.
- Independent ignored build root: build/permissions-review in that worktree.
- Provisional fb115365e81a04b42c0ee6ce9eb3d6e7e5384f0e is superseded; no verdict was issued on it. Direct final-versus-provisional diff is empty and tree IDs match. Every build/test below ran after checking out exact final 24d0.

## Reviewed scope

All 13 changed paths were inspected against exact deployed parent dd74:

- qindaqt/CMakeLists.txt; qindaqt/README.md.
- qindaqt/capture-authority/README.md; qindaqt/capture-authority/capturelaunchpolicy.cpp.
- qindaqt/service-permissions/tests/CMakeLists.txt; permission_list_test.cpp; service_lookup_test.cpp; data/org.qindaqt.Lock.desktop; data/org.qindaqt.PortalCapture.desktop; data/org.qindaqt.PortalCaptureBackend.desktop.
- qindaqt/session-lock/README.md; qindaqt/session-lock/lockerlauncher.cpp.
- src/utils/serviceutils.h.

The decoder uses the selected KService source path, validates the service and nonempty path, then reads the exact native field through KDesktopFile/readXdgListEntry. It handles proper trailing delimiters and escaped literals without trimming interface names or adding a stock-key fallback. All three public discovery routes and both private admission consumers use this same boundary. Root-managed original/canonical file/parent checks, exact command/list requirements, ptrace/environment guards and owned private connection checks remain intact. This repair neither bypasses the black/input/capture fence nor supplies unlock authority.

The exported serviceutils header already belongs to the Devel SDK install set. New includes are the installed KF6 KConfigGroup/KDesktopFile headers; KF6::ConfigCore is already a public kwin link dependency. No new source-relative helper, exported-file dependency, install rule or path was introduced. Actual packaged locker metadata is byte-identical to the lock fixture; capture fixtures intentionally exercise the valid trailing-delimiter spelling as well as the installed helper's unterminated spelling covered by the parameterized rows.

Exact parent dd74 is retained, and direct shortcut/backend source diffs against it are empty. Its six deployed shortcut physical-release/repeat/introspection fixes remain ancestors. Governing QindaQt-ADR: 0340 trailer and affected fork documentation are present.

## Independent executable evidence

| Gate | Direct result |
| --- | --- |
| `portageq envvar MAKEOPTS` | exit 0, actual laptop setting `-j32 -l16` |
| `cmake -S qindaqt/service-permissions/tests -B build/permissions-review -G Ninja -DCMAKE_BUILD_TYPE=Debug` | exit 0 |
| `nice -n12 ionice -c3 cmake --build build/permissions-review -- -j32 -l16` | exit 0, all 20 build steps; real production locker object, capture policy and PID resolver compiled |
| `ctest --test-dir build/permissions-review --output-on-failure --no-tests=error -V` | exit 0; **3/3 CTests, 32/32 Qt checks**, 0 failures/skips/blacklisted, 0.07 seconds |
| Metadata Qt checks | 21 pass including setup/cleanup; both native keys, optional separators, multiple values, comma literal, escaped separator/backslash round trip, empty/absent fields, missing/removed source, locker/capture metadata and KDE-only denial |
| Service lookup Qt checks | 7 pass including setup/cleanup; actual KService cache/menu, desktop-ID and executable discovery, real copied child images and production /proc PID resolution; native positive plus legacy/empty/absent/missing negative rows |
| Capture policy Qt checks | 4 pass including setup/cleanup; user-file/alias refusal and stripped environment overrides |
| `qindaqt/tools/rename-identity --check` | exit 0, all reported rename counts zero |
| `clang-format --style=file:/usr/share/ECM/kde-modules/clang-format.cmake --dry-run --Werror src/utils/serviceutils.h qindaqt/service-permissions/tests/permission_list_test.cpp qindaqt/service-permissions/tests/service_lookup_test.cpp` | exit 0 |
| `git diff HEAD^ HEAD --check`; `git status --porcelain` | exit 0 / empty; detached exact final source tree clean; build directory confirmed ignored |
| `git merge-base --is-ancestor dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698 HEAD` | exit 0 |

The authored implementer negative-control result was read, not relabeled as an independently rerun result. Reviewer CTest log is retained in build/permissions-review/Testing/Temporary/LastTest.log. No physical/GPU session, installed source, user service cache, credentials, or applications were touched by this review.

## Remaining boundary and requested action

No blocking source findings. This ACCEPT qualifies the immutable source and focused independent gates. Full production compositor linking, coherent package/archive/Manifest/plugin verification, positive private launcher/actual greeter/PAM execution and physical adoption remain manager gates. Existing capture trust tests do not claim positive root-managed launch or a real consumer pipeline. The review did not install, launch the actual locker, lock the desktop, or access real credentials.

Requested next action: manager integrate this exact accepted candidate preserving deployed dd74, rerun required combined/package/private-native gates, and update/preserve consumer documentation and incident evidence in the same delivery. Read the current Platform queue and relevant peer thread; I offer bounded independent exact recipe/archive/private-launch evidence review for this incident next. No unrelated queue work claimed, and compiler/private-runtime leases are released.
