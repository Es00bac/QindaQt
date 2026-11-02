// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/tls_listener.h>

#include <QtNetwork/QSslConfiguration>

namespace QindaQt::Link {

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

bool TlsListener::listen(const QHostAddress &address, quint16 port) { return m_server.listen(address, port); }

} // namespace QindaQt::Link
