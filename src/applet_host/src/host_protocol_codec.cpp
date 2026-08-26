// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/host_protocol_codec.h"

#include "qindaqt/applets/manifest_types.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace QindaQt::AppletHost {
namespace {

bool decodeObject(const QByteArray &message, QJsonObject *object, QString *error)
{
    if (message.size() > HostProtocolCodec::MaximumMessageBytes) {
        *error = QStringLiteral("Protocol message exceeds the 64 KiB limit");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("Invalid protocol JSON: %1").arg(parseError.errorString());
        return false;
    }
    *object = document.object();
    return true;
}

QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject decisionToJson(const CapabilityDecision &decision)
{
    return {{QStringLiteral("capability"),
             QindaQt::Applets::toString(decision.capability)},
            {QStringLiteral("decision"), toString(decision.disposition)},
            {QStringLiteral("basis"), toString(decision.basis)},
            {QStringLiteral("reason"), decision.reason}};
}

} // namespace

QByteArray HostProtocolCodec::encodeHello(const HostHello &hello)
{
    const QByteArray token = hello.launchToken.toBase64(QByteArray::Base64UrlEncoding
                                                        | QByteArray::OmitTrailingEquals);
    return compact({{QStringLiteral("type"), QStringLiteral("hello")},
                    {QStringLiteral("protocolVersion"), hello.protocolVersion.toString()},
                    {QStringLiteral("appletApiVersion"), hello.appletApiVersion.toString()},
                    {QStringLiteral("manifestId"), hello.manifestId},
                    {QStringLiteral("launchToken"), QString::fromLatin1(token)}});
}

HelloDecodeResult HostProtocolCodec::decodeHello(const QByteArray &message)
{
    QJsonObject object;
    QString error;
    if (!decodeObject(message, &object, &error)) {
        return {.ok = false, .hello = {}, .error = error};
    }
    if (object.value(QStringLiteral("type")).toString() != QLatin1String("hello")) {
        return {.ok = false,
                .hello = {},
                .error = QStringLiteral("Protocol message is not a hello")};
    }

    const auto protocol = ProtocolVersion::parse(
        object.value(QStringLiteral("protocolVersion")).toString());
    const auto appletApi = QindaQt::Applets::ApiVersion::parse(
        object.value(QStringLiteral("appletApiVersion")).toString());
    const auto token = QByteArray::fromBase64Encoding(
        object.value(QStringLiteral("launchToken")).toString().toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    const QString manifestId = object.value(QStringLiteral("manifestId")).toString();
    if (!protocol.has_value() || !appletApi.has_value() || !token || (*token).isEmpty()
        || manifestId.isEmpty()) {
        return {.ok = false,
                .hello = {},
                .error = QStringLiteral("Hello contains an invalid version, identity, or launch token")};
    }
    return {.ok = true,
            .hello = {.protocolVersion = *protocol,
                      .appletApiVersion = *appletApi,
                      .manifestId = manifestId,
                      .launchToken = *token},
            .error = {}};
}

QByteArray HostProtocolCodec::encodeResponse(const HandshakeResponse &response)
{
    QJsonArray decisions;
    for (const CapabilityDecision &decision : response.capabilityDecisions) {
        decisions.append(decisionToJson(decision));
    }
    QJsonObject object{{QStringLiteral("type"), QStringLiteral("handshake-response")},
                       {QStringLiteral("status"), toString(response.status)},
                       {QStringLiteral("message"), response.message},
                       {QStringLiteral("capabilities"), decisions}};
    if (response.negotiatedProtocol.has_value()) {
        object.insert(QStringLiteral("protocolVersion"),
                      response.negotiatedProtocol->toString());
    }
    return compact(object);
}

ResponseDecodeResult HostProtocolCodec::decodeResponse(const QByteArray &message)
{
    QJsonObject object;
    QString error;
    if (!decodeObject(message, &object, &error)) {
        return {.ok = false, .response = {}, .error = error};
    }
    if (object.value(QStringLiteral("type")).toString()
        != QLatin1String("handshake-response")) {
        return {.ok = false,
                .response = {},
                .error = QStringLiteral("Protocol message is not a handshake response")};
    }
    const auto status = handshakeStatusFromString(
        object.value(QStringLiteral("status")).toString());
    if (!status.has_value()) {
        return {.ok = false,
                .response = {},
                .error = QStringLiteral("Handshake response status is unknown")};
    }

    std::optional<ProtocolVersion> protocol;
    const QJsonValue protocolValue = object.value(QStringLiteral("protocolVersion"));
    if (!protocolValue.isUndefined()) {
        protocol = ProtocolVersion::parse(protocolValue.toString());
        if (!protocol.has_value()) {
            return {.ok = false,
                    .response = {},
                    .error = QStringLiteral("Handshake response protocol version is invalid")};
        }
    }

    const QJsonValue decisionsValue = object.value(QStringLiteral("capabilities"));
    if (!decisionsValue.isArray()) {
        return {.ok = false,
                .response = {},
                .error = QStringLiteral("Handshake response capabilities must be an array")};
    }
    QVector<CapabilityDecision> decisions;
    for (const QJsonValue &value : decisionsValue.toArray()) {
        if (!value.isObject()) {
            return {.ok = false,
                    .response = {},
                    .error = QStringLiteral("Capability decision must be an object")};
        }
        const QJsonObject decision = value.toObject();
        const auto capability = QindaQt::Applets::capabilityFromString(
            decision.value(QStringLiteral("capability")).toString());
        const auto disposition = capabilityDispositionFromString(
            decision.value(QStringLiteral("decision")).toString());
        const auto basis = decisionBasisFromString(
            decision.value(QStringLiteral("basis")).toString());
        const QString reason = decision.value(QStringLiteral("reason")).toString();
        if (!capability.has_value() || !disposition.has_value() || !basis.has_value()
            || reason.isEmpty()) {
            return {.ok = false,
                    .response = {},
                    .error = QStringLiteral("Capability decision is malformed")};
        }
        decisions.append({*capability, *disposition, *basis, reason});
    }
    if (*status == HandshakeStatus::Accepted && !protocol.has_value()) {
        return {.ok = false,
                .response = {},
                .error = QStringLiteral("Accepted handshake response requires a protocol version")};
    }

    return {.ok = true,
            .response = {.status = *status,
                         .negotiatedProtocol = protocol,
                         .capabilityDecisions = decisions,
                         .message = object.value(QStringLiteral("message")).toString()},
            .error = {}};
}

} // namespace QindaQt::AppletHost
