# ADR-0335: Native remote input through the compositor EIS engine

- **Status:** Proposed
- **Date:** 2026-10-02
- **Owners:** Portal (remote_input) and compositor fork EIS plugin
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0088](0088-enable-kde-remote-desktop-for-qindaqt.md), [ADR-0318](0318-native-portal-foundation.md), [ADR-0324](0324-native-portal-capture-boundaries.md)

## Context

RemoteDesktop and InputCapture are still served by xdg-desktop-portal-kde with
a process-local `XDG_CURRENT_DESKTOP=KDE` drop-in. The QindaQt compositor fork
already contains KWin's libeis engine, but its D-Bus methods admitted any
session-bus caller and survived native lock. The native portal foundation
already authenticates the frontend, proves the selected compositor and owns
consent and lock admission.

## Decision

Adapt xdg-desktop-portal-kde 6.6.6 RemoteDesktop and InputCapture
session/device/EIS flow into `src/services/portal/remote_input`, composed in
the existing resident. InputCapture zones come from a compositor `zones()`
hook, so barriers are validated in the coordinates the compositor enforces.
Input flows only through compositor EIS transports on the attachment-proven
owner after explicit native consent. Upstream notices stay on the adapted
files (RemoteDesktop LGPL-2.0-or-later; InputCapture LGPL-2.1-only OR
LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL). Legacy Notify* calls fail explicitly instead of adding a second
injection path.

The fork EIS plugin admits only the current portal backend owner (the KDE name
transitionally), restricts cookie/capture control to the creator, refuses work
while locked and destroys contexts on lock. The RemoteDesktop Clipboard adjunct
uses a compositor-owned clipboard handle in that plugin: an `AbstractDataSource`
publishes the session's MIME types and forwards Wayland paste FDs in targeted
signals, because the protected resident cannot obtain `ext-data-control-v1`.
Clipboard consent is a separate opt-in choice in the Start dialog.

## Consequences

- No new process, dependency or persistence; consent reuses the QindaTK helper.
- Routing stays on KDE until the real frontend/private-compositor EIS and
  clipboard gates pass; then the KDE admission and ADR-0088 drop-in are
  removed in the same change.
- Persistence/restore tokens are not offered; every Start asks.
- See [Native remote-input portal](../architecture/portal-remote-input.md).
