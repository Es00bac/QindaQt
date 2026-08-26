// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/capability_policy.h"
#include "qindaqt/applet_host/protocol_version.h"

#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/applet_manifest.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::AppletHost {

struct HostHello final {
    ProtocolVersion protocolVersion = ProtocolVersion::current();
    QindaQt::Applets::ApiVersion appletApiVersion = QindaQt::Applets::ApiVersion::current();
    QString manifestId;
    QByteArray launchToken;
};

enum class HandshakeStatus {
    Accepted,
    InvalidManifest,
    IdentityMismatch,
    LaunchTokenMismatch,
    ProtocolMismatch,
    AppletApiMismatch,
    PolicyUnavailable,
};

struct HandshakeResponse final {
    HandshakeStatus status = HandshakeStatus::InvalidManifest;
    std::optional<ProtocolVersion> negotiatedProtocol;
    QVector<CapabilityDecision> capabilityDecisions;
    QString message;

    [[nodiscard]] bool accepted() const;
};

class HostHandshake final {
public:
    // The launch token binds a hello to one launcher-created process. Transport
    // authentication and sandbox setup remain responsibilities of the adapter.
    [[nodiscard]] static HandshakeResponse evaluate(
        const HostHello &hello,
        const QindaQt::Applets::AppletManifest &manifest,
        const QByteArray &expectedLaunchToken,
        const CapabilityEvaluation &capabilityEvaluation,
        const ProtocolVersion &hostProtocol = ProtocolVersion::current(),
        const QindaQt::Applets::ApiVersion &hostApi = QindaQt::Applets::ApiVersion::current());
};

[[nodiscard]] QString toString(HandshakeStatus value);
[[nodiscard]] std::optional<HandshakeStatus> handshakeStatusFromString(const QString &value);

} // namespace QindaQt::AppletHost
