// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_ports.h>
#include <qindaqt/services/display_transaction/transaction_machine.h>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::DisplayService
{

namespace Private
{
class BrightnessAuthority;
}

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

struct BrightnessRequestResult {
    bool available = false;
    // False only for an Accepted request: exactly one BrightnessFinish with
    // requestId later drains from the model.
    bool final = true;
    quint64 requestId = 0;
    Display::OperationResult operation;
};

struct BrightnessFinish {
    quint64 requestId = 0;
    Display::OperationResult operation;
};

using EpochFactory = std::function<QString()>;

class DisplayServiceModel final
{
public:
    // clock and port are borrowed on the constructing thread and must outlive
    // the model. EpochFactory is copied and invoked only for a newly accepted
    // unique-owner lineage; it supplies a bounded, restart-unique seed. The
    // model combines that seed with a process-monotonic lineage so a repeated
    // seed cannot republish an accepted public epoch. All returned/accessed
    // values are model-owned until the next call. Rejected same-owner calls
    // preserve live public truth. A rejected replacement-owner call reports
    // stateChanged when it first revokes the old lineage; a failed first-lineage
    // recovery still consumes its outer callback fence but publishes no
    // snapshot or machine.
    DisplayServiceModel(DisplayTransaction::MonotonicClock &clock,
                        TransactionPort &port,
                        EpochFactory epochFactory,
                        DisplayTransaction::Timing timing = {},
                        std::optional<DisplayTransaction::Journal> startupJournal =
                            std::nullopt);
    ~DisplayServiceModel();

    [[nodiscard]] bool available() const noexcept;
    // Returns the accepted machine snapshot composed, by one whole-value copy
    // at this read boundary, with exactly zero or one validated public
    // transaction summary projected from the machine view. The pointer is
    // model-owned and stays valid until the next call on this model.
    [[nodiscard]] const Display::Snapshot *snapshot() const;
    [[nodiscard]] const DisplayTransaction::MachineView *view() const noexcept;
    [[nodiscard]] const QString &sourceOwner() const noexcept;
    [[nodiscard]] quint64 sourceGeneration() const noexcept;
    [[nodiscard]] quint64 machineLineage() const noexcept;

    InventoryObservationResult observeInventory(const InventoryFrame &frame);
    // Clears source-derived live state. An active journal is retained as
    // pending recovery authority; the process-monotonic lineage remains
    // consumed so recovery cannot republish an old epoch.
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

    // D7 immediate brightness (ADR-0150). The publication joins the accepted
    // machine snapshot and is nullptr while that snapshot is unavailable.
    [[nodiscard]] const Display::BrightnessSnapshot *brightnessSnapshot() const;
    [[nodiscard]] BrightnessRequestResult setOutputBrightness(
        const Display::BrightnessRequest &request);
    void brightnessDevicesObserved(const DeviceBrightnessFrame &frame);
    void brightnessCompleted(quint64 requestId, BrightnessApplyOutcome outcome);
    void brightnessTick();
    // Zero when no brightness request is pending.
    [[nodiscard]] quint64 brightnessDeadlineMonotonicMilliseconds() const noexcept;
    // Drain what earlier calls finished or republished. Callers publish the
    // Changed hint before delivering finishes.
    [[nodiscard]] QList<BrightnessFinish> takeBrightnessFinishes();
    [[nodiscard]] bool takeBrightnessPublicationChanged();

private:
    [[nodiscard]] ServiceOperationResult operation(
        Display::OperationKind kind, const QString &transactionId,
        const std::function<DisplayTransaction::CommandResult()> &command);
    [[nodiscard]] InventoryObservationResult observeFrame(const InventoryFrame &frame);
    [[nodiscard]] InventoryObservationResult establishLineage(
        const InventoryFrame &frame);
    [[nodiscard]] DisplayTransaction::CommandResult routeObservation(
        const Display::Snapshot &snapshot, bool outputSetChanged);
    [[nodiscard]] const Display::Snapshot *machineSnapshot() const noexcept;

    DisplayTransaction::MonotonicClock &m_clock;
    TransactionPort &m_port;
    EpochFactory m_epochFactory;
    DisplayTransaction::Timing m_timing;
    std::unique_ptr<DisplayTransaction::Machine> m_machine;
    InventoryFrame m_frame;
    QString m_sourceOwner;
    QString m_serviceEpoch;
    quint64 m_machineLineage = 0;
    DisplayTransaction::SafetyState m_safety = DisplayTransaction::SafetyState::Unknown;
    // AGENT-GUARD: This value is authority awaiting a complete replacement
    // inventory, not a cache. It is consumed only after D1 accepts recover();
    // owner/loss reset must retain it while journalActive is true.
    std::optional<DisplayTransaction::Journal> m_pendingRecoveryJournal;
    // Composed lazily by snapshot(); mutable so the read boundary stays const.
    mutable Display::Snapshot m_publicSnapshot;
    std::unique_ptr<Private::BrightnessAuthority> m_brightness;
};

} // namespace QindaQt::DisplayService
