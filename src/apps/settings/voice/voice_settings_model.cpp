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
    return m_preferenceState == PreferenceState::Ready && m_hasPreferenceBaseline
           && m_settingsClient.state() == ClientState::Ready
           && m_settingsClient.currentOwner() == m_settingsOwner;
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
        // A same-owner lost reply has an unknown outcome. Keep the user's
        // requested draft for review after readback, but never submit it again.
        m_preserveDraftOnResync =
            m_settingsClient.state() == ClientState::Degraded
            && m_settingsClient.currentOwner() == m_settingsOwner
            && m_preferenceState == PreferenceState::Saving;
        m_hasPreferenceBaseline = false;
        m_committingVoiceInput = false;
        m_committingPanelTranscript = false;
        m_pendingVoiceInput = false;
        m_pendingPanelTranscript = false;
        m_waitingForCommittedSnapshot = false;
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
    // AGENT-GUARD: commitFinished precedes the authoritative readback. An
    // explicit Off must continue to close this route through a conflict or
    // uncertain outcome; a stale same-owner read below the commit's known
    // revision cannot reopen Voice1.
    if (m_voiceUseWithdrawn) {
        if (snapshot->owner == m_offWithdrawalOwner
            && snapshot->revision < m_offWithdrawalMinRevision) {
            m_settingsClient.refresh();
            return;
        }
        m_voiceUseWithdrawn = false;
    }
    const bool wasDirty = preferenceDirty();
    const bool lineageChanged =
        snapshot->owner != m_settingsOwner || snapshot->epoch != m_settingsEpoch;
    const bool preserveDraft = m_preserveDraftOnResync && !lineageChanged;
    m_preserveDraftOnResync = false;
    m_settingsOwner = snapshot->owner;
    m_settingsEpoch = snapshot->epoch;
    m_voiceInput = snapshot->values
                       .value(QString::fromLatin1(VoiceInputSettingsKey), false)
                       .toBool();
    m_panelTranscript =
        snapshot->values.value(QString::fromLatin1(VoicePanelTranscriptSettingsKey), true)
            .toBool();
    // A draft survives an unrelated revision, but never a new owner or epoch.
    if (lineageChanged || (!preserveDraft && (!m_hasPreferenceBaseline || !wasDirty))) {
        m_draftVoiceInput = m_voiceInput;
        m_draftPanelTranscript = m_panelTranscript;
    }
    m_hasPreferenceBaseline = true;
    if (lineageChanged && m_preferenceState == PreferenceState::Saving) {
        m_committingVoiceInput = false;
        m_committingPanelTranscript = false;
        m_pendingVoiceInput = false;
        m_pendingPanelTranscript = false;
        m_waitingForCommittedSnapshot = false;
        m_preferenceError = tr("The settings service changed while saving. Review the new values.");
        m_preferenceState = PreferenceState::Ready;
    } else if (m_waitingForCommittedSnapshot
               && snapshot->revision >= m_committedRevision) {
        m_waitingForCommittedSnapshot = false;
        if (!commitNextDraftKey()) {
            m_preferenceState = PreferenceState::Ready;
        }
    } else if (m_preferenceState == PreferenceState::Loading
               || m_preferenceState == PreferenceState::Unavailable) {
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
    m_requestedVoiceInput = m_draftVoiceInput;
    m_requestedPanelTranscript = m_draftPanelTranscript;
    m_pendingVoiceInput = m_requestedVoiceInput != m_voiceInput;
    m_pendingPanelTranscript = m_requestedPanelTranscript != m_panelTranscript;
    if (m_pendingVoiceInput && !m_requestedVoiceInput) {
        m_voiceUseWithdrawn = true;
        m_offWithdrawalOwner = m_settingsOwner;
        m_offWithdrawalMinRevision = m_settingsClient.snapshot()->revision;
    }
    m_preferenceState = PreferenceState::Saving;
    // Withdraw this route's use immediately when Apply includes Off,
    // before the asynchronous Settings1 commit is sent.
    Q_EMIT viewChanged();
    if (!commitNextDraftKey()) {
        // If an Off write could not even be submitted, keep this route
        // withdrawn until a readback resolves the still-confirmed value.
        m_preferenceState =
            m_voiceUseWithdrawn ? PreferenceState::Loading : PreferenceState::Ready;
        if (m_voiceUseWithdrawn) {
            m_settingsClient.refresh();
        }
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
    if (m_pendingVoiceInput) {
        if (!m_settingsClient.setUserValue(QString::fromLatin1(VoiceInputSettingsKey),
                                           m_requestedVoiceInput, &error)) {
            setPreferenceState(PreferenceState::Ready, error);
            return false;
        }
        m_committingVoiceInput = true;
        return true;
    }
    if (m_pendingPanelTranscript) {
        if (!m_settingsClient.setUserValue(
                QString::fromLatin1(VoicePanelTranscriptSettingsKey),
                m_requestedPanelTranscript, &error)) {
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
        // Earlier keys remain committed; never infer the rejected key's value.
        m_committingVoiceInput = false;
        m_committingPanelTranscript = false;
        m_pendingVoiceInput = false;
        m_pendingPanelTranscript = false;
        if (m_voiceUseWithdrawn) {
            m_offWithdrawalMinRevision =
                qMax(m_offWithdrawalMinRevision, outcome.revisionAfter);
        }
        setPreferenceState(PreferenceState::Loading,
                           outcome.message.isEmpty()
                               ? tr("The voice preference could not be saved.")
                               : outcome.message);
        return;
    }
    if (m_committingVoiceInput) {
        m_pendingVoiceInput = false;
    } else {
        m_pendingPanelTranscript = false;
    }
    m_committingVoiceInput = false;
    m_committingPanelTranscript = false;
    // AGENT-GUARD: SettingsClient emits commitFinished before fetching the
    // authoritative next revision. The second key must wait for that snapshot,
    // and no requested value may be manufactured as a confirmed value.
    m_committedRevision = outcome.revisionAfter;
    if (m_voiceUseWithdrawn) {
        m_offWithdrawalMinRevision =
            qMax(m_offWithdrawalMinRevision, outcome.revisionAfter);
    }
    m_waitingForCommittedSnapshot = true;
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleSettingsUncertain(const QString &message)
{
    if (!m_committingVoiceInput && !m_committingPanelTranscript) {
        return;
    }
    m_committingVoiceInput = false;
    m_committingPanelTranscript = false;
    m_pendingVoiceInput = false;
    m_pendingPanelTranscript = false;
    m_waitingForCommittedSnapshot = false;
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
