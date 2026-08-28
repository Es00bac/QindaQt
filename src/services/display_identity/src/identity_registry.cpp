// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_identity/identity_registry.h>

#include <qindaqt/services/display_identity/identity_limits.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QindaQt::DisplayIdentity
{
namespace
{

RegistryResult failure(const RegistryError error, const char *reason)
{
    return {.registry = {},
            .error = error,
            .reasonCode = QString::fromLatin1(reason),
            .migrated = false};
}

bool boundedText(const QString &value, const qsizetype maximum, const bool required = false)
{
    if (required && value.isEmpty()) {
        return false;
    }
    if (value.contains(QChar::Null) || value.toUtf8().size() > maximum) {
        return false;
    }
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

bool validAlias(const QString &alias)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$"));
    return alias.isEmpty()
        || (alias.toUtf8().size() <= kMaxAliasUtf8Bytes && expression.match(alias).hasMatch());
}

RegistryResult validateRegistry(Registry registry, const bool migrated)
{
    if (registry.schemaVersion != kRegistrySchemaVersion) {
        return failure(RegistryError::UnsupportedSchema, "unsupported-registry-schema");
    }
    if (registry.entries.size() > kMaxRegistryEntries) {
        return failure(RegistryError::TooManyEntries, "too-many-registry-entries");
    }
    QSet<QString> stableIds;
    QSet<QString> aliases;
    for (const RegistryEntry &entry : registry.entries) {
        if (!boundedText(entry.stableId, kMaxStableIdUtf8Bytes, true)
            || !validAlias(entry.alias) || !boundedText(entry.label, kMaxLabelUtf8Bytes)
            || !boundedText(entry.lastConnector, kMaxConnectorUtf8Bytes)
            || !boundedText(entry.manufacturer, kMaxManufacturerUtf8Bytes)
            || !boundedText(entry.model, kMaxModelUtf8Bytes) || entry.seenSequence == 0) {
            return failure(RegistryError::InvalidEntry, "invalid-registry-entry");
        }
        if (stableIds.contains(entry.stableId)) {
            return failure(RegistryError::DuplicateStableId, "duplicate-stable-id");
        }
        if (!entry.alias.isEmpty() && entry.ambiguous) {
            return failure(RegistryError::AmbiguousAlias, "alias-on-ambiguous-output");
        }
        if (!entry.alias.isEmpty() && aliases.contains(entry.alias)) {
            return failure(RegistryError::DuplicateAlias, "duplicate-output-alias");
        }
        stableIds.insert(entry.stableId);
        if (!entry.alias.isEmpty()) {
            aliases.insert(entry.alias);
        }
    }
    if (aliases.size() > kMaxAliases) {
        return failure(RegistryError::TooManyAliases, "too-many-output-aliases");
    }
    std::sort(registry.entries.begin(), registry.entries.end(),
              [](const RegistryEntry &left, const RegistryEntry &right) {
                  return left.stableId < right.stableId;
              });
    return {.registry = std::move(registry),
            .error = RegistryError::None,
            .reasonCode = {},
            .migrated = migrated};
}

bool exactSchemaVersion(const QJsonValue &value, quint32 &version)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (number != std::floor(number) || number < 1.0
        || number > static_cast<double>(std::numeric_limits<quint32>::max())) {
        return false;
    }
    version = static_cast<quint32>(number);
    return true;
}

bool readSequence(const QJsonValue &value, quint64 &sequence)
{
    if (!value.isString()) {
        return false;
    }
    bool converted = false;
    const quint64 decoded = value.toString().toULongLong(&converted, 10);
    if (!converted || decoded == 0 || QString::number(decoded) != value.toString()) {
        return false;
    }
    sequence = decoded;
    return true;
}

bool readEntry(const QJsonObject &object, RegistryEntry &entry, const bool legacy)
{
    const QSet<QString> expected = legacy
        ? QSet<QString>{QStringLiteral("stableId"), QStringLiteral("label"),
                        QStringLiteral("lastConnector"), QStringLiteral("manufacturer"),
                        QStringLiteral("model"), QStringLiteral("internal"),
                        QStringLiteral("seenSequence")}
        : QSet<QString>{QStringLiteral("stableId"), QStringLiteral("alias"),
                        QStringLiteral("label"), QStringLiteral("lastConnector"),
                        QStringLiteral("manufacturer"), QStringLiteral("model"),
                        QStringLiteral("internal"), QStringLiteral("ambiguous"),
                        QStringLiteral("seenSequence")};
    QSet<QString> actual;
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        actual.insert(iterator.key());
    }
    if (actual != expected
        || !object.value(QStringLiteral("stableId")).isString()
        || (!legacy && !object.value(QStringLiteral("alias")).isString())
        || !object.value(QStringLiteral("label")).isString()
        || !object.value(QStringLiteral("lastConnector")).isString()
        || !object.value(QStringLiteral("manufacturer")).isString()
        || !object.value(QStringLiteral("model")).isString()
        || !object.value(QStringLiteral("internal")).isBool()
        || (!legacy && !object.value(QStringLiteral("ambiguous")).isBool())) {
        return false;
    }
    entry.stableId = object.value(QStringLiteral("stableId")).toString();
    entry.alias = legacy ? QString{} : object.value(QStringLiteral("alias")).toString();
    entry.label = object.value(QStringLiteral("label")).toString();
    entry.lastConnector = object.value(QStringLiteral("lastConnector")).toString();
    entry.manufacturer = object.value(QStringLiteral("manufacturer")).toString();
    entry.model = object.value(QStringLiteral("model")).toString();
    entry.internal = object.value(QStringLiteral("internal")).toBool();
    entry.ambiguous = legacy ? false : object.value(QStringLiteral("ambiguous")).toBool();
    return readSequence(object.value(QStringLiteral("seenSequence")), entry.seenSequence);
}

