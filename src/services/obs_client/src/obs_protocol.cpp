// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_protocol.h>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace QindaQt::Obs::Protocol {
namespace {

// A frame OBS never sends is not worth parsing, and an unbounded one is not
// worth holding: both are protocol faults.
constexpr int MaxFrameBytes = 4 * 1024 * 1024;
constexpr int MaxScenes = 512;
constexpr int MaxInputs = 512;
constexpr int MaxMappingEntries = 256;

QString stringOr(const QJsonObject &object, const char *key,
                 const QString &fallback = {}) {
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

bool boolOr(const QJsonObject &object, const char *key, bool fallback = false) {
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

// -1 is this client's "OBS did not report it"; a present-but-not-a-number
// field is treated the same way rather than coerced to zero.
qint64 numberOr(const QJsonObject &object, const char *key,
                qint64 fallback = -1) {
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? qint64(value.toDouble()) : fallback;
}

std::optional<Hello> decodeHello(const QJsonObject &d) {
    Hello hello;
    hello.obsWebSocketVersion = stringOr(d, "obsWebSocketVersion");
    const QJsonValue rpc = d.value(QStringLiteral("rpcVersion"));
    if (!rpc.isDouble()) {
        return std::nullopt;
    }
    hello.rpcVersion = rpc.toInt();
    const QJsonValue authentication = d.value(QStringLiteral("authentication"));
    if (authentication.isObject()) {
        const QJsonObject object = authentication.toObject();
        hello.challenge = stringOr(object, "challenge");
        hello.salt = stringOr(object, "salt");
        // Half an authentication block is not an authentication block.
        if (hello.challenge.isEmpty() != hello.salt.isEmpty()) {
            return std::nullopt;
        }
    } else if (!authentication.isUndefined() && !authentication.isNull()) {
        return std::nullopt;
    }
    return hello;
}

std::optional<RequestResponse> decodeResponse(const QJsonObject &d) {
    RequestResponse response;
    response.requestType = stringOr(d, "requestType");
    response.requestId = stringOr(d, "requestId");
    // Without an id the reply cannot be matched to anything, which is worse
    // than no reply: it would resolve an unrelated pending request.
    if (response.requestId.isEmpty()) {
        return std::nullopt;
    }
    const QJsonValue status = d.value(QStringLiteral("requestStatus"));
    if (!status.isObject()) {
        return std::nullopt;
    }
    const QJsonObject statusObject = status.toObject();
    const QJsonValue result = statusObject.value(QStringLiteral("result"));
    if (!result.isBool()) {
        return std::nullopt;
    }
    response.ok = result.toBool();
    response.code = int(numberOr(statusObject, "code", 0));
    response.comment = stringOr(statusObject, "comment");
    response.data = d.value(QStringLiteral("responseData")).toObject();
    return response;
}

std::optional<Event> decodeEvent(const QJsonObject &d) {
    Event event;
    event.eventType = stringOr(d, "eventType");
    if (event.eventType.isEmpty()) {
        return std::nullopt;
    }
    event.data = d.value(QStringLiteral("eventData")).toObject();
    return event;
}

ConsoleSourceMapping mappingEntryFrom(const QJsonObject &entry) {
    ConsoleSourceMapping mapping;
    mapping.consoleId = stringOr(entry, "consoleId");
    mapping.code = stringOr(entry, "code");
    mapping.label = stringOr(entry, "label");
    mapping.sourceName = stringOr(entry, "sourceName");
    mapping.sourceKind = stringOr(entry, "sourceKind");
    mapping.captureKind = stringOr(entry, "captureKind");
    mapping.captureDevice = stringOr(entry, "captureDevice");
    mapping.muted = boolOr(entry, "muted");
    const QJsonValue gain = entry.value(QStringLiteral("gainDb"));
    mapping.gainDb = gain.isDouble() ? gain.toDouble() : 0.0;
    return mapping;
}

QList<ConsoleSourceMapping> mappingArrayFrom(const QJsonObject &data,
                                             const char *key) {
    QList<ConsoleSourceMapping> entries;
    const QJsonArray array = data.value(QLatin1String(key)).toArray();
    for (const QJsonValue &value : array) {
        if (!value.isObject() || entries.size() >= MaxMappingEntries) {
            continue;
        }
        const ConsoleSourceMapping entry = mappingEntryFrom(value.toObject());
        // An entry with no console id names nothing the route could act on.
        if (entry.consoleId.isEmpty()) {
            continue;
        }
        entries.append(entry);
    }
    return entries;
}

QString sha256Base64(const QByteArray &input) {
    return QString::fromLatin1(
        QCryptographicHash::hash(input, QCryptographicHash::Sha256)
            .toBase64());
}

} // namespace

std::optional<Frame> decodeFrame(const QString &text) {
    if (text.isEmpty() || text.size() > MaxFrameBytes) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument document =
        QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonValue op = root.value(QStringLiteral("op"));
    const QJsonValue payload = root.value(QStringLiteral("d"));
    if (!op.isDouble() || !payload.isObject()) {
        return std::nullopt;
    }
    const QJsonObject d = payload.toObject();
    Frame frame;
    switch (op.toInt()) {
    case int(OpCode::Hello): {
        const auto hello = decodeHello(d);
        if (!hello.has_value()) {
            return std::nullopt;
        }
        frame.op = OpCode::Hello;
        frame.hello = *hello;
        return frame;
    }
    case int(OpCode::Identified): {
        const QJsonValue negotiated =
            d.value(QStringLiteral("negotiatedRpcVersion"));
        if (!negotiated.isDouble()) {
            return std::nullopt;
        }
        frame.op = OpCode::Identified;
        frame.negotiatedRpcVersion = negotiated.toInt();
        return frame;
    }
    case int(OpCode::Event): {
        const auto event = decodeEvent(d);
        if (!event.has_value()) {
            return std::nullopt;
        }
        frame.op = OpCode::Event;
        frame.event = *event;
        return frame;
    }
    case int(OpCode::RequestResponse): {
        const auto response = decodeResponse(d);
        if (!response.has_value()) {
            return std::nullopt;
        }
        frame.op = OpCode::RequestResponse;
        frame.response = *response;
        return frame;
    }
    default:
        // AGENT-GUARD: An opcode this client does not speak is a fault, not
        // something to ignore quietly. RequestBatch responses land here
        // because this client never sends a batch.
        return std::nullopt;
    }
}

QString authenticationString(const QString &password, const QString &salt,
                             const QString &challenge) {
    if (password.isEmpty()) {
        return {};
    }
    const QString secret = sha256Base64(password.toUtf8() + salt.toUtf8());
    return sha256Base64(secret.toUtf8() + challenge.toUtf8());
}

QString encodeIdentify(const Hello &hello, const QString &password,
                       int eventSubscriptions) {
    QJsonObject d{
        {QStringLiteral("rpcVersion"), RpcVersion},
        {QStringLiteral("eventSubscriptions"), eventSubscriptions},
    };
    if (hello.requiresAuthentication()) {
        const QString authentication =
            authenticationString(password, hello.salt, hello.challenge);
        // An empty string here is an identify with no credential, which the
        // server refuses with its own message. Sending nothing at all would
        // make the refusal look like a transport fault instead.
        d.insert(QStringLiteral("authentication"), authentication);
    }
    const QJsonObject root{{QStringLiteral("op"), int(OpCode::Identify)},
                           {QStringLiteral("d"), d}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString encodeRequest(const QString &requestType, const QString &requestId,
                      const QJsonObject &requestData) {
    QJsonObject d{
        {QStringLiteral("requestType"), requestType},
        {QStringLiteral("requestId"), requestId},
    };
    if (!requestData.isEmpty()) {
        d.insert(QStringLiteral("requestData"), requestData);
    }
    const QJsonObject root{{QStringLiteral("op"), int(OpCode::Request)},
                           {QStringLiteral("d"), d}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString encodeVendorRequest(const QString &vendorName,
                            const QString &vendorRequestType,
                            const QString &requestId,
                            const QJsonObject &requestData) {
    const QJsonObject inner{
        {QStringLiteral("vendorName"), vendorName},
        {QStringLiteral("requestType"), vendorRequestType},
        {QStringLiteral("requestData"), requestData},
    };
    return encodeRequest(QString::fromLatin1(Requests::CallVendorRequest),
                         requestId, inner);
}

std::optional<QJsonObject> unwrapVendorPayload(const QJsonObject &envelope,
                                               const QString &vendorName,
                                               const QString &expectedType) {
    if (stringOr(envelope, "vendorName") != vendorName) {
        return std::nullopt;
    }
    // A vendor response names the request type; a VendorEvent names the
    // event type. Both live under the same key.
    const QString type = envelope.contains(QStringLiteral("requestType"))
                             ? stringOr(envelope, "requestType")
                             : stringOr(envelope, "eventType");
    if (type != expectedType) {
        return std::nullopt;
    }
    const QJsonValue payload =
        envelope.contains(QStringLiteral("responseData"))
            ? envelope.value(QStringLiteral("responseData"))
            : envelope.value(QStringLiteral("eventData"));
    if (!payload.isObject()) {
        return std::nullopt;
    }
    return payload.toObject();
}

OutputStatus recordStatusFrom(const QJsonObject &data) {
    OutputStatus status;
    status.active = boolOr(data, "outputActive");
    status.paused = boolOr(data, "outputPaused");
    status.durationMs = numberOr(data, "outputDuration");
    return status;
}

OutputStatus streamStatusFrom(const QJsonObject &data) {
    OutputStatus status;
    status.active = boolOr(data, "outputActive");
    status.reconnecting = boolOr(data, "outputReconnecting");
    status.durationMs = numberOr(data, "outputDuration");
    status.skippedFrames = numberOr(data, "outputSkippedFrames");
    status.totalFrames = numberOr(data, "outputTotalFrames");
    return status;
}

OutputStatus virtualCamStatusFrom(const QJsonObject &data) {
    OutputStatus status;
    status.active = boolOr(data, "outputActive");
    return status;
}

SceneList sceneListFrom(const QJsonObject &data) {
    SceneList list;
    list.currentProgramScene = stringOr(data, "currentProgramSceneName");
    list.currentPreviewScene = stringOr(data, "currentPreviewSceneName");
    const QJsonArray scenes = data.value(QStringLiteral("scenes")).toArray();
    // OBS lists scenes in reverse UI order; the route shows them the way the
    // user sees them in OBS.
    QStringList names;
    for (const QJsonValue &value : scenes) {
        if (!value.isObject() || names.size() >= MaxScenes) {
            continue;
        }
        const QString name = stringOr(value.toObject(), "sceneName");
        if (!name.isEmpty()) {
            names.prepend(name);
        }
    }
    list.names = names;
    return list;
}

QList<AudioInput> audioInputsFrom(const QJsonObject &data) {
    QList<AudioInput> inputs;
    const QJsonArray array = data.value(QStringLiteral("inputs")).toArray();
    for (const QJsonValue &value : array) {
        if (!value.isObject() || inputs.size() >= MaxInputs) {
            continue;
        }
        const QJsonObject object = value.toObject();
        AudioInput input;
        input.name = stringOr(object, "inputName");
        input.inputKind = stringOr(object, "inputKind");
        if (input.name.isEmpty()) {
            continue;
        }
        inputs.append(input);
    }
    return inputs;
}

ConsoleMapping consoleMappingFrom(const QJsonObject &data) {
    ConsoleMapping mapping;
    const QJsonValue version = data.value(QStringLiteral("bridgeVersion"));
    // AGENT-GUARD: Without a bridge version this is not the QindaQt bridge's
    // payload. Leaving bridgeVersion at 0 is what lets the route say "the
    // plugin is missing" rather than showing an empty table as if the
    // console had no buses.
    if (!version.isDouble() || version.toInt() <= 0) {
        return mapping;
    }
    mapping.bridgeVersion = version.toInt();
    mapping.buses = mappingArrayFrom(data, "buses");
    mapping.strips = mappingArrayFrom(data, "strips");
    mapping.audioState = stringOr(data, "audioState");
    mapping.reasonCode = stringOr(data, "reasonCode");
    const QJsonValue epoch = data.value(QStringLiteral("epoch"));
    mapping.epoch = epoch.isDouble() ? quint64(epoch.toDouble()) : 0;
    const QJsonValue revision = data.value(QStringLiteral("revision"));
    mapping.revision = revision.isDouble() ? quint64(revision.toDouble()) : 0;
    return mapping;
}

} // namespace QindaQt::Obs::Protocol

namespace QindaQt::Obs {

QString connectionStateName(ConnectionState state) {
    switch (state) {
    case ConnectionState::Connecting:
        return QStringLiteral("connecting");
    case ConnectionState::Authenticating:
        return QStringLiteral("authenticating");
    case ConnectionState::Ready:
        return QStringLiteral("ready");
    case ConnectionState::Degraded:
        return QStringLiteral("degraded");
    case ConnectionState::Disconnected:
        break;
    }
    return QStringLiteral("disconnected");
}

QString outputKindName(OutputKind kind) {
    switch (kind) {
    case OutputKind::Stream:
        return QStringLiteral("stream");
    case OutputKind::VirtualCam:
        return QStringLiteral("virtual-camera");
    case OutputKind::Record:
        break;
    }
    return QStringLiteral("record");
}

} // namespace QindaQt::Obs
