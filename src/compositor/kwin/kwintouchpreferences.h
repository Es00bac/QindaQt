// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "touchedgeactions.h"

#include <QDBusConnection>
#include <QObject>
#include <memory>

namespace QindaQt::Services::SettingsClient {
class QtSettingsTransport;
class SettingsClient;
}

namespace QindaQt::Compositor::KWinIntegration {

// The compositor's purpose-scoped Settings1 subscription for `input.touch.*`
// (ADR-0126/0129): an older resident service that does not know the keys
// costs only these preferences, never the chrome or the theme.
class KWinTouchPreferences final : public QObject {
    Q_OBJECT

public:
    explicit KWinTouchPreferences(QDBusConnection bus, QObject *parent = nullptr);
    ~KWinTouchPreferences() override;

    [[nodiscard]] const TouchPreferences &preferences() const { return m_preferences; }

Q_SIGNALS:
    void preferencesChanged();

private:
    void refresh();

    std::unique_ptr<Services::SettingsClient::QtSettingsTransport> m_transport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> m_settings;
    TouchPreferences m_preferences;
};

} // namespace QindaQt::Compositor::KWinIntegration
