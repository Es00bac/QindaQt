// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/bluetooth_applet/bluetooth_request_state.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <algorithm>

namespace QindaQt::Shell::BluetoothApplet
{
namespace
{

const Bluetooth::Adapter *findAdapter(const Bluetooth::Snapshot &snapshot,
                                      const Bluetooth::Handle &handle)
{
    const auto it = std::ranges::find_if(
        snapshot.adapters, [&handle](const Bluetooth::Adapter &candidate) {
            return candidate.handle == handle;
        });
    return it == snapshot.adapters.cend() ? nullptr : &*it;
}

const Bluetooth::Device *findDevice(const Bluetooth::Snapshot &snapshot,
                                    const Bluetooth::Handle &handle)
{
    const auto it = std::ranges::find_if(
        snapshot.devices, [&handle](const Bluetooth::Device &candidate) {
            return candidate.handle == handle;
        });
    return it == snapshot.devices.cend() ? nullptr : &*it;
}

RequestState rejected(const Bluetooth::OperationRequest &operation,
                      const QString &feedback)
{
    return {.phase = RequestPhase::Failed,
            .operation = operation,
            .feedback = feedback};
}

QString failureFeedback(const Bluetooth::OperationResult &result)
{
    switch (result.status) {
    case Bluetooth::OperationStatus::Rejected:
        return QStringLiteral("The Bluetooth request was rejected: %1.")
            .arg(result.reasonCode);
    case Bluetooth::OperationStatus::Unsupported:
        return QStringLiteral("That Bluetooth operation is not supported.");
    case Bluetooth::OperationStatus::Busy:
        return QStringLiteral("Bluetooth is busy; try again after it settles.");
    case Bluetooth::OperationStatus::Failed:
        return QStringLiteral("The Bluetooth operation failed: %1.")
            .arg(result.reasonCode);
    case Bluetooth::OperationStatus::Uncertain:
        return QStringLiteral(
            "The Bluetooth result is uncertain. Check current state before retrying.");
    case Bluetooth::OperationStatus::Succeeded:
        return {};
    }
    return QStringLiteral("The Bluetooth result could not be understood.");
}

} // namespace

RequestState beginBluetoothRequest(
    const Bluetooth::Snapshot &snapshot,
    const Bluetooth::OperationRequest &operation,
    const bool controlGranted,
    const std::optional<Bluetooth::Handle> discoveryLease)
{
    if (!controlGranted) {
        return rejected(operation,
                        QStringLiteral("Bluetooth controls are not allowed for this applet."));
    }
    if (!Bluetooth::validateSnapshot(snapshot).accepted
        || snapshot.availability != Bluetooth::Availability::Ready
        || operation.target.epoch != snapshot.epoch) {
        return rejected(operation,
                        QStringLiteral("Bluetooth state is not current, so no request was sent."));
    }
    if (!Bluetooth::validateOperationRequest(operation).accepted) {
        return rejected(operation, QStringLiteral("That Bluetooth request was not accepted."));
    }

    switch (operation.kind) {
    case Bluetooth::OperationKind::SetAdapterPower: {
        const Bluetooth::Adapter *adapter = findAdapter(snapshot, operation.target);
        if (!snapshot.capabilities.testFlag(Bluetooth::Capability::SetAdapterPower)
            || adapter == nullptr) {
            return rejected(operation, QStringLiteral("Adapter power control is unavailable."));
        }
        if (adapter->powered == operation.powered) {
            return rejected(operation, QStringLiteral("The adapter is already in that power state."));
        }
        break;
    }
    case Bluetooth::OperationKind::AcquireDiscovery: {
        const Bluetooth::Adapter *adapter = findAdapter(snapshot, operation.target);
        if (!snapshot.capabilities.testFlag(Bluetooth::Capability::DiscoveryLease)
            || adapter == nullptr || !adapter->powered) {
            return rejected(operation, QStringLiteral("Discovery is unavailable for that adapter."));
        }
        if (discoveryLease.has_value()) {
            return rejected(operation, QStringLiteral("This applet already holds a discovery lease."));
        }
        break;
    }
    case Bluetooth::OperationKind::ReleaseDiscovery:
        if (!snapshot.capabilities.testFlag(Bluetooth::Capability::DiscoveryLease)
            || findAdapter(snapshot, operation.target) == nullptr
            || !discoveryLease.has_value()
            || *discoveryLease != operation.target) {
            return rejected(operation, QStringLiteral("This applet holds no matching discovery lease."));
        }
        break;
    case Bluetooth::OperationKind::Connect: {
        const Bluetooth::Device *device = findDevice(snapshot, operation.target);
        const Bluetooth::Adapter *adapter = device == nullptr
            ? nullptr : findAdapter(snapshot, device->adapterHandle);
        if (!snapshot.capabilities.testFlag(Bluetooth::Capability::ConnectPaired)
            || device == nullptr || adapter == nullptr || !adapter->powered
            || !device->paired || device->connected) {
            return rejected(operation, QStringLiteral("That paired device cannot be connected now."));
        }
        break;
    }
    case Bluetooth::OperationKind::Disconnect: {
        const Bluetooth::Device *device = findDevice(snapshot, operation.target);
        if (!snapshot.capabilities.testFlag(Bluetooth::Capability::DisconnectPaired)
            || device == nullptr || !device->connected) {
            return rejected(operation, QStringLiteral("That device cannot be disconnected now."));
        }
        break;
    }
    case Bluetooth::OperationKind::Pair:
    case Bluetooth::OperationKind::CancelPairing:
    case Bluetooth::OperationKind::RemoveDevice:
    case Bluetooth::OperationKind::SetTrusted:
    case Bluetooth::OperationKind::ReplyConfirmation:
    case Bluetooth::OperationKind::ReplyPasskey:
    case Bluetooth::OperationKind::ReplyPin:
    case Bluetooth::OperationKind::CancelPrompt:
        return rejected(operation,
                        QStringLiteral("That operation is outside this applet request path."));
    }

    return {.phase = RequestPhase::Pending,
            .operation = operation,
            .initiatingEpoch = snapshot.epoch,
            .initiatingRevision = snapshot.revision,
            .feedback = {}};
}

RequestState applyBluetoothResult(const RequestState &request,
                                  const Bluetooth::OperationResult &result)
{
    if (!request.pending()) {
        return request;
    }
    RequestState completed = request;
    const bool exactInitiator = result.wireValid
        && result.kind == request.operation.kind
        && result.initiatingEpoch == request.initiatingEpoch
        && result.initiatingRevision == request.initiatingRevision;
    if (!exactInitiator || !Bluetooth::validateOperationResult(result).accepted) {
        completed.phase = RequestPhase::Uncertain;
        completed.feedback = QStringLiteral(
            "The Bluetooth service returned an unreadable result. Check current state before retrying.");
        return completed;
    }
    if (result.status == Bluetooth::OperationStatus::Succeeded) {
        if (result.observedEpoch != request.initiatingEpoch
            || result.observedRevision < request.initiatingRevision) {
            completed.phase = RequestPhase::Uncertain;
            completed.feedback = QStringLiteral(
                "The Bluetooth result has stale lineage. Check current state before retrying.");
            return completed;
        }
        completed.phase = RequestPhase::Succeeded;
        completed.feedback.clear();
        return completed;
    }
    completed.phase = result.status == Bluetooth::OperationStatus::Uncertain
        ? RequestPhase::Uncertain : RequestPhase::Failed;
    completed.feedback = failureFeedback(result);
    return completed;
}

RequestState observeBluetoothAuthority(const RequestState &request,
                                       const bool exactOwnerAvailable,
                                       const quint64 currentEpoch)
{
    if (!request.pending()) {
        return request;
    }
    if (exactOwnerAvailable && currentEpoch == request.initiatingEpoch) {
        return request;
    }
    RequestState uncertain = request;
    uncertain.phase = RequestPhase::Uncertain;
    uncertain.feedback = QStringLiteral(
        "Bluetooth authority changed. Check current state before retrying.");
    return uncertain;
}

} // namespace QindaQt::Shell::BluetoothApplet
