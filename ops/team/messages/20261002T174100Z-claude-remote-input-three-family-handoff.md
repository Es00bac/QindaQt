# Claude remote-input three-family handoff

2026-10-02T17:41:00Z

Supersedes handoff 20261002T173300Z (e5309b00), per manager decision 20261002T172500Z.

## Exact candidate

- Desktop: worker/pf-claude-remote-input-20261002 at
  dd9d05c1a1b2481f337a09f7a4363f48deb168ad (base 7358b792).
- Fork: worker/pf-claude-remote-input-20261002 at
  1a287a709268e949b398e4cd5e62793675e22b93 (base 68c4d74f).
Both on the qinda bare hubs.

## Outcome

Native RemoteDesktop v2, InputCapture v1 and Clipboard v1 backends in
`src/services/portal/remote_input`, composed in the existing resident and adapted
from xdg-desktop-portal-kde 6.6.6 (9a5cc0e8) with upstream notices kept. They reuse
PortalRequests, PortalSessionBinding's exact compositor owner, the existing
AccessConsent/QindaTK helper and native lock monitor; no KDE portal, KGlobalAccel or
Plasma process dependency. Input and clipboard use only the fork EIS plugin on the
attachment-proven compositor owner after explicit consent (clipboard is a separate,
default-off choice). Selector/`.portal` unchanged (`kde`).

Fork `src/plugins/eis`: portal-backend-only admission (KDE name transitional),
creator-only cookie/capture/clipboard handles, nothing issued while locked,
contexts/captures/clipboard handles dropped on `screenAboutToLock`,
`InputCaptureManager.zones()`, and a compositor-owned clipboard handle whose
AbstractDataSource forwards Wayland paste FDs in targeted signals.

## Changed paths

Desktop: src/services/portal/remote_input/** and tests/services/portal/remote_input/**
(new), docs/wiki/architecture/portal-remote-input.md and
docs/wiki/adr/0335-native-remote-input-portal.md (new); additive coordination edits
in src/services/portal/CMakeLists.txt, src/services/portal/foundation/CMakeLists.txt,
src/services/portal/src/foundation_composition.cpp, tests/services/portal/CMakeLists.txt,
tests/services/portal/check_boundary.cmake, mkdocs.yml, docs/wiki/adr/index.md,
docs/wiki/architecture/module-boundaries.md, docs/wiki/architecture/portal-foundation.md;
ops board files.
Fork: src/plugins/eis/{CMakeLists.txt, eisbackend.{h,cpp}, eisclipboard.{h,cpp},
eisinputcapture.{h,cpp}, eisinputcapturemanager.{h,cpp}, eisportaladmission.{h,cpp}}.

## Verification (actual)

- Configure: `cmake --preset dev -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec -DCMAKE_PREFIX_PATH=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-manager-capture-production-20261001/stage/usr -DQINDAQT_CAPTURE_AUTHORITY_INCLUDE_DIR=<same>/include` exit 0.
- Build (strict -Werror, configured `-- -j24 -l24` per the user's MAKEOPTS policy):
  `qindaqt_portal_remote_desktop_tests qindaqt_portal_input_capture_tests xdg-desktop-portal-qindaqt qindaqt_portal_access_tests qindaqt_portal_service_tests qindaqt_portal_process_lifecycle_tests` exit 0.
- `QT_FATAL_WARNINGS=1 ctest --test-dir build/dev -R '^qindaqt\.portal-(remote-desktop|input-capture|access|process-lifecycle|source-boundary|source-boundary-negative)$'`: 6/6 pass;
  `qindaqt.portal-service` passes with its documented ordinary warning handling
  (intentional wrong-signature ReadAll diagnostic aborts it only under fatal warnings).
- Qt totals: remote-desktop 12 passed/0 failed (incl. 3 clipboard rows), input-capture 7/0;
  20/20 repeated runs under QT_FATAL_WARNINGS=1.
- Real resident on a zero-activation private dbus-daemon stays up and exports all
  three interfaces; all 37 standard members (16+13+8) match the installed backend
  XML in/out wire signatures.
- Fork eis sources (incl. eisclipboard.cpp): `-fsyntax-only` with the exact configured
  fork flags, exit 0. `mkdocs build --strict` exit 0; docs_validation exit 0.

## Remaining native qualification (concrete)

1. Fork eis plugin compile/link and runtime: needs the manager's fork native-driver
   rebuild (libkwin; excluded from my consumer grant).
2. Private native frontend row: real xdg-desktop-portal 1.20.4 selecting a test
   portals.conf for RemoteDesktop/InputCapture/Clipboard, production consent input,
   ConnectToEIS injecting observable input, barrier activation, a real Wayland
   paste through SelectionTransfer/SelectionWrite and SelectionRead, then Close
   and native lock retiring all of them.
3. Then staged metadata/selector change removing the KDE route and the fork's KDE
   admission together (ADR-0335 consequences).
Known limits: Notify* reply NotSupported; ScreenCast sources on RemoteDesktop
sessions unsupported; no persistence/restore tokens.

## Requested next action

Independent review of desktop dd9d05c1 + fork 1a287a70, then the native gates above.
Implementation stopped.
