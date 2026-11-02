// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror_connection.h"

#include <QHostAddress>

namespace qindaqt::link_mirror {
namespace {

constexpr int kRetryDelayMs = 150;

} // namespace

MirrorConnection::MirrorConnection(QObject *parent) : QObject(parent)
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(kRetryDelayMs);
    connect(&m_retry, &QTimer::timeout, this, &MirrorConnection::attempt);
    connect(&m_socket, &QTcpSocket::connected, this, [this] { m_socket.write(helloFrame(m_token)); });
    connect(&m_socket, &QTcpSocket::readyRead, this, &MirrorConnection::readFrames);
    connect(&m_socket, &QTcpSocket::disconnected, this, &MirrorConnection::handleDisconnect);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (error != QAbstractSocket::RemoteHostClosedError) {
            handleDisconnect();
        }
    });
}

void MirrorConnection::connectTo(quint16 port, const QString &token, int timeoutMs)
{
    m_port = port;
    m_token = token;
    m_timeoutMs = timeoutMs;
    m_started.start();
    attempt();
}

void MirrorConnection::attempt()
{
    if (m_closing) {
        return;
    }
    m_decoder = FrameDecoder();
    m_socket.abort();
    m_socket.connectToHost(QHostAddress::LocalHost, m_port);
}

void MirrorConnection::send(const QByteArray &frame)
{
    if (m_ready && !frame.isEmpty()) {
        m_socket.write(frame);
    }
}

void MirrorConnection::close()
{
    if (m_closing) {
        return;
    }
    m_closing = true;
    m_retry.stop();
    if (m_ready && m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.write(stopFrame());
        m_socket.flush();
        m_socket.waitForBytesWritten(1000);
    }
    m_socket.disconnectFromHost();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.waitForDisconnected(1000);
    }
    m_socket.abort();
}

void MirrorConnection::handleDisconnect()
{
    if (m_closing) {
        return;
    }
    if (m_ready) {
        // A session that reached ready never reconnects: the server exits with its socket.
        m_ready = false;
        m_closing = true;
        Q_EMIT disconnected();
        return;
    }
    if (m_started.elapsed() < m_timeoutMs) {
        if (!m_retry.isActive()) {
            m_retry.start();
        }
        return;
    }
    m_closing = true;
    Q_EMIT failed(QStringLiteral("the phone mirror server did not answer within %1 ms").arg(m_timeoutMs));
}

void MirrorConnection::readFrames()
{
    m_decoder.append(m_socket.readAll());
    Frame frame;
    for (;;) {
        const DecodeStatus status = m_decoder.next(frame);
        if (status == DecodeStatus::NeedsMore) {
            return;
        }
        if (status != DecodeStatus::Frame) {
            m_closing = true;
            m_socket.abort();
            Q_EMIT failed(QStringLiteral("protocol error from the phone: %1").arg(decodeStatusName(status)));
            return;
        }
        dispatch(frame);
        if (m_closing) {
            return;
        }
    }
}

void MirrorConnection::dispatch(const Frame &frame)
{
    switch (frame.channel) {
    case Channel::Video:
        if (frame.type == video::Packet) {
            if (const auto packet = parseVideoPacket(frame.payload)) {
                Q_EMIT videoPacket(*packet);
            }
        } else if (frame.type == video::Format) {
            if (const auto format = parseVideoFormat(frame.payload)) {
                Q_EMIT videoFormat(*format);
            }
        }
        return;
    case Channel::Control:
        if (frame.type == control::Ready) {
            if (const auto info = parseReady(frame.payload)) {
                m_ready = true;
                Q_EMIT ready(*info);
            }
        } else if (frame.type == control::Display) {
            if (const auto display = parseDisplay(frame.payload)) {
                Q_EMIT displayChanged(*display);
            }
        } else if (frame.type == control::Error) {
            if (const auto error = parseError(frame.payload)) {
                Q_EMIT serverError(*error);
            }
        } else if (frame.type == control::AppExited) {
            if (const auto reason = parseAppExited(frame.payload)) {
                Q_EMIT appExited(*reason);
            }
        }
        return;
    case Channel::Inventory:
        if (frame.type == inventory::App) {
            if (const auto app = parseApp(frame.payload)) {
                Q_EMIT appReceived(*app);
            }
        } else if (frame.type == inventory::End) {
            Q_EMIT inventoryFinished(parseInventoryEnd(frame.payload).value_or(0));
        }
        return;
    }
}

} // namespace qindaqt::link_mirror
