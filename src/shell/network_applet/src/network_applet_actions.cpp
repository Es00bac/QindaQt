// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_controller.h"

#include <algorithm>

namespace QindaQt::Shell::NetworkApplet
{
namespace
{

RequestTarget targetFor(const RequestAction action, const QString &id,
                        const QString &label)
{
    RequestTarget target;
    target.action = action;
    target.id = id;
    target.label = label;
    return target;
}

} // namespace

bool NetworkAppletController::refuse(const RequestTarget &target,
                                     const QString &reasonCode)
{
    if (m_request.active()) {
        // AGENT-GUARD: a refused second intent must never replace the fence
        // or text of the request still in flight; it simply does nothing.
        return false;
    }
    settle(refuseNetworkRequest(target, reasonCode));
    reproject();
    return false;
}

bool NetworkAppletController::dispatch(const RequestTarget &target,
                                       const std::function<bool(QString *)> &send)
{
    if (!m_controlGranted) {
        return refuse(target, QStringLiteral("control-not-granted"));
    }
    if (m_request.active()) {
        // AGENT-GUARD: one request at a time. A second dispatch would make the
        // untagged operationFinished reply ambiguous between the two.
        return false;
    }
    const Network::Model::ModelState state = m_client->projection();
    QString error;
    if (!send(&error)) {
        return refuse(target, error.isEmpty() ? QStringLiteral("client-not-ready") : error);
    }
    settle(beginNetworkRequest(target, state.owner, state.epoch, state.revision));
    reproject();
    return true;
}

bool NetworkAppletController::requestRadio(const QString &radioId, const bool enable)
{
    const auto row = std::ranges::find_if(
        m_model.radios, [&radioId](const RadioRow &candidate) {
            return candidate.id == radioId;
        });
    RequestTarget target = targetFor(RequestAction::SetRadio, {}, {});
    target.enable = enable;
    if (row == m_model.radios.cend()) {
        target.label = radioId;
        return refuse(target, QStringLiteral("radio-absent"));
    }
    target.id = row->id;
    target.label = row->label;
    target.radio = row->kind;
    const Network::RadioKind kind = row->kind;
    return dispatch(target, [this, kind, enable](QString *error) {
        return m_client->setRadio(kind, enable, error);
    });
}

bool NetworkAppletController::requestConnect(const QString &accessPointId)
{
    const auto row = std::ranges::find_if(
        m_model.accessPoints, [&accessPointId](const AccessPointRow &candidate) {
            return candidate.id == accessPointId;
        });
    if (row == m_model.accessPoints.cend()) {
        return refuse(targetFor(RequestAction::ConnectVisible, accessPointId,
                                QStringLiteral("that network")),
                      QStringLiteral("unknown-access-point"));
    }
    if (row->active) {
        return refuse(targetFor(RequestAction::ConnectKnown, row->knownNetworkId,
                                row->label),
                      QStringLiteral("network-already-active"));
    }
    if (row->saved) {
        const QString knownId = row->knownNetworkId;
        return dispatch(targetFor(RequestAction::ConnectKnown, knownId, row->label),
                        [this, knownId](QString *error) {
                            return m_client->connectKnownNetwork(knownId, error);
                        });
    }
    // AGENT-CONTRACT: ADR-0069. Only the opaque access-point id crosses to
    // Network1; a password, if needed, is requested by the separate secret
    // agent. This applet has no path that could carry one.
    const QString pointId = row->id;
    RequestTarget target = targetFor(RequestAction::ConnectVisible, pointId, row->label);
    target.ssid = row->label;
    target.security = row->security;
    return dispatch(target,
                    [this, pointId](QString *error) {
                        return m_client->connectVisibleNetwork(pointId, error);
                    });
}

bool NetworkAppletController::requestDisconnect(const QString &connectionId)
{
    const auto row = std::ranges::find_if(
        m_model.connections, [&connectionId](const ConnectionRow &candidate) {
            return candidate.id == connectionId;
        });
    if (row == m_model.connections.cend()) {
        return refuse(targetFor(RequestAction::Disconnect, connectionId,
                                QStringLiteral("that connection")),
                      QStringLiteral("device-not-connected"));
    }
    const QString interfaceName = row->id;
    return dispatch(targetFor(RequestAction::Disconnect, interfaceName, row->label),
                    [this, interfaceName](QString *error) {
                        return m_client->disconnectDevice(interfaceName, error);
                    });
}

bool NetworkAppletController::requestScan()
{
    return dispatch(targetFor(RequestAction::Scan, {}, QStringLiteral("networks")),
                    [this](QString *error) {
                        return m_client->requestScan(kScanDeadlineMilliseconds, error);
                    });
}

bool NetworkAppletController::openSettings()
{
    if (!m_settingsLaunch) {
        return false;
    }
    if (!m_settingsLaunch()) {
        if (m_request.active()) {
            // Opening Settings is not a network mutation; never let its
            // failure overwrite the fence of an in-flight request.
            return false;
        }
        RequestState failed;
        failed.phase = RequestPhase::Failed;
        failed.feedback = QStringLiteral("Network Settings could not be opened.");
        m_request = failed;
        reproject();
        return false;
    }
    return true;
}

} // namespace QindaQt::Shell::NetworkApplet
