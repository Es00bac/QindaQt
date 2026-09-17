// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_client/wiz_transport.h>

#include <QtCore/QList>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>

namespace QindaQt::Wiz
{

// The real datagram socket for Wiz luminaires.
//
// AGENT-CONTRACT: the vendor firmware pushes unsolicited state to UDP 38900 on
// whatever address it was registered with. This transport therefore tries to
// bind that port first and falls back to an ephemeral port, which still works
// for request/response but reports no listener address, so the client stops
// asking lights to push notifications nobody would receive.
//
// AGENT-GUARD: received datagrams are handed on unparsed and unauthenticated.
// Any host on the network can send them; nothing downstream may treat a
// datagram's source address as identity.
class WizUdpTransport final : public WizTransport
{
    Q_OBJECT

public:
    explicit WizUdpTransport(QObject *parent = nullptr);
    ~WizUdpTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    [[nodiscard]] bool send(const QString &address, quint16 port,
                            const QByteArray &datagram) override;
    [[nodiscard]] bool broadcast(quint16 port, const QByteArray &datagram) override;
    [[nodiscard]] QString listenerAddress() const override;
    [[nodiscard]] QString listenerHardwareAddress() const override;

private Q_SLOTS:
    void readPendingDatagrams();
    void reportSocketError();

private:
    // Per-interface broadcast addresses plus the global one. A single
    // 255.255.255.255 send does not always leave the right interface on a
    // multi-homed host.
    [[nodiscard]] QList<QHostAddress> broadcastTargets() const;
    void refreshLocalIdentity();

    QUdpSocket m_socket;
    bool m_notificationPortBound = false;
    QString m_listenerAddress;
    QString m_listenerHardwareAddress;
};

} // namespace QindaQt::Wiz
