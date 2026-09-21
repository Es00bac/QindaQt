// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QtGlobal>

#include <qindaqt/services/xembed_tray_proxy/tray_icon_image.h>

namespace QindaQt::XEmbedTray
{

// One embedded X11 tray icon as the coordinator sees it. The production
// implementation is XcbEmbeddedIcon (src/x11); tests substitute a fake.
// Window identity crosses this boundary as a bare quint32 XID: the
// coordinator must never name an X11 type.
class TrayIconHost : public QObject
{
    Q_OBJECT
public:
    explicit TrayIconHost(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    // Embed the client into an offscreen container. Returns false when the
    // window vanished or is hostile before embedding completed.
    virtual bool embed() = 0;
    // Read the current icon pixels; null when capture fails, fully
    // transparent frames included so callers can keep last-known-good.
    virtual TrayIconImage captureIcon() = 0;
    // Client-visible name (bounded, may be empty).
    virtual QString clientTitle() const = 0;
    // Deliver one synthetic button press+release at the panel-supplied
    // global coordinates. Buttons follow X11 numbering: 1 left, 2 middle,
    // 3 right, 4/5 vertical scroll, 6/7 horizontal scroll.
    virtual void forwardButton(quint8 button, qint32 rootX, qint32 rootY) = 0;
    // Detach the client back to the root window (so a successor tray can
    // dock it) and tear down the container. After retire() the host emits
    // no further signals.
    virtual void retire() = 0;

Q_SIGNALS:
    // The client painted; the coordinator recaptures. Rate is unbounded at
    // this layer; coalescing is the coordinator's policy.
    void iconDamaged();
    // Destroy, unmap, or reparent-away: the icon no longer exists.
    void clientGone();
    void clientTitleChanged();
};

// The X11 side of the proxy: connection, tray selection ownership, and the
// per-icon host factory. XcbTrayBackend (src/x11) is the production
// implementation; coordinator unit tests substitute a fake.
class XEmbedTrayBackend : public QObject
{
    Q_OBJECT
public:
    explicit XEmbedTrayBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    // Connect and attempt the initial claim. False means no X display (or a
    // broken one); inert-but-running is a normal outcome, not an error, and
    // is reported through ownsSelection() == false after start() == true.
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool ownsSelection() const = 0;
    // Retry after selectionFreed() or when starting inert.
    virtual void claimSelection() = 0;

    // Create a host for a docked client window, or nullptr when the window
    // is already gone or unusable. Ownership passes to the caller.
    virtual TrayIconHost *createIconHost(quint32 clientWindow) = 0;

Q_SIGNALS:
    void dockRequested(quint32 clientWindow);
    void selectionClaimed();
    // Another owner took the selection: retire everything, stay inert.
    void selectionLost();
    // The previous owner released the selection: a claim may succeed now.
    void selectionFreed();
};

} // namespace QindaQt::XEmbedTray
