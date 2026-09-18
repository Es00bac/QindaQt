// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_client/wiz_transport.h>

#include <QtCore/QList>

namespace QindaQt::Wiz::Testing
{

// Records what the client asked the network to do and lets a test answer as a
// luminaire would, with no socket and no waiting.
class FakeWizTransport final : public WizTransport
{
    Q_OBJECT

public:
    struct Sent {
        QString address;
        quint16 port = 0;
        QByteArray datagram;
    };

    using WizTransport::WizTransport;

    bool startSucceeds = true;
    QString startError = QStringLiteral("no socket");
    bool sendSucceeds = true;
    bool broadcastSucceeds = true;
    QString listener = QStringLiteral("10.0.0.7");
    QString listenerMac = QStringLiteral("aabbccddeeff");
    QList<Sent> unicasts;
    QList<QByteArray> broadcasts;
    bool started = false;

    [[nodiscard]] bool start(QString *error) override
    {
        if (!startSucceeds) {
            if (error != nullptr) {
                *error = startError;
            }
            return false;
        }
        started = true;
        return true;
    }

    void stop() override { started = false; }

    [[nodiscard]] bool send(const QString &address, const quint16 port,
                            const QByteArray &datagram) override
    {
        if (!sendSucceeds) {
            return false;
        }
        unicasts.append(Sent{address, port, datagram});
        return true;
    }

    [[nodiscard]] bool broadcast(const quint16, const QByteArray &datagram) override
    {
        if (!broadcastSucceeds) {
            return false;
        }
        broadcasts.append(datagram);
        return true;
    }

    [[nodiscard]] QString listenerAddress() const override { return listener; }
    [[nodiscard]] QString listenerHardwareAddress() const override
    {
        return listenerMac;
    }

    // A reply, which the firmware sends from the control port.
    void deliver(const QString &address, const QByteArray &datagram)
    {
        deliverFrom(address, 38899, datagram);
    }

    // A datagram from an arbitrary source port, as an unsolicited push is.
    void deliverFrom(const QString &address, const quint16 port,
                     const QByteArray &datagram)
    {
        Q_EMIT datagramReceived(address, port, datagram);
    }

    void fail(const QString &reason) { Q_EMIT transportFailed(reason); }

    [[nodiscard]] int unicastCount(const QByteArray &methodName) const
    {
        int count = 0;
        for (const Sent &sent : unicasts) {
            if (sent.datagram.contains(methodName)) {
                ++count;
            }
        }
        return count;
    }
};

// Time advances only when a test says so.
class FakeWizClock final : public WizClock
{
public:
    quint64 now = 0;
    [[nodiscard]] quint64 monotonicMilliseconds() const override { return now; }
};

} // namespace QindaQt::Wiz::Testing
