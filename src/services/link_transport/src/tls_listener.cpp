// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/tls_listener.h>

#include <qindaqt/services/link_transport/accept_limiter.h>

#include <QtNetwork/QSslConfiguration>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace QindaQt::Link {

void LimitedSslServer::incomingConnection(qintptr descriptor)
{
    if (m_limiter != nullptr) {
        sockaddr_storage ss{};
        socklen_t len = sizeof(ss);
        QHostAddress source;
        if (::getpeername(int(descriptor), reinterpret_cast<sockaddr *>(&ss), &len) == 0) {
            if (ss.ss_family == AF_INET) {
                source = QHostAddress(quint32(ntohl(reinterpret_cast<sockaddr_in *>(&ss)->sin_addr.s_addr)));
            } else if (ss.ss_family == AF_INET6) {
                source = QHostAddress(reinterpret_cast<sockaddr_in6 *>(&ss)->sin6_addr.s6_addr);
                bool mapped = false;
                const quint32 v4 = source.toIPv4Address(&mapped);
                if (mapped) source = QHostAddress(v4);
            }
        }
        // An unreadable source is refused: it cannot be rate limited.
        if (source.isNull() || !m_limiter->allow(source)) {
            ::close(int(descriptor));
            return;
        }
    }
    QSslServer::incomingConnection(descriptor);
}

TlsListener::TlsListener(const DeviceIdentity &identity, Origin origin, QObject *parent)
    : QObject(parent), m_server(this), m_origin(origin)
{
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_3);
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    config.setLocalCertificate(identity.certificate);
    config.setPrivateKey(identity.key);
    m_server.setSslConfiguration(config);
    connect(&m_server, &QSslServer::sslErrors, this,
            [](QSslSocket *socket, const QList<QSslError> &) { socket->ignoreSslErrors(); });
    connect(&m_server, &QSslServer::pendingConnectionAvailable, this, [this] {
        while (auto *socket = qobject_cast<QSslSocket *>(m_server.nextPendingConnection())) {
            if (socket->isEncrypted()) {
                Q_EMIT connectionReady(socket, m_origin);
            } else {
                socket->deleteLater();
            }
        }
    });
}

void TlsListener::setAcceptLimiter(AcceptLimiter *limiter, int handshakeTimeoutMs)
{
    m_server.setLimiter(limiter);
    m_server.setHandshakeTimeout(handshakeTimeoutMs);
}

bool TlsListener::listen(const QHostAddress &address, quint16 port) { return m_server.listen(address, port); }

} // namespace QindaQt::Link
