# Claude remote-input handoff

2026-10-02T17:33:00Z

## Exact candidate

- Desktop: worker/pf-claude-remote-input-20261002 at
  c5e94f46ce10c56444de592c2bb07835fee55df1 (base 7358b792; the code is at
  2d62d7c6, c5e94f46 adds wiki evidence only).
- Fork: worker/pf-claude-remote-input-20261002 at
  53937bd8593d689bb361d85a2db9ca17ec0c8877 (base 68c4d74f).
Both are on the qinda bare hubs.

## Outcome

Native `org.freedesktop.impl.portal.RemoteDesktop` v2 and `InputCapture` v1
backends in new `src/services/portal/remote_input`, adapted from
xdg-desktop-portal-kde 6.6.6 (9a5cc0e8) with upstream notices kept, and composed
in the existing resident. They reuse PortalRequests, PortalSessionBinding (exact
compositor owner), the existing AccessConsent/QindaTK helper and native lock
monitor. Input flows only through the fork EIS engine on the attachment-proven
compositor owner after explicit consent. No KDE portal/KGlobalAccel/Plasma
process dependency. Selector and `.portal` are unchanged (still `kde`).

Fork: EIS RemoteDesktop/InputCapture admit only the current portal backend owner
(qindaqt, KDE transitionally), creator-only cookie/capture control, no work
while locked, contexts destroyed and captures disabled on `screenAboutToLock`,
plus `InputCaptureManager.zones()`/`zonesChanged`.

## Changed paths

Desktop (30 files): src/services/portal/remote_input/** (new), tests/services/portal/remote_input/** (new),
docs/wiki/architecture/portal-remote-input.md (new), docs/wiki/adr/0335-native-remote-input-portal.md (new).
Coordination points (additive): src/services/portal/CMakeLists.txt,
src/services/portal/foundation/CMakeLists.txt, src/services/portal/src/foundation_composition.cpp,
tests/services/portal/CMakeLists.txt, tests/services/portal/check_boundary.cmake,
mkdocs.yml, docs/wiki/adr/index.md, docs/wiki/architecture/module-boundaries.md (one row),
docs/wiki/architecture/portal-foundation.md (one link); ops board files.
Fork (8 files): src/plugins/eis/{CMakeLists.txt, eisbackend.cpp, eisinputcapture.{h,cpp},
eisinputcapturemanager.{h,cpp}, eisportaladmission.{h,cpp}}.

## Verification (actual)

- `cmake --preset dev -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec -DCMAKE_PREFIX_PATH=<pf-manager-capture-production-20261001 stage>/usr -DQINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR=<stage>/include`: exit 0.
- `cmake --build build/dev --target qindaqt_portal_remote_desktop_tests qindaqt_portal_input_capture_tests xdg-desktop-portal-qindaqt qindaqt_portal_access_tests qindaqt_portal_service_tests qindaqt_portal_process_lifecycle_tests -- -j24 -l24` (strict -Werror): exit 0.
- `ctest --test-dir build/dev -R '^qindaqt\.portal-(remote-desktop|input-capture|access|service|process-lifecycle|source-boundary|source-boundary-negative)$'`: 7/7 passed.
  Qt totals: remote-desktop 9 passed/0 failed, input-capture 7/0; same with
  QT_FATAL_WARNINGS=1; 10/10 repeated runs pass.
- Real resident on a zero-activation private dbus-daemon: exports RemoteDesktop
  (AvailableDeviceTypes 7) and InputCapture; all 29 standard members match the
  installed backend XML wire signatures (only Qt's return-first ordering differs).
- Fork eis sources: `-fsyntax-only` with the exact configured fork flags, exit 0.
- `mkdocs build --strict` exit 0; `tools/docs_validation.py` exit 0 (492 docs).

## Remaining bounded caveats

1. Fork eis plugin not compiled/linked or run (needs a fork build tree I do not own).
2. No real frontend + private native compositor EIS row yet (needs GPU/native grant).
3. Clipboard not implemented (`clipboard_enabled=false`): data-control admission blocker;
   successor design in portal-remote-input.md "Clipboard gap".
4. Notify* reply NotSupported (no non-EIS injection path); ScreenCast sources on a
   RemoteDesktop session unsupported; no persistence/restore.
5. Fork still admits the KDE backend name until routing moves.

## Requested next action

Independent review of desktop c5e94f46 + fork 53937bd8. Then manager fork
native-driver rebuild with the eis plugin and a private native frontend EIS row
(grant/connect/inject, Close, native lock). Decide whether the Clipboard
compositor-hook successor proceeds. Implementation stopped for review.
