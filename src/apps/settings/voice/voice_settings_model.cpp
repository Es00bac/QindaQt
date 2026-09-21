// SPDX-License-Identifier: GPL-3.0-or-later

// Settings1 preference half of the Voice route. The Voice1 projection half is
// in voice_settings_provider.cpp; both are one class so QML binds one model.

#include <qindaqt/apps/settings_voice/voice_settings_model.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/voice_client/voice_client.h>

namespace QindaQt::Apps::SettingsVoice {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsProtocol::SettingsWireStatus;

VoiceSettingsModel::VoiceSettingsModel(
    Services::SettingsClient::SettingsClient &settingsClient,
    Services::Voice::VoiceClient &voiceClient, QObject *parent)
    : QObject(parent), m_settingsClient(settingsClient), m_voiceClient(voiceClient)
{
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &VoiceSettingsModel::handleSettingsState);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &VoiceSettingsModel::handleSettingsSnapshot);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &VoiceSettingsModel::handleSettingsCommit);
    connect(&m_settingsClient, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &VoiceSettingsModel::handleSettingsUncertain);
    connect(&m_voiceClient, &Services::Voice::VoiceClient::stateChanged, this,
            [this](Services::Voice::ClientState, const QString &) { handleVoiceState(); });
    connect(&m_voiceClient, &Services::Voice::VoiceClient::snapshotChanged, this,
            &VoiceSettingsModel::handleVoiceSnapshot);
    connect(&m_voiceClient, &Services::Voice::VoiceClient::operationCompleted, this,
            &VoiceSettingsModel::handleVoiceResult);
    handleSettingsState();
    handleVoiceState();
}

bool VoiceSettingsModel::preferenceLoading() const noexcept
{
    return m_preferenceState == PreferenceState::Loading;
}

bool VoiceSettingsModel::preferenceReady() const noexcept
{
    return m_preferenceState == PreferenceState::Ready;
}

bool VoiceSettingsModel::preferenceSaving() const noexcept
{
    return m_preferenceState == PreferenceState::Saving;
}

bool VoiceSettingsModel::preferenceUnavailable() const noexcept
{
    return m_preferenceState == PreferenceState::Unavailable;
}

bool VoiceSettingsModel::canEditPreference() const noexcept
{
    return m_preferenceState == PreferenceState::Ready && m_hasPreferenceBaseline;
}

bool VoiceSettingsModel::preferenceDirty() const noexcept
{
    return m_hasPreferenceBaseline
           && (m_draftVoiceInput != m_voiceInput
               || m_draftPanelTranscript != m_panelTranscript);
}

bool VoiceSettingsModel::applyAvailable() const noexcept
{
    return canEditPreference() && preferenceDirty() && !m_settingsClient.writeInFlight();
}

QString VoiceSettingsModel::preferenceStatusText() const
{
    switch (m_preferenceState) {
    case PreferenceState::Loading:
        return tr("Reading the voice preferences…");
    case PreferenceState::Saving:
        return tr("Saving…");
    case PreferenceState::Unavailable:
        return tr("The settings service is unavailable.");
    case PreferenceState::Ready:
        break;
    }
    return preferenceDirty() ? tr("Unsaved changes.") : QString();
}

