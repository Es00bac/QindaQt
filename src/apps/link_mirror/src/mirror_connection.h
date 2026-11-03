// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/mirror_messages.h"
#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// The desktop end of the mirror socket: connects through the adb forward, performs the hello
// and demultiplexes incoming frames into typed signals.
//
// AGENT-NOTE: adb accepts a forwarded TCP connection even while nothing listens on the phone
// and then closes it at once. Until `ready` arrives, a disconnect therefore means "server not
// listening yet" and the connection retries until the deadline.
//
// Threading: GUI thread. Ownership: owns its socket.
namespace qindaqt::link_mirror {

class MirrorConnection : public QObject {
    Q_OBJECT

public:
    explicit MirrorConnection(QObject *parent = nullptr);

    void connectTo(quint16 port, const QString &token, int timeoutMs);
    void send(const QByteArray &frame);
    // Sends stop (when ready) and closes the socket.
    void close();
    [[nodiscard]] bool isReady() const { return m_ready; }

Q_SIGNALS:
    void ready(const qindaqt::link_mirror::ReadyInfo &info);
    void displayChanged(const qindaqt::link_mirror::DisplayInfo &display);
    void serverError(const qindaqt::link_mirror::ServerError &error);
    void appExited(const QString &reason);
    void screenStateChanged(bool on);
    void videoFormat(const qindaqt::link_mirror::VideoFormat &format);
    void videoPacket(const qindaqt::link_mirror::VideoPacket &packet);
    void appReceived(const qindaqt::link_mirror::AppEntry &app);
    void inventoryFinished(int count);
    void failed(const QString &reason);
    void disconnected();

private:
    void attempt();
    void readFrames();
    void dispatch(const Frame &frame);
    void handleDisconnect();

    QTcpSocket m_socket;
    QTimer m_retry;
    QElapsedTimer m_started;
    FrameDecoder m_decoder;
    QString m_token;
    quint16 m_port = 0;
    int m_timeoutMs = 0;
    bool m_ready = false;
    bool m_closing = false;
};

} // namespace qindaqt::link_mirror
