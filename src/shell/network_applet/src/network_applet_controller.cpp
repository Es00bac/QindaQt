// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_controller.h"

#include <qindaqt/shell/network_applet/network_applet_presentation.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Shell::NetworkApplet
{
namespace
{

using Network::Client::ClientState;

QString phaseToken(const ServicePhase phase)
{
    switch (phase) {
    case ServicePhase::Loading:
        return QStringLiteral("loading");
    case ServicePhase::Ready:
        return QStringLiteral("ready");
    case ServicePhase::Degraded:
        return QStringLiteral("degraded");
    case ServicePhase::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

ServicePhase clientPhase(const ClientState state)
{
    switch (state) {
    case ClientState::Unavailable:
        return ServicePhase::Unavailable;
    case ClientState::Connecting:
        return ServicePhase::Loading;
    case ClientState::Ready:
        return ServicePhase::Ready;
    case ClientState::Degraded:
        return ServicePhase::Degraded;
    }
    return ServicePhase::Unavailable;
}

} // namespace

NetworkAppletController::NetworkAppletController(
    Network::Client::NetworkClient *client, const bool networkReadGranted,
    const bool networkControlGranted, QObject *parent)
    : NetworkAppletController(client, networkReadGranted, networkControlGranted,
                              Timing{}, parent)
{
}

NetworkAppletController::NetworkAppletController(
    Network::Client::NetworkClient *client, const bool networkReadGranted,
    const bool networkControlGranted, const Timing timing, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_readGranted(networkReadGranted)
    , m_controlGranted(networkReadGranted && networkControlGranted)
    , m_timing(timing)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_client->thread() == thread());
    m_readbackPoll.setInterval(m_timing.readbackPollMilliseconds);
    connect(&m_readbackPoll, &QTimer::timeout, this,
            &NetworkAppletController::pollReadback);
    m_confirmDeadline.setSingleShot(true);
    connect(&m_confirmDeadline, &QTimer::timeout, this, [this] {
        settle(expireNetworkRequest(m_request));
        reproject();
    });
    using Network::Client::NetworkClient;
    connect(m_client, &NetworkClient::stateChanged, this,
            &NetworkAppletController::reproject);
    connect(m_client, &NetworkClient::snapshotChanged, this,
            &NetworkAppletController::reproject);
    connect(m_client, &NetworkClient::operationAdmissionChanged, this,
            &NetworkAppletController::reproject);
    connect(m_client, &NetworkClient::operationFinished, this,
            &NetworkAppletController::handleOperationFinished);
    connect(m_client, &NetworkClient::operationUncertain, this,
            &NetworkAppletController::handleOperationUncertain);
    reproject();
}

NetworkAppletController::~NetworkAppletController() = default;

QString NetworkAppletController::phase() const
{
    return phaseToken(m_model.phase);
}

QString NetworkAppletController::indicator() const
{
    switch (m_model.indicator) {
    case Indicator::Unavailable:
        return QStringLiteral("unavailable");
    case Indicator::RadioOff:
        return QStringLiteral("radio-off");
    case Indicator::Disconnected:
        return QStringLiteral("disconnected");
    case Indicator::Wired:
        return QStringLiteral("wired");
    case Indicator::Wireless:
        return QStringLiteral("wireless");
    case Indicator::Mobile:
        return QStringLiteral("mobile");
    }
    return QStringLiteral("unavailable");
}

QString NetworkAppletController::requestPhase() const
{
    switch (m_request.phase) {
    case RequestPhase::Idle:
        return QStringLiteral("idle");
    case RequestPhase::Pending:
        return QStringLiteral("pending");
    case RequestPhase::Confirming:
        return QStringLiteral("confirming");
    case RequestPhase::Succeeded:
        return QStringLiteral("succeeded");
    case RequestPhase::Failed:
        return QStringLiteral("failed");
    case RequestPhase::Uncertain:
        return QStringLiteral("uncertain");
    }
    return QStringLiteral("idle");
}

bool NetworkAppletController::rowPending(const RequestAction action,
                                         const QString &id) const
{
    return m_request.active() && m_request.target.action == action
        && m_request.target.id == id;
}

QVariantList NetworkAppletController::radioRows() const
{
    QVariantList rows;
    rows.reserve(m_model.radios.size());
    for (const RadioRow &row : m_model.radios) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("softwareEnabled"), row.softwareEnabled},
            {QStringLiteral("hardwareEnabled"), row.hardwareEnabled},
            {QStringLiteral("enabled"), row.softwareEnabled && row.hardwareEnabled},
            {QStringLiteral("canToggle"), row.canToggle},
            {QStringLiteral("pending"),
             m_request.active() && m_request.target.action == RequestAction::SetRadio
                 && m_request.target.radio == row.kind},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"), row.accessibleDescription},
        });
    }
    return rows;
}