void VoiceSettingsModel::setPreferenceState(const PreferenceState state, QString error)
{
    m_preferenceState = state;
    m_preferenceError = std::move(error);
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleSettingsState()
{
    switch (m_settingsClient.state()) {
    case ClientState::Ready:
        if (m_preferenceState == PreferenceState::Unavailable) {
            setPreferenceState(PreferenceState::Loading);
        }
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        // A lost settings owner invalidates the baseline the draft was taken
        // against; keeping the draft would let Apply write a value the user
        // composed against a different lineage.
        m_hasPreferenceBaseline = false;
        setPreferenceState(PreferenceState::Unavailable, m_settingsClient.lastError());
        return;
    case ClientState::Authenticating:
        break;
    }
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleSettingsSnapshot()
{
    const auto &snapshot = m_settingsClient.snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    const bool lineageChanged =
        snapshot->owner != m_settingsOwner || snapshot->epoch != m_settingsEpoch;
    m_settingsOwner = snapshot->owner;
    m_settingsEpoch = snapshot->epoch;
    m_voiceInput = snapshot->values
                       .value(QString::fromLatin1(VoiceInputSettingsKey), false)
                       .toBool();
    m_panelTranscript =
        snapshot->values.value(QString::fromLatin1(VoicePanelTranscriptSettingsKey), true)
            .toBool();
    // A draft survives an unrelated revision, but never a new owner or epoch.
    if (!m_hasPreferenceBaseline || lineageChanged || !preferenceDirty()) {
        m_draftVoiceInput = m_voiceInput;
        m_draftPanelTranscript = m_panelTranscript;
    }
    m_hasPreferenceBaseline = true;
    if (m_preferenceState == PreferenceState::Loading) {
        m_preferenceState = PreferenceState::Ready;
    }
    Q_EMIT viewChanged();
}

bool VoiceSettingsModel::setDraftVoiceInputEnabled(const bool enabled)
{
    if (!canEditPreference() || m_draftVoiceInput == enabled) {
        return false;
    }
    m_draftVoiceInput = enabled;
    Q_EMIT viewChanged();
    return true;
}

bool VoiceSettingsModel::setDraftPanelTranscriptEnabled(const bool enabled)
{
    if (!canEditPreference() || m_draftPanelTranscript == enabled) {
        return false;
    }
    m_draftPanelTranscript = enabled;
    Q_EMIT viewChanged();
    return true;
}

bool VoiceSettingsModel::cancelPreferenceDraft()
{
    if (!preferenceDirty()) {
        return false;
    }
    m_draftVoiceInput = m_voiceInput;
    m_draftPanelTranscript = m_panelTranscript;
    m_preferenceError.clear();
    Q_EMIT viewChanged();
    return true;
}

bool VoiceSettingsModel::applyPreferences()
{
    if (!applyAvailable()) {
        return false;
    }
    m_preferenceError.clear();
    m_preferenceState = PreferenceState::Saving;
    if (!commitNextDraftKey()) {
        // Nothing left to write is not a failure; the draft simply matched.
        m_preferenceState = PreferenceState::Ready;
        Q_EMIT viewChanged();
        return false;
    }
    Q_EMIT viewChanged();
    return true;
}

bool VoiceSettingsModel::commitNextDraftKey()
{
    m_committingVoiceInput = false;
    m_committingPanelTranscript = false;
    QString error;
    if (m_draftVoiceInput != m_voiceInput) {
        if (!m_settingsClient.setUserValue(QString::fromLatin1(VoiceInputSettingsKey),
                                           m_draftVoiceInput, &error)) {
            setPreferenceState(PreferenceState::Ready, error);
            return false;
        }
        m_committingVoiceInput = true;
        return true;
    }
    if (m_draftPanelTranscript != m_panelTranscript) {
        if (!m_settingsClient.setUserValue(
                QString::fromLatin1(VoicePanelTranscriptSettingsKey),
                m_draftPanelTranscript, &error)) {
            setPreferenceState(PreferenceState::Ready, error);
            return false;
        }
        m_committingPanelTranscript = true;
        return true;
    }
    return false;
}

void VoiceSettingsModel::handleSettingsCommit(const CommitOutcome &outcome)
{
    if (!m_committingVoiceInput && !m_committingPanelTranscript) {
        return;
    }
    if (outcome.status != SettingsWireStatus::Applied) {
        // The other key, if it was already written, stays written. Reporting
        // the failure and stopping is the only honest outcome available.
        setPreferenceState(PreferenceState::Ready,
                           outcome.message.isEmpty()
                               ? tr("The voice preference could not be saved.")
                               : outcome.message);
        return;
    }
    if (m_committingVoiceInput) {
        m_voiceInput = m_draftVoiceInput;
    } else {
        m_panelTranscript = m_draftPanelTranscript;
    }
    if (commitNextDraftKey()) {
        Q_EMIT viewChanged();
        return;
    }
    if (m_preferenceState == PreferenceState::Saving) {
        m_preferenceState = PreferenceState::Ready;
    }
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleSettingsUncertain(const QString &message)
{
    if (!m_committingVoiceInput && !m_committingPanelTranscript) {
        return;
    }
    m_committingVoiceInput = false;
    m_committingPanelTranscript = false;
    // AGENT-GUARD: never replay an uncertain settings write. The next snapshot
    // is what tells the user which value actually landed.
    setPreferenceState(PreferenceState::Ready,
                       message.isEmpty()
                           ? tr("The voice preference may not have been saved.")
                           : message);
    m_settingsClient.refresh();
}

void VoiceSettingsModel::retryPreference()
{
    m_preferenceError.clear();
    if (m_preferenceState == PreferenceState::Unavailable) {
        m_preferenceState = PreferenceState::Loading;
    }
    m_settingsClient.refresh();
    Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsVoice
