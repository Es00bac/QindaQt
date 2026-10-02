// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "shortcut_ui.h"
#include "shortcut_wire.h"
#include <qindaqt/services/shortcuts_client/transport.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <memory>
namespace QindaQt::Services::Portal {
// Same-thread borrowed request, UI and native authority outlive this adaptor.
// Owns only GlobalShortcuts v1 session/action lifetimes, not input or UI policy.
class GlobalShortcutsAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    GlobalShortcutsAdaptor(QObject &, RequestRegistry &, ShortcutUi &, QindaQt::Services::Shortcuts::QtShortcutTransport &, QDBusConnection);
    ~GlobalShortcutsAdaptor() override;
    uint version() const { return 1; }
public Q_SLOTS:
    quint32 CreateSession(const QDBusObjectPath &, const QDBusObjectPath &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 BindShortcuts(const QDBusObjectPath &, const QDBusObjectPath &, const PortalShortcuts &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 ListShortcuts(const QDBusObjectPath &, const QDBusObjectPath &, const QDBusMessage &, QVariantMap &);
Q_SIGNALS:
    void Activated(const QDBusObjectPath &, const QString &, quint64, const QVariantMap &);
    void Deactivated(const QDBusObjectPath &, const QString &, quint64, const QVariantMap &);
    void ShortcutsChanged(const QDBusObjectPath &, const PortalShortcuts &);
private:
    class Private; std::unique_ptr<Private> d;
};
}
