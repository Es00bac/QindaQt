// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
namespace QindaQt::Power {
// Same-thread admission for the actual current supervisor bus identity, never
// a caller-provided PID/name. Native-exclusive admission is constructor-owner
// truth and defaults off; a replacement loses every consumer registration.
class IdleConsumerAuthority final : public QObject {
    Q_OBJECT
public:
    explicit IdleConsumerAuthority(QDBusConnection connection, QObject *parent = nullptr);
    void setNativeAdmission(bool admitted);
    bool accepts(const QString &actualSender);
    void refresh();
Q_SIGNALS:
    void revoked();
private:
    QDBusConnection m_connection;
    QDBusServiceWatcher m_watcher;
    QString m_sessionOwner;
    bool m_nativeAdmission = false;
    bool m_legacyAbsent = false;
};
}
