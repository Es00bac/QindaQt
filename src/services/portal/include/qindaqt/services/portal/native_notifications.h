// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/notification_policy.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <memory>
namespace QindaQt::Services::Portal {
// Async same-thread native server port. It owns transport subscriptions, never
// app identity or portal action routing. At most 32 outstanding operations,
// each bounded to 5s; provider owner/UID and incoming signal sender are checked.
// This is forwarding to the selected native Notifications owner, not a legacy
// backend fallback or executable attestation. No notification payload is logged.
class NativeNotifications : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void notify(quint64 token, quint32 replaces, const PortalNotification &) = 0;
    virtual void close(quint64 token, quint32 id) = 0;
Q_SIGNALS:
    void notified(quint64 token, bool success, quint32 id);
    void removed(quint64 token, bool success);
    void action(quint32 id, const QString &key, const QString &activationToken);
    void closed(quint32 id);
    void unavailable();
};
class QtNativeNotifications final : public NativeNotifications {
    Q_OBJECT
public:
    explicit QtNativeNotifications(QDBusConnection, QObject *parent = nullptr);
    ~QtNativeNotifications() override;
    void notify(quint64, quint32, const PortalNotification &) override;
    void close(quint64, quint32) override;
private Q_SLOTS:
    void invoked(quint32 id, const QString &key, const QDBusMessage &);
    void activation(quint32 id, const QString &token, const QDBusMessage &);
    void notificationClosed(quint32 id, quint32 reason, const QDBusMessage &);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal
