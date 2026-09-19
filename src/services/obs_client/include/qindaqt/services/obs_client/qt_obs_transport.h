// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_transport.h>

#include <QWebSocket>

namespace QindaQt::Obs {

// Production transport: one QWebSocket to obs-websocket.
//
// AGENT-GUARD: Only loopback addresses are accepted. obs-websocket has no
// transport security, so a host key that pointed somewhere else would send
// the password and every scene name over the network in the clear. A
// non-loopback URL is refused here rather than validated in the route alone,
// because the route is not the only caller.
class QtObsTransport final : public ObsTransport {
    Q_OBJECT

public:
    explicit QtObsTransport(QObject *parent = nullptr);
    ~QtObsTransport() override;

    void open(const QString &url) override;
    void close() override;
    void sendText(const QString &text) override;
    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] int closeCode() const override;

    // True when `url` is a `ws://` URL on the loopback interface with a
    // usable port. Exported so the Streaming route can refuse the address
    // while the user is still typing it.
    [[nodiscard]] static bool isLoopbackWebSocketUrl(const QString &url);

private:
    QWebSocket m_socket;
    bool m_open = false;
};

} // namespace QindaQt::Obs
