// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/native_notifications.h>
#include <qindaqt/services/portal/request_registry.h>
#include <QDBusAbstractAdaptor>
#include <memory>
namespace QindaQt::Services::Portal {
// Public standard Notification backend v2, with independent app/id replacement
// namespaces and bounded action metadata. Borrowed same-thread native port and
// frontend admission registry outlive this adaptor. Owner retirement removes
// native notifications and cancels pending replies before metadata disposal.
class NotificationAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Notification")
    Q_PROPERTY(quint32 version READ version CONSTANT)
    Q_PROPERTY(QVariantMap SupportedOptions READ supportedOptions CONSTANT)
public:
    NotificationAdaptor(QObject &host, RequestRegistry &, NativeNotifications &, QDBusConnection);
    ~NotificationAdaptor() override;
    quint32 version() const { return 2; }
    QVariantMap supportedOptions() const { return {}; }
public Q_SLOTS:
    void AddNotification(const QString &appId, const QString &id, const QVariantMap &, const QDBusMessage &);
    void RemoveNotification(const QString &appId, const QString &id, const QDBusMessage &);
Q_SIGNALS:
    void ActionInvoked(const QString &appId, const QString &id, const QString &action, const QVariantList &parameter);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal
