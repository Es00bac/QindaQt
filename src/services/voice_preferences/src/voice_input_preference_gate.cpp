// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_preferences/voice_input_preference_gate.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/voice_protocol/voice_settings_keys.h>

#include <QMetaType>

namespace QindaQt::Services::VoicePreferences {

VoiceInputPreferenceGate::VoiceInputPreferenceGate(
    SettingsClient::SettingsClient &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    connect(&m_settings, &SettingsClient::SettingsClient::stateChanged, this,
            &VoiceInputPreferenceGate::reconcile);
    connect(&m_settings, &SettingsClient::SettingsClient::snapshotChanged, this,
            &VoiceInputPreferenceGate::reconcile);
    reconcile();
}

void VoiceInputPreferenceGate::reconcile()
{
    const auto &snapshot = m_settings.snapshot();
    const auto state = m_settings.state();
    const bool current = snapshot.has_value()
                         && !m_settings.currentOwner().isEmpty()
                         && snapshot->owner == m_settings.currentOwner()
                         && (state == SettingsClient::ClientState::Ready
                             || state == SettingsClient::ClientState::Authenticating);
    const QVariant value =
        current ? snapshot->values.value(
                      QString::fromLatin1(Voice::kVoiceInputSettingsKey))
                : QVariant{};
    const bool allowed = value.metaType().id() == QMetaType::Bool
                         && value.toBool();
    if (m_allowed == allowed) {
        return;
    }
    m_allowed = allowed;
    Q_EMIT allowedChanged(allowed);
}

} // namespace QindaQt::Services::VoicePreferences
