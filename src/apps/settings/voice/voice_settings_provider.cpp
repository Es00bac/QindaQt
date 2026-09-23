// SPDX-License-Identifier: GPL-3.0-or-later

// Voice1 projection half of the Voice route. The Settings1 preference half is
// in voice_settings_model.cpp.

#include <qindaqt/apps/settings_voice/voice_settings_model.h>

#include <qindaqt/services/voice_client/voice_client.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtCore/QVariantMap>

#include <array>

namespace QindaQt::Apps::SettingsVoice {
namespace {

using Services::Voice::Capability;
using Services::Voice::CaptureMode;
using Services::Voice::DeliveryRoute;
using Services::Voice::OperationKind;
using Services::Voice::OperationResult;
using Services::Voice::OperationStatus;
using Services::Voice::SessionState;

struct CapabilityToken {
    quint32 bit;
    const char *label;
};

constexpr std::array kCapabilityTokens{
    CapabilityToken{Capability::CapabilityCommandMode, QT_TR_NOOP("Spoken commands")},
    CapabilityToken{Capability::CapabilityRealtimePartials,
                    QT_TR_NOOP("Live partial text")},
    CapabilityToken{Capability::CapabilityRetry, QT_TR_NOOP("Retry a failed insertion")},
    CapabilityToken{Capability::CapabilityUndo, QT_TR_NOOP("Undo the last dictation")},
    CapabilityToken{Capability::CapabilityCopy, QT_TR_NOOP("Copy the last dictation")},
    CapabilityToken{Capability::CapabilityProviderSelection,
                    QT_TR_NOOP("Choose a speech provider")},
};

} // namespace

QString VoiceSettingsModel::serviceState() const
{
    switch (m_voiceClient.state()) {
    case Services::Voice::ClientState::Ready:    return QStringLiteral("ready");
    case Services::Voice::ClientState::Starting: return QStringLiteral("starting");
    case Services::Voice::ClientState::Stopped:  return QStringLiteral("stopped");
    case Services::Voice::ClientState::Unavailable: break;
    }
    return QStringLiteral("unavailable");
}

QString VoiceSettingsModel::serviceStatusText() const
{
    if (m_serviceReady) {
        return {};
    }
    if (m_voiceClient.state() == Services::Voice::ClientState::Stopped) {
        return m_hasPreferenceBaseline && !m_voiceInput
                   ? tr("Voice input is off. Switch it on to connect to a provider.")
                   : tr("Waiting for confirmed voice preferences before connecting.");
    }
    const QString reason = m_voiceClient.reasonCode();
    if (m_voiceClient.state() == Services::Voice::ClientState::Starting) {
        return tr("Connecting to the voice provider…");
    }
    if (reason == QLatin1String("service-unavailable") || reason.isEmpty()) {
        return tr("No voice provider is running. Install one, such as Gabbee, and "
                  "switch voice input on.");
    }
    return tr("Voice input is unavailable: %1").arg(reason);
}

QString VoiceSettingsModel::sessionStateText() const
{
    if (!m_serviceReady) {
        return tr("Not connected");
    }
    const bool command = m_snapshot.mode == CaptureMode::Command;
    switch (m_snapshot.state) {
    case SessionState::Idle:         return tr("Ready");
    case SessionState::Arming:       return tr("Opening the microphone");
    case SessionState::Listening:    return command ? tr("Listening for a command")
                                                    : tr("Listening");
    case SessionState::Transcribing: return tr("Transcribing");
    case SessionState::Delivering:   return command ? tr("Running the command")
                                                    : tr("Inserting text");
    case SessionState::Error:        return tr("Failed: %1").arg(m_snapshot.reasonCode);
    case SessionState::Unknown:      break;
    }
    return tr("Not connected");
}

bool VoiceSettingsModel::capturing() const noexcept
{
    return m_serviceReady
           && (m_snapshot.state == SessionState::Arming
               || m_snapshot.state == SessionState::Listening
               || m_snapshot.state == SessionState::Transcribing);
}

QString VoiceSettingsModel::providerLabel() const
{
    return m_snapshot.providerLabel.isEmpty() ? m_snapshot.providerId
                                              : m_snapshot.providerLabel;
}

QString VoiceSettingsModel::routeLabel() const
{
    switch (m_snapshot.lastRoute) {
    case DeliveryRoute::InputMethod:   return tr("Input method (IBus)");
    case DeliveryRoute::Accessibility: return tr("Accessibility (AT-SPI)");
    case DeliveryRoute::Clipboard:     return tr("Clipboard paste");
    case DeliveryRoute::KeySynthesis:  return tr("Synthesised key events");
    case DeliveryRoute::None:          break;
    }
    return {};
}

QVariantList VoiceSettingsModel::providerRows() const
{
    QVariantList rows;
    rows.reserve(m_snapshot.providers.size());
    for (const auto &provider : m_snapshot.providers) {
        rows.append(QVariantMap{
            {QStringLiteral("providerId"), provider.id},
            {QStringLiteral("label"), provider.label.isEmpty() ? provider.id
                                                               : provider.label},
            {QStringLiteral("available"), provider.available},
            {QStringLiteral("current"), provider.id == m_snapshot.providerId},
        });
    }
    return rows;
}

QVariantList VoiceSettingsModel::capabilityRows() const
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(kCapabilityTokens.size()));
    for (const auto &token : kCapabilityTokens) {
        rows.append(QVariantMap{
            {QStringLiteral("label"), tr(token.label)},
            {QStringLiteral("supported"),
             m_serviceReady && (m_snapshot.capabilities & token.bit) != 0},
        });
    }
    return rows;
}

