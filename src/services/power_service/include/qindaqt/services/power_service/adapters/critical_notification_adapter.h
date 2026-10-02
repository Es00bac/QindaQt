// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_service/critical_notification.h>
#include <QDBusConnection>
#include <QDBusMessage>
class QDBusServiceWatcher;
namespace QindaQt::Power::Upstream {
// Confined standard producer transport, not a notification presentation client.
// Owns one exact-owner ID and nonce action. Unknown Notify outcomes have no safe
// close ID: no replay/action, with explicitly finite server expiry (ADR-0331).
class CriticalNotificationAdapter final : public CriticalNotification {
    Q_OBJECT
public:
    explicit CriticalNotificationAdapter(QDBusConnection bus, QObject *parent = nullptr);
    ~CriticalNotificationAdapter() override;
    void show(const QString &action, int seconds) override;
    void update(int seconds) override;
    void close() override;
private Q_SLOTS:
    void actionInvoked(const QDBusMessage &message);
    void notificationClosed(const QDBusMessage &message);
private:
    QString currentOwner() const;
    void notify(int seconds, bool first);
    void reject();
    QDBusConnection m_bus;
    QDBusServiceWatcher *m_watcher = nullptr;
    QString m_owner, m_action, m_cancelKey;
    quint32 m_id = 0;
    int m_seconds = 0;
    bool m_wanted = false, m_pending = false, m_closing = false, m_closeRequested = false;
    bool m_subscribed = false;
};
}