QJsonObject writeEntry(const RegistryEntry &entry)
{
    return {{QStringLiteral("stableId"), entry.stableId},
            {QStringLiteral("alias"), entry.alias},
            {QStringLiteral("label"), entry.label},
            {QStringLiteral("lastConnector"), entry.lastConnector},
            {QStringLiteral("manufacturer"), entry.manufacturer},
            {QStringLiteral("model"), entry.model},
            {QStringLiteral("internal"), entry.internal},
            {QStringLiteral("ambiguous"), entry.ambiguous},
            {QStringLiteral("seenSequence"), QString::number(entry.seenSequence)}};
}

} // namespace

RegistryResult decodeRegistry(const QJsonObject &document)
{
    if (QJsonDocument(document).toJson(QJsonDocument::Compact).size() > kMaxRegistryJsonBytes) {
        return failure(RegistryError::InvalidShape, "registry-document-too-large");
    }
    quint32 schemaVersion = 0;
    if (!exactSchemaVersion(document.value(QStringLiteral("schemaVersion")), schemaVersion)) {
        return failure(RegistryError::InvalidSchema, "invalid-registry-schema");
    }
    if (schemaVersion != 1 && schemaVersion != kRegistrySchemaVersion) {
        return failure(RegistryError::UnsupportedSchema, "unsupported-registry-schema");
    }
    const QSet<QString> expected{QStringLiteral("schemaVersion"), QStringLiteral("outputs")};
    QSet<QString> actual;
    for (auto iterator = document.constBegin(); iterator != document.constEnd(); ++iterator) {
        actual.insert(iterator.key());
    }
    if (actual != expected
        || !document.value(QStringLiteral("outputs")).isArray()) {
        return failure(RegistryError::InvalidShape, "invalid-registry-document");
    }
    const QJsonArray entries = document.value(QStringLiteral("outputs")).toArray();
    if (entries.size() > kMaxRegistryEntries) {
        return failure(RegistryError::TooManyEntries, "too-many-registry-entries");
    }
    Registry registry;
    registry.schemaVersion = kRegistrySchemaVersion;
    registry.entries.reserve(entries.size());
    for (const QJsonValue &value : entries) {
        if (!value.isObject()) {
            return failure(RegistryError::InvalidEntry, "invalid-registry-entry");
        }
        RegistryEntry entry;
        if (!readEntry(value.toObject(), entry, schemaVersion == 1)) {
            return failure(RegistryError::InvalidEntry, "invalid-registry-entry");
        }
        registry.entries.push_back(std::move(entry));
    }
    return validateRegistry(std::move(registry), schemaVersion == 1);
}

RegistryResult encodeRegistry(const Registry &registry, QJsonObject &destination)
{
    RegistryResult validation = validateRegistry(registry, false);
    if (!validation.succeeded()) {
        return validation;
    }
    QJsonArray entries;
    for (const RegistryEntry &entry : validation.registry.entries) {
        entries.push_back(writeEntry(entry));
    }
    QJsonObject encoded{{QStringLiteral("schemaVersion"),
                         static_cast<int>(kRegistrySchemaVersion)},
                        {QStringLiteral("outputs"), entries}};
    if (QJsonDocument(encoded).toJson(QJsonDocument::Compact).size() > kMaxRegistryJsonBytes) {
        return failure(RegistryError::InvalidShape, "registry-document-too-large");
    }
    destination = std::move(encoded);
    return validation;
}

