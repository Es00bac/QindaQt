// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "screensaver_preferences.h"

#include <qindaqt/services/settings_client/settings_client.h>

namespace QindaQt::Session::DesktopControls {

class ScreensaverCatalog;

// Production preference truth: one purpose-scoped Settings1 client reading
// only `power.screensaver` and `power.screensaverMinutes`. Settings1 rejects a
// whole snapshot on one unknown key (ADR-0126), so this client is scoped to
// exactly that pair and never widened.
//
// The catalog is the discovery seam (ADR-0226): a persisted token resolves to
// a saver only when the catalog knows it, so a stale, hand-edited, or
// downgraded value reads back as "none" and never reaches QProcess.
//
// Unlike the display-off preference, an absent service or a lost owner leaves
// the saver disabled: starting a program on a guessed preference is worse than
// doing nothing, so only a confirmed snapshot can arm it.
class Settings1ScreensaverPreferences final : public ScreensaverPreferencesProvider {
    Q_OBJECT

public:
    static const QStringList &scopedKeys();

    explicit Settings1ScreensaverPreferences(
        Services::SettingsClient::SettingsClient &client,
        const ScreensaverCatalog &catalog,
        QObject *parent = nullptr);
    ~Settings1ScreensaverPreferences() override;

    Settings1ScreensaverPreferences(const Settings1ScreensaverPreferences &) = delete;
    Settings1ScreensaverPreferences &operator=(const Settings1ScreensaverPreferences &) = delete;

    [[nodiscard]] ScreensaverPreferences currentPreferences() const override;
    void refresh() override;

private Q_SLOTS:
    void onSnapshotChanged();

private:
    Services::SettingsClient::SettingsClient &m_client;
    const ScreensaverCatalog &m_catalog;
    // Starts disabled; the first confirmed snapshot supplies real truth.
    ScreensaverPreferences m_current{};
};

} // namespace QindaQt::Session::DesktopControls
