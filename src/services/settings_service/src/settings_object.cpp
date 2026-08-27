// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_object_p.h"

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_encode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/settings/settings_types.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QSet>

#include <utility>

namespace QindaQt::Services::SettingsService::Private {
namespace {

using namespace QindaQt::Services::SettingsProtocol;

SettingsWireStatus wireStatusFor(RepositoryCommitStatus status)
{
    switch (status) {
    case RepositoryCommitStatus::Applied:
        return SettingsWireStatus::Applied;
    case RepositoryCommitStatus::ValidationFailed:
        return SettingsWireStatus::ValidationFailed;
    case RepositoryCommitStatus::Conflict:
        return SettingsWireStatus::Conflict;
    case RepositoryCommitStatus::ReadOnlyLayer:
        return SettingsWireStatus::ReadOnlyLayer;
    case RepositoryCommitStatus::PersistenceFailed:
        return SettingsWireStatus::PersistenceFailed;
    case RepositoryCommitStatus::RevisionExhausted:
        return SettingsWireStatus::RevisionExhausted;
    }
    return SettingsWireStatus::ValidationFailed;
}

QVariantMap sourceLayerNames(const QMap<QString, Settings::SettingLayer> &layers)
{
    QVariantMap result;
    for (auto iterator = layers.cbegin(); iterator != layers.cend(); ++iterator) {
        result.insert(iterator.key(), Settings::toString(iterator.value()));
    }
    return result;
}

std::optional<QVariantMap> encodeValuesForWire(const QVariantMap &values,
                                               qsizetype maximumBytes,
                                               qsizetype maximumNodes,
                                               QString *error)
{
    AggregateValueDecodeBudget aggregate{maximumBytes, maximumNodes};
    QVariantMap result;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        auto encoded = encodeBoundedJsonValueForWire(iterator.value(), aggregate, error);
        if (!encoded.has_value()) {
            return std::nullopt;
        }
        result.insert(iterator.key(), std::move(*encoded));
    }
    return result;
}

QVariantMap malformedReply(QString message, quint32 settingsSchemaVersion,
                           QString epoch, quint64 revision)
{
    return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(SettingsWireStatus::MalformedRequest)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), settingsSchemaVersion},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevisionBefore), revision},
            {QLatin1StringView(WireContract::FieldRevisionAfter), revision},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{}},
            {QLatin1StringView(WireContract::FieldSourceLayers), QVariantMap{}},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage), std::move(message)}};
}

QVariantMap malformedSnapshotReply(QString message, quint32 settingsSchemaVersion,
                                   QString epoch, quint64 revision)
{
    // AGENT-CONTRACT: GetSnapshot always returns its exact eight-field
    // envelope, including on malformed requests. A commit-shaped error would
    // be rejected by the client's bounded top-level decoder before it could
    // report the actual failure.
    return {{QLatin1StringView(WireContract::FieldStatus),
             static_cast<quint32>(SettingsWireStatus::MalformedRequest)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             settingsSchemaVersion},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{}},
            {QLatin1StringView(WireContract::FieldSourceLayers), QVariantMap{}},
            {QLatin1StringView(WireContract::FieldMessage), std::move(message)}};
}

