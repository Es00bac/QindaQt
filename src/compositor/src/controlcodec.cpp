// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/controlcodec.h"

#include "qindaqt/compositor/controllimits.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <limits>

namespace QindaQt::Compositor {
namespace {

void fail(ControlFailure *failure, QString code, QString message)
{
    if (failure) {
        *failure = {std::move(code), std::move(message), -1};
    }
}

std::optional<quint16> versionPart(const QJsonObject &protocol,
                                  QLatin1StringView key,
                                  ControlFailure *failure)
{
    const auto value = protocol.value(key);
    if (!value.isDouble()) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("protocol.%1 must be an integer").arg(key));
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (number < 0 || number > std::numeric_limits<quint16>::max()
        || number != static_cast<double>(static_cast<quint16>(number))) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("protocol.%1 is outside the supported integer range").arg(key));
        return std::nullopt;
    }
    return static_cast<quint16>(number);
}

QString statusName(ReplyStatus status)
{
    switch (status) {
    case ReplyStatus::Committed:
        return QStringLiteral("committed");
    case ReplyStatus::Conflict:
        return QStringLiteral("conflict");
    case ReplyStatus::Rejected:
        return QStringLiteral("rejected");
    }
    Q_UNREACHABLE_RETURN(QStringLiteral("rejected"));
}

QJsonObject versionObject(ProtocolVersion version)
{
    return {{QStringLiteral("major"), version.major},
            {QStringLiteral("minor"), version.minor}};
}

} // namespace

std::optional<ControlRequest> ControlCodec::parseRequest(const QJsonObject &object,
                                                         ControlFailure *failure)
{
    const auto protocolValue = object.value(QStringLiteral("protocol"));
    if (!protocolValue.isObject()) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("protocol must be an object"));
        return std::nullopt;
    }
    const auto protocol = protocolValue.toObject();
    const auto major = versionPart(protocol, QLatin1StringView("major"), failure);
    const auto minor = versionPart(protocol, QLatin1StringView("minor"), failure);
    if (!major || !minor) {
        return std::nullopt;
    }

    const auto transactionId = object.value(QStringLiteral("transactionId"));
    const auto containerId = object.value(QStringLiteral("containerId"));
    const auto revision = object.value(QStringLiteral("expectedRevision"));
    const auto operations = object.value(QStringLiteral("operations"));
    if (!transactionId.isString() || transactionId.toString().isEmpty()
        || !containerId.isString() || containerId.toString().isEmpty()) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("transactionId and containerId must be non-empty strings"));
        return std::nullopt;
    }
    if (transactionId.toString().size() > ControlLimits::MaxIdentifierCharacters
        || containerId.toString().size() > ControlLimits::MaxIdentifierCharacters) {
        fail(failure, QStringLiteral("request-too-large"),
             QStringLiteral("transactionId and containerId are limited to %1 characters")
                 .arg(ControlLimits::MaxIdentifierCharacters));
        return std::nullopt;
    }
    // AGENT-CONTRACT: Revisions are decimal strings on the wire. JSON numbers
    // cannot exactly represent every quint64 used by long-running sessions.
    const auto revisionText = revision.toString();
    const bool decimalOnly = !revisionText.isEmpty()
        && std::all_of(revisionText.cbegin(), revisionText.cend(), [](QChar character) {
               return character >= QLatin1Char('0') && character <= QLatin1Char('9');
           });
    bool revisionOk = false;
    const auto parsedRevision = revision.isString() && decimalOnly
        ? revisionText.toULongLong(&revisionOk)
        : 0;
    if (!revisionOk) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("expectedRevision must be an unsigned decimal string"));
        return std::nullopt;
    }
    if (!operations.isArray() || operations.toArray().isEmpty()) {
        fail(failure, QStringLiteral("malformed-request"),
             QStringLiteral("operations must be a non-empty array"));
        return std::nullopt;
    }
    if (operations.toArray().size() > ControlLimits::MaxOperations) {
        fail(failure, QStringLiteral("request-too-large"),
             QStringLiteral("operations is limited to %1 entries")
                 .arg(ControlLimits::MaxOperations));
        return std::nullopt;
    }

    QVector<QJsonObject> parsedOperations;
    const auto array = operations.toArray();
    parsedOperations.reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array[index].isObject()) {
            fail(failure, QStringLiteral("malformed-request"),
                 QStringLiteral("operations[%1] must be an object").arg(index));
            if (failure) {
                failure->operationIndex = index;
            }
            return std::nullopt;
        }
        parsedOperations.append(array[index].toObject());
    }
    return ControlRequest{{*major, *minor},
                          transactionId.toString(),
                          containerId.toString(),
                          parsedRevision,
                          std::move(parsedOperations)};
}

QJsonObject ControlCodec::replyToJson(const ControlReply &reply)
{
    QJsonObject object{{QStringLiteral("protocol"), versionObject(reply.protocol)},
                       {QStringLiteral("transactionId"), reply.transactionId},
                       {QStringLiteral("containerId"), reply.containerId},
                       {QStringLiteral("status"), statusName(reply.status)},
                       {QStringLiteral("revision"), QString::number(reply.revision)}};
    if (!reply.snapshot.isEmpty()) {
        object.insert(QStringLiteral("snapshot"), reply.snapshot);
    }
    if (reply.status != ReplyStatus::Committed) {
        QJsonObject failure{{QStringLiteral("code"), reply.failure.code},
                            {QStringLiteral("message"), reply.failure.message}};
        if (reply.failure.operationIndex >= 0) {
            failure.insert(QStringLiteral("operationIndex"), reply.failure.operationIndex);
        }
        object.insert(QStringLiteral("failure"), failure);
    }
    return object;
}

QJsonObject ControlCodec::capabilities()
{
    const QJsonArray operations{
        QStringLiteral("activate-page"),
        QStringLiteral("add-page"),
        QStringLiteral("detach-window"),
        QStringLiteral("move-page"),
        QStringLiteral("set-split-ratio"),
        QStringLiteral("split-window"),
        QStringLiteral("swap-windows"),
    };
    return {{QStringLiteral("interface"), QStringLiteral("org.qindaqt.Compositor1")},
            {QStringLiteral("protocol"), versionObject({})},
            {QStringLiteral("transactional"), true},
            {QStringLiteral("revisionEncoding"), QStringLiteral("unsigned-decimal-string")},
            {QStringLiteral("limits"),
             QJsonObject{{QStringLiteral("maxRequestBytes"), ControlLimits::MaxRequestBytes},
                         {QStringLiteral("maxOperations"), ControlLimits::MaxOperations},
                         {QStringLiteral("maxIdentifierCharacters"),
                          ControlLimits::MaxIdentifierCharacters}}},
            {QStringLiteral("methods"),
             QJsonArray{QStringLiteral("Capabilities"), QStringLiteral("Snapshot"),
                        QStringLiteral("Submit")}},
            {QStringLiteral("events"), QJsonArray{QStringLiteral("ContainerCommitted")}},
            {QStringLiteral("operations"), operations}};
}

QByteArray ControlCodec::compactJson(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace QindaQt::Compositor
