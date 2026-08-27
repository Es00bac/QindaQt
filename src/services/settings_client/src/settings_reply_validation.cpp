// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_reply_validation_p.h"

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

#include <QSet>

#include <limits>

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
    AggregateValueDecodeBudget aggregate{WireContract::MaximumSnapshotValueBytes,
                                         WireContract::MaximumSnapshotValueNodes};
    for (auto iterator = map->cbegin(); iterator != map->cend(); ++iterator) {
        QString error;
        if (!BoundedSettingsValueCodec::validateKey(iterator.key(), &error)) {
            return std::nullopt;
        }
        auto value = decodeBoundedJsonValue(iterator.value(), aggregate, &error);
        if (!value.has_value()) {
            return std::nullopt;
        }
        decoded.insert(iterator.key(), *value);
    }
    return decoded;
}

std::optional<QStringList> boundedChangedKeys(const QVariant &wireValue)
{
    return decodeBoundedKeyList(wireValue, WireContract::MaximumChangedKeysPerSignal);
}

namespace {

bool hasExactFields(const QVariantMap &wire, std::initializer_list<const char *> names)
{
    if (wire.size() != qsizetype(names.size())) {
        return false;
    }
    for (const char *name : names) {
        if (!wire.contains(QLatin1StringView(name))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<QString> boundedWireMessage(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QString) {
        return std::nullopt;
    }
    const QString message = value.toString();
    if (message.contains(QChar::Null)
        || message.toUtf8().size() > WireContract::MaximumMessageBytes) {
        return std::nullopt;
    }
    return message;
}

namespace {

bool revisionsMatchStatus(SettingsWireStatus status,
                          quint64 before,
                          quint64 after,
                          quint64 base,
                          const QStringList &changed,
                          const QString &key)
{
    switch (status) {
    case SettingsWireStatus::Applied:
        if (before != base || after < before || after - before > 1) {
            return false;
        }
        return after == before ? changed.isEmpty()
                               : changed == QStringList{key};
    case SettingsWireStatus::Conflict:
        return before == after && after > base && changed.isEmpty();
    case SettingsWireStatus::RevisionExhausted:
        return before == base && after == base
               && base == std::numeric_limits<quint64>::max() && changed.isEmpty();
    case SettingsWireStatus::ValidationFailed:
    case SettingsWireStatus::ReadOnlyLayer:
    case SettingsWireStatus::PersistenceFailed:
    case SettingsWireStatus::UnknownKey:
    case SettingsWireStatus::MalformedRequest:
        return before == base && after == base && changed.isEmpty();
    case SettingsWireStatus::EpochMismatch:
        // An exact-owner client with an accepted epoch cannot receive this
        // coherently. Treat it as uncertain lineage, never a confirmed result.
        return false;
    }
    return false;
}

bool authorityMapsMatchStatus(SettingsWireStatus status,
                              const QVariantMap &values,
                              const QVariantMap &sources,
                              const QString &key)
{
    if (status == SettingsWireStatus::UnknownKey) {
        // No value/source authority exists for an unknown schema key. Accept
        // only the exact empty pair, never a fabricated null or partial map.
        return values.isEmpty() && sources.isEmpty();
    }
    return values.size() == 1 && sources.size() == 1
           && values.contains(key) && sources.contains(key);
}

} // namespace

bool hasExactSnapshotFields(const QVariantMap &wire)
{
    return hasExactFields(wire,
                          {WireContract::FieldStatus,
                           WireContract::FieldWireSchemaVersion,
                           WireContract::FieldSettingsSchemaVersion,
                           WireContract::FieldEpoch,
                           WireContract::FieldRevision,
                           WireContract::FieldValues,
                           WireContract::FieldSourceLayers,
                           WireContract::FieldMessage});
}

std::optional<CommitOutcome> validatedCommitReply(
    const QVariantMap &wire,
    const CommitReplyContext &context)
{
    if (!hasExactFields(wire,
                        {WireContract::FieldStatus,
                         WireContract::FieldWireSchemaVersion,
                         WireContract::FieldSettingsSchemaVersion,
                         WireContract::FieldEpoch,
                         WireContract::FieldRevisionBefore,
                         WireContract::FieldRevisionAfter,
                         WireContract::FieldValues,
                         WireContract::FieldSourceLayers,
                         WireContract::FieldChangedKeys,
                         WireContract::FieldMessage})) {
        return std::nullopt;
    }
    const auto status = wireStatus(wire);
    const auto before = exactUnsigned64(
        wire.value(QLatin1StringView(WireContract::FieldRevisionBefore)));
    const auto after = exactUnsigned64(
        wire.value(QLatin1StringView(WireContract::FieldRevisionAfter)));
    const auto values = boundedValueMap(
        wire.value(QLatin1StringView(WireContract::FieldValues)));
    const auto sources = boundedSourceMap(
        wire.value(QLatin1StringView(WireContract::FieldSourceLayers)));
    const auto changed = boundedChangedKeys(
        wire.value(QLatin1StringView(WireContract::FieldChangedKeys)));
    const auto message = boundedWireMessage(
        wire.value(QLatin1StringView(WireContract::FieldMessage)));
    const QVariant epochField = wire.value(QLatin1StringView(WireContract::FieldEpoch));
    quint32 settingsSchemaVersion = 0;
    if (!status || !before || !after || !values || !sources || !changed || !message
        || epochField.metaType().id() != QMetaType::QString
        || epochField.toString() != context.epoch
        || !validEpoch(epochField.toString())
        || !validVersions(wire, &settingsSchemaVersion)
        || settingsSchemaVersion != context.settingsSchemaVersion
        || !authorityMapsMatchStatus(*status, *values, *sources, context.key)
        || !revisionsMatchStatus(*status, *before, *after, context.baseRevision,
                                 *changed, context.key)) {
        return std::nullopt;
    }
    return CommitOutcome{*status, *before, *after, *values, *sources, *changed, *message};
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
