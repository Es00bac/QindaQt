// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_reply_validation_p.h"

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

namespace QindaQt::Services::SettingsClient::Private {

using namespace SettingsProtocol;

std::optional<quint64> exactUnsigned64(const QVariant &value)
{
    return value.metaType().id() == QMetaType::ULongLong
               ? std::optional<quint64>(value.toULongLong()) : std::nullopt;
}

std::optional<SettingsWireStatus> wireStatus(const QVariantMap &wire)
{
    const QVariant value = wire.value(QLatin1StringView(WireContract::FieldStatus));
    return value.metaType().id() == QMetaType::UInt
               ? fromWireStatus(value.toUInt()) : std::nullopt;
}

bool validEpoch(const QString &epoch)
{
    return !epoch.isEmpty() && !epoch.contains(QChar::Null)
           && epoch.toUtf8().size() <= WireContract::MaximumEpochBytes;
}

bool validVersions(const QVariantMap &wire, quint32 *settingsSchemaVersion)
{
    const QVariant wireVersion = wire.value(
        QLatin1StringView(WireContract::FieldWireSchemaVersion));
    const QVariant settingsVersion = wire.value(
        QLatin1StringView(WireContract::FieldSettingsSchemaVersion));
    if (wireVersion.metaType().id() != QMetaType::UInt
        || wireVersion.toUInt() != WireContract::WireSchemaVersion
        || settingsVersion.metaType().id() != QMetaType::UInt
        || settingsVersion.toUInt() == 0) {
        return false;
    }
    *settingsSchemaVersion = settingsVersion.toUInt();
    return true;
}

std::optional<QVariantMap> boundedValueMap(const QVariant &wireValue)
{
    auto map = decodeBoundedVariantMap(wireValue, WireContract::MaximumRequestedKeys);
    if (!map.has_value()) {
        return std::nullopt;
    }
    QVariantMap decoded;
    qsizetype aggregateBytes = 0;
    qsizetype aggregateNodes = 0;
    for (auto iterator = map->cbegin(); iterator != map->cend(); ++iterator) {
        QString error;
        if (!BoundedSettingsValueCodec::validateKey(iterator.key(), &error)) {
            return std::nullopt;
        }
        auto value = decodeBoundedJsonValue(iterator.value(), &error);
        BoundedSettingsValueCodec::Usage usage;
        if (!value.has_value()
            || !BoundedSettingsValueCodec::validateValue(*value, &error, &usage)
            || aggregateBytes > WireContract::MaximumSnapshotValueBytes - usage.bytes
            || aggregateNodes > WireContract::MaximumSnapshotValueNodes - usage.nodes) {
            return std::nullopt;
        }
        aggregateBytes += usage.bytes;
        aggregateNodes += usage.nodes;
        decoded.insert(iterator.key(), *value);
    }
    return decoded;
}

std::optional<QVariantMap> boundedSourceMap(const QVariant &wireValue)
{
    auto map = decodeBoundedVariantMap(wireValue, WireContract::MaximumRequestedKeys);
    if (!map.has_value()) {
        return std::nullopt;
    }
    for (auto iterator = map->cbegin(); iterator != map->cend(); ++iterator) {
        if (iterator.value().metaType().id() != QMetaType::QString
            || !BoundedSettingsValueCodec::validateKey(iterator.key())) {
            return std::nullopt;
        }
        const QString source = iterator.value().toString();
        if (source != QLatin1String("system-defaults")
            && source != QLatin1String("profile-defaults")
            && source != QLatin1String("user-overrides")
            && source != QLatin1String("session-overrides")) {
            return std::nullopt;
        }
    }
    return *map;
}

} // namespace QindaQt::Services::SettingsClient::Private
