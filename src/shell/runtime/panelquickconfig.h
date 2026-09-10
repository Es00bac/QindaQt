// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Shell {

class SettingsRouteLauncher;

// AGENT-CONTRACT: the panel right-click configuration facade. One instance is
// shared by every panel surface; it owns no panel state itself — the single
// source of truth is the Settings1 `panels.configuration` object value, whose
// per-panel entries are strictly validated here (unknown keys and out-of-range
// values are dropped, never partially trusted). Consumers merge the returned
// map over their own defaults. All methods are GUI-thread confined; the
// settings client is borrowed and must outlive this object. The settings
// route launcher is a borrowed optional: without it (a shell started without
// the notification trust bundle) openCustomize simply reports failure.
class PanelQuickConfig final : public QObject {
    Q_OBJECT

public:
    PanelQuickConfig(Services::SettingsClient::SettingsClient &settings,
                     SettingsRouteLauncher *settingsRoutes,
                     QObject *parent = nullptr);

    // Validated per-panel settings: transparency (bool), dockZoom (bool),
    // dockTileSize (int 56..64). Absent keys are simply missing from the map.
    Q_INVOKABLE QVariantMap panelSettings(const QString &panelId) const;

    // Persists one validated setting for the panel. Returns false (and writes
    // nothing) for unknown keys or out-of-range values.
    Q_INVOKABLE bool setPanelSetting(const QString &panelId,
                                     const QString &key, const QVariant &value);

    // Opens the first-party Settings app on the Customize route.
    Q_INVOKABLE bool openCustomize();

    void start();

Q_SIGNALS:
    void panelSettingsChanged();

private:
    void decodeFromSnapshot();
    bool persistPanelConfig(const QString &panelId, const QVariantMap &config);

    Services::SettingsClient::SettingsClient &m_settings;
    SettingsRouteLauncher *m_settingsRoutes = nullptr;
    QVariantMap m_resolved;
    bool m_started = false;
};

} // namespace QindaQt::Shell
