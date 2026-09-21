// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtDBus/QDBusObjectPath>

#include <functional>

#include <qindaqt/services/xembed_tray_proxy/tray_icon_image.h>

#include "sni_image_types.h"

namespace QindaQt::XEmbedTray
{

// AGENT-CONTRACT: hand-rolled org.kde.StatusNotifierItem implementation for
// one proxied XEmbed icon. Property names, method names, and the New* signal
// names are the StatusNotifierItem specification and are consumed verbatim by
// the shell's item client (src/shell/status_notifier/item_client); renaming
// any of them silently breaks the tray. The item must stay presentable with
// an empty IconName and a pixmap-only icon — that is the Wine shape covered
// by the shell regression row projectsAPixmapOnlyItemWithNoExportedMenu.
//
// AGENT-GUARD: the only Qt signals on this object are the specification's
// New* signals. registerObject exports every Qt signal as a D-Bus signal, so
// adding a helper signal here would leak a non-spec signal onto the wire;
// the coordinator-facing path is the std::function button handler instead.
class StatusNotifierItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")

    Q_PROPERTY(QString Category READ category)
    Q_PROPERTY(QString Id READ identity)
    Q_PROPERTY(QString Title READ title)
    Q_PROPERTY(QString Status READ status)
    Q_PROPERTY(QString IconName READ iconName)
    Q_PROPERTY(SniImageList IconPixmap READ iconPixmap)
    Q_PROPERTY(QDBusObjectPath Menu READ menu)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu)
    Q_PROPERTY(quint32 WindowId READ windowId)

public:
    // Called on Activate/SecondaryActivate/ContextMenu/Scroll with the X11
    // button number and the host-supplied coordinates. Installed by the
    // coordinator; invoked on this object's thread.
    using ButtonHandler = std::function<void(quint8 button, qint32 x, qint32 y)>;

    explicit StatusNotifierItem(quint32 windowId, QObject *parent = nullptr);

    // AGENT-CONTRACT: title/identity text is truncated to the shell
    // registry's UTF-8 budgets (kMaxTitleUtf8Bytes/kMaxIdentityUtf8Bytes, 256)
    // at this boundary so a hostile _NET_WM_NAME never reaches the wire.
    void setTitle(const QString &title);
    void setIcon(const TrayIconImage &image);
    void setButtonHandler(ButtonHandler handler);

    [[nodiscard]] QString category() const { return QStringLiteral("ApplicationStatus"); }
    [[nodiscard]] QString identity() const;
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString status() const { return QStringLiteral("Active"); }
    [[nodiscard]] QString iconName() const { return QString(); }
    [[nodiscard]] SniImageList iconPixmap() const { return SniImageList{m_icon}; }
    // A Wine tray menu is an X11 popup the client owns and places itself;
    // there is deliberately no dbusmenu to point at.
    [[nodiscard]] QDBusObjectPath menu() const
    {
        return QDBusObjectPath(QStringLiteral("/NO_DBUSMENU"));
    }
    [[nodiscard]] bool itemIsMenu() const { return false; }
    [[nodiscard]] quint32 windowId() const { return m_windowId; }

    [[nodiscard]] bool hasIcon() const { return m_icon.width > 0; }

public Q_SLOTS:
    void Activate(int x, int y);
    void SecondaryActivate(int x, int y);
    void ContextMenu(int x, int y);
    void Scroll(int delta, const QString &orientation);

Q_SIGNALS:
    // StatusNotifierItem specification signals, exported onto D-Bus.
    void NewIcon();
    void NewTitle();
    void NewStatus(const QString &status);

private:
    quint32 m_windowId = 0;
    QString m_title;
    SniImage m_icon;
    ButtonHandler m_buttonHandler;
};

} // namespace QindaQt::XEmbedTray
