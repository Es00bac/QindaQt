// SPDX-License-Identifier: GPL-3.0-or-later
#include "bluez_adapter_backend_p.h"

namespace QindaQt::Bluetooth {
using QindaQt::BluetoothRadio::Disposition;

bool BluezAdapterBackend::powerCurrent(quint64 operationId) const {
    const auto it = d->powers.constFind(operationId);
    if (it == d->powers.cend() || !d->running || it->generation != d->generation
        || it->owner.isEmpty() || it->owner != d->transport.owner()) return false;
    const auto *adapter = d->store.adapter(it->path);
    return adapter && adapter->address == it->request.adapterAddress;
}
void BluezAdapterBackend::submitSetPower(quint64 operationId, const BackendRequest &request) {
    const auto *adapter = d->store.adapterByAddress(request.adapterAddress);
    if (!adapter || d->transport.owner().isEmpty()) {
        finishOperation(operationId, BackendOperationStatus::Rejected, QStringLiteral("stale-handle"));
        return;
    }
    if (d->powers.size() >= 32 || d->powers.contains(operationId)) {
        finishOperation(operationId, BackendOperationStatus::Rejected, QStringLiteral("radio-busy"));
        return;
    }
    d->powers.insert(operationId, {request, adapter->path, d->transport.owner(), d->generation});
    if (!request.powered || !d->radio) {
        dispatchPower(operationId);
        return;
    }
    const QPointer<BluezAdapterBackend> lifetime(this);
    const quint64 id = d->radio->observeAndUnblock(d->transport.owner(), adapter->path,
        request.adapterAddress, request.callerId, [lifetime, operationId] {
            return lifetime && lifetime->powerCurrent(operationId);
        });
    const auto it = d->powers.find(operationId);
    if (it == d->powers.end()) {
        if (id && d->radio) d->radio->cancel(id);
        return;
    }
    if (!id) {
        d->powers.erase(it);
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("radio-not-authorized"));
        return;
    }
    it->radioId = id;
}
void BluezAdapterBackend::handleRadioFinished(quint64 radioId,
    const QindaQt::BluetoothRadio::Result &result) {
    quint64 operationId = 0;
    for (auto it = d->powers.cbegin(); it != d->powers.cend(); ++it)
        if (it->radioId == radioId) { operationId = it.key(); break; }
    if (!operationId) return;
    if (!powerCurrent(operationId)) { retirePower(); return; }
    d->powers[operationId].radioId = 0;
    if (!QindaQt::BluetoothRadio::validResult(result)) {
        d->powers.remove(operationId);
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("radio-change-uncertain"));
        return;
    }
    switch (result.disposition) {
    case Disposition::VerifiedUnblocked:
    case Disposition::NoWriteUnavailable:
        // A definitive no-write absence keeps the pre-existing direct BlueZ
        // behavior. It is never represented as verified unblocked radio truth.
        dispatchPower(operationId);
        return;
    case Disposition::Refused:
    case Disposition::Uncertain:
        d->powers.remove(operationId);
        finishOperation(operationId, result.disposition == Disposition::Uncertain
            ? BackendOperationStatus::Uncertain : BackendOperationStatus::Rejected,
            result.reasonCode);
        return;
    }
}
void BluezAdapterBackend::dispatchPower(quint64 operationId) {
    if (!powerCurrent(operationId)) { retirePower(); return; }
    const auto power = d->powers.value(operationId);
    const quint64 callId = d->transport.setAdapterPowered(power.path, power.request.powered);
    if (!callId) {
        d->powers.remove(operationId);
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-power-uncertain"));
        return;
    }
    d->powers[operationId].bluezId = callId;
    d->outstanding.insert(callId, {.operationId = operationId,
        .kind = OperationKind::SetAdapterPower, .callerId = power.request.callerId,
        .adapterAddress = power.request.adapterAddress, .deviceAddress = {},
        .powered = power.request.powered});
}
void BluezAdapterBackend::finishPowerCall(quint64 operationId, bool succeeded,
    const QString &errorName) {
    if (!d->powers.contains(operationId)) return;
    const bool current = powerCurrent(operationId);
    d->powers.remove(operationId);
    if (!current) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("stale-handle"));
        return;
    }
    if (succeeded) {
        // AGENT-GUARD: Do not synthesize Powered from a Set reply. BlueZ property
        // publications independently establish current state, even when a
        // preceding request returned Error.Failed (ADR-0359).
        finishOperation(operationId, BackendOperationStatus::Succeeded,
                        QStringLiteral("adapter-power-set"));
        return;
    }
    if (errorName == QLatin1String("org.bluez.Error.Blocked")) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("radio-blocked"));
    } else if (errorName == QLatin1String("org.bluez.Error.NotAuthorized")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.AccessDenied")) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("radio-not-authorized"));
    } else {
        // Failed is also returned for asynchronous controller failures; it
        // does not prove absence of an effect or identify an rfkill cause.
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-power-uncertain"));
    }
}
void BluezAdapterBackend::retirePower(const QString &path, const QString &caller, bool notify) {
    for (auto it = d->queuedPowers.begin(); it != d->queuedPowers.end();) {
        if ((!path.isEmpty() && it->path != path)
            || (!caller.isEmpty() && it->request.callerId != caller)) { ++it; continue; }
        it = d->queuedPowers.erase(it);
    }
    QList<State::Power> retired;
    QList<quint64> ids;
    for (auto it = d->powers.begin(); it != d->powers.end();) {
        if ((!path.isEmpty() && it->path != path)
            || (!caller.isEmpty() && it->request.callerId != caller)) { ++it; continue; }
        ids.append(it.key());
        retired.append(it.value());
        if (it->bluezId) d->outstanding.remove(it->bluezId);
        it = d->powers.erase(it);
    }
    // Remove authority before cancellation or externally visible completion.
    // Same-address remove/add never revives a captured request.
    for (qsizetype index = 0; index < retired.size(); ++index) {
        const auto &power = retired[index];
        if (power.radioId && d->radio) d->radio->cancel(power.radioId);
        if (notify && d->running)
            finishOperation(ids[index], BackendOperationStatus::Uncertain,
                power.bluezId ? QStringLiteral("bluez-power-uncertain")
                              : QStringLiteral("radio-change-uncertain"));
    }
}
}
