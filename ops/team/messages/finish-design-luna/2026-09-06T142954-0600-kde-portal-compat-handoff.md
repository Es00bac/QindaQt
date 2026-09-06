# KDE RemoteDesktop compatibility environment handoff

**Candidate:** pending commit from base `5400320ea54dac16926a672deb779fea0922da90`  
**Worktree:** `/home/cabewse/work_SPaC3/container-wm/.cache/finish-kde-portal-env`  
**Scope:** portal package/activation config, focused tests, ADR/wiki only.

## Change

The QindaQt PortalP0 component installs:

```text
${KDE_INSTALL_SYSTEMDUSERUNITDIR}/plasma-xdg-desktop-portal-kde.service.d/20-qindaqt-remotedesktop.conf
```

The drop-in sets `XDG_CURRENT_DESKTOP=KDE` only for the KDE portal backend.
QindaQt's frontend process remains `QindaQt`; `qindaqt-portals.conf` and
`qindaqt.portal` are unchanged. The external KDE D-Bus descriptor is checked
for `SystemdService=plasma-xdg-desktop-portal-kde.service`, proving normal
D-Bus activation reaches the drop-in-covered unit.

ADR-0088 and the portal architecture/agent-input docs explain the process-local
compatibility boundary. `check_kde_portal_compat.cmake` checks the drop-in,
host D-Bus descriptor, and KDE portal metadata; the staged package test checks
the installed drop-in and activation target.

## Verification

```text
cmake --preset dev                                      PASS
cmake --build build/dev --target [portal targets] -j2  PASS (94/94 build steps)
ctest -R qindaqt.portal-(process-lifecycle|frontend-selection|frontend-toolkit|staged-package|kde-compat) PASS 5/5
./tools/validate-docs                                  PASS — 184 Markdown documents
mkdocs build --strict                                   PASS
git diff --check                                        PASS
```

Read-only live split-identity proof at 2026-09-06T14:29:51–14:29:54-06:00:

```text
KDE backend PID 1372760: XDG_CURRENT_DESKTOP=KDE
QindaQt frontend PID 1372777: XDG_CURRENT_DESKTOP=QindaQt
KDE RemoteDesktop + InputCapture interfaces: 2
KDE backend AvailableDeviceTypes: 7
QindaQt frontend AvailableDeviceTypes: 7
restored backend/frontend environment: QindaQt / QindaQt
```

No `CreateSession`, `SelectDevices`, `Start`, approval, or input call was made.

## Requested action

Independently review the exact commit, then integrate it with the PortalP0
package. After installation, restart the KDE and frontend portal user units and
repeat the read-only property check; no host input qualification is required
for this candidate.
