// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/smart_lights_applet/smart_lights_request_state.h"

#include "qindaqt/services/wiz_protocol/wiz_validation.h"

#include <QtCore/QCoreApplication>

namespace QindaQt::Shell::SmartLightsApplet
{
namespace
{

[[nodiscard]] QString translate(const char *text)
{
    return QCoreApplication::translate("QindaQt::Shell::SmartLightsApplet", text);
}

[[nodiscard]] RequestState refused(const Wiz::OperationRequest &operation,
                                   const QString &feedback)
{
    RequestState state;
    state.phase = RequestPhase::Failed;
    state.operation = operation;
    state.feedback = feedback;
    return state;
}

[[nodiscard]] const Wiz::Device *findDevice(const Wiz::Snapshot &snapshot,
                                            const QString &mac)
{
    for (const Wiz::Device &device : snapshot.devices) {
        if (device.identity.mac == mac) {
            return &device;
        }
    }
    return nullptr;
}

[[nodiscard]] QString describeRefusal(const QString &reasonCode)
{
    if (reasonCode == QLatin1String("no-color")) {
        return translate("This light does not have colour channels.");
    }
    if (reasonCode == QLatin1String("no-color-temperature")) {
        return translate("This light cannot change its white temperature.");
    }
    if (reasonCode == QLatin1String("no-dimming")) {
        return translate("This light cannot be dimmed.");
    }
    if (reasonCode == QLatin1String("no-scenes")
        || reasonCode == QLatin1String("scene-unsupported")) {
        return translate("This light cannot run that scene.");
    }
    if (reasonCode == QLatin1String("scene-not-dynamic")) {
        return translate("Speed applies only while an animated scene is playing.");
    }
    if (reasonCode == QLatin1String("capabilities-unknown")) {
        return translate("This light has not said yet what it can do.");
    }
    return translate("This light cannot do that.");
}

} // namespace

RequestState beginSmartLightRequest(const Wiz::Snapshot &snapshot,
                                    const Wiz::OperationRequest &operation,
                                    const bool controlGranted)
{
    if (!controlGranted) {
        return refused(operation,
                       translate("This desktop is not permitted to control lights."));
    }
    if (snapshot.availability != Wiz::Availability::Ready
        && snapshot.availability != Wiz::Availability::Degraded) {
        return refused(operation, translate("Smart lights are not available."));
    }

    // Discovery and a whole-network refresh have no single target device.
    const bool untargeted = operation.kind == Wiz::OperationKind::Discover
        || (operation.kind == Wiz::OperationKind::Refresh
            && operation.targetMac.isEmpty());
    if (!untargeted) {
        const Wiz::Device *device = findDevice(snapshot, operation.targetMac);
        if (device == nullptr) {
            return refused(operation, translate("That light is no longer listed."));
        }
        if (device->reachability == Wiz::Reachability::Unreachable
            || device->reachability == Wiz::Reachability::Unknown) {
            return refused(operation, translate("%1 is not responding.").arg(device->label));
        }
        if (operation.kind != Wiz::OperationKind::Refresh) {
            const Wiz::ValidatedRequest validated =
                Wiz::validateStateRequest(*device, operation.state);
            if (!validated.accepted()) {
                return refused(operation, describeRefusal(validated.reasonCode));
            }
        }
    }

    RequestState state;
    state.phase = RequestPhase::Pending;
    state.operation = operation;
    state.initiatingEpoch = snapshot.epoch;
    state.initiatingRevision = snapshot.revision;
    return state;
}

RequestState applySmartLightResult(const RequestState &request,
                                   const Wiz::OperationResult &result)
{
    if (!request.pending()) {
        return request;
    }
    if (result.kind != request.operation.kind
        || result.targetMac != request.operation.targetMac
        || result.initiatingEpoch != request.initiatingEpoch) {
        // Not this request's result. Saying nothing is correct: the pending
        // request is still outstanding.
        return request;
    }

    RequestState updated = request;
    switch (result.status) {
    case Wiz::OperationStatus::Succeeded:
        updated.phase = RequestPhase::Succeeded;
        updated.feedback.clear();
        break;
    case Wiz::OperationStatus::Unsupported:
        updated.phase = RequestPhase::Failed;
        updated.feedback = describeRefusal(result.reasonCode);
        break;
    case Wiz::OperationStatus::Rejected:
        updated.phase = RequestPhase::Failed;
        updated.feedback = translate("That change was not accepted.");
        break;
    case Wiz::OperationStatus::Failed:
        updated.phase = RequestPhase::Failed;
        updated.feedback = result.diagnostic.isEmpty()
            ? translate("The light refused that change.")
            : result.diagnostic;
        break;
    case Wiz::OperationStatus::Busy:
        updated.phase = RequestPhase::Failed;
        updated.feedback = translate("Too many changes at once. Try again.");
        break;
    case Wiz::OperationStatus::Uncertain:
        updated.phase = RequestPhase::Uncertain;
        updated.feedback = translate("The light did not confirm that change.");
        break;
    }
    return updated;
}

RequestState observeSmartLightAuthority(const RequestState &request,
                                        const bool serviceAvailable,
                                        const quint64 currentEpoch)
{
    if (!request.pending()) {
        return request;
    }
    if (serviceAvailable && currentEpoch == request.initiatingEpoch) {
        return request;
    }
    RequestState updated = request;
    updated.phase = RequestPhase::Uncertain;
    updated.feedback = translate("The light did not confirm that change.");
    return updated;
}

} // namespace QindaQt::Shell::SmartLightsApplet
