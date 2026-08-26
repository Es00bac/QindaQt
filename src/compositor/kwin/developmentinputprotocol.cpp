// SPDX-License-Identifier: GPL-3.0-or-later
#include "developmentinputprotocol.h"

#include "qindaqt/compositor/controllimits.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto MalformedCode = "malformed-input-request";
constexpr auto MalformedMessage = "input request does not match schema version 1";
constexpr auto DisabledCode = "control-disabled";
constexpr auto DisabledMessage = "external compositor mutations are disabled";
constexpr auto UnavailableCode = "input-injection-unavailable";
constexpr auto UnavailableMessage = "development input injector is unavailable";

void setFailure(DevelopmentInputFailure *failure, QString code, QString message)
{
    if (failure) {
        *failure = {std::move(code), std::move(message)};
    }
}

bool hasExactlyFields(const QJsonObject &object, const QSet<QString> &fields)
{
    if (object.size() != fields.size()) {
        return false;
    }
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!fields.contains(iterator.key())) {
            return false;
        }
    }
    return true;
}

bool isBoundedCoordinate(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }
    const auto coordinate = value.toDouble();
    return std::isfinite(coordinate)
        && std::abs(coordinate) <= DevelopmentInputCodec::MaxLogicalCoordinateMagnitude;
}

std::optional<DevelopmentInputEvent> parseEvent(const QJsonValue &value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const auto object = value.toObject();
    const auto type = object.value(QStringLiteral("type"));
    if (!type.isString()) {
        return std::nullopt;
    }

    if (type.toString() == QStringLiteral("pointer-absolute")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("x"),
                                       QStringLiteral("y")})
            || !isBoundedCoordinate(object.value(QStringLiteral("x")))
            || !isBoundedCoordinate(object.value(QStringLiteral("y")))) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::PointerAbsolute;
        event.position = QPointF(object.value(QStringLiteral("x")).toDouble(),
                                 object.value(QStringLiteral("y")).toDouble());
        return event;
    }

    if (type.toString() == QStringLiteral("key")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("key"),
                                       QStringLiteral("pressed")})
            || !object.value(QStringLiteral("key")).isString()
            || !object.value(QStringLiteral("pressed")).isBool()) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::Key;
        event.pressed = object.value(QStringLiteral("pressed")).toBool();
        const auto key = object.value(QStringLiteral("key")).toString();
        if (key == QStringLiteral("left-meta")) {
            event.key = DevelopmentInputKey::LeftMeta;
        } else if (key == QStringLiteral("left-shift")) {
            event.key = DevelopmentInputKey::LeftShift;
        } else if (key == QStringLiteral("down")) {
            event.key = DevelopmentInputKey::Down;
        } else if (key == QStringLiteral("enter")) {
            event.key = DevelopmentInputKey::Enter;
        } else {
            return std::nullopt;
        }
        return event;
    }

    if (type.toString() == QStringLiteral("button")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("button"),
                                       QStringLiteral("pressed")})
            || !object.value(QStringLiteral("button")).isString()
            || !object.value(QStringLiteral("pressed")).isBool()) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::Button;
        event.pressed = object.value(QStringLiteral("pressed")).toBool();
        const auto button = object.value(QStringLiteral("button")).toString();
        if (button == QStringLiteral("left")) {
            event.button = DevelopmentInputButton::Left;
        } else if (button == QStringLiteral("right")) {
            event.button = DevelopmentInputButton::Right;
        } else {
            return std::nullopt;
        }
        return event;
    }

    return std::nullopt;
}

