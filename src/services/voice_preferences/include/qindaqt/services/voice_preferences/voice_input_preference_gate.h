// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Services::VoicePreferences {

// Same-thread, borrowed Settings1 consumer. The caller owns and starts a
// SettingsClient scoped to services.voiceInput, and keeps it alive longer than
// this gate. The gate never writes a preference or starts a Voice1 provider.
//
// AGENT-CONTRACT: allowed() is a desktop admission decision, not provider
// availability or the provider's own shortcut-armed state. It is false until
// an exact boolean true is confirmed for the current Settings1 owner. A
// same-owner refresh retains that confirmation; owner change/loss or degraded
// authority revokes it synchronously. Consumers must stop their VoiceClient
// when allowedChanged(false) arrives and must not replay capture requests.
class VoiceInputPreferenceGate final : public QObject {
    Q_OBJECT
public:
    explicit VoiceInputPreferenceGate(
        SettingsClient::SettingsClient &settings, QObject *parent = nullptr);

    [[nodiscard]] bool allowed() const noexcept { return m_allowed; }

Q_SIGNALS:
    void allowedChanged(bool allowed);

private:
    void reconcile();

    SettingsClient::SettingsClient &m_settings;
    bool m_allowed = false;
};

} // namespace QindaQt::Services::VoicePreferences
