// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_client/peripheral_transport.h>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
namespace QindaQt::Power {
class QtPeripheralTransport final : public PeripheralTransport {
    Q_OBJECT
public:
    explicit QtPeripheralTransport(const QDBusConnection &connection,QObject *parent=nullptr);
    ~QtPeripheralTransport() override;
    void bind(const QString &owner) override;
    void request(quint64 token) override;
    void cancel() override;
private Q_SLOTS:
    void onReceipt(const QDBusMessage &message);
    void onInvalidated(const QDBusMessage &message);
private:
    bool authentic(const QDBusMessage &message,const QString &member,const QString &signature) const;
    QDBusConnection m_connection;
    QString m_owner,m_nonce;
    quint64 m_token=0;
    bool m_subscribed=false;
};
}
