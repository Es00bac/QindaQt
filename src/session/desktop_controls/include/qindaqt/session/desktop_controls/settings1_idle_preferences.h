// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "idle_display_preferences.h"

#include <qindaqt/services/settings_client/settings_client.h>

namespace QindaQt::Session::DesktopControls {

// Production preference truth: one purpose-scoped Settings1 client reading
// only `power.idleDisplayOffMinutes`. An absent service, a lost owner, or a
// missing value falls back to the documented default; the injected client
// must be scoped to exactly that key so an unrelated layer can never leak in.
class Settings1IdlePreferences final : public IdlePreferencesProvider {
    Q_OBJECT

public:
    static const QStringList &scopedKey();

    explicit Settings1IdlePreferences(
        Services::SettingsClient::SettingsClient &client,
        QObject *parent = nullptr);
    ~Settings1IdlePreferences() override;

    Settings1IdlePreferences(const Settings1IdlePreferences &) = delete;
    Settings1IdlePreferences &operator=(const Settings1IdlePreferences &) = delete;

    [[nodiscard]] IdleDisplayPreferences currentPreferences() const override;
    void refresh() override;

private Q_SLOTS:
    void onSnapshotChanged();

private:
    Services::SettingsClient::SettingsClient &m_client;
    // Starts at the documented schema default so an unresponsive Settings1
    // owner yields the standard desktop behavior rather than disabling it.
    IdleDisplayPreferences m_current{true, IdleDisplayPreferences::defaultTimeoutMinutes()};
};

} // namespace QindaQt::Session::DesktopControls
