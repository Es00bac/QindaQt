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

bool isBoundedRelativeDelta(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }
    const auto delta = value.toDouble();
    return std::isfinite(delta)
        && std::abs(delta) <= DevelopmentInputCodec::MaxRelativeDeltaMagnitude;
}

bool isTouchId(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }
    const double id = value.toDouble();
    return id >= 0.0 && id <= DevelopmentInputCodec::MaxTouchId && id == std::floor(id);
}

struct KeyName final
{
    QLatin1StringView name;
    DevelopmentInputKey key;
};

// AGENT-CONTRACT: These names are the wire grammar of InjectTestInput; the
// evdev mapping lives in kwindevelopmentinputinjector.cpp. Add a key to both.
constexpr KeyName kKeyNames[] = {
    {QLatin1StringView("left-meta"), DevelopmentInputKey::LeftMeta},
    {QLatin1StringView("left-alt"), DevelopmentInputKey::LeftAlt},
    {QLatin1StringView("left-control"), DevelopmentInputKey::LeftControl},
    {QLatin1StringView("left-shift"), DevelopmentInputKey::LeftShift},
    {QLatin1StringView("f1"), DevelopmentInputKey::F1},
    {QLatin1StringView("f11"), DevelopmentInputKey::F11},
    {QLatin1StringView("tab"), DevelopmentInputKey::Tab},
    {QLatin1StringView("escape"), DevelopmentInputKey::Escape},
    {QLatin1StringView("space"), DevelopmentInputKey::Space},
    {QLatin1StringView("up"), DevelopmentInputKey::Up},
    {QLatin1StringView("down"), DevelopmentInputKey::Down},
    {QLatin1StringView("left"), DevelopmentInputKey::Left},
    {QLatin1StringView("right"), DevelopmentInputKey::Right},
    {QLatin1StringView("enter"), DevelopmentInputKey::Enter},
    {QLatin1StringView("volume-up"), DevelopmentInputKey::VolumeUp},
    {QLatin1StringView("print"), DevelopmentInputKey::Print},
    {QLatin1StringView("a"), DevelopmentInputKey::A},
    {QLatin1StringView("b"), DevelopmentInputKey::B},
    {QLatin1StringView("c"), DevelopmentInputKey::C},
    {QLatin1StringView("d"), DevelopmentInputKey::D},
    {QLatin1StringView("e"), DevelopmentInputKey::E},
    {QLatin1StringView("f"), DevelopmentInputKey::F},
    {QLatin1StringView("g"), DevelopmentInputKey::G},
    {QLatin1StringView("h"), DevelopmentInputKey::H},
    {QLatin1StringView("i"), DevelopmentInputKey::I},
    {QLatin1StringView("j"), DevelopmentInputKey::J},
    {QLatin1StringView("k"), DevelopmentInputKey::K},
    {QLatin1StringView("l"), DevelopmentInputKey::L},
    {QLatin1StringView("m"), DevelopmentInputKey::M},
    {QLatin1StringView("n"), DevelopmentInputKey::N},
    {QLatin1StringView("o"), DevelopmentInputKey::O},
    {QLatin1StringView("p"), DevelopmentInputKey::P},
    {QLatin1StringView("q"), DevelopmentInputKey::Q},
    {QLatin1StringView("r"), DevelopmentInputKey::R},
    {QLatin1StringView("s"), DevelopmentInputKey::S},
    {QLatin1StringView("t"), DevelopmentInputKey::T},
    {QLatin1StringView("u"), DevelopmentInputKey::U},
    {QLatin1StringView("v"), DevelopmentInputKey::V},
    {QLatin1StringView("w"), DevelopmentInputKey::W},
    {QLatin1StringView("x"), DevelopmentInputKey::X},
    {QLatin1StringView("y"), DevelopmentInputKey::Y},
    {QLatin1StringView("z"), DevelopmentInputKey::Z},
    {QLatin1StringView("0"), DevelopmentInputKey::D0},
    {QLatin1StringView("1"), DevelopmentInputKey::D1},
    {QLatin1StringView("2"), DevelopmentInputKey::D2},
    {QLatin1StringView("3"), DevelopmentInputKey::D3},
    {QLatin1StringView("4"), DevelopmentInputKey::D4},
    {QLatin1StringView("5"), DevelopmentInputKey::D5},
    {QLatin1StringView("6"), DevelopmentInputKey::D6},
    {QLatin1StringView("7"), DevelopmentInputKey::D7},
    {QLatin1StringView("8"), DevelopmentInputKey::D8},
    {QLatin1StringView("9"), DevelopmentInputKey::D9},
};

