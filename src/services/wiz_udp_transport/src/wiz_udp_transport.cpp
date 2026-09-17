// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_udp_transport/wiz_udp_transport.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QNetworkDatagram>
#include <QtNetwork/QNetworkInterface>

namespace QindaQt::Wiz
{
namespace
{

// The port the vendor firmware pushes syncPilot notifications to.
constexpr quint16 notificationPort = 38900;

} // namespace

WizUdpTransport::WizUdpTransport(QObject *parent)
    : WizTransport(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this,
            &WizUdpTransport::readPendingDatagrams);
    connect(&m_socket, &QUdpSocket::errorOccurred, this,
            &WizUdpTransport::reportSocketError);
}

WizUdpTransport::~WizUdpTransport()
{
    WizUdpTransport::stop();
}

bool WizUdpTransport::start(QString *error)
{
    if (m_socket.state() == QAbstractSocket::BoundState) {
        return true;
    }
    // ShareAddress lets a second desktop session (or another light controller)
    // coexist on the notification port instead of one of them failing to start.
    m_notificationPortBound =
        m_socket.bind(QHostAddress::AnyIPv4, notificationPort,
                      QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
    if (!m_notificationPortBound && !m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        if (error != nullptr) {
            *error = m_socket.errorString();
        }
        return false;
    }
    refreshLocalIdentity();
    return true;
}

void WizUdpTransport::stop()
{
    m_socket.close();
    m_notificationPortBound = false;
    m_listenerAddress.clear();
    m_listenerHardwareAddress.clear();
}

bool WizUdpTransport::send(const QString &address, const quint16 port,
                           const QByteArray &datagram)
{
    if (m_socket.state() != QAbstractSocket::BoundState || datagram.isEmpty()
        || datagram.size() > Limits::maximumDatagramBytes) {
        return false;
    }
    const QHostAddress target(address);
    if (target.isNull() || target.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }
    return m_socket.writeDatagram(datagram, target, port) == datagram.size();
}

bool WizUdpTransport::broadcast(const quint16 port, const QByteArray &datagram)
{
    if (m_socket.state() != QAbstractSocket::BoundState || datagram.isEmpty()) {
        return false;
    }
    bool delivered = false;
    for (const QHostAddress &target : broadcastTargets()) {
        if (m_socket.writeDatagram(datagram, target, port) == datagram.size()) {
            delivered = true;
        }
    }
    return delivered;
}

QString WizUdpTransport::listenerAddress() const
{
    // Without the notification port there is nothing for a light to push to.
    return m_notificationPortBound ? m_listenerAddress : QString();
}

QString WizUdpTransport::listenerHardwareAddress() const
{
    return m_notificationPortBound ? m_listenerHardwareAddress : QString();
}

QList<QHostAddress> WizUdpTransport::broadcastTargets() const
{
    QList<QHostAddress> targets;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)
            || !flags.testFlag(QNetworkInterface::CanBroadcast)) {
            continue;
        }
        const auto entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            const QHostAddress broadcastAddress = entry.broadcast();
            if (!broadcastAddress.isNull() && !targets.contains(broadcastAddress)) {
                targets.append(broadcastAddress);
            }
        }
    }
    if (targets.isEmpty()) {
        targets.append(QHostAddress(QHostAddress::Broadcast));
    }
    return targets;
}

void WizUdpTransport::refreshLocalIdentity()
{
    m_listenerAddress.clear();
    m_listenerHardwareAddress.clear();
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            m_listenerAddress = entry.ip().toString();
            // The firmware keys its subscription on this pair; the separators
            // are stripped because that is the form it echoes back.
            m_listenerHardwareAddress =
                interface.hardwareAddress().remove(QLatin1Char(':')).toLower();
            return;
        }
    }
}

void WizUdpTransport::readPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram(
            Limits::maximumDatagramBytes);
        if (!datagram.isValid()) {
            continue;
        }
        const QHostAddress sender = datagram.senderAddress();
        if (sender.isNull()) {
            continue;
        }
        Q_EMIT datagramReceived(sender.toString(),
                                static_cast<quint16>(datagram.senderPort()),
                                datagram.data());
    }
}

void WizUdpTransport::reportSocketError()
{
    // A datagram socket reports per-send failures too (an unreachable host,
    // for instance). Only losing the binding is a transport fault.
    if (m_socket.state() == QAbstractSocket::BoundState) {
        return;
    }
    Q_EMIT transportFailed(m_socket.errorString());
}

} // namespace QindaQt::Wiz