RegistryResult reconcileRegistry(const Registry &registry,
                                 const QList<ResolvedOutput> &connectedOutputs,
                                 const quint64 seenSequence)
{
    RegistryResult result = validateRegistry(registry, false);
    if (!result.succeeded()) {
        return result;
    }
    if (seenSequence == 0 || seenSequence == std::numeric_limits<quint64>::max()) {
        return failure(RegistryError::SequenceExhausted, "invalid-seen-sequence");
    }
    if (connectedOutputs.size() > kMaxConnectedOutputs) {
        return failure(RegistryError::TooManyEntries, "too-many-connected-outputs");
    }

    QSet<QString> connectedIds;
    for (const ResolvedOutput &output : connectedOutputs) {
        if (!boundedText(output.stableId, kMaxStableIdUtf8Bytes, true)
            || !boundedText(output.connectorName, kMaxConnectorUtf8Bytes, true)
            || !boundedText(output.manufacturer, kMaxManufacturerUtf8Bytes)
            || !boundedText(output.model, kMaxModelUtf8Bytes)
            || static_cast<int>(output.source) < static_cast<int>(IdentitySource::EdidIdentifier)
            || static_cast<int>(output.source) > static_cast<int>(IdentitySource::ConnectorHash)) {
            return failure(RegistryError::InvalidEntry, "invalid-connected-output");
        }
        if (connectedIds.contains(output.stableId)) {
            return failure(RegistryError::DuplicateStableId,
                           "duplicate-connected-stable-id");
        }
        connectedIds.insert(output.stableId);
        auto iterator = std::find_if(result.registry.entries.begin(),
                                     result.registry.entries.end(),
                                     [&](const RegistryEntry &entry) {
                                         return entry.stableId == output.stableId;
                                     });
        if (iterator == result.registry.entries.end()) {
            result.registry.entries.push_back(
                {.stableId = output.stableId,
                 .alias = {},
                 .label = output.model.isEmpty() ? output.connectorName : output.model,
                 .lastConnector = output.connectorName,
                 .manufacturer = output.manufacturer,
                 .model = output.model,
                 .internal = output.internal,
                 .ambiguous = output.ambiguous,
                 .seenSequence = seenSequence});
            continue;
        }
        iterator->lastConnector = output.connectorName;
        iterator->manufacturer = output.manufacturer;
        iterator->model = output.model;
        iterator->internal = output.internal;
        iterator->ambiguous = output.ambiguous;
        iterator->seenSequence = seenSequence;
        if (iterator->ambiguous) {
            iterator->alias.clear();
        }
    }
    while (result.registry.entries.size() > kMaxRegistryEntries) {
        auto evict = std::min_element(
            result.registry.entries.begin(), result.registry.entries.end(),
            [&](const RegistryEntry &left, const RegistryEntry &right) {
                const bool leftConnected = connectedIds.contains(left.stableId);
                const bool rightConnected = connectedIds.contains(right.stableId);
                if (leftConnected != rightConnected) {
                    return !leftConnected;
                }
                if (left.seenSequence != right.seenSequence) {
                    return left.seenSequence < right.seenSequence;
                }
                return left.stableId < right.stableId;
            });
        if (evict == result.registry.entries.end() || connectedIds.contains(evict->stableId)) {
            return failure(RegistryError::TooManyEntries, "registry-capacity-exhausted");
        }
        result.registry.entries.erase(evict);
    }
    return validateRegistry(std::move(result.registry), false);
}

RegistryResult setAlias(const Registry &registry, const QString &stableId,
                        const QString &alias)
{
    RegistryResult result = validateRegistry(registry, false);
    if (!result.succeeded()) {
        return result;
    }
    if (!validAlias(alias)) {
        return failure(RegistryError::InvalidAlias, "invalid-output-alias");
    }
    auto iterator = std::find_if(result.registry.entries.begin(), result.registry.entries.end(),
                                 [&](const RegistryEntry &entry) {
                                     return entry.stableId == stableId;
                                 });
    if (iterator == result.registry.entries.end()) {
        return failure(RegistryError::UnknownStableId, "unknown-stable-id");
    }
    if (!alias.isEmpty() && iterator->ambiguous) {
        return failure(RegistryError::AmbiguousAlias, "alias-on-ambiguous-output");
    }
    if (!alias.isEmpty()
        && std::any_of(result.registry.entries.cbegin(), result.registry.entries.cend(),
                       [&](const RegistryEntry &entry) {
                           return entry.stableId != stableId && entry.alias == alias;
                       })) {
        return failure(RegistryError::DuplicateAlias, "duplicate-output-alias");
    }
    iterator->alias = alias;
    return validateRegistry(std::move(result.registry), false);
}

} // namespace QindaQt::DisplayIdentity
