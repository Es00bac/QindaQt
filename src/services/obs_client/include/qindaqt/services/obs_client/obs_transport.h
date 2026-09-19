// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Obs {

// The socket under the client, as a seam.
//
// AGENT-CONTRACT: Implementations move text frames and connection edges and
// decide nothing else — no retry, no state, no parsing. Transport and client
// share one Qt thread; every signal is emitted asynchronously, and a frame
// arriving after close() is permitted and dropped by the client.
class ObsTransport : public QObject {
    Q_OBJECT

public:
    explicit ObsTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~ObsTransport() override = default;

    // Opens a connection to `url` (`ws://host:port`). Calling open() while
    // already open is a no-op, not a second socket.
    virtual void open(const QString &url) = 0;
    virtual void close() = 0;
    // Sends one text frame. Sending while not connected is dropped by the
    // implementation; the client never queues behind a closed socket,
    // because a queued toggle that fires on reconnect would surprise the user
    // minutes later.
    virtual void sendText(const QString &text) = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    // Peer close code for the most recent disconnected signal, or 0 when
    // unavailable. The client interprets OBS's authentication/protocol codes;
    // the transport must preserve the number instead of guessing from text.
    [[nodiscard]] virtual int closeCode() const { return 0; }

Q_SIGNALS:
    void connected();
    // `reason` is the transport's own words; the client maps it to a reason
    // code and never shows it raw where a code is expected.
    void disconnected(const QString &reason);
    void textReceived(const QString &text);
    void errorOccurred(const QString &reason);
};

} // namespace QindaQt::Obs
