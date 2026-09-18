// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>

#include <qindaqt/services/settings_client/settings_client.h>

namespace QindaQt::Apps::SettingsStreaming {

// Production preferences: one purpose-scoped Settings1 client reading and
// writing only the three `services.obs*` keys.
//
// AGENT-GUARD: Settings1 rejects a whole snapshot on one unknown key
// (ADR-0126), so this scope stays exactly these three and the resident
// settings service must know them from data/settings/schema-v2.json before
// the route can save anything.
class Settings1StreamingPreferences final : public StreamingPreferences {
    Q_OBJECT
public:
    static const QStringList &scopedKeys();

    explicit Settings1StreamingPreferences(
        Services::SettingsClient::SettingsClient &client,
        QObject *parent = nullptr);

    [[nodiscard]] bool isLoaded() const override { return m_loaded; }
    [[nodiscard]] int webSocketPort() const override { return m_port; }
    [[nodiscard]] bool autoConnect() const override { return m_autoConnect; }
    [[nodiscard]] bool startObsAtLogin() const override {
        return m_startAtLogin;
    }
    bool setWebSocketPort(int port) override;
    bool setAutoConnect(bool enabled) override;
    bool setStartObsAtLogin(bool enabled) override;

private Q_SLOTS:
    void onSnapshotChanged();

private:
    Services::SettingsClient::SettingsClient &m_client;
    // The documented defaults, so an unreachable Settings1 owner yields the
    // standard behavior rather than a dead route.
    int m_port = 4455;
    bool m_autoConnect = true;
    bool m_startAtLogin = false;
    bool m_loaded = false;
};

} // namespace QindaQt::Apps::SettingsStreaming
