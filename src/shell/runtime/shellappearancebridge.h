// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "shellpreferencevalues.h"

#include <QObject>

#include <optional>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Themes {
class ThemeCatalog;
}

namespace QindaQt::Shell {

class ShellTokenPublisher;

// Composes confirmed Settings1 appearance/accessibility preferences into shell
// token publication. Reacts only to complete valid snapshots: owner loss, bus
// failure, and malformed values retain the last confirmed safe state instead
// of reverting to defaults (the same retention rule as the notification
// quieting bridge). An explicit --theme command-line selection outranks the
// appearance.theme preference for the process lifetime.
//
// Layout profile changes are surfaced as layoutProfilePreferenceChanged for
// the runtime to adopt through surface reconciliation; the initial snapshot
// only latches the baseline and never re-triggers adoption.
class ShellAppearanceBridge final : public QObject {
    Q_OBJECT

public:
    ShellAppearanceBridge(Services::SettingsClient::SettingsClient &settings,
                          Themes::ThemeCatalog &themes,
                          ShellTokenPublisher &tokens, bool themeLockedByCli,
                          QObject *parent = nullptr);

    [[nodiscard]] const std::optional<ShellPreferenceValues> &lastConfirmed()
        const noexcept
    {
        return m_lastConfirmed;
    }

signals:
    // Emitted after each newly confirmed snapshot has been applied to token
    // publication (theme, font family, accessibility inputs). Consumers that
    // hold raw theme maps use this to refresh existing and future surfaces.
    void confirmedPreferencesChanged();

    // Emitted only when a newly confirmed snapshot names a different
    // panels.layoutProfile than the last confirmed one. The runtime owns the
    // adoption policy (catalog reload, precedence, reconciliation).
    void layoutProfilePreferenceChanged(const QString &profileId);

private:
    void applySnapshot();

    Services::SettingsClient::SettingsClient &m_settings;
    Themes::ThemeCatalog &m_themes;
    ShellTokenPublisher &m_tokens;
    std::optional<ShellPreferenceValues> m_lastConfirmed;
    QString m_lastLayoutProfileId;
    bool m_layoutProfileLatched = false;
    bool m_themeLockedByCli = false;
};

} // namespace QindaQt::Shell
