// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/host_handshake.h"

namespace QindaQt::AppletHost {
namespace {

HandshakeResponse reject(HandshakeStatus status, const QString &message)
{
    return {.status = status,
            .negotiatedProtocol = std::nullopt,
            .capabilityDecisions = {},
            .message = message};
}

bool decisionsMatchManifest(const QVector<CapabilityDecision> &decisions,
                            const QindaQt::Applets::AppletManifest &manifest)
{
    if (decisions.size() != manifest.capabilities.size()) {
        return false;
    }
    for (qsizetype index = 0; index < decisions.size(); ++index) {
        if (decisions.at(index).capability != manifest.capabilities.at(index)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool HandshakeResponse::accepted() const
{
    return status == HandshakeStatus::Accepted;
}

HandshakeResponse HostHandshake::evaluate(
    const HostHello &hello,
    const QindaQt::Applets::AppletManifest &manifest,
    const QByteArray &expectedLaunchToken,
    const CapabilityEvaluation &capabilityEvaluation,
    const ProtocolVersion &hostProtocol,
    const QindaQt::Applets::ApiVersion &hostApi)
{
    const QindaQt::Applets::ManifestValidation validation = manifest.validate();
    if (!validation.isValid()) {
        return reject(HandshakeStatus::InvalidManifest, validation.summary());
    }
    if (hello.manifestId != manifest.id) {
        return reject(HandshakeStatus::IdentityMismatch,
                      QStringLiteral("Hello manifest id does not match launch manifest"));
    }
    if (expectedLaunchToken.isEmpty() || hello.launchToken != expectedLaunchToken) {
        return reject(HandshakeStatus::LaunchTokenMismatch,
                      QStringLiteral("Hello launch token does not match this process instance"));
    }

    const std::optional<ProtocolVersion> negotiated = hostProtocol.negotiate(hello.protocolVersion);
    if (!negotiated.has_value()) {
        return reject(HandshakeStatus::ProtocolMismatch,
                      QStringLiteral("Host protocol %1 cannot negotiate with peer protocol %2")
                          .arg(hostProtocol.toString(), hello.protocolVersion.toString()));
    }
    if (!manifest.apiVersion.isSupportedBy(hostApi)
        || hello.appletApiVersion != manifest.apiVersion) {
        return reject(HandshakeStatus::AppletApiMismatch,
                      QStringLiteral("Applet API declaration or hello is incompatible with host API %1")
                          .arg(hostApi.toString()));
    }
    if (!capabilityEvaluation.ok
        || !decisionsMatchManifest(capabilityEvaluation.decisions, manifest)) {
        return reject(HandshakeStatus::PolicyUnavailable,
                      capabilityEvaluation.error.isEmpty()
                          ? QStringLiteral("Capability decisions do not cover the manifest request")
                          : capabilityEvaluation.error);
    }

    return {.status = HandshakeStatus::Accepted,
            .negotiatedProtocol = negotiated,
            .capabilityDecisions = capabilityEvaluation.decisions,
            .message = QStringLiteral("Host handshake accepted")};
}

QString toString(HandshakeStatus value)
{
    switch (value) {
    case HandshakeStatus::Accepted:
        return QStringLiteral("accepted");
    case HandshakeStatus::InvalidManifest:
        return QStringLiteral("invalid-manifest");
    case HandshakeStatus::IdentityMismatch:
        return QStringLiteral("identity-mismatch");
    case HandshakeStatus::LaunchTokenMismatch:
        return QStringLiteral("launch-token-mismatch");
    case HandshakeStatus::ProtocolMismatch:
        return QStringLiteral("protocol-mismatch");
    case HandshakeStatus::AppletApiMismatch:
        return QStringLiteral("applet-api-mismatch");
    case HandshakeStatus::PolicyUnavailable:
        return QStringLiteral("policy-unavailable");
    }
    return {};
}

std::optional<HandshakeStatus> handshakeStatusFromString(const QString &value)
{
    constexpr HandshakeStatus statuses[] = {
        HandshakeStatus::Accepted,
        HandshakeStatus::InvalidManifest,
        HandshakeStatus::IdentityMismatch,
        HandshakeStatus::LaunchTokenMismatch,
        HandshakeStatus::ProtocolMismatch,
        HandshakeStatus::AppletApiMismatch,
        HandshakeStatus::PolicyUnavailable,
    };
    for (const HandshakeStatus status : statuses) {
        if (value == toString(status)) {
            return status;
        }
    }
    return std::nullopt;
}

} // namespace QindaQt::AppletHost
