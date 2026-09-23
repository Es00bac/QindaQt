// SPDX-License-Identifier: GPL-3.0-or-later

// Live provider projection for the Voice console. The session history lives in
// voice_console_history.cpp so the two concerns stay separately readable.

#include "voice_console_model.h"

#include <qindaqt/services/voice_client/voice_client.h>
#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtCore/QVariantMap>

#include <array>

namespace QindaQt::Apps::Voice {

using Services::Voice::Capability;
using Services::Voice::CaptureMode;
using Services::Voice::ClientState;
using Services::Voice::DeliveryRoute;
using Services::Voice::OperationKind;
using Services::Voice::OperationResult;
using Services::Voice::OperationStatus;
using Services::Voice::SessionState;
using Services::Voice::Snapshot;

namespace {

struct CapabilityToken {
    quint32 bit;
    const char *label;
};

constexpr std::array kCapabilityTokens{
    CapabilityToken{Capability::CapabilityCommandMode, QT_TR_NOOP("Spoken commands")},
    CapabilityToken{Capability::CapabilityRealtimePartials,
                    QT_TR_NOOP("Live partial text")},
    CapabilityToken{Capability::CapabilityRetry, QT_TR_NOOP("Retry")},
    CapabilityToken{Capability::CapabilityUndo, QT_TR_NOOP("Undo")},
    CapabilityToken{Capability::CapabilityCopy, QT_TR_NOOP("Copy")},
    CapabilityToken{Capability::CapabilityProviderSelection,
                    QT_TR_NOOP("Provider choice")},
};

} // namespace

VoiceConsoleModel::VoiceConsoleModel(Services::Voice::VoiceClient &client,
                                     QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &Services::Voice::VoiceClient::stateChanged, this,
            [this](ClientState, const QString &) { handleState(); });
    connect(&m_client, &Services::Voice::VoiceClient::snapshotChanged, this,
            &VoiceConsoleModel::handleSnapshot);
    connect(&m_client, &Services::Voice::VoiceClient::levelChanged, this,
            &VoiceConsoleModel::handleLevel);
    connect(&m_client, &Services::Voice::VoiceClient::operationCompleted, this,
            &VoiceConsoleModel::handleResult);
    handleState();
}

QString VoiceConsoleModel::phase() const
{
    switch (m_client.state()) {
    case ClientState::Ready:    return QStringLiteral("ready");
    case ClientState::Starting: return QStringLiteral("starting");
    case ClientState::Stopped:  return QStringLiteral("stopped");
    case ClientState::Unavailable: break;
    }
    return QStringLiteral("unavailable");
}

QString VoiceConsoleModel::statusText() const
{
    if (m_ready) {
        return {};
    }
    if (m_client.state() == ClientState::Stopped) {
        return tr("Voice input is off in Settings or its preferences are unavailable.");
    }
    if (m_client.state() == ClientState::Starting) {
        return tr("Connecting to the voice provider…");
    }
    const QString reason = m_client.reasonCode();
    if (reason.isEmpty() || reason == QLatin1String("service-unavailable")) {
        return tr("No voice provider is running. Install one, such as Gabbee, then "
                  "switch voice input on in Settings.");
    }
    return tr("Voice input is unavailable: %1").arg(reason);
}

QString VoiceConsoleModel::stateLabel() const
{
    if (!m_ready) {
        return tr("Not connected");
    }
    const bool command = m_snapshot.mode == CaptureMode::Command;
    switch (m_snapshot.state) {
    case SessionState::Idle:         return tr("Ready");
    case SessionState::Arming:       return tr("Opening");
    case SessionState::Listening:    return command ? tr("Command") : tr("Listening");
    case SessionState::Transcribing: return tr("Transcribing");
    case SessionState::Delivering:   return command ? tr("Running") : tr("Inserting");
    case SessionState::Error:        return tr("Failed");
    case SessionState::Unknown:      break;
    }
    return tr("Not connected");
}

QString VoiceConsoleModel::stateVariant() const
{
    if (!m_ready) {
        return QStringLiteral("default");
    }
    switch (m_snapshot.state) {
    case SessionState::Listening:
    case SessionState::Arming:
        return QStringLiteral("accent");
    case SessionState::Transcribing:
    case SessionState::Delivering:
        return QStringLiteral("info");
    case SessionState::Error:
        return QStringLiteral("danger");
    case SessionState::Idle:
        return QStringLiteral("success");
    case SessionState::Unknown:
        break;
    }
    return QStringLiteral("default");
}

bool VoiceConsoleModel::capturing() const noexcept
{
    return m_ready
           && (m_snapshot.state == SessionState::Arming
               || m_snapshot.state == SessionState::Listening
               || m_snapshot.state == SessionState::Transcribing);
}

