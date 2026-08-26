// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/controlendpoint.h"

#include "qindaqt/compositor/containercontrolbridge.h"
#include "qindaqt/compositor/controlcodec.h"
#include "qindaqt/compositor/controllimits.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace QindaQt::Compositor {

ControlEndpoint::ControlEndpoint(ContainerControlBridge &bridge, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
{
    connect(&bridge, &ContainerControlBridge::containerCommitted, this,
            [this](const QString &containerId, quint64 revision, const QJsonObject &snapshot) {
                const auto protocol = ControlCodec::capabilities().value(QStringLiteral("protocol"));
                const QJsonObject event{{QStringLiteral("protocol"), protocol},
                                        {QStringLiteral("event"),
                                         QStringLiteral("container-committed")},
                                        {QStringLiteral("containerId"), containerId},
                                        {QStringLiteral("revision"), QString::number(revision)},
                                        {QStringLiteral("snapshot"), snapshot}};
                Q_EMIT ContainerCommitted(ControlCodec::compactJson(event));
            });
}

QByteArray ControlEndpoint::Capabilities() const
{
    return ControlCodec::compactJson(ControlCodec::capabilities());
}

QByteArray ControlEndpoint::Snapshot(const QString &containerId) const
{
    if (containerId.size() > ControlLimits::MaxIdentifierCharacters) {
        const ControlReply reply{{}, {}, {}, ReplyStatus::Rejected, 0, {},
                                 {QStringLiteral("request-too-large"),
                                  QStringLiteral("containerId exceeds the identifier limit"),
                                  -1}};
        return ControlCodec::compactJson(ControlCodec::replyToJson(reply));
    }
    const auto snapshot = m_bridge.snapshot(containerId);
    const auto revision = m_bridge.revision(containerId);
    if (!snapshot || !revision) {
        const ControlReply reply{{}, {}, containerId, ReplyStatus::Rejected, 0, {},
                                 {QStringLiteral("unknown-container"),
                                  QStringLiteral("unknown container '%1'").arg(containerId),
                                  -1}};
        return ControlCodec::compactJson(ControlCodec::replyToJson(reply));
    }
    const auto protocol = ControlCodec::capabilities().value(QStringLiteral("protocol"));
    return ControlCodec::compactJson(
        {{QStringLiteral("protocol"), protocol},
         {QStringLiteral("containerId"), containerId},
         {QStringLiteral("status"), QStringLiteral("ok")},
         {QStringLiteral("revision"), QString::number(*revision)},
         {QStringLiteral("snapshot"), *snapshot}});
}

QByteArray ControlEndpoint::Submit(const QByteArray &requestJson)
{
    if (requestJson.size() > ControlLimits::MaxRequestBytes) {
        const ControlReply reply{{}, {}, {}, ReplyStatus::Rejected, 0, {},
                                 {QStringLiteral("request-too-large"),
                                  QStringLiteral("request exceeds the %1-byte limit")
                                      .arg(ControlLimits::MaxRequestBytes),
                                  -1}};
        return ControlCodec::compactJson(ControlCodec::replyToJson(reply));
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(requestJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const ControlReply reply{{}, {}, {}, ReplyStatus::Rejected, 0, {},
                                 {QStringLiteral("malformed-json"),
                                  parseError.error == QJsonParseError::NoError
                                      ? QStringLiteral("request must be a JSON object")
                                      : parseError.errorString(),
                                  -1}};
        return ControlCodec::compactJson(ControlCodec::replyToJson(reply));
    }

    ControlFailure failure;
    const auto request = ControlCodec::parseRequest(document.object(), &failure);
    if (!request) {
        const auto object = document.object();
        const ControlReply reply{{},
                                 object.value(QStringLiteral("transactionId")).toString(),
                                 object.value(QStringLiteral("containerId")).toString(),
                                 ReplyStatus::Rejected,
                                 0,
                                 {},
                                 failure};
        return ControlCodec::compactJson(ControlCodec::replyToJson(reply));
    }
    return ControlCodec::compactJson(ControlCodec::replyToJson(m_bridge.submit(*request)));
}

} // namespace QindaQt::Compositor
