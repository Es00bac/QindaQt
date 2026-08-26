// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/host_handshake.h"

#include <QByteArray>
#include <QtTypes>

namespace QindaQt::AppletHost {

struct HelloDecodeResult final {
    bool ok = false;
    HostHello hello;
    QString error;
};

struct ResponseDecodeResult final {
    bool ok = false;
    HandshakeResponse response;
    QString error;
};

class HostProtocolCodec final {
public:
    static constexpr qsizetype MaximumMessageBytes = 64 * 1024;

    [[nodiscard]] static QByteArray encodeHello(const HostHello &hello);
    [[nodiscard]] static HelloDecodeResult decodeHello(const QByteArray &message);
    [[nodiscard]] static QByteArray encodeResponse(const HandshakeResponse &response);
    [[nodiscard]] static ResponseDecodeResult decodeResponse(const QByteArray &message);
};

} // namespace QindaQt::AppletHost
