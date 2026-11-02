// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/identity.h>
#include <qindaqt/services/link_transport/session.h>

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslServer>

namespace QindaQt::Link {

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
    void close() { m_server.close(); }

Q_SIGNALS:
    void connectionReady(QSslSocket *socket, QindaQt::Link::Origin origin);

private:
    QSslServer m_server;
    Origin m_origin;
};

} // namespace QindaQt::Link