[[nodiscard]] std::optional<DevelopmentInputKey> keyFromName(const QString &name)
{
    for (const auto &entry : kKeyNames) {
        if (name == entry.name) {
            return entry.key;
        }
    }
    return std::nullopt;
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

    if (type.toString() == QStringLiteral("pointer-relative")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("dx"),
                                       QStringLiteral("dy")})
            || !isBoundedRelativeDelta(object.value(QStringLiteral("dx")))
            || !isBoundedRelativeDelta(object.value(QStringLiteral("dy")))) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::PointerRelative;
        event.position = QPointF(object.value(QStringLiteral("dx")).toDouble(),
                                 object.value(QStringLiteral("dy")).toDouble());
        return event;
    }

    if (type.toString() == QStringLiteral("touch-down")
        || type.toString() == QStringLiteral("touch-motion")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("id"),
                                       QStringLiteral("x"), QStringLiteral("y")})
            || !isTouchId(object.value(QStringLiteral("id")))
            || !isBoundedCoordinate(object.value(QStringLiteral("x")))
            || !isBoundedCoordinate(object.value(QStringLiteral("y")))) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = type.toString() == QStringLiteral("touch-down")
            ? DevelopmentInputEventType::TouchDown : DevelopmentInputEventType::TouchMotion;
        event.touchId = object.value(QStringLiteral("id")).toInt();
        event.position = QPointF(object.value(QStringLiteral("x")).toDouble(),
                                 object.value(QStringLiteral("y")).toDouble());
        return event;
    }

    if (type.toString() == QStringLiteral("touch-up")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("id")})
            || !isTouchId(object.value(QStringLiteral("id")))) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::TouchUp;
        event.touchId = object.value(QStringLiteral("id")).toInt();
        return event;
    }

    if (type.toString() == QStringLiteral("pointer-axis")) {
        if (!hasExactlyFields(object, {QStringLiteral("type"), QStringLiteral("axis"),
                                       QStringLiteral("delta")})
            || !object.value(QStringLiteral("axis")).isString()
            || !object.value(QStringLiteral("delta")).isDouble()) {
            return std::nullopt;
        }
        const auto delta = object.value(QStringLiteral("delta")).toDouble();
        if (!std::isfinite(delta) || qFuzzyIsNull(delta)
            || std::abs(delta) > DevelopmentInputCodec::MaxAxisDeltaMagnitude) {
            return std::nullopt;
        }
        DevelopmentInputEvent event;
        event.type = DevelopmentInputEventType::PointerAxis;
        event.axisDelta = delta;
        const auto axis = object.value(QStringLiteral("axis")).toString();
        if (axis == QStringLiteral("vertical")) {
            event.axis = DevelopmentInputAxis::Vertical;
        } else if (axis == QStringLiteral("horizontal")) {
            event.axis = DevelopmentInputAxis::Horizontal;
        } else {
            return std::nullopt;
        }
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
        const auto parsedKey = keyFromName(key);
        if (!parsedKey) {
            return std::nullopt;
        }
        event.key = *parsedKey;
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
            {QStringLiteral("maxRelativeDeltaMagnitude"),
             DevelopmentInputCodec::MaxRelativeDeltaMagnitude},
            {QStringLiteral("maxAxisDeltaMagnitude"),
             DevelopmentInputCodec::MaxAxisDeltaMagnitude},
            {QStringLiteral("deviceId"), developmentInputDeviceId()},
            {QStringLiteral("eventTypes"),
             QJsonArray{QStringLiteral("pointer-absolute"),
                        QStringLiteral("pointer-relative"), QStringLiteral("key"),
                        QStringLiteral("button"),
                        QStringLiteral("touch-down"), QStringLiteral("touch-motion"),
                        QStringLiteral("touch-up"), QStringLiteral("pointer-axis")}}};
}

QString developmentInputDeviceId()
{
    return QStringLiteral("qindaqt-development-input");
}

} // namespace QindaQt::Compositor::KWinIntegration
