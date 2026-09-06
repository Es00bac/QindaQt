# ADR-0088: Scope the KDE portal compatibility identity to its backend process

- **Status:** Proposed
- **Date:** 2026-09-06
- **Owners:** Portal platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt intentionally keeps its own desktop identity and routes
`org.freedesktop.impl.portal.Settings` to the QindaQt backend. The same
selector routes RemoteDesktop to KDE, whose public `kde.portal` metadata
advertises the interface and whose KWin EIS object is available in the QindaQt
session.

The KDE backend, however, creates its RemoteDesktop, InputCapture, and Wayland
adaptors only when its own process sees `XDG_CURRENT_DESKTOP=KDE`. In a QindaQt
session it otherwise starts successfully while omitting those adaptors, making
the standard frontend report `AvailableDeviceTypes=0`. Changing the session's
desktop identity would change frontend selection and break the QindaQt-specific
Settings route.

## Decision

Install a systemd user drop-in for
`plasma-xdg-desktop-portal-kde.service` that sets
`XDG_CURRENT_DESKTOP=KDE` only in that KDE backend process. Keep the QindaQt
frontend process and the QindaQt portal selector unchanged. The installed KDE
D-Bus activation descriptor's `SystemdService=plasma-xdg-desktop-portal-kde.service`
continues to be the activation path, so both explicit systemd starts and normal
D-Bus activation receive the same scoped environment.

QindaQt does not implement or advertise RemoteDesktop, InputCapture, EIS, or a
second portal backend. The drop-in is a compatibility seam for the installed
KDE backend and is fail-safe when the KDE package is absent.

## Consequences

- RemoteDesktop and InputCapture become available through the existing KDE/KWin
  public path in QindaQt sessions; a live split-identity proof reports
  `AvailableDeviceTypes=7`.
- QindaQt's global desktop identity and `qindaqt-portals.conf` remain stable,
  so Settings still selects the QindaQt backend and other families retain their
  explicit order.
- The package depends on the KDE portal service retaining its documented
  systemd activation name. Installation tests inspect the drop-in, KDE D-Bus
  descriptor, and KDE portal metadata together.
- The backend compatibility override is distribution integration policy; if KDE
  changes its activation contract or removes the desktop-name gate, remove or
  revise this ADR and drop-in.

## Revisit when

Revisit when the KDE portal no longer gates RemoteDesktop on
`XDG_CURRENT_DESKTOP=KDE`, the service name changes, QindaQt owns a native
RemoteDesktop backend, or a distribution provides a better backend-specific
environment mechanism.
