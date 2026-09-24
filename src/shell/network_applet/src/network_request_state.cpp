// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/network_applet/network_request_state.h>

#include <algorithm>

namespace QindaQt::Shell::NetworkApplet
{
namespace
{

using Network::OperationKind;
using Network::OperationStatus;
using Network::Model::ModelState;

OperationKind operationKind(const RequestAction action)
{
    switch (action) {
    case RequestAction::Scan:
        return OperationKind::RequestScan;
    case RequestAction::ConnectKnown:
        return OperationKind::ConnectKnownNetwork;
    case RequestAction::ConnectVisible:
        return OperationKind::ConnectVisibleNetwork;
    case RequestAction::Disconnect:
        return OperationKind::DisconnectActive;
    case RequestAction::SetRadio:
        return OperationKind::SetRadio;
    }
    return OperationKind::RequestScan;
}

QString radioName(const Network::RadioKind kind)
{
    return kind == Network::RadioKind::Wifi ? QStringLiteral("Wi-Fi")
                                            : QStringLiteral("Mobile broadband");
}

QString pendingText(const RequestTarget &target)
{
    switch (target.action) {
    case RequestAction::Scan:
        return QStringLiteral("Searching for networks…");
    case RequestAction::ConnectKnown:
        return QStringLiteral("Connecting to %1…").arg(target.label);
    case RequestAction::ConnectVisible:
        // AGENT-CONTRACT: ADR-0069. The applet never receives or shows a
        // secret; a password, when needed, is entered in the separate
        // network secret agent's own prompt.
        return QStringLiteral(
                   "Connecting to %1… If it needs a password, enter it in the "
                   "password prompt.")
            .arg(target.label);
    case RequestAction::Disconnect:
        return QStringLiteral("Disconnecting %1…").arg(target.label);
    case RequestAction::SetRadio:
        return target.enable
            ? QStringLiteral("Turning %1 on…").arg(radioName(target.radio))
            : QStringLiteral("Turning %1 off…").arg(radioName(target.radio));
    }
    return QStringLiteral("Working…");
}

QString successText(const RequestTarget &target)
{
    switch (target.action) {
    case RequestAction::Scan:
        return QStringLiteral("Scan requested. The list updates as networks are found.");
    case RequestAction::ConnectKnown:
    case RequestAction::ConnectVisible:
        return QStringLiteral("Connected to %1.").arg(target.label);
    case RequestAction::Disconnect:
        return QStringLiteral("Disconnected %1.").arg(target.label);
    case RequestAction::SetRadio:
        return target.enable ? QStringLiteral("%1 is on.").arg(radioName(target.radio))
                             : QStringLiteral("%1 is off.").arg(radioName(target.radio));
    }
    return {};
}

QString failurePrefix(const RequestTarget &target)
{
    switch (target.action) {
    case RequestAction::Scan:
        return QStringLiteral("Could not search for networks");
    case RequestAction::ConnectKnown:
    case RequestAction::ConnectVisible:
        return QStringLiteral("Could not connect to %1").arg(target.label);
    case RequestAction::Disconnect:
        return QStringLiteral("Could not disconnect %1").arg(target.label);
    case RequestAction::SetRadio:
        return QStringLiteral("Could not turn %1 %2")
            .arg(radioName(target.radio),
                 target.enable ? QStringLiteral("on") : QStringLiteral("off"));
    }
    return QStringLiteral("The network request failed");
}

RequestState uncertain(const RequestState &request, const QString &why)
{
    RequestState next = request;
    next.phase = RequestPhase::Uncertain;
    next.feedback = QStringLiteral("%1 Check the current state before trying again.")
                        .arg(why);
    return next;
}

// True when `state` shows the requested outcome.
bool confirms(const RequestTarget &target, const ModelState &state)
{
    switch (target.action) {
    case RequestAction::Scan:
        return true;
    case RequestAction::ConnectKnown:
        return std::ranges::any_of(state.activeConnections,
                                   [&target](const Network::ActiveConnection &active) {
                                       return active.knownNetworkId == target.id;
                                   });
    case RequestAction::ConnectVisible:
        return std::ranges::any_of(
            state.activeConnections, [&](const Network::ActiveConnection &active) {
                return std::ranges::any_of(
                    state.knownNetworks, [&](const Network::KnownNetwork &known) {
                        return known.id == active.knownNetworkId && !known.hidden
                            && known.ssid == target.ssid
                            && known.security == target.security;
                    });
            });
    case RequestAction::Disconnect:
        return std::ranges::none_of(state.activeConnections,
                                    [&target](const Network::ActiveConnection &active) {
                                        return active.deviceInterface == target.id;
                                    });
    case RequestAction::SetRadio:
        return std::ranges::any_of(state.radios, [&target](const Network::Radio &radio) {
            return radio.kind == target.radio && radio.softwareEnabled == target.enable;
        });
    }
    return false;
}

} // namespace

QString networkReasonText(const QString &reasonCode)
{
    if (reasonCode == QLatin1StringView("radio-control-unsupported")) {
        return QStringLiteral("the network service does not allow changing this radio");
    }
    if (reasonCode == QLatin1StringView("radio-absent")) {
        return QStringLiteral("the radio is not present");
    }
    if (reasonCode == QLatin1StringView("radio-hardware-disabled")) {
        return QStringLiteral("a hardware switch is blocking the radio");
    }
    if (reasonCode == QLatin1StringView("radio-already-in-state")) {
        return QStringLiteral("the radio is already in that state");
    }
    if (reasonCode == QLatin1StringView("scan-unsupported")) {
        return QStringLiteral("the network service does not allow scanning");
    }
    if (reasonCode == QLatin1StringView("scan-busy")
        || reasonCode == QLatin1StringView("scan-lease-held")) {
        return QStringLiteral("a search is already in progress");
    }
    if (reasonCode == QLatin1StringView("operation-in-flight")) {
        return QStringLiteral("another network change is in progress");
    }
    if (reasonCode == QLatin1StringView("known-network-control-unsupported")
        || reasonCode == QLatin1StringView("visible-network-control-unsupported")) {
        return QStringLiteral("the network service does not allow joining networks");
    }
    if (reasonCode == QLatin1StringView("active-connection-control-unsupported")) {
        return QStringLiteral("the network service does not allow disconnecting");
    }
    if (reasonCode == QLatin1StringView("hidden-network-unsupported")
        || reasonCode == QLatin1StringView("wep-network-unsupported")
        || reasonCode == QLatin1StringView("enterprise-network-unsupported")) {
        return QStringLiteral("this kind of network must be set up in Network Settings");
    }
    if (reasonCode == QLatin1StringView("network-already-active")) {
        return QStringLiteral("it is already connected");
    }
    if (reasonCode == QLatin1StringView("network-already-known")) {
        return QStringLiteral("it already has a saved profile");
    }
    if (reasonCode == QLatin1StringView("device-not-connected")) {
        return QStringLiteral("it is no longer connected");
    }
    if (reasonCode == QLatin1StringView("unknown-known-network")
        || reasonCode == QLatin1StringView("unknown-access-point")
        || reasonCode == QLatin1StringView("unknown-device")) {
        return QStringLiteral("it is no longer listed");
    }
    if (reasonCode == QLatin1StringView("credentials-required")) {
        return QStringLiteral("a password is required and none was provided");
    }
    if (reasonCode == QLatin1StringView("client-not-ready")
        || reasonCode == QLatin1StringView("service-not-ready")) {
        return QStringLiteral("the network service is not ready");
    }
    if (reasonCode == QLatin1StringView("control-not-granted")) {
        return QStringLiteral("network controls are not allowed for this applet");
    }
    return QStringLiteral("the network service refused the change");
}

RequestState refuseNetworkRequest(const RequestTarget &target, const QString &reasonCode)
{
    RequestState refused;
    refused.phase = RequestPhase::Failed;
    refused.target = target;
    refused.feedback = QStringLiteral("%1: %2.").arg(failurePrefix(target),
                                                     networkReasonText(reasonCode));
    return refused;
}

RequestState beginNetworkRequest(const RequestTarget &target, const QString &owner,
                                 const quint64 epoch, const quint64 revision)
{
    return {.phase = RequestPhase::Pending,
            .target = target,
            .owner = owner,
            .epoch = epoch,
            .revision = revision,
            .feedback = pendingText(target)};
}

RequestState applyNetworkResult(const RequestState &request,
                                const Network::OperationResult &result)
{
    if (request.phase != RequestPhase::Pending) {
        return request;
    }
    if (!result.wireValid || result.kind != operationKind(request.target.action)
        || result.initiatingEpoch != request.epoch
        || result.initiatingRevision != request.revision) {
        return uncertain(request, QStringLiteral("The network service reply did not match the request."));
    }
    RequestState next = request;
    switch (result.status) {
    case OperationStatus::Succeeded:
        if (request.target.action == RequestAction::Scan) {
            next.phase = RequestPhase::Succeeded;
            next.feedback = successText(request.target);
        } else {
            // AGENT-GUARD: an accepted reply is not the new state. Keep the
            // request fenced until a newer snapshot shows it (ADR-0251).
            next.phase = RequestPhase::Confirming;
        }
        return next;
    case OperationStatus::Uncertain:
        return uncertain(request, QStringLiteral("The network service could not confirm the change."));
    case OperationStatus::Rejected:
    case OperationStatus::Unsupported:
    case OperationStatus::Failed:
    case OperationStatus::Busy:
        next.phase = RequestPhase::Failed;
        next.feedback = QStringLiteral("%1: %2.").arg(
            failurePrefix(request.target),
            result.status == OperationStatus::Busy
                ? QStringLiteral("the network is busy; try again when it settles")
                : networkReasonText(result.reasonCode));
        return next;
    }
    return uncertain(request, QStringLiteral("The network service reply could not be read."));
}

RequestState applyNetworkUncertain(const RequestState &request)
{
    if (!request.active()) {
        return request;
    }
    return uncertain(request, QStringLiteral("The outcome is uncertain."));
}

RequestState observeNetworkState(const RequestState &request, const ModelState &state,
                                 const bool current)
{
    if (!request.active()) {
        return request;
    }
    if (!state.hasSnapshot || state.owner != request.owner
        || state.epoch != request.epoch) {
        return uncertain(request, QStringLiteral("The network service changed."));
    }
    if (request.phase != RequestPhase::Confirming || !current
        || state.revision <= request.revision) {
        return request;
    }
    RequestState next = request;
    if (confirms(request.target, state)) {
        next.phase = RequestPhase::Succeeded;
        next.feedback = successText(request.target);
        return next;
    }
    if (request.target.action == RequestAction::SetRadio) {
        // Radio state is reported synchronously by the service, so a newer
        // snapshot showing the old state is a conflict, as in Settings.
        next.phase = RequestPhase::Failed;
        next.feedback = QStringLiteral("%1: the network service still reports it %2.")
                            .arg(failurePrefix(request.target),
                                 request.target.enable ? QStringLiteral("off")
                                                       : QStringLiteral("on"));
    }
    // Connection changes settle over several snapshots; keep waiting.
    return next;
}

RequestState expireNetworkRequest(const RequestState &request)
{
    if (!request.active()) {
        return request;
    }
    return uncertain(request, QStringLiteral("The change was not confirmed in time."));
}

} // namespace QindaQt::Shell::NetworkApplet
