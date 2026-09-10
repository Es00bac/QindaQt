// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelquickconfig.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "settingsroutelauncher.h"

#include <QDebug>

namespace QindaQt::Shell {

namespace {

constexpr auto kKey = "panels.configuration";
constexpr int kMinTileSize = 56;
constexpr int kMaxTileSize = 64;

// Normalizes one raw per-panel entry: keeps only the documented keys and only
// values inside their schema bounds; everything else is dropped so a hostile
// snapshot can never smuggle settings into panel presentation.
QVariantMap sanitizePanelEntry(const QVariant &entry)
{
    if (entry.metaType().id() != QMetaType::QVariantMap) {
        return {};
    }
    QVariantMap sanitized;
    const QVariantMap map = entry.toMap();
    const auto transparency = map.constFind(QStringLiteral("transparency"));
    if (transparency != map.constEnd()
        && transparency->metaType().id() == QMetaType::Bool) {
        sanitized.insert(QStringLiteral("transparency"), transparency->toBool());
    }
    const auto dockZoom = map.constFind(QStringLiteral("dockZoom"));
    if (dockZoom != map.constEnd()
        && dockZoom->metaType().id() == QMetaType::Bool) {
        sanitized.insert(QStringLiteral("dockZoom"), dockZoom->toBool());
    }
    const auto tileSize = map.constFind(QStringLiteral("dockTileSize"));
    if (tileSize != map.constEnd()) {
        bool valid = false;
        const int value = tileSize->toInt(&valid);
        if (valid
            && (tileSize->metaType().id() == QMetaType::Int
                || tileSize->metaType().id() == QMetaType::LongLong
                || tileSize->metaType().id() == QMetaType::Double)) {
            sanitized.insert(QStringLiteral("dockTileSize"),
                             qBound(kMinTileSize, value, kMaxTileSize));
        }
    }
    return sanitized;
}

} // namespace

PanelQuickConfig::PanelQuickConfig(
    Services::SettingsClient::SettingsClient &settings,
    SettingsRouteLauncher *settingsRoutes, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_settingsRoutes(settingsRoutes)
{
}

void PanelQuickConfig::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    connect(&m_settings,
            &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &PanelQuickConfig::decodeFromSnapshot);
    decodeFromSnapshot();
}

QVariantMap PanelQuickConfig::panelSettings(const QString &panelId) const {
    const auto entry = m_resolved.constFind(panelId);
    if (entry == m_resolved.constEnd()) {
        return {};
    }
    return entry->toMap();
}

bool PanelQuickConfig::setPanelSetting(const QString &panelId,
                                       const QString &key,
                                       const QVariant &value) {
    if (panelId.trimmed().isEmpty()) {
        return false;
    }
    QVariantMap sanitized = sanitizePanelEntry(QVariantMap{{key, value}});
    if (sanitized.isEmpty()) {
        return false;
    }
    QVariantMap merged = panelSettings(panelId);
    for (auto it = sanitized.constBegin(); it != sanitized.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return persistPanelConfig(panelId, merged);
}

bool PanelQuickConfig::openCustomize() {
    return m_settingsRoutes != nullptr && m_settingsRoutes->openCustomize();
}

bool PanelQuickConfig::persistPanelConfig(const QString &panelId,
                                          const QVariantMap &config) {
    QVariantMap current;
    const auto &snapshot = m_settings.snapshot();
    if (snapshot) {
        current = snapshot->values.value(QLatin1String(kKey)).toMap();
    }
    QVariantMap merged = current;
    merged.insert(panelId, config);
    if (!current.isEmpty() && merged == current) {
        return true;
    }
    QString error;
    if (!m_settings.setUserValue(QLatin1String(kKey), merged, &error)) {
        qWarning().noquote()
            << "QindaQt shell could not persist the panel configuration:"
            << error;
        return false;
    }
    m_resolved.insert(panelId, config);
    Q_EMIT panelSettingsChanged();
    return true;
}

void PanelQuickConfig::decodeFromSnapshot() {
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot) {
        return;
    }
    const QVariant value = snapshot->values.value(QLatin1String(kKey));
    QVariantMap resolved;
    if (value.metaType().id() == QMetaType::QVariantMap) {
        const QVariantMap raw = value.toMap();
        for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
            const QVariantMap sanitized = sanitizePanelEntry(it.value());
            if (!sanitized.isEmpty()) {
                resolved.insert(it.key(), sanitized);
            }
        }
    }
    if (resolved != m_resolved) {
        m_resolved = resolved;
        Q_EMIT panelSettingsChanged();
    }
}

} // namespace QindaQt::Shell
