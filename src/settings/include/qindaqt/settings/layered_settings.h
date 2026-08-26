// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_schema.h"
#include "qindaqt/settings/settings_transaction.h"

#include <QVariantMap>

#include <array>
#include <optional>

namespace QindaQt::Settings {

class LayeredSettings final {
public:
    explicit LayeredSettings(SettingsSchema schema);

    [[nodiscard]] const SettingsSchema &schema() const noexcept { return m_schema; }
    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] QVariant value(const QString &key) const;
    [[nodiscard]] std::optional<SettingLayer> sourceLayer(const QString &key) const;
    [[nodiscard]] QVariantMap effectiveValues() const;
    [[nodiscard]] QVariantMap layerValues(SettingLayer layer) const;

    [[nodiscard]] SettingsTransaction beginTransaction(SettingLayer layer) const;
    [[nodiscard]] CommitResult commit(const SettingsTransaction &transaction);

    // AGENT-NOTE: Complete replacement is the startup/load path. Interactive
    // edits use optimistic transactions so concurrent clients cannot lose changes.
    [[nodiscard]] CommitResult replaceLayer(SettingLayer layer, const QVariantMap &values);
    [[nodiscard]] CommitResult clearSessionOverrides();

private:
    struct ResolvedSetting final {
        QVariant value;
        SettingLayer source = SettingLayer::SystemDefaults;
        bool found = false;
    };

    using Layers = std::array<QVariantMap, 4>;

    [[nodiscard]] static std::size_t layerIndex(SettingLayer layer) noexcept;
    [[nodiscard]] static ResolvedSetting resolve(const QString &key, const Layers &layers);
    [[nodiscard]] CommitResult applyLayer(SettingLayer layer, QVariantMap normalizedValues);

    SettingsSchema m_schema;
    Layers m_layers;
    quint64 m_revision = 0;
};

} // namespace QindaQt::Settings
