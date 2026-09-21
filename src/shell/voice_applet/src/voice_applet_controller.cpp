// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/voice_applet/voice_applet_controller.h>

#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtCore/QVariantMap>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell::VoiceApplet {

using Services::Voice::ClientState;
using Services::Voice::OperationKind;
using Services::Voice::OperationResult;
using Services::Voice::Snapshot;

VoiceAppletController::VoiceAppletController(VoiceClientInterface *client,
                                             const bool readGranted,
                                             const bool controlGranted, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_readGranted(readGranted)
    // Control without read would enable buttons over a projection this applet
    // was never granted; read is the floor for both.
    , m_controlGranted(readGranted && controlGranted)
{
    Q_ASSERT(m_client != nullptr);
    connect(m_client, &VoiceClientInterface::stateChanged, this,
            [this](ClientState, const QString &) { refreshModel(); });
    connect(m_client, &VoiceClientInterface::snapshotChanged, this,
            [this](const Snapshot &) { refreshModel(); });
    connect(m_client, &VoiceClientInterface::levelChanged, this, [this](quint32) {
        // Level moves at capture rate. It is projected without touching the
        // rest of the model so a meter cannot churn every bound property.
        const quint32 previous = m_model.levelPercent;
        m_model.levelPercent =
            m_model.capturing ? Services::Voice::clampLevelPercent(m_client->levelPercent())
                              : 0;
        if (m_model.levelPercent != previous) {
            Q_EMIT levelChanged();
        }
    });
    connect(m_client, &VoiceClientInterface::operationCompleted, this,
            &VoiceAppletController::onOperationCompleted);
    refreshModel();
}

VoiceAppletController::~VoiceAppletController() = default;

QString VoiceAppletController::phase() const
{
    switch (m_model.phase) {
    case ServicePhase::Ready:    return QStringLiteral("ready");
    case ServicePhase::Starting: return QStringLiteral("starting");
    case ServicePhase::Unavailable: break;
    }
    return QStringLiteral("unavailable");
}

QVariantList VoiceAppletController::actionRows() const
{
    QVariantList rows;
    rows.reserve(m_model.actions.size());
    for (const ActionModel &action : m_model.actions) {
        rows.append(QVariantMap{{QStringLiteral("actionId"), action.id},
                                {QStringLiteral("label"), action.label},
                                {QStringLiteral("iconName"), action.iconName},
                                {QStringLiteral("enabled"), action.enabled}});
    }
    return rows;
}

void VoiceAppletController::refreshModel()
{
    const VoiceAppletModel previous = m_model;
    m_model = projectVoiceApplet(m_client->clientState(), m_client->reasonCode(),
                                 m_client->hasSnapshot(), m_client->snapshot(),
                                 m_client->levelPercent(), m_readGranted,
                                 m_controlGranted, m_transcriptVisible);
    // A pending intent cannot survive the provider going away; resolving it
    // here is what keeps the popup from showing a spinner forever.
    const RequestState observed =
        observeVoiceAuthority(m_request, m_model.phase == ServicePhase::Ready);
    if (observed != m_request) {
        m_request = observed;
        publishFeedback(observed.feedback);
    }
    if (m_model != previous) {
        Q_EMIT stateChanged();
    }
    if (m_model.levelPercent != previous.levelPercent) {
        Q_EMIT levelChanged();
    }
}

void VoiceAppletController::publishFeedback(const QString &feedback)
{
    if (m_feedback == feedback) {
        return;
    }
    m_feedback = feedback;
    Q_EMIT feedbackChanged();
}

void VoiceAppletController::setExpanded(const bool expanded)
{
    if (m_expanded == expanded) {
        return;
    }
    m_expanded = expanded;
    if (expanded) {
        m_client->refresh();
    }
}

bool VoiceAppletController::invokeAction(const QString &actionId)
{
    OperationKind kind = OperationKind::StartDictation;
    if (!kindForActionId(actionId, kind)) {
        publishFeedback(tr("That voice action is not available."));
        return false;
    }
    // AGENT-GUARD: an id QML was never offered must not become a dispatch. The
    // projection is the authority on what exists, not the caller.
    const auto offered = std::find_if(
        m_model.actions.cbegin(), m_model.actions.cend(),
        [&actionId](const ActionModel &action) { return action.id == actionId; });
    if (offered == m_model.actions.cend() || !offered->enabled) {
        publishFeedback(tr("That voice action is not available right now."));
        return false;
    }
    return dispatch(kind, {}, false);
}

bool VoiceAppletController::setVoiceEnabled(const bool enabled)
{
    return dispatch(OperationKind::SetEnabled, {}, enabled);
}

bool VoiceAppletController::selectProvider(const QString &providerId)
{
    return dispatch(OperationKind::SetProvider, providerId, false);
}

void VoiceAppletController::setTranscriptVisible(const bool visible)
{
    if (m_transcriptVisible == visible) {
        return;
    }
    m_transcriptVisible = visible;
    refreshModel();
}

void VoiceAppletController::setSettingsLaunch(Launch launch)
{
    m_settingsLaunch = std::move(launch);
}

void VoiceAppletController::setConsoleLaunch(Launch launch)
{
    m_consoleLaunch = std::move(launch);
}

bool VoiceAppletController::openSettings()
{
    if (!m_settingsLaunch) {
        publishFeedback(tr("Settings cannot be opened from here."));
        return false;
    }
    if (!m_settingsLaunch()) {
        publishFeedback(tr("The Voice page of Settings could not be opened."));
        return false;
    }
    return true;
}

bool VoiceAppletController::openConsole()
{
    if (!m_consoleLaunch) {
        publishFeedback(tr("The Voice application cannot be opened from here."));
        return false;
    }
    if (!m_consoleLaunch()) {
        publishFeedback(tr("The Voice application could not be opened."));
        return false;
    }
    return true;
}

void VoiceAppletController::dismissFeedback()
{
    if (m_request.pending()) {
        return;
    }
    m_request = RequestState{};
    publishFeedback({});
    Q_EMIT stateChanged();
}

bool VoiceAppletController::dispatch(const OperationKind kind, const QString &providerId,
                                     const bool enable)
{
    if (m_request.pending()) {
        publishFeedback(tr("Voice input is still working on the previous request."));
        return false;
    }
    RequestState next = beginVoiceRequest(m_client->snapshot(), m_client->hasSnapshot(),
                                          kind, m_controlGranted);
    if (!next.pending()) {
        m_request = next;
        publishFeedback(next.feedback);
        Q_EMIT stateChanged();
        return false;
    }
    const quint64 requestId = m_client->submit(kind, providerId, enable);
    if (requestId == 0) {
        m_request = RequestState{.phase = RequestPhase::Failed,
                                 .kind = kind,
                                 .requestId = 0,
                                 .initiatingRevision = next.initiatingRevision,
                                 .feedback = tr("Voice input could not accept the "
                                                "request.")};
        publishFeedback(m_request.feedback);
        Q_EMIT stateChanged();
        return false;
    }
    next.requestId = requestId;
    m_request = next;
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

void VoiceAppletController::onOperationCompleted(const quint64 requestId,
                                                 const OperationResult &result)
{
    Q_UNUSED(requestId)
    const RequestState next = applyVoiceResult(m_request, result);
    if (next == m_request) {
        return;
    }
    m_request = next;
    publishFeedback(next.feedback);
    Q_EMIT stateChanged();
}

} // namespace QindaQt::Shell::VoiceApplet
