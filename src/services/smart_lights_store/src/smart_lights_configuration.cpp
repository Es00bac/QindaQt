// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/smart_lights_store/smart_lights_configuration.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

namespace QindaQt::SmartLights
{
namespace
{

using Wiz::Limits::maximumDevices;
using Wiz::Limits::maximumLabelLength;
using Wiz::Limits::maximumPresets;

constexpr quint32 supportedSchemaVersion = 1;
// A preset over every tracked luminaire is the largest legitimate one.
constexpr int maximumPresetMembers = maximumDevices;

[[nodiscard]] QJsonObject encodeState(const Wiz::StateRequest &state)
{
    QJsonObject object;
    if (state.setPower) {
        object.insert(QStringLiteral("on"), state.on);
    }
    if (state.setDimming) {
        object.insert(QStringLiteral("brightness"), static_cast<int>(state.dimmingPercent));
    }
    if (state.setTemperature) {
        object.insert(QStringLiteral("kelvin"), static_cast<int>(state.temperatureKelvin));
    }
    if (state.setColor) {
        object.insert(QStringLiteral("red"), static_cast<int>(state.red));
        object.insert(QStringLiteral("green"), static_cast<int>(state.green));
        object.insert(QStringLiteral("blue"), static_cast<int>(state.blue));
        object.insert(QStringLiteral("coolWhite"), static_cast<int>(state.coolWhite));
        object.insert(QStringLiteral("warmWhite"), static_cast<int>(state.warmWhite));
    }
    if (state.setScene) {
        object.insert(QStringLiteral("scene"), static_cast<int>(state.sceneId));
    }
    if (state.setSpeed) {
        object.insert(QStringLiteral("speed"), static_cast<int>(state.speedPercent));
    }
    return object;
}

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

[[nodiscard]] std::optional<Wiz::StateRequest> decodeState(const QJsonObject &object)
{
    Wiz::StateRequest state;
    const QJsonValue on = object.value(QStringLiteral("on"));
    if (on.isBool()) {
        state.setPower = true;
        state.on = on.toBool();
    }
    int value = 0;
    if (readBoundedInt(object, QStringLiteral("brightness"), 1,
                       Wiz::Limits::maximumDimmingPercent, value)) {
        state.setDimming = true;
        state.dimmingPercent = static_cast<quint8>(value);
    }
    if (readBoundedInt(object, QStringLiteral("kelvin"), Wiz::Limits::minimumKelvin,
                       Wiz::Limits::maximumKelvin, value)) {
        state.setTemperature = true;
        state.temperatureKelvin = static_cast<quint16>(value);
    }
    int red = 0;
    int green = 0;
    int blue = 0;
    if (readBoundedInt(object, QStringLiteral("red"), 0, 255, red)
        && readBoundedInt(object, QStringLiteral("green"), 0, 255, green)
        && readBoundedInt(object, QStringLiteral("blue"), 0, 255, blue)) {
        state.setColor = true;
        state.red = static_cast<quint8>(red);
        state.green = static_cast<quint8>(green);
        state.blue = static_cast<quint8>(blue);
        int white = 0;
        if (readBoundedInt(object, QStringLiteral("coolWhite"), 0, 255, white)) {
            state.coolWhite = static_cast<quint8>(white);
        }
        if (readBoundedInt(object, QStringLiteral("warmWhite"), 0, 255, white)) {
            state.warmWhite = static_cast<quint8>(white);
        }
    }
    if (readBoundedInt(object, QStringLiteral("scene"), 1, Wiz::Limits::rhythmSceneId,
                       value)) {
        state.setScene = true;
        state.sceneId = static_cast<quint16>(value);
    }
    if (readBoundedInt(object, QStringLiteral("speed"), Wiz::Limits::minimumSpeedPercent,
                       Wiz::Limits::maximumSpeedPercent, value)) {
        state.setSpeed = true;
        state.speedPercent = static_cast<quint8>(value);
    }
    if (state.isEmpty()) {
        return std::nullopt;
    }
    return state;
}

[[nodiscard]] QString readText(const QJsonObject &object, const QString &key,
                               const int maximumLength)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return {};
    }
    const QString text = value.toString().trimmed();
    if (text.isEmpty() || text.size() > maximumLength) {
        return {};
    }
    for (const QChar character : text) {
        if (character.category() == QChar::Other_Control) {
            return {};
        }
    }
    return text;
}

} // namespace

