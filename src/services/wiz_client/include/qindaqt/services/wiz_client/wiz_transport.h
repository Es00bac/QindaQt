// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Wiz
{

// Datagram seam between the client's policy and an actual socket.
//
// AGENT-CONTRACT: implementations and callers share one Qt thread, deliver
// every received datagram asynchronously, and never interpret payloads. The
// client decides retries, timeouts, and what a silent device means; a
// transport only reports whether a datagram could be handed to the network.
class WizTransport : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~WizTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;

    [[nodiscard]] virtual bool send(const QString &address, quint16 port,
                                    const QByteArray &datagram) = 0;

    // Sends to every attached IPv4 broadcast domain. Discovery is the only
    // legitimate use: luminaires answer it with their own identity.
    [[nodiscard]] virtual bool broadcast(quint16 port, const QByteArray &datagram) = 0;

    // The address a luminaire should push notifications to, or empty when the
    // transport cannot receive them. The client only subscribes when this is
    // non-empty, so a light is never asked to send to an address nobody reads.
    [[nodiscard]] virtual QString listenerAddress() const = 0;
    [[nodiscard]] virtual QString listenerHardwareAddress() const = 0;

Q_SIGNALS:
    void datagramReceived(const QString &address, quint16 port,
                          const QByteArray &datagram);
    // The socket became unusable. The client treats this as loss of authority
    // over every device, not as a per-device failure.
    void transportFailed(const QString &reason);
};

// Monotonic time seam. Injecting it keeps timeout, retry, and reachability
// behaviour testable without waiting on a real clock.
class WizClock
{
public:
    virtual ~WizClock() = default;
    [[nodiscard]] virtual quint64 monotonicMilliseconds() const = 0;
};

// QElapsedTimer-backed clock for production composition.
class SystemWizClock final : public WizClock
{
public:
    SystemWizClock();
    [[nodiscard]] quint64 monotonicMilliseconds() const override;

private:
    QElapsedTimer m_timer;
};

} // namespace QindaQt::Wiz
