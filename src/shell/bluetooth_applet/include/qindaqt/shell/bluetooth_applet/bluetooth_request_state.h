// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QString>

#include <optional>

namespace QindaQt::Shell::BluetoothApplet
{

enum class RequestPhase {
    Idle,
    Pending,
    Succeeded,
    Failed,
    Uncertain,
};

struct RequestState {
    RequestPhase phase = RequestPhase::Idle;
    Bluetooth::OperationRequest operation;
    quint64 initiatingEpoch = 0;
    quint64 initiatingRevision = 0;
    QString feedback;

    [[nodiscard]] bool pending() const noexcept
    {
        return phase == RequestPhase::Pending;
    }

    friend bool operator==(const RequestState &, const RequestState &) = default;
};

// Admission is intentionally duplicated at the final presentation boundary:
// it consumes only a validated current snapshot and never trusts QML row state.
[[nodiscard]] RequestState beginBluetoothRequest(
    const Bluetooth::Snapshot &snapshot,
    const Bluetooth::OperationRequest &operation,
    bool controlGranted,
    std::optional<Bluetooth::Handle> discoveryLease = std::nullopt);

// Only the exact operation kind and initiating lineage can complete a pending
// request. Foreign/stale results are uncertainty, never success or replay.
[[nodiscard]] RequestState applyBluetoothResult(
    const RequestState &request,
    const Bluetooth::OperationResult &result);

[[nodiscard]] RequestState observeBluetoothAuthority(
    const RequestState &request,
    bool exactOwnerAvailable,
    quint64 currentEpoch);

} // namespace QindaQt::Shell::BluetoothApplet