QVariantList NetworkAppletController::connectionRows() const
{
    QVariantList rows;
    rows.reserve(m_model.connections.size());
    for (const ConnectionRow &row : m_model.connections) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("kindLabel"), row.kindLabel},
            {QStringLiteral("wired"), row.kind == Network::DeviceKind::Ethernet},
            {QStringLiteral("canDisconnect"), row.canDisconnect},
            {QStringLiteral("pending"), rowPending(RequestAction::Disconnect, row.id)},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"), row.accessibleDescription},
        });
    }
    return rows;
}

QVariantList NetworkAppletController::accessPointRows() const
{
    QVariantList rows;
    rows.reserve(m_model.accessPoints.size());
    for (const AccessPointRow &row : m_model.accessPoints) {
        const bool pending = rowPending(RequestAction::ConnectVisible, row.id)
            || (!row.knownNetworkId.isEmpty()
                && rowPending(RequestAction::ConnectKnown, row.knownNetworkId));
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("securityLabel"), row.securityLabel},
            {QStringLiteral("secured"), row.secured},
            {QStringLiteral("saved"), row.saved},
            {QStringLiteral("active"), row.active},
            {QStringLiteral("signalPercent"), row.signalPercent},
            {QStringLiteral("canConnect"), row.canConnect},
            {QStringLiteral("pending"), pending},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"), row.accessibleDescription},
        });
    }
    return rows;
}

void NetworkAppletController::settle(const RequestState &next)
{
    const bool wasConfirming = m_request.phase == RequestPhase::Confirming;
    m_request = next;
    if (next.phase != RequestPhase::Confirming) {
        m_readbackPoll.stop();
        m_confirmDeadline.stop();
        return;
    }
    if (!wasConfirming) {
        const bool connecting = next.target.action == RequestAction::ConnectKnown
            || next.target.action == RequestAction::ConnectVisible;
        m_confirmDeadline.start(connecting ? m_timing.connectConfirmMilliseconds
                                           : m_timing.confirmMilliseconds);
        m_readbackPoll.start();
    }
}

void NetworkAppletController::pollReadback()
{
    // AGENT-NOTE: refresh is a read. NetworkClient suppresses an unchanged
    // snapshot, so confirmation must keep asking until truth moves or the
    // bounded deadline retires the request.
    if (m_request.phase == RequestPhase::Confirming) {
        m_client->refresh();
    }
}

void NetworkAppletController::handleOperationFinished(
    const Network::OperationResult &result)
{
    if (m_request.phase != RequestPhase::Pending) {
        return;
    }
    settle(applyNetworkResult(m_request, result));
    reproject();
}

void NetworkAppletController::handleOperationUncertain(const QString &message)
{
    Q_UNUSED(message);
    if (!m_request.active()) {
        return;
    }
    settle(applyNetworkUncertain(m_request));
    reproject();
}

void NetworkAppletController::reproject()
{
    const Network::Model::ModelState state = m_client->projection();
    const bool current = m_client->snapshotCurrent();
    if (m_request.active()) {
        settle(observeNetworkState(m_request, state, current));
    }
    if (state.owner != m_lastOwner) {
        // AGENT-GUARD: a settled outcome describes the previous owner's
        // truth. Drop it rather than let it read as a fact about the new one.
        if (m_request.phase == RequestPhase::Succeeded
            || m_request.phase == RequestPhase::Failed) {
            m_request = {};
        }
        m_lastOwner = state.owner;
    }
    ProjectionContext context;
    context.readGranted = m_readGranted;
    context.controlGranted = m_controlGranted;
    context.snapshotCurrent = current;
    context.clientPhase = clientPhase(m_client->state());
    context.admissionOpen = m_client->operationAdmissionReady() && !m_request.active();
    context.clientDiagnostic = m_client->lastError();
    NetworkAppletModel next = projectNetworkApplet(m_client->model(), context);
    // AGENT-NOTE: QML Repeaters rebuild their rows (and drop keyboard focus)
    // on every notification, so publish only real changes.
    if (next == m_model && m_request == m_publishedRequest) {
        return;
    }
    m_model = std::move(next);
    m_publishedRequest = m_request;
    Q_EMIT stateChanged();
}

void NetworkAppletController::setSettingsLaunch(Launch launch)
{
    m_settingsLaunch = std::move(launch);
    Q_EMIT stateChanged();
}

void NetworkAppletController::setExpanded(const bool expanded)
{
    const bool closing = m_expanded && !expanded;
    m_expanded = expanded;
    if (expanded && m_readGranted) {
        m_client->refresh();
        return;
    }
    if (closing && !m_request.active() && m_request.phase != RequestPhase::Idle) {
        // A closed popup forgets settled feedback; an active request keeps
        // its fence and its text so reopening shows it still in progress.
        m_request = {};
        reproject();
    }
}

void NetworkAppletController::clearFeedback()
{
    if (m_request.active() || m_request.phase == RequestPhase::Idle) {
        return;
    }
    m_request = {};
    reproject();
}

} // namespace QindaQt::Shell::NetworkApplet
