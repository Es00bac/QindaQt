// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QindaQt::Bluetooth
{

struct OperationSubmission {
    bool pending = false;
    quint64 operationId = 0;
    OperationResult immediateResult;
};

// AGENT-CONTRACT: BluetoothModel owns authoritative Qt-thread publication,
// handle lineage, and operation coordination for Bluetooth1. It is used and
// owned on exactly one Qt thread. The borrowed backend shares that thread and
// must outlive the model. Every public mutation result carries the initiating
// epoch/revision; stop() is a publication barrier that makes every outstanding
// operation Uncertain. start() after stop() advances the epoch, invalidating
// all previously issued handles. Discovery leases are caller-scoped and
// reference-counted by the backend; the model enforces the documented bounds
// against backend-reported lease state and fails closed on contradictions.
class BluetoothModel : public QObject
{
    Q_OBJECT

public:
    // epochSeed == 0 derives a restart-unique epoch from wall clock plus
    // entropy; tests pass an explicit nonzero seed for reproducibility.
    explicit BluetoothModel(AdapterBackend *backend, quint64 epochSeed = 0,
                            QObject *parent = nullptr);

    [[nodiscard]] const Snapshot &snapshot() const noexcept;
    [[nodiscard]] OperationSubmission submit(const OperationRequest &request,
                                             const QString &callerId);
    // Drops every discovery lease held by a vanished caller and stops
    // discovery where that caller held the last reference.
    void ownerVanished(const QString &callerId);
    void start();
    void stop();

Q_SIGNALS:
    void snapshotChanged(const QindaQt::Bluetooth::Snapshot &snapshot);
    void invalidated(quint64 epoch, quint64 revision);
    void operationCompleted(quint64 operationId,
                            const QindaQt::Bluetooth::OperationResult &result);

private Q_SLOTS:
    void acceptInventory(quint64 generation,
                         const QindaQt::Bluetooth::BackendInventory &inventory);
    void acceptBackendResult(quint64 generation, quint64 operationId,
                             const QindaQt::Bluetooth::BackendOperationOutcome &outcome);

private:
    struct PendingOperation {
        OperationKind kind = OperationKind::Connect;
        quint64 epoch = 0;
        quint64 revision = 0;
    };

    [[nodiscard]] OperationResult immediate(const OperationRequest &request,
                                            OperationStatus status,
                                            const QString &reasonCode) const;
    [[nodiscard]] QString validateRequest(const OperationRequest &request,
                                          const QString &callerId) const;
    [[nodiscard]] const Adapter *findAdapter(quint64 serial) const;
    [[nodiscard]] const Device *findDevice(quint64 serial) const;
    void makePendingUncertain(const QString &reasonCode);
    void publishBackendMalformed();
    [[nodiscard]] Snapshot projectInventory(const BackendInventory &inventory) const;
    [[nodiscard]] bool leaseBoundsRespected(const BackendInventory &inventory) const;
    [[nodiscard]] quint64 advanceEpoch();

    AdapterBackend *m_backend = nullptr;
    Snapshot m_snapshot;
    BackendInventory m_inventory;
    QHash<quint64, PendingOperation> m_pending;
    quint64 m_nextOperationId = 1;
    quint64 m_backendGeneration = 0;
    bool m_hasInventory = false;
    bool m_running = false;
};

} // namespace QindaQt::Bluetooth
