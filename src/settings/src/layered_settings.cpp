// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/layered_settings.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Settings {
namespace {

CommitResult rejected(CommitStatus status,
                      SettingLayer layer,
                      quint64 revision,
                      QString message,
                      ValidationResult validation = {})
{
    CommitResult result;
    result.status = status;
    result.message = std::move(message);
    result.validation = std::move(validation);
    result.changes.revisionBefore = revision;
    result.changes.revisionAfter = revision;
    result.changes.editedLayer = layer;
    return result;
}

} // namespace

LayeredSettings::LayeredSettings(SettingsSchema schema)
    : m_schema(std::move(schema))
{
    // AGENT-CONTRACT: Array order is the precedence order. Resolution walks
    // backward, so changing the enum values or this layout changes public behavior.
    m_layers.at(layerIndex(SettingLayer::SystemDefaults)) = m_schema.systemDefaults();
}

QVariant LayeredSettings::value(const QString &key) const
{
    return resolve(key, m_layers).value;
}

std::optional<SettingLayer> LayeredSettings::sourceLayer(const QString &key) const
{
    const auto resolved = resolve(key, m_layers);
    if (!resolved.found) {
        return std::nullopt;
    }
    return resolved.source;
}

QVariantMap LayeredSettings::effectiveValues() const
{
    QVariantMap values;
    for (auto iterator = m_schema.definitions().cbegin(); iterator != m_schema.definitions().cend();
         ++iterator) {
        values.insert(iterator.key(), resolve(iterator.key(), m_layers).value);
    }
    return values;
}

QVariantMap LayeredSettings::layerValues(SettingLayer layer) const
{
    return m_layers.at(layerIndex(layer));
}

SettingsTransaction LayeredSettings::beginTransaction(SettingLayer layer) const
{
    return SettingsTransaction(layer, m_revision);
}

CommitResult LayeredSettings::commit(const SettingsTransaction &transaction)
{
    if (!isMutableLayer(transaction.layer())) {
        return rejected(CommitStatus::ReadOnlyLayer,
                        transaction.layer(),
                        m_revision,
                        QStringLiteral("system defaults are owned by the schema"));
    }
    if (transaction.baseRevision() != m_revision) {
        return rejected(CommitStatus::Conflict,
                        transaction.layer(),
                        m_revision,
                        QStringLiteral("transaction revision %1 is stale; current revision is %2")
                            .arg(transaction.baseRevision())
                            .arg(m_revision));
    }

    QVariantMap candidate = layerValues(transaction.layer());
    ValidationResult validation;
    for (auto iterator = transaction.operations().cbegin();
         iterator != transaction.operations().cend();
         ++iterator) {
        if (!m_schema.contains(iterator.key())) {
            validation.add(iterator.key(),
                           QStringLiteral("unknown-key"),
                           QStringLiteral("setting is not defined by the active schema"));
            continue;
        }
        if (iterator->kind == SettingOperationKind::Remove) {
            candidate.remove(iterator.key());
            continue;
        }

        const auto normalized = m_schema.normalizedValue(iterator.key(), iterator->value, &validation);
        if (normalized.has_value()) {
            candidate.insert(iterator.key(), *normalized);
        }
    }
    if (!validation.isValid()) {
        return rejected(CommitStatus::ValidationFailed,
                        transaction.layer(),
                        m_revision,
                        validation.summary(),
                        validation);
    }

    // AGENT-GUARD: No model state changes before all operations validate. Shell
    // previews and settings clients cannot recover from partially applied batches.
    return applyLayer(transaction.layer(), std::move(candidate));
}

CommitResult LayeredSettings::replaceLayer(SettingLayer layer, const QVariantMap &values)
{
    if (!isMutableLayer(layer)) {
        return rejected(CommitStatus::ReadOnlyLayer,
                        layer,
                        m_revision,
                        QStringLiteral("system defaults are owned by the schema"));
    }

    ValidationResult validation;
    const auto normalized = m_schema.normalizedLayer(values, &validation);
    if (!normalized.has_value()) {
        return rejected(CommitStatus::ValidationFailed,
                        layer,
                        m_revision,
                        validation.summary(),
                        validation);
    }
    return applyLayer(layer, *normalized);
}

CommitResult LayeredSettings::clearSessionOverrides()
{
    return replaceLayer(SettingLayer::SessionOverrides, {});
}

std::size_t LayeredSettings::layerIndex(SettingLayer layer) noexcept
{
    return static_cast<std::size_t>(layer);
}

LayeredSettings::ResolvedSetting LayeredSettings::resolve(const QString &key, const Layers &layers)
{
    for (auto iterator = layers.crbegin(); iterator != layers.crend(); ++iterator) {
        const auto value = iterator->constFind(key);
        if (value != iterator->cend()) {
            const auto index = static_cast<std::size_t>(std::distance(iterator, layers.crend()) - 1);
            return {value.value(), static_cast<SettingLayer>(index), true};
        }
    }
    return {};
}

CommitResult LayeredSettings::applyLayer(SettingLayer layer, QVariantMap normalizedValues)
{
    const auto index = layerIndex(layer);
    const auto &beforeLayer = m_layers.at(index);
    QSet<QString> keys;
    for (auto iterator = beforeLayer.cbegin(); iterator != beforeLayer.cend(); ++iterator) {
        keys.insert(iterator.key());
    }
    for (auto iterator = normalizedValues.cbegin(); iterator != normalizedValues.cend(); ++iterator) {
        keys.insert(iterator.key());
    }

    QStringList touched;
    for (const auto &key : keys) {
        const bool wasPresent = beforeLayer.contains(key);
        const bool isPresent = normalizedValues.contains(key);
        if (wasPresent != isPresent
            || (wasPresent && beforeLayer.value(key) != normalizedValues.value(key))) {
            touched.append(key);
        }
    }
    std::sort(touched.begin(), touched.end());

    CommitResult result;
    result.status = CommitStatus::Applied;
    result.changes.revisionBefore = m_revision;
    result.changes.revisionAfter = m_revision;
    result.changes.editedLayer = layer;
    result.changes.touchedKeys = touched;
    if (touched.isEmpty()) {
        return result;
    }

    auto afterLayers = m_layers;
    afterLayers.at(index) = normalizedValues;
    for (const auto &key : touched) {
        const auto before = resolve(key, m_layers);
        const auto after = resolve(key, afterLayers);
        if (before.value == after.value && before.source == after.source) {
            continue;
        }
        const auto *definition = m_schema.definition(key);
        result.changes.effectiveChanges.append({key,
                                                definition->domain,
                                                before.value,
                                                after.value,
                                                before.source,
                                                after.source});
    }

    m_layers = std::move(afterLayers);
    ++m_revision;
    result.changes.revisionAfter = m_revision;
    return result;
}

} // namespace QindaQt::Settings