bool VoiceConsoleModel::commandMode() const noexcept
{
    return m_ready && m_snapshot.mode == CaptureMode::Command;
}

QString VoiceConsoleModel::providerLabel() const
{
    return m_snapshot.providerLabel.isEmpty() ? m_snapshot.providerId
                                              : m_snapshot.providerLabel;
}

QString VoiceConsoleModel::routeLabel() const
{
    switch (m_snapshot.lastRoute) {
    case DeliveryRoute::InputMethod:   return tr("Input method");
    case DeliveryRoute::Accessibility: return tr("Accessibility");
    case DeliveryRoute::Clipboard:     return tr("Clipboard paste");
    case DeliveryRoute::KeySynthesis:  return tr("Synthesised keys");
    case DeliveryRoute::None:          break;
    }
    return {};
}

QVariantList VoiceConsoleModel::providerRows() const
{
    QVariantList rows;
    rows.reserve(m_snapshot.providers.size());
    for (const auto &provider : m_snapshot.providers) {
        rows.append(QVariantMap{
            {QStringLiteral("providerId"), provider.id},
            {QStringLiteral("label"),
             provider.label.isEmpty() ? provider.id : provider.label},
            {QStringLiteral("available"), provider.available},
            {QStringLiteral("current"), provider.id == m_snapshot.providerId},
        });
    }
    return rows;
}

QVariantList VoiceConsoleModel::capabilityRows() const
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(kCapabilityTokens.size()));
    for (const auto &token : kCapabilityTokens) {
        rows.append(QVariantMap{
            {QStringLiteral("label"), tr(token.label)},
            {QStringLiteral("supported"),
             m_ready && (m_snapshot.capabilities & token.bit) != 0},
        });
    }
    return rows;
}

bool VoiceConsoleModel::canDictate() const noexcept
{
    return m_ready && m_snapshot.enabled && !capturing() && !busy();
}

bool VoiceConsoleModel::canCommand() const noexcept
{
    return canDictate()
           && (m_snapshot.capabilities & Capability::CapabilityCommandMode) != 0;
}

bool VoiceConsoleModel::canFinish() const noexcept
{
    return m_ready && capturing() && !busy();
}

bool VoiceConsoleModel::canRetry() const noexcept
{
    return m_ready && !busy() && m_snapshot.state == SessionState::Error
           && (m_snapshot.capabilities & Capability::CapabilityRetry) != 0;
}

bool VoiceConsoleModel::canUndo() const noexcept
{
    return m_ready && !busy() && !capturing() && !m_snapshot.lastText.isEmpty()
           && (m_snapshot.capabilities & Capability::CapabilityUndo) != 0;
}

bool VoiceConsoleModel::canCopy() const noexcept
{
    return m_ready && !busy() && !m_snapshot.lastText.isEmpty()
           && (m_snapshot.capabilities & Capability::CapabilityCopy) != 0;
}

bool VoiceConsoleModel::canChooseProvider() const noexcept
{
    return m_ready && !busy() && !capturing() && m_snapshot.providers.size() > 1
           && (m_snapshot.capabilities & Capability::CapabilityProviderSelection) != 0;
}

void VoiceConsoleModel::handleState()
{
    const bool ready =
        m_client.state() == ClientState::Ready && m_client.hasSnapshot();
    const QString owner = m_client.owner();
    if (owner != m_owner) {
        m_owner = owner;
        if (m_requestId != 0) {
            m_requestId = 0;
            publishFeedback(tr("The voice provider went away before it answered."),
                            QStringLiteral("warning"));
        }
    }
    if (!ready) {
        m_snapshot = {};
        m_level = 0;
        Q_EMIT levelChanged();
    }
    m_ready = ready;
    Q_EMIT viewChanged();
}

void VoiceConsoleModel::handleSnapshot(const Snapshot &snapshot)
{
    m_ready = true;
    // AGENT-GUARD: adopt the snapshot first. recordDelivery() labels the entry
    // with routeLabel(), which reads m_snapshot; recording before the
    // assignment stamped every entry with the *previous* dictation's route.
    m_snapshot = snapshot;
    recordDelivery(snapshot);
    Q_EMIT viewChanged();
}

void VoiceConsoleModel::handleLevel(const quint32 level)
{
    const quint32 bounded = Services::Voice::clampLevelPercent(level);
    if (bounded == m_level) {
        return;
    }
    m_level = bounded;
    Q_EMIT levelChanged();
}