// Decodes one operation entry {key, kind, value} into a repository
// operation, or returns nullopt with *error set for any bound violation,
// unknown/missing field, or unsupported value shape. Duplicate-key rejection
// happens at the caller, across the whole batch.
std::optional<SettingsRepository::Operation> decodeOperation(
    const QVariant &entry,
    AggregateValueDecodeBudget &aggregate,
    QString *error)
{
    const auto map = decodeBoundedVariantMap(entry, WireContract::OperationFieldCount);
    if (!map.has_value()) {
        *error = QStringLiteral("operation entry is not a bounded object");
        return std::nullopt;
    }
    const QVariant keyField = map->value(QLatin1StringView(WireContract::FieldKey));
    const QVariant kindField = map->value(QLatin1StringView(WireContract::FieldKind));
    if (keyField.metaType().id() != QMetaType::QString
        || kindField.metaType().id() != QMetaType::QString) {
        *error = QStringLiteral("operation key and kind must be strings");
        return std::nullopt;
    }
    const QString key = keyField.toString();
    if (!BoundedSettingsValueCodec::validateKey(key, error)) {
        return std::nullopt;
    }
    const QString kind = kindField.toString();
    SettingsRepository::Operation operation;
    operation.key = key;
    if (kind == QLatin1StringView(WireContract::OperationKindRemove)) {
        if (map->size() != 2 || map->contains(QLatin1StringView(WireContract::FieldValue))) {
            *error = QStringLiteral("a \"remove\" operation has exactly key and kind");
            return std::nullopt;
        }
        operation.remove = true;
        return operation;
    }
    if (kind != QLatin1StringView(WireContract::OperationKindSet)) {
        *error = QStringLiteral("operation kind must be \"set\" or \"remove\"");
        return std::nullopt;
    }
    if (map->size() != WireContract::OperationFieldCount
        || !map->contains(QLatin1StringView(WireContract::FieldValue))) {
        *error = QStringLiteral("a \"set\" operation requires a value");
        return std::nullopt;
    }
    auto value = decodeBoundedJsonValue(
        map->value(QLatin1StringView(WireContract::FieldValue)), aggregate, error);
    if (!value.has_value()) {
        return std::nullopt;
    }
    operation.value = std::move(*value);
    return operation;
}

} // namespace

