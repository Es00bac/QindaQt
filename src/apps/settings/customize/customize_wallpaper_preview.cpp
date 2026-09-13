// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_wallpaper_preview.h"

#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/apps/settings_appearance/wallpaper_catalog.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QFileInfo>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Apps::SettingsCustomize {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::SettingsClient;
using SettingsAppearance::discoverBundledWallpapers;
using SettingsAppearance::wallpaperModeFromToken;
using SettingsAppearance::wallpaperModeToken;

CustomizeWallpaperPreview::CustomizeWallpaperPreview(
    SettingsClient &client, QStringList wallpaperRoots, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_roots(std::move(wallpaperRoots))
{
    connect(&m_client, &SettingsClient::snapshotChanged, this,
            [this] { recompute(); });
    connect(&m_client, &SettingsClient::stateChanged, this,
            [this] { recompute(); });
    recompute();
}

QString CustomizeWallpaperPreview::resolvePreference(
    const QString &preference) const
{
    if (preference.startsWith(QStringLiteral("qindaqt:"))) {
        const QString name = preference.sliced(8);
        if (name.isEmpty() || name.contains(QLatin1Char('/'))) {
            return {};
        }
        const QVariantList bundled = discoverBundledWallpapers(m_roots);
        for (const QVariant &entry : bundled) {
            const QVariantMap map = entry.toMap();
            if (map.value(QStringLiteral("value")).toString() == preference) {
                return map.value(QStringLiteral("path")).toString();
            }
        }
        return {};
    }
    const QFileInfo file(preference);
    return file.isAbsolute() && file.isFile() && file.isReadable()
        ? file.absoluteFilePath()
        : QString{};
}

void CustomizeWallpaperPreview::recompute()
{
    QUrl source;
    QString mode = QStringLiteral("scaled");
    QString status = QStringLiteral("unavailable");

    const auto &snapshot = m_client.snapshot();
    if (m_client.state() == ClientState::Ready && snapshot.has_value()) {
        const QVariant wallpaperValue = snapshot->values.value(
            QLatin1String(SettingsAppearance::AppearanceKeys::Wallpaper));
        if (wallpaperValue.metaType().id() != QMetaType::QString) {
            // AGENT-GUARD: a missing or mistyped value is not a "no
            // wallpaper" signal; only an explicit empty string means none.
            status = QStringLiteral("invalid");
        } else {
            const QString preference = wallpaperValue.toString();
            if (preference.isEmpty()) {
                status = QStringLiteral("none");
            } else {
                const QVariant modeValue = snapshot->values.value(
                    QLatin1String(SettingsAppearance::AppearanceKeys::WallpaperMode));
                const auto decodedMode = modeValue.metaType().id()
                            == QMetaType::QString
                    ? wallpaperModeFromToken(modeValue.toString())
                    : std::nullopt;
                if (!decodedMode.has_value()) {
                    status = QStringLiteral("invalid");
                } else {
                    const QString resolved = resolvePreference(preference);
                    if (resolved.isEmpty()) {
                        status = QStringLiteral("invalid");
                    } else {
                        source = QUrl::fromLocalFile(resolved);
                        mode = wallpaperModeToken(*decodedMode);
                        status = QStringLiteral("ready");
                    }
                }
            }
        }
    }

    if (source == m_source && mode == m_mode && status == m_status) {
        return;
    }
    m_source = source;
    m_mode = mode;
    m_status = status;
    Q_EMIT changed();
}

} // namespace QindaQt::Apps::SettingsCustomize
