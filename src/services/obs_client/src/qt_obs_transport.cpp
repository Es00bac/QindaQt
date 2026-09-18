// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/qt_obs_transport.h>

#include <QHostAddress>
#include <QUrl>

namespace QindaQt::Obs {

QtObsTransport::QtObsTransport(QObject *parent) : ObsTransport(parent) {
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        m_open = true;
        Q_EMIT connected();
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        const bool wasOpen = m_open;
        m_open = false;
        // A close that follows a failed connect is reported once, by the
        // socket; a close with no prior open is still a close to the client.
        Q_UNUSED(wasOpen)
        Q_EMIT disconnected(m_socket.closeReason());
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this,
            &ObsTransport::textReceived);
    connect(&m_socket, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                Q_EMIT errorOccurred(m_socket.errorString());
            });
}

QtObsTransport::~QtObsTransport() {
    m_socket.abort();
}

bool QtObsTransport::isLoopbackWebSocketUrl(const QString &url) {
    const QUrl parsed(url);
    if (!parsed.isValid() || parsed.scheme() != QLatin1String("ws")) {
        return false;
    }
    if (parsed.port() <= 0 || parsed.port() > 65535) {
        return false;
    }
    const QString host = parsed.host();
    if (host == QLatin1String("localhost")) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

void QtObsTransport::open(const QString &url) {
    if (m_open || m_socket.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    if (!isLoopbackWebSocketUrl(url)) {
        Q_EMIT errorOccurred(
            QStringLiteral("obs-websocket address must be loopback"));
        Q_EMIT disconnected(QStringLiteral("obs-address-not-loopback"));
        return;
    }
    m_socket.open(QUrl(url));
}

void QtObsTransport::close() {
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        return;
    }
    m_socket.close();
}

void QtObsTransport::sendText(const QString &text) {
    if (!m_open) {
        return;
    }
    m_socket.sendTextMessage(text);
}

bool QtObsTransport::isOpen() const { return m_open; }

} // namespace QindaQt::Obs