SettingsObject::SettingsObject(QDBusConnection connection, SettingsRepository &repository, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_repository(repository)
{
}

QVariantMap SettingsObject::GetSnapshot(const QStringList &keys)
{
    if (keys.isEmpty() || keys.size() > WireContract::MaximumRequestedKeys) {
        return malformedSnapshotReply(
            QStringLiteral("GetSnapshot requires between 1 and %1 keys")
                .arg(WireContract::MaximumRequestedKeys),
            quint32(m_repository.schema().version()),
            m_repository.epoch(), m_repository.revision());
    }
    QSet<QString> seenKeys;
    for (const auto &key : keys) {
        QString keyError;
        if (!BoundedSettingsValueCodec::validateKey(key, &keyError) || seenKeys.contains(key)) {
            return malformedSnapshotReply(
                seenKeys.contains(key)
                    ? QStringLiteral("duplicate snapshot key: %1").arg(key)
                    : keyError,
                quint32(m_repository.schema().version()),
                m_repository.epoch(), m_repository.revision());
        }
        seenKeys.insert(key);
    }
    const auto snapshot = m_repository.snapshot(keys);
    if (!snapshot.ok) {
        return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(SettingsWireStatus::UnknownKey)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
                {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
                {QLatin1StringView(WireContract::FieldRevision), m_repository.revision()},
                {QLatin1StringView(WireContract::FieldValues), QVariantMap{}},
                {QLatin1StringView(WireContract::FieldSourceLayers), QVariantMap{}},
                {QLatin1StringView(WireContract::FieldMessage),
                 QStringLiteral("unknown key: %1").arg(snapshot.unknownKey)}};
    }
    QString boundsError;
    auto wireValues = encodeValuesForWire(snapshot.values,
                                          WireContract::MaximumSnapshotValueBytes,
                                          WireContract::MaximumSnapshotValueNodes,
                                          &boundsError);
    if (!wireValues.has_value()) {
        return malformedSnapshotReply(boundsError,
                                      quint32(m_repository.schema().version()),
                                      m_repository.epoch(), m_repository.revision());
    }
    return encodeSnapshot(snapshot, std::move(*wireValues));
}

QVariantMap SettingsObject::encodeSnapshot(const RepositorySnapshot &snapshot,
                                           QVariantMap wireValues) const
{
    return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
            {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
            {QLatin1StringView(WireContract::FieldRevision), snapshot.revision},
            {QLatin1StringView(WireContract::FieldValues), std::move(wireValues)},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayerNames(snapshot.sourceLayers)},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap SettingsObject::CommitUserTransaction(const QString &epoch, quint64 baseRevision,
                                                   const QVariantList &operations)
{
    if (operations.isEmpty() || operations.size() > WireContract::MaximumOperationsPerTransaction) {
        return malformedReply(QStringLiteral("CommitUserTransaction requires between 1 and %1 operations")
                                  .arg(WireContract::MaximumOperationsPerTransaction),
                              quint32(m_repository.schema().version()),
                              m_repository.epoch(), m_repository.revision());
    }

    QVector<SettingsRepository::Operation> decoded;
    decoded.reserve(operations.size());
    QSet<QString> seenKeys;
    AggregateValueDecodeBudget aggregate{WireContract::MaximumTransactionValueBytes,
                                         WireContract::MaximumTransactionValueNodes};
    for (const auto &entry : operations) {
        QString decodeError;
        auto operation = decodeOperation(entry, aggregate, &decodeError);
        if (!operation.has_value()) {
            return malformedReply(decodeError, quint32(m_repository.schema().version()),
                                  m_repository.epoch(), m_repository.revision());
        }
        if (seenKeys.contains(operation->key)) {
            return malformedReply(QStringLiteral("duplicate operation key: %1").arg(operation->key),
                                  quint32(m_repository.schema().version()),
                                  m_repository.epoch(), m_repository.revision());
        }
        seenKeys.insert(operation->key);
        decoded.append(std::move(*operation));
    }

    if (epoch != m_repository.epoch()) {
        return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(SettingsWireStatus::EpochMismatch)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
                {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
                {QLatin1StringView(WireContract::FieldRevisionBefore), m_repository.revision()},
                {QLatin1StringView(WireContract::FieldRevisionAfter), m_repository.revision()},
                {QLatin1StringView(WireContract::FieldValues), QVariantMap{}},
                {QLatin1StringView(WireContract::FieldSourceLayers), QVariantMap{}},
                {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
                {QLatin1StringView(WireContract::FieldMessage),
                 QStringLiteral("service epoch has changed; fetch a fresh snapshot")}};
    }

    const auto result = m_repository.commitUserOverrides(baseRevision, decoded);
    if (result.ok() && !result.changedKeys.isEmpty()) {
        publishChanged(result);
    }
    QString boundsError;
    auto wireValues = encodeValuesForWire(result.currentValues,
                                          WireContract::MaximumSnapshotValueBytes,
                                          WireContract::MaximumSnapshotValueNodes,
                                          &boundsError);
    if (!wireValues.has_value()) {
        return malformedReply(boundsError, quint32(m_repository.schema().version()),
                              m_repository.epoch(), m_repository.revision());
    }
    return encodeCommitResult(result, std::move(*wireValues));
}

QVariantMap SettingsObject::encodeCommitResult(const RepositoryCommitResult &result,
                                               QVariantMap wireValues) const
{
    return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(wireStatusFor(result.status))},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
            {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
            {QLatin1StringView(WireContract::FieldRevisionBefore), result.revisionBefore},
            {QLatin1StringView(WireContract::FieldRevisionAfter), result.revisionAfter},
            {QLatin1StringView(WireContract::FieldValues), std::move(wireValues)},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayerNames(result.currentSourceLayers)},
            {QLatin1StringView(WireContract::FieldChangedKeys), result.changedKeys},
            {QLatin1StringView(WireContract::FieldMessage), result.message}};
}

void SettingsObject::publishChanged(const RepositoryCommitResult &result)
{
    auto signal = QDBusMessage::createSignal(QString::fromLatin1(WireContract::ObjectPath),
                                             QString::fromLatin1(WireContract::InterfaceName),
                                             QString::fromLatin1(WireContract::SettingsChangedSignal));
    QStringList changedKeys = result.changedKeys;
    if (changedKeys.size() > WireContract::MaximumChangedKeysPerSignal) {
        changedKeys = changedKeys.mid(0, WireContract::MaximumChangedKeysPerSignal);
    }
    signal << m_repository.epoch() << result.revisionAfter << changedKeys;
    const bool sent = m_connection.send(signal);
    Q_UNUSED(sent)
}

} // namespace QindaQt::Services::SettingsService::Private