bool VoiceSettingsModel::canStartDictation() const noexcept
{
    return m_serviceReady && m_snapshot.enabled && !capturing() && !providerBusy();
}

bool VoiceSettingsModel::canCancelDictation() const noexcept
{
    return m_serviceReady && capturing() && !providerBusy();
}

bool VoiceSettingsModel::canChooseProvider() const noexcept
{
    return m_serviceReady && !providerBusy() && !capturing()
           && (m_snapshot.capabilities & Capability::CapabilityProviderSelection) != 0
           && m_snapshot.providers.size() > 1;
}

void VoiceSettingsModel::handleVoiceState()
{
    const bool ready = m_voiceClient.state() == Services::Voice::ClientState::Ready
                       && m_voiceClient.hasSnapshot();
    const QString owner = m_voiceClient.owner();
    if (owner != m_voiceOwner) {
        m_voiceOwner = owner;
        // A replaced provider cannot answer a request the previous one took.
        if (m_voiceRequestId != 0) {
            m_voiceRequestId = 0;
            m_voiceError = tr("The voice provider went away before it answered.");
        }
    }
    if (!ready) {
        m_snapshot = {};
    }
    m_serviceReady = ready;
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleVoiceSnapshot(const Services::Voice::Snapshot &snapshot)
{
    m_snapshot = snapshot;
    m_serviceReady = true;
    Q_EMIT viewChanged();
}

void VoiceSettingsModel::handleVoiceResult(const quint64 requestId,
                                           const OperationResult &result)
{
    if (m_voiceRequestId == 0 || requestId != m_voiceRequestId) {
        return;
    }
    m_voiceRequestId = 0;
    switch (result.status) {
    case OperationStatus::Succeeded:
        m_voiceError.clear();
        break;
    case OperationStatus::Busy:
        m_voiceError = tr("The voice provider is busy.");
        break;
    case OperationStatus::Rejected:
        m_voiceError = tr("The voice provider refused the request: %1")
                           .arg(result.reasonCode);
        break;
    case OperationStatus::Failed:
        m_voiceError = tr("The request failed: %1").arg(result.reasonCode);
        break;
    case OperationStatus::Uncertain:
        // AGENT-GUARD: never replayed. StartDictation is not idempotent.
        m_voiceError = tr("The request may or may not have been carried out. Check "
                          "the state before repeating it.");
        break;
    }
    Q_EMIT viewChanged();
}

bool VoiceSettingsModel::submit(const OperationKind kind, const QString &providerId,
                                const bool enable)
{
    if (!m_serviceReady || m_voiceRequestId != 0) {
        return false;
    }
    if (!Services::Voice::capabilityForKind(kind, m_snapshot.capabilities)) {
        m_voiceError = tr("The current voice provider does not support that.");
        Q_EMIT viewChanged();
        return false;
    }
    quint64 requestId = 0;
    switch (kind) {
    case OperationKind::StartDictation: requestId = m_voiceClient.startDictation(); break;
    case OperationKind::Cancel:         requestId = m_voiceClient.cancel(); break;
    case OperationKind::SetProvider:    requestId = m_voiceClient.setProvider(providerId); break;
    case OperationKind::SetEnabled:     requestId = m_voiceClient.setEnabled(enable); break;
    case OperationKind::StartCommand:
    case OperationKind::Finish:
    case OperationKind::Retry:
    case OperationKind::Undo:
    case OperationKind::CopyLast:
        // Deliberately not offered from Settings: they belong to the panel
        // applet and the Voice console, where the last dictation is in view.
        return false;
    }
    if (requestId == 0) {
        m_voiceError = tr("The voice provider could not accept the request.");
        Q_EMIT viewChanged();
        return false;
    }
    m_voiceRequestId = requestId;
    m_voiceRequestKind = kind;
    m_voiceError.clear();
    Q_EMIT viewChanged();
    return true;
}

bool VoiceSettingsModel::setShortcutsArmed(const bool armed)
{
    return submit(OperationKind::SetEnabled, {}, armed);
}

bool VoiceSettingsModel::selectProvider(const QString &providerId)
{
    if (!canChooseProvider() || providerId == m_snapshot.providerId) {
        return false;
    }
    return submit(OperationKind::SetProvider, providerId, false);
}

bool VoiceSettingsModel::startDictation()
{
    return canStartDictation() && submit(OperationKind::StartDictation, {}, false);
}

bool VoiceSettingsModel::cancelDictation()
{
    return canCancelDictation() && submit(OperationKind::Cancel, {}, false);
}

bool VoiceSettingsModel::canRetryProvider() const noexcept
{
    return m_hasPreferenceBaseline && m_voiceInput
           && m_settingsClient.currentOwner() == m_settingsOwner
           && m_preferenceState == PreferenceState::Ready
           && m_voiceClient.state() == Services::Voice::ClientState::Unavailable;
}

void VoiceSettingsModel::retryProvider()
{
    if (!canRetryProvider()) {
        return;
    }
    m_voiceError.clear();
    // The route composition checks the desktop gate before rediscovery;
    // this model never activates a provider on its own.
    Q_EMIT providerRetryRequested();
    Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsVoice
