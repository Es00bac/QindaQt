// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_ports.h>
#include <qindaqt/services/display_transaction/transaction_machine.h>

#include <functional>
#include <memory>

namespace QindaQt::DisplayService
{

enum class InventoryObservationStatus {
    AcceptedNewLineage,
    AcceptedChanged,
    AcceptedUnchanged,
    Rejected,
};

struct InventoryObservationResult {
    InventoryObservationStatus status = InventoryObservationStatus::Rejected;
    InventoryError error = InventoryError::None;
    QString reasonCode;
    bool stateChanged = false;

    [[nodiscard]] bool accepted() const noexcept
    {
        return status != InventoryObservationStatus::Rejected;
    }
};

struct ServiceOperationResult {
    bool available = false;
    DisplayTransaction::CommandResult command;
    Display::OperationResult operation;
};

using EpochFactory = std::function<QString()>;

class DisplayServiceModel final
{
public:
    // clock and port are borrowed on the constructing thread and must outlive
    // the model. EpochFactory is copied and invoked only for a newly accepted
    // unique-owner lineage. All returned/accessed values are model-owned until
    // the next call; rejected calls preserve every observable field.
    DisplayServiceModel(DisplayTransaction::MonotonicClock &clock,
                        TransactionPort &port,
                        EpochFactory epochFactory,
                        DisplayTransaction::Timing timing = {});
    ~DisplayServiceModel();

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] const Display::Snapshot *snapshot() const noexcept;
    [[nodiscard]] const DisplayTransaction::MachineView *view() const noexcept;
    [[nodiscard]] const QString &sourceOwner() const noexcept;
    [[nodiscard]] quint64 sourceGeneration() const noexcept;
    [[nodiscard]] quint64 machineLineage() const noexcept;

    InventoryObservationResult observeInventory(const InventoryFrame &frame);
    // Clears all source-derived and transaction state. The last epoch is kept
    // only to reject an epoch factory that accidentally reuses it at recovery.
    [[nodiscard]] bool transportLost();

    [[nodiscard]] ServiceOperationResult stage(const QString &transactionId,
                                               const Display::Candidate &candidate);
    [[nodiscard]] ServiceOperationResult preview(const QString &transactionId);
    [[nodiscard]] ServiceOperationResult confirm(const QString &transactionId);
    [[nodiscard]] ServiceOperationResult cancel(const QString &transactionId);

    DisplayTransaction::CommandResult applyCompleted(
        quint64 machineLineage, quint64 token,
        DisplayTransaction::ApplyOutcome outcome);
    DisplayTransaction::CommandResult topologySettled();
    DisplayTransaction::CommandResult safetyChanged(
        DisplayTransaction::SafetyState safety);
    DisplayTransaction::CommandResult prepareForSuspend();
    DisplayTransaction::CommandResult tick();

private:
    [[nodiscard]] ServiceOperationResult operation(
        Display::OperationKind kind, const QString &transactionId,
        const std::function<DisplayTransaction::CommandResult()> &command);
    [[nodiscard]] InventoryObservationResult establishLineage(
        const InventoryFrame &frame);
    [[nodiscard]] DisplayTransaction::CommandResult routeObservation(
        const Display::Snapshot &snapshot, bool outputSetChanged);

    DisplayTransaction::MonotonicClock &m_clock;
    TransactionPort &m_port;
    EpochFactory m_epochFactory;
    DisplayTransaction::Timing m_timing;
    std::unique_ptr<DisplayTransaction::Machine> m_machine;
    InventoryFrame m_frame;
    QString m_sourceOwner;
    QString m_lastEpoch;
    quint64 m_machineLineage = 0;
    DisplayTransaction::SafetyState m_safety = DisplayTransaction::SafetyState::Unknown;
};

} // namespace QindaQt::DisplayService
