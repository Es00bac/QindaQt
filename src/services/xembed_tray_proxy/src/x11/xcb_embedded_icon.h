// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/xembed_tray_proxy/tray_backend.h>

#include <QtCore/QString>
#include <QtCore/QTimer>

#include <xcb/damage.h>
#include <xcb/xcb.h>

namespace QindaQt::XEmbedTray
{

class XcbTrayBackend;

// One XEmbed tray client embedded into an offscreen container window. All
// X11 conversation for a single docked icon lives here; the backend routes
// the client window's events to this object. See ADR-0229 for the protocol
// contract (container with opacity 0 and empty input shape, manual composite
// redirect, save-set, damage-driven capture).
class XcbEmbeddedIcon : public TrayIconHost
{
    Q_OBJECT
public:
    XcbEmbeddedIcon(XcbTrayBackend *backend, quint32 clientWindow,
                    QObject *parent = nullptr);
    ~XcbEmbeddedIcon() override;

    bool embed() override;
    TrayIconImage captureIcon() override;
    QString clientTitle() const override;
    void forwardButton(quint8 button, qint32 rootX, qint32 rootY) override;
    void retire() override;

    // Backend event routing. Each maps to one X event class for the client
    // (or its container); they must be no-ops after retire().
    void onDamageNotify();
    void onDestroyNotify();
    void onUnmapNotify();
    void onReparentNotify(quint32 newParent);
    void onConfigureRequest(quint16 width, quint16 height);
    void onPropertyNotify(quint32 atom);

private:
    enum class InjectMode { Direct, XTest };

    void setActiveForInput(bool active);
    void refreshTitle();
    bool wantsButtonEvents(xcb_window_t window) const;

    XcbTrayBackend *m_backend = nullptr;
    quint32 m_clientWindow = 0;
    xcb_window_t m_containerWindow = XCB_NONE;
    xcb_damage_damage_t m_damageId = 0;
    quint32 m_width = 0;
    quint32 m_height = 0;
    QString m_title;
    InjectMode m_injectMode = InjectMode::Direct;
    bool m_retired = false;
};

} // namespace QindaQt::XEmbedTray
