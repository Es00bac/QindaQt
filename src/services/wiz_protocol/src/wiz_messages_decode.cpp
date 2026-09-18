// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_messages.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>

#include <algorithm>

namespace QindaQt::Wiz
{
namespace
{

// A luminaire is an unauthenticated peer. Every numeric read below is bounded
// here, so a hostile datagram cannot push an out-of-range value into the model
// or a projected control.
[[nodiscard]] bool readBoundedInt(const QJsonObject &object, const QString &key,
                                  const int minimum, const int maximum, int &out)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    const double raw = value.toDouble();
    if (raw < static_cast<double>(minimum) || raw > static_cast<double>(maximum)) {
        return false;
    }
    out = static_cast<int>(raw);
    return true;
}

[[nodiscard]] QString readBoundedString(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return {};
    }
    const QString text = value.toString();
    if (text.size() > Limits::maximumTextLength) {
        return {};
    }
    // Control characters would corrupt a label rendered in the shell.
    for (const QChar character : text) {
        if (character.category() == QChar::Other_Control) {
            return {};
        }
    }
    return text;
}

[[nodiscard]] bool readChannel(const QJsonObject &object, const QString &key, quint8 &out)
{
    int raw = 0;
    if (!readBoundedInt(object, key, 0, 255, raw)) {
        return false;
    }
    out = static_cast<quint8>(raw);
    return true;
}

void decodePilot(const QJsonObject &object, DecodedMessage &message)
{
    PilotState pilot;
    bool anyField = false;

    const QJsonValue state = object.value(QStringLiteral("state"));
    if (state.isBool()) {
        pilot.on = state.toBool();
        anyField = true;
    }

    int dimming = 0;
    if (readBoundedInt(object, QStringLiteral("dimming"), 0,
                       Limits::maximumDimmingPercent, dimming)) {
        pilot.dimmingKnown = true;
        pilot.dimmingPercent = static_cast<quint8>(dimming);
        anyField = true;
    }

    int temperature = 0;
    if (readBoundedInt(object, QStringLiteral("temp"), 0, Limits::maximumKelvin,
                       temperature)) {
        pilot.temperatureKnown = true;
        pilot.temperatureKelvin = static_cast<quint16>(temperature);
        anyField = true;
    }

    // The five colour channels are reported together; a partial set is not
    // usable colour truth.
    quint8 red = 0;
    quint8 green = 0;
    quint8 blue = 0;
    quint8 cool = 0;
    quint8 warm = 0;
    if (readChannel(object, QStringLiteral("r"), red)
        && readChannel(object, QStringLiteral("g"), green)
        && readChannel(object, QStringLiteral("b"), blue)) {
        // Some models omit the white channels in colour mode; absent means
        // "channel not driven", which is the zero already in place.
        if (!readChannel(object, QStringLiteral("c"), cool)) {
            cool = 0;
        }
        if (!readChannel(object, QStringLiteral("w"), warm)) {
            warm = 0;
        }
        pilot.colorKnown = true;
        pilot.red = red;
        pilot.green = green;
        pilot.blue = blue;
        pilot.coolWhite = cool;
        pilot.warmWhite = warm;
        anyField = true;
    }

    int sceneId = 0;
    if (readBoundedInt(object, QStringLiteral("sceneId"), 0, Limits::rhythmSceneId,
                       sceneId)) {
        pilot.sceneKnown = true;
        pilot.sceneId = static_cast<quint16>(sceneId);
        anyField = true;
    }

    int speed = 0;
    if (readBoundedInt(object, QStringLiteral("speed"), 0,
                       Limits::maximumSpeedPercent, speed)) {
        pilot.speedKnown = true;
        pilot.speedPercent = static_cast<quint8>(speed);
        anyField = true;
    }

    int rssi = 0;
    if (readBoundedInt(object, QStringLiteral("rssi"), -120, 0, rssi)) {
        pilot.signalKnown = true;
        pilot.signalDbm = static_cast<qint16>(rssi);
        anyField = true;
    }

    if (anyField) {
        message.pilotKnown = true;
        message.pilot = pilot;
    }
}

