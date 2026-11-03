// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/identity.h>
#include <qindaqt/services/link_transport/session.h>

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslServer>

namespace QindaQt::Link {

class AcceptLimiter;

// QSslServer that can refuse a connection by source address before the handshake starts.
// AGENT-NOTE: QSslServer::incomingConnection receives only the descriptor, so the peer address is read
// with getpeername(2); a refused descriptor is closed without any TLS work.
class LimitedSslServer final : public QSslServer {
public:
    using QSslServer::QSslServer;
    void setLimiter(AcceptLimiter *limiter) { m_limiter = limiter; }

protected:
    void incomingConnection(qintptr descriptor) override;

private:
    AcceptLimiter *m_limiter = nullptr;
};

// TLS 1.3-only listener that demands a client certificate and hands over
// fully handshaken sockets. Chain and self-signed errors are ignored here
// because trust is a key pin, enforced by Session/Hub against the trust store.
//
// AGENT-GUARD: never add a code path that treats a successful handshake as
// authorization. The only authority is a pinned SPKI digest plus pairing.
class TlsListener final : public QObject {
    Q_OBJECT
public:
    TlsListener(const DeviceIdentity &identity, Origin origin, QObject *parent = nullptr);
    // Port 0 picks an ephemeral port (tests).
    [[nodiscard]] bool listen(const QHostAddress &address, quint16 port);
    [[nodiscard]] quint16 port() const { return m_server.serverPort(); }
    [[nodiscard]] Origin origin() const { return m_origin; }
    [[nodiscard]] QHostAddress address() const { return m_server.serverAddress(); }
    [[nodiscard]] QString errorString() const { return m_server.errorString(); }
    void close() { m_server.close(); }
    // LAN listeners: refuse connections beyond the limiter's budget before any TLS work, and give the
    // handshake only `handshakeTimeoutMs`. Not owned. AGENT-GUARD: LAN listeners must always set this.
    void setAcceptLimiter(AcceptLimiter *limiter, int handshakeTimeoutMs);

Q_SIGNALS:
    void connectionReady(QSslSocket *socket, QindaQt::Link::Origin origin);

private:
    LimitedSslServer m_server;
    Origin m_origin;
};

} // namespace QindaQt::Link