QByteArray response(QString status, QString code = {}, QString message = {},
                    std::optional<qsizetype> eventCount = std::nullopt)
{
    QJsonObject object{{QStringLiteral("status"), std::move(status)}};
    if (!code.isEmpty()) {
        object.insert(QStringLiteral("failure"),
                      QJsonObject{{QStringLiteral("code"), std::move(code)},
                                  {QStringLiteral("message"), std::move(message)}});
    }
    if (eventCount) {
        object.insert(QStringLiteral("eventCount"), static_cast<qint64>(*eventCount));
        object.insert(QStringLiteral("deviceId"), developmentInputDeviceId());
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

std::optional<DevelopmentInputBatch>
DevelopmentInputCodec::parse(const QByteArray &requestJson,
                             DevelopmentInputFailure *failure)
{
    if (requestJson.size() > ControlLimits::MaxRequestBytes) {
        setFailure(failure, QStringLiteral("request-too-large"),
                   QStringLiteral("input request exceeds the %1-byte limit")
                       .arg(ControlLimits::MaxRequestBytes));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(requestJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setFailure(failure, QString::fromLatin1(MalformedCode),
                   QString::fromLatin1(MalformedMessage));
        return std::nullopt;
    }
    const auto object = document.object();
    if (!hasExactlyFields(object, {QStringLiteral("schemaVersion"),
                                   QStringLiteral("events")})
        || !object.value(QStringLiteral("schemaVersion")).isDouble()
        || object.value(QStringLiteral("schemaVersion")).toInteger(-1) != SchemaVersion
        || !object.value(QStringLiteral("events")).isArray()) {
        setFailure(failure, QString::fromLatin1(MalformedCode),
                   QString::fromLatin1(MalformedMessage));
        return std::nullopt;
    }

    const auto array = object.value(QStringLiteral("events")).toArray();
    if (array.size() > MaxEvents) {
        setFailure(failure, QStringLiteral("request-too-large"),
                   QStringLiteral("input request may contain at most %1 events")
                       .arg(MaxEvents));
        return std::nullopt;
    }
    if (array.isEmpty()) {
        setFailure(failure, QString::fromLatin1(MalformedCode),
                   QString::fromLatin1(MalformedMessage));
        return std::nullopt;
    }

    DevelopmentInputBatch batch;
    batch.events.reserve(array.size());
    for (const auto &value : array) {
        auto event = parseEvent(value);
        if (!event) {
            setFailure(failure, QString::fromLatin1(MalformedCode),
                       QString::fromLatin1(MalformedMessage));
            return std::nullopt;
        }
        batch.events.append(*event);
    }
    return batch;
}

DevelopmentInputController::DevelopmentInputController(bool mutationsEnabled,
                                                       DevelopmentInputSink *sink)
    : m_mutationsEnabled(mutationsEnabled)
    , m_sink(sink)
{
}

QByteArray DevelopmentInputController::injectTestInput(
    const QByteArray &requestJson) const
{
    // AGENT-GUARD: This test-only method is public on an unauthenticated user
    // bus. Production must reject before payload size, syntax, or schema can
    // affect the reply, keeping the disabled surface inert and non-oracular.
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QString::fromLatin1(DisabledCode),
                        QString::fromLatin1(DisabledMessage));
    }
    if (requestJson.size() > ControlLimits::MaxRequestBytes) {
        return response(QStringLiteral("rejected"), QStringLiteral("request-too-large"),
                        QStringLiteral("input request exceeds the %1-byte limit")
                            .arg(ControlLimits::MaxRequestBytes));
    }

    DevelopmentInputFailure failure;
    const auto batch = DevelopmentInputCodec::parse(requestJson, &failure);
    if (!batch) {
        return response(QStringLiteral("rejected"), failure.code, failure.message);
    }
    if (!m_sink || !m_sink->isAvailable() || !m_sink->inject(*batch)) {
        return response(QStringLiteral("rejected"), QString::fromLatin1(UnavailableCode),
                        QString::fromLatin1(UnavailableMessage));
    }
    return response(QStringLiteral("injected"), {}, {}, batch->events.size());
}

QJsonObject DevelopmentInputController::capabilities() const
{
    return {{QStringLiteral("enabled"), m_mutationsEnabled},
            {QStringLiteral("available"),
             m_mutationsEnabled && m_sink && m_sink->isAvailable()},
            {QStringLiteral("schemaVersion"), DevelopmentInputCodec::SchemaVersion},
            {QStringLiteral("maxEvents"),
             static_cast<qint64>(DevelopmentInputCodec::MaxEvents)},
            {QStringLiteral("maxLogicalCoordinateMagnitude"),
             DevelopmentInputCodec::MaxLogicalCoordinateMagnitude},
            {QStringLiteral("deviceId"), developmentInputDeviceId()},
            {QStringLiteral("eventTypes"),
             QJsonArray{QStringLiteral("pointer-absolute"), QStringLiteral("key"),
                        QStringLiteral("button")}}};
}

QString developmentInputDeviceId()
{
    return QStringLiteral("qindaqt-development-input");
}

} // namespace QindaQt::Compositor::KWinIntegration