void decodeSystemConfig(const QJsonObject &object, DecodedMessage &message)
{
    SystemConfigPayload payload;
    payload.mac = message.mac;
    payload.moduleName = readBoundedString(object, QStringLiteral("moduleName"));
    payload.firmwareVersion = readBoundedString(object, QStringLiteral("fwVersion"));
    int homeId = 0;
    if (readBoundedInt(object, QStringLiteral("homeId"), 0, 2147483647, homeId)) {
        payload.homeId = static_cast<quint32>(homeId);
    }
    int roomId = 0;
    if (readBoundedInt(object, QStringLiteral("roomId"), 0, 2147483647, roomId)) {
        payload.roomId = static_cast<quint32>(roomId);
    }
    if (payload.moduleName.isEmpty() && payload.firmwareVersion.isEmpty()) {
        return;
    }
    message.systemConfigKnown = true;
    message.systemConfig = payload;
}

void decodeModelConfig(const QJsonObject &object, DecodedMessage &message)
{
    ModelConfigPayload payload;

    // cctRange is [absolute minimum, preferred minimum, preferred maximum,
    // absolute maximum]. The outer pair is the controllable envelope.
    const QJsonValue range = object.value(QStringLiteral("cctRange"));
    if (range.isArray()) {
        const QJsonArray values = range.toArray();
        if (values.size() >= 4 && values.at(0).isDouble() && values.at(3).isDouble()) {
            const int minimum = static_cast<int>(values.at(0).toDouble());
            const int maximum = static_cast<int>(values.at(3).toDouble());
            if (minimum >= Limits::minimumKelvin && maximum <= Limits::maximumKelvin
                && maximum > minimum) {
                payload.temperatureRangeKnown = true;
                payload.minimumKelvin = minimum;
                payload.maximumKelvin = maximum;
            }
        }
    }

    int floorPercent = 0;
    if (readBoundedInt(object, QStringLiteral("minDimLevel"), 0,
                       Limits::maximumDimmingPercent, floorPercent)) {
        payload.dimmingFloorKnown = true;
        payload.minimumDimmingPercent = floorPercent;
    }

    int heads = 0;
    if (readBoundedInt(object, QStringLiteral("headTotal"), 1, 8, heads)) {
        payload.headCount = heads;
    }

    message.modelConfigKnown = true;
    message.modelConfig = payload;
}

} // namespace

std::optional<DecodedMessage> decodeMessage(const QByteArray &datagram)
{
    if (datagram.isEmpty() || datagram.size() > Limits::maximumDatagramBytes) {
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(datagram, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QString methodName = readBoundedString(root, QStringLiteral("method"));
    if (methodName.isEmpty()) {
        return std::nullopt;
    }

    DecodedMessage message;
    message.method = methodFromString(methodName);

    const QJsonValue error = root.value(QStringLiteral("error"));
    if (error.isObject()) {
        const QJsonObject errorObject = error.toObject();
        message.hasError = true;
        int code = 0;
        if (readBoundedInt(errorObject, QStringLiteral("code"), -2147483647, 2147483647,
                           code)) {
            message.errorCode = code;
        }
        message.errorMessage = readBoundedString(errorObject, QStringLiteral("message"));
        return message;
    }

    // Replies carry `result`; an unsolicited notification carries `params`.
    QJsonValue payload = root.value(QStringLiteral("result"));
    if (!payload.isObject()) {
        payload = root.value(QStringLiteral("params"));
        message.unsolicited = payload.isObject();
    }
    if (!payload.isObject()) {
        return message;
    }
    const QJsonObject body = payload.toObject();

    message.mac = normalizeMac(readBoundedString(body, QStringLiteral("mac")));

    const QJsonValue success = body.value(QStringLiteral("success"));
    if (success.isBool()) {
        message.acknowledged = success.toBool();
    }

    switch (message.method) {
    case Method::GetPilot:
    case Method::SyncPilot:
        decodePilot(body, message);
        break;
    case Method::GetSystemConfig:
        decodeSystemConfig(body, message);
        break;
    case Method::GetModelConfig:
        decodeModelConfig(body, message);
        break;
    case Method::SetPilot:
    case Method::Registration:
    case Method::Unknown:
        break;
    }
    return message;
}

} // namespace QindaQt::Wiz