void VoiceConsoleModel::handleResult(const quint64 requestId,
                                     const OperationResult &result)
{
    if (m_requestId == 0 || requestId != m_requestId) {
        return;
    }
    m_requestId = 0;
    switch (result.status) {
    case OperationStatus::Succeeded:
        publishFeedback({}, QStringLiteral("info"));
        break;
    case OperationStatus::Busy:
        publishFeedback(tr("The voice provider is busy."), QStringLiteral("warning"));
        break;
    case OperationStatus::Rejected:
        publishFeedback(tr("Refused: %1").arg(result.reasonCode),
                        QStringLiteral("warning"));
        break;
    case OperationStatus::Failed:
        publishFeedback(tr("Failed: %1").arg(result.reasonCode),
                        QStringLiteral("danger"));
        break;
    case OperationStatus::Uncertain:
        // AGENT-GUARD: never replayed from here. Starting a second dictation
        // over one that may already be live would deliver the same utterance
        // twice into the user's document.
        publishFeedback(tr("That may or may not have happened. Check before "
                           "repeating it."),
                        QStringLiteral("warning"));
        break;
    }
    Q_EMIT viewChanged();
}

void VoiceConsoleModel::publishFeedback(QString text, QString variant)
{
    if (m_feedback == text && m_feedbackVariant == variant) {
        return;
    }
    m_feedback = std::move(text);
    m_feedbackVariant = std::move(variant);
}

void VoiceConsoleModel::dismissFeedback()
{
    publishFeedback({}, QStringLiteral("info"));
    Q_EMIT viewChanged();
}

bool VoiceConsoleModel::canRetryConnection() const noexcept
{
    return m_client.state() == ClientState::Unavailable;
}

void VoiceConsoleModel::retryConnection()
{
    if (!canRetryConnection()) {
        return;
    }
    publishFeedback({}, QStringLiteral("info"));
    Q_EMIT connectionRetryRequested();
    Q_EMIT viewChanged();
}

bool VoiceConsoleModel::submit(const OperationKind kind, const QString &providerId,
                               const bool enable)
{
    if (!m_ready || m_requestId != 0) {
        return false;
    }
    if (!Services::Voice::capabilityForKind(kind, m_snapshot.capabilities)) {
        publishFeedback(tr("The current voice provider does not support that."),
                        QStringLiteral("warning"));
        Q_EMIT viewChanged();
        return false;
    }
    quint64 requestId = 0;
    switch (kind) {
    case OperationKind::StartDictation: requestId = m_client.startDictation(); break;
    case OperationKind::StartCommand:   requestId = m_client.startCommand(); break;
    case OperationKind::Finish:         requestId = m_client.finish(); break;
    case OperationKind::Cancel:         requestId = m_client.cancel(); break;
    case OperationKind::Retry:          requestId = m_client.retry(); break;
    case OperationKind::Undo:           requestId = m_client.undo(); break;
    case OperationKind::CopyLast:       requestId = m_client.copyLast(); break;
    case OperationKind::SetProvider:    requestId = m_client.setProvider(providerId); break;
    case OperationKind::SetEnabled:     requestId = m_client.setEnabled(enable); break;
    }
    if (requestId == 0) {
        publishFeedback(tr("The voice provider could not accept the request."),
                        QStringLiteral("danger"));
        Q_EMIT viewChanged();
        return false;
    }
    m_requestId = requestId;
    publishFeedback({}, QStringLiteral("info"));
    Q_EMIT viewChanged();
    return true;
}

bool VoiceConsoleModel::startDictation()
{
    return canDictate() && submit(OperationKind::StartDictation, {}, false);
}

bool VoiceConsoleModel::startCommand()
{
    return canCommand() && submit(OperationKind::StartCommand, {}, false);
}

bool VoiceConsoleModel::finish()
{
    return canFinish() && submit(OperationKind::Finish, {}, false);
}

bool VoiceConsoleModel::cancel()
{
    return canFinish() && submit(OperationKind::Cancel, {}, false);
}

bool VoiceConsoleModel::retry()
{
    return canRetry() && submit(OperationKind::Retry, {}, false);
}

bool VoiceConsoleModel::undo()
{
    return canUndo() && submit(OperationKind::Undo, {}, false);
}

bool VoiceConsoleModel::copyLast()
{
    return canCopy() && submit(OperationKind::CopyLast, {}, false);
}

bool VoiceConsoleModel::setShortcutsArmed(const bool armed)
{
    return submit(OperationKind::SetEnabled, {}, armed);
}

bool VoiceConsoleModel::selectProvider(const QString &providerId)
{
    if (!canChooseProvider() || providerId == m_snapshot.providerId) {
        return false;
    }
    return submit(OperationKind::SetProvider, providerId, false);
}

} // namespace QindaQt::Apps::Voice
