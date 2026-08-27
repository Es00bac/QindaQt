// SPDX-License-Identifier: LGPL-3.0-or-later
#include "settings_object_p.h"

#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
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

bool valuesFitAggregateBounds(const QVariantMap &values, qsizetype maximumBytes,
                              qsizetype maximumNodes, QString *error)
{
    qsizetype aggregateBytes = 0;
    qsizetype aggregateNodes = 0;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        BoundedSettingsValueCodec::Usage usage;
        if (!BoundedSettingsValueCodec::validateValue(iterator.value(), error, &usage)
            || aggregateBytes > maximumBytes - usage.bytes
            || aggregateNodes > maximumNodes - usage.nodes) {
            if (error != nullptr && error->isEmpty()) {
                *error = QStringLiteral("aggregate values exceed protocol bounds");
            }
            return false;
        }
        aggregateBytes += usage.bytes;
        aggregateNodes += usage.nodes;
    }
    return true;
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

// Decodes one operation entry {key, kind, value} into a repository
// operation, or returns nullopt with *error set for any bound violation,
// unknown/missing field, or unsupported value shape. Duplicate-key rejection
// happens at the caller, across the whole batch.
std::optional<SettingsRepository::Operation> decodeOperation(const QVariant &entry, QString *error)
{
    const auto map = decodeBoundedVariantMap(entry, 3);
    if (!map.has_value()) {
        *error = QStringLiteral("operation entry is not a bounded object");
        return std::nullopt;
    }
    const QString key = map->value(QLatin1StringView(WireContract::FieldKey)).toString();
    if (!BoundedSettingsValueCodec::validateKey(key, error)) {
        return std::nullopt;
    }
    const QString kind = map->value(QLatin1StringView(WireContract::FieldKind)).toString();
    SettingsRepository::Operation operation;
    operation.key = key;
    if (kind == QLatin1StringView(WireContract::OperationKindRemove)) {
        operation.remove = true;
        return operation;
    }
    if (kind != QLatin1StringView(WireContract::OperationKindSet)) {
        *error = QStringLiteral("operation kind must be \"set\" or \"remove\"");
        return std::nullopt;
    }
    if (!map->contains(QLatin1StringView(WireContract::FieldValue))) {
        *error = QStringLiteral("a \"set\" operation requires a value");
        return std::nullopt;
    }
    auto value = decodeBoundedJsonValue(
        map->value(QLatin1StringView(WireContract::FieldValue)), error);
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
        return malformedReply(QStringLiteral("GetSnapshot requires between 1 and %1 keys")
                                  .arg(WireContract::MaximumRequestedKeys),
                              quint32(m_repository.schema().version()),
                              m_repository.epoch(), m_repository.revision());
    }
    QSet<QString> seenKeys;
    for (const auto &key : keys) {
        QString keyError;
        if (!BoundedSettingsValueCodec::validateKey(key, &keyError) || seenKeys.contains(key)) {
            return malformedReply(seenKeys.contains(key)
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
    if (!valuesFitAggregateBounds(snapshot.values,
                                  WireContract::MaximumSnapshotValueBytes,
                                  WireContract::MaximumSnapshotValueNodes,
                                  &boundsError)) {
        return malformedReply(boundsError, quint32(m_repository.schema().version()),
                              m_repository.epoch(), m_repository.revision());
    }
    return encodeSnapshot(snapshot);
}

QVariantMap SettingsObject::encodeSnapshot(const RepositorySnapshot &snapshot) const
{
    return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
            {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
            {QLatin1StringView(WireContract::FieldRevision), snapshot.revision},
            {QLatin1StringView(WireContract::FieldValues), snapshot.values},
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
    qsizetype aggregateBytes = 0;
    qsizetype aggregateNodes = 0;
    for (const auto &entry : operations) {
        QString decodeError;
        auto operation = decodeOperation(entry, &decodeError);
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
        if (!operation->remove) {
            BoundedSettingsValueCodec::Usage usage;
            QString usageError;
            if (!BoundedSettingsValueCodec::validateValue(operation->value, &usageError, &usage)
                || aggregateBytes > WireContract::MaximumTransactionValueBytes - usage.bytes) {
                return malformedReply(usageError.isEmpty()
                                          ? QStringLiteral("transaction values exceed %1 aggregate bytes")
                                                .arg(WireContract::MaximumTransactionValueBytes)
                                          : usageError,
                                      quint32(m_repository.schema().version()),
                                      m_repository.epoch(), m_repository.revision());
            }
            if (aggregateNodes > WireContract::MaximumTransactionValueNodes - usage.nodes) {
                return malformedReply(
                    QStringLiteral("transaction values exceed %1 aggregate nodes")
                        .arg(WireContract::MaximumTransactionValueNodes),
                    quint32(m_repository.schema().version()),
                    m_repository.epoch(), m_repository.revision());
            }
            aggregateBytes += usage.bytes;
            aggregateNodes += usage.nodes;
        }
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
    return encodeCommitResult(result);
}

QVariantMap SettingsObject::encodeCommitResult(const RepositoryCommitResult &result) const
{
    return {{QLatin1StringView(WireContract::FieldStatus), static_cast<quint32>(wireStatusFor(result.status))},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(m_repository.schema().version())},
            {QLatin1StringView(WireContract::FieldEpoch), m_repository.epoch()},
            {QLatin1StringView(WireContract::FieldRevisionBefore), result.revisionBefore},
            {QLatin1StringView(WireContract::FieldRevisionAfter), result.revisionAfter},
            {QLatin1StringView(WireContract::FieldValues), result.currentValues},
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