QByteArray encodeConfiguration(const StoredConfiguration &configuration)
{
    QJsonArray devices;
    for (const StoredDevice &device : configuration.devices) {
        QJsonObject object;
        object.insert(QStringLiteral("mac"), device.mac);
        if (!device.label.isEmpty()) {
            object.insert(QStringLiteral("label"), device.label);
        }
        if (!device.lastAddress.isEmpty()) {
            object.insert(QStringLiteral("lastAddress"), device.lastAddress);
        }
        devices.append(object);
    }

    QJsonArray presets;
    for (const StoredPreset &preset : configuration.presets) {
        QJsonArray members;
        for (const PresetMember &member : preset.members) {
            QJsonObject memberObject;
            memberObject.insert(QStringLiteral("mac"), member.mac);
            memberObject.insert(QStringLiteral("state"), encodeState(member.state));
            members.append(memberObject);
        }
        QJsonObject object;
        object.insert(QStringLiteral("id"), preset.id);
        object.insert(QStringLiteral("name"), preset.name);
        object.insert(QStringLiteral("members"), members);
        presets.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"),
                static_cast<int>(configuration.schemaVersion));
    root.insert(QStringLiteral("devices"), devices);
    root.insert(QStringLiteral("presets"), presets);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<StoredConfiguration> decodeConfiguration(const QByteArray &document)
{
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = parsed.object();
    int schemaVersion = 0;
    if (!readBoundedInt(root, QStringLiteral("schemaVersion"), 1,
                        static_cast<int>(supportedSchemaVersion), schemaVersion)) {
        // A newer file belongs to a newer desktop. Refusing it whole keeps the
        // user's presets intact for that version instead of rewriting them
        // with whatever this build happened to understand.
        return std::nullopt;
    }

    StoredConfiguration configuration;
    configuration.schemaVersion = static_cast<quint32>(schemaVersion);

    const QJsonValue devices = root.value(QStringLiteral("devices"));
    if (devices.isArray()) {
        const QJsonArray array = devices.toArray();
        for (const QJsonValue &entry : array) {
            if (configuration.devices.size() >= maximumDevices) {
                break;
            }
            if (!entry.isObject()) {
                continue;
            }
            const QJsonObject object = entry.toObject();
            StoredDevice device;
            device.mac = Wiz::normalizeMac(
                readText(object, QStringLiteral("mac"), maximumLabelLength));
            if (device.mac.isEmpty()) {
                continue;
            }
            device.label = readText(object, QStringLiteral("label"), maximumLabelLength);
            device.lastAddress =
                readText(object, QStringLiteral("lastAddress"), maximumLabelLength);
            configuration.devices.append(device);
        }
    }

    const QJsonValue presets = root.value(QStringLiteral("presets"));
    if (presets.isArray()) {
        const QJsonArray array = presets.toArray();
        for (const QJsonValue &entry : array) {
            if (configuration.presets.size() >= maximumPresets) {
                break;
            }
            if (!entry.isObject()) {
                continue;
            }
            const QJsonObject object = entry.toObject();
            StoredPreset preset;
            preset.id = readText(object, QStringLiteral("id"), maximumLabelLength);
            preset.name = readText(object, QStringLiteral("name"), maximumLabelLength);
            if (preset.id.isEmpty() || preset.name.isEmpty()) {
                continue;
            }
            const QJsonValue members = object.value(QStringLiteral("members"));
            if (!members.isArray()) {
                continue;
            }
            const QJsonArray memberArray = members.toArray();
            for (const QJsonValue &memberEntry : memberArray) {
                if (preset.members.size() >= maximumPresetMembers) {
                    break;
                }
                if (!memberEntry.isObject()) {
                    continue;
                }
                const QJsonObject memberObject = memberEntry.toObject();
                PresetMember member;
                member.mac = Wiz::normalizeMac(
                    readText(memberObject, QStringLiteral("mac"), maximumLabelLength));
                if (member.mac.isEmpty()) {
                    continue;
                }
                const QJsonValue state = memberObject.value(QStringLiteral("state"));
                if (!state.isObject()) {
                    continue;
                }
                const auto decoded = decodeState(state.toObject());
                if (!decoded.has_value()) {
                    continue;
                }
                member.state = *decoded;
                preset.members.append(member);
            }
            if (preset.members.isEmpty()) {
                continue;
            }
            configuration.presets.append(preset);
        }
    }
    return configuration;
}

QString makePresetId(const QString &name, const QList<StoredPreset> &existing)
{
    QString base;
    base.reserve(name.size());
    for (const QChar character : name.toLower()) {
        if (character.isLetterOrNumber()) {
            base.append(character);
        } else if (!base.isEmpty() && base.back() != QLatin1Char('-')) {
            base.append(QLatin1Char('-'));
        }
    }
    while (base.endsWith(QLatin1Char('-'))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        base = QStringLiteral("preset");
    }
    base = base.left(maximumLabelLength - 4);

    const auto taken = [&existing](const QString &candidate) {
        for (const StoredPreset &preset : existing) {
            if (preset.id == candidate) {
                return true;
            }
        }
        return false;
    };
    if (!taken(base)) {
        return base;
    }
    for (int suffix = 2; suffix < 1000; ++suffix) {
        const QString candidate = QStringLiteral("%1-%2").arg(base).arg(suffix);
        if (!taken(candidate)) {
            return candidate;
        }
    }
    return base;
}

} // namespace QindaQt::SmartLights
