// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>

namespace QindaQt::ObsBridge {

class BridgeController;

// The bridge's control API for obs-websocket clients (ADR-0208): vendor
// "qindaqt", request "GetConsoleMapping" answering with every console bus
// and strip and the OBS source it became, and event "ConsoleMappingChanged"
// whenever that mapping or the Audio1 state changes. Registration is
// skipped, without error, when obs-websocket is not loaded or the module
// was built without its API header.
class WebSocketVendor final {
public:
    explicit WebSocketVendor(BridgeController &controller);
    ~WebSocketVendor();

    WebSocketVendor(const WebSocketVendor &) = delete;
    WebSocketVendor &operator=(const WebSocketVendor &) = delete;

    [[nodiscard]] bool registered() const noexcept { return m_vendor != nullptr; }

private:
    static void handleMapping(void *request, void *response, void *priv);
    void emitChanged();

    BridgeController &m_controller;
    void *m_vendor = nullptr;
    QMetaObject::Connection m_connection;
};

} // namespace QindaQt::ObsBridge
