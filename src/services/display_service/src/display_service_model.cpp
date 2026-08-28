// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_service/display_service_model.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QSet>

#include <limits>
#include <utility>

namespace QindaQt::DisplayService
{
namespace
{

Display::ErrorCode protocolError(const DisplayTransaction::CommandError error)
{
    using CommandError = DisplayTransaction::CommandError;
    switch (error) {
    case CommandError::None:
        return Display::ErrorCode::None;
    case CommandError::InvalidSnapshot:
    case CommandError::InvalidJournal:
    case CommandError::InvalidCandidate:
    case CommandError::InvalidTransactionId:
    case CommandError::NoOp:
        return Display::ErrorCode::InvalidCandidate;
    case CommandError::StaleRevision:
        return Display::ErrorCode::StaleRevision;
    case CommandError::TransactionActive:
        return Display::ErrorCode::TransactionActive;
    case CommandError::UnknownTransaction:
        return Display::ErrorCode::UnknownTransaction;
    case CommandError::InvalidTransition:
    case CommandError::CallbackOutOfOrder:
        return Display::ErrorCode::InvalidTransition;
    case CommandError::Locked:
        return Display::ErrorCode::Locked;
    case CommandError::Suspend:
    case CommandError::ApplyUncertain:
        return Display::ErrorCode::CompositorUnavailable;
    case CommandError::JournalFailure:
        return Display::ErrorCode::JournalFailure;
    case CommandError::ApplyRejected:
        return Display::ErrorCode::CompositorRejected;
    case CommandError::ObservationMismatch:
        return Display::ErrorCode::ExternalChange;
    case CommandError::ObservationTimeout:
        return Display::ErrorCode::Timeout;
    case CommandError::ExternalChange:
        return Display::ErrorCode::ExternalChange;
    case CommandError::TopologyChanged:
        return Display::ErrorCode::TopologyChanged;
    case CommandError::RevertFailed:
        return Display::ErrorCode::RevertFailed;
    }
    return Display::ErrorCode::InvalidTransition;
}

QString errorReason(const DisplayTransaction::CommandError error)
{
    using CommandError = DisplayTransaction::CommandError;
    switch (error) {
    case CommandError::None: return {};
    case CommandError::InvalidSnapshot: return QStringLiteral("invalid-snapshot");
    case CommandError::InvalidJournal: return QStringLiteral("invalid-journal");
    case CommandError::InvalidTransition: return QStringLiteral("invalid-transition");
    case CommandError::StaleRevision: return QStringLiteral("stale-revision");
    case CommandError::TransactionActive: return QStringLiteral("transaction-active");
    case CommandError::UnknownTransaction: return QStringLiteral("unknown-transaction");
    case CommandError::InvalidTransactionId: return QStringLiteral("invalid-transaction-id");
    case CommandError::InvalidCandidate: return QStringLiteral("invalid-candidate");
    case CommandError::NoOp: return QStringLiteral("no-op");
    case CommandError::Locked: return QStringLiteral("locked");
    case CommandError::Suspend: return QStringLiteral("suspend");
    case CommandError::JournalFailure: return QStringLiteral("journal-failure");
    case CommandError::CallbackOutOfOrder: return QStringLiteral("callback-out-of-order");
    case CommandError::ApplyRejected: return QStringLiteral("apply-rejected");
    case CommandError::ApplyUncertain: return QStringLiteral("apply-uncertain");
    case CommandError::ObservationMismatch: return QStringLiteral("observation-mismatch");
    case CommandError::ObservationTimeout: return QStringLiteral("observation-timeout");
    case CommandError::ExternalChange: return QStringLiteral("external-change");
    case CommandError::TopologyChanged: return QStringLiteral("topology-changed");
    case CommandError::RevertFailed: return QStringLiteral("revert-failed");
    }
    return QStringLiteral("invalid-transition");
}

bool sameOutputSet(const Display::Snapshot &left, const Display::Snapshot &right)
{
    if (left.outputs.size() != right.outputs.size()) {
        return false;
    }
    QSet<QString> ids;
    for (const Display::Output &output : left.outputs) {
        ids.insert(output.stableId);
    }
    for (const Display::Output &output : right.outputs) {
        if (!ids.contains(output.stableId)) {
            return false;
        }
    }
    return true;
}

DisplayTransaction::CommandResult unavailableCommand()
{
    return {.accepted = false,
            .stateChanged = false,
            .error = DisplayTransaction::CommandError::InvalidTransition,
            .state = DisplayTransaction::MachineState::Discovering,
            .transactionId = {}};
}

QString publicEpoch(const QString &restartSeed, const quint64 machineLineage)
{
    const QByteArray digest = QCryptographicHash::hash(
        restartSeed.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("d2:%1:%2")
        .arg(machineLineage)
        .arg(QString::fromLatin1(digest));
}

} // namespace

DisplayServiceModel::DisplayServiceModel(DisplayTransaction::MonotonicClock &clock,
                                         TransactionPort &port,
                                         EpochFactory epochFactory,
                                         DisplayTransaction::Timing timing)
    : m_clock(clock)
    , m_port(port)
    , m_epochFactory(std::move(epochFactory))
    , m_timing(timing)
{
}

DisplayServiceModel::~DisplayServiceModel() = default;

bool DisplayServiceModel::available() const noexcept
{
    return m_machine != nullptr;
}

const Display::Snapshot *DisplayServiceModel::snapshot() const noexcept
{
    return m_machine == nullptr ? nullptr : &m_machine->currentSnapshot();
}

const DisplayTransaction::MachineView *DisplayServiceModel::view() const noexcept
{
    return m_machine == nullptr ? nullptr : &m_machine->view();
}

const QString &DisplayServiceModel::sourceOwner() const noexcept
{
    return m_sourceOwner;
}

quint64 DisplayServiceModel::sourceGeneration() const noexcept
{
    return m_frame.outputGeneration;
}

quint64 DisplayServiceModel::machineLineage() const noexcept
{
    return m_machineLineage;
}

InventoryObservationResult DisplayServiceModel::establishLineage(
    const InventoryFrame &frame)
{
    if (m_machine != nullptr) {
        (void)transportLost();
    }
    if (!m_epochFactory) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::ProjectionFailure,
                .reasonCode = QStringLiteral("missing-epoch-factory"),
                .stateChanged = false};
    }
    const QString restartSeed = m_epochFactory();
    if (!Display::isBoundedText(restartSeed,
                                Display::kMaxServiceEpochUtf8Bytes)) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::ProjectionFailure,
                .reasonCode = QStringLiteral("invalid-service-epoch-seed"),
                .stateChanged = false};
    }
    if (m_machineLineage == std::numeric_limits<quint64>::max()) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::ProjectionFailure,
                .reasonCode = QStringLiteral("machine-lineage-exhausted"),
                .stateChanged = false};
    }
    const quint64 nextMachineLineage = m_machineLineage + 1;
    const QString epoch = publicEpoch(restartSeed, nextMachineLineage);
    // AGENT-GUARD: Never expose the raw seed or omit the process lineage. A
    // bounded A/B/A factory sequence must not recreate the first public fence.
    const InventoryProjectionResult projection = projectInventory(frame, epoch);
    if (!projection.accepted()) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = projection.error,
                .reasonCode = projection.reasonCode,
                .stateChanged = false};
    }

    auto machine = std::make_unique<DisplayTransaction::Machine>(m_clock, m_port,
                                                                 m_timing);
    const DisplayTransaction::CommandResult initialized =
        machine->initialize(projection.snapshot, m_safety);
    if (!initialized.accepted) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::ProjectionFailure,
                .reasonCode = QStringLiteral("transaction-initialization-failed"),
                .stateChanged = false};
    }
    m_machineLineage = nextMachineLineage;
    m_port.beginMachineLineage(m_machineLineage);
    m_machine = std::move(machine);
    m_frame = frame;
    m_sourceOwner = frame.uniqueOwner;
    m_serviceEpoch = epoch;
    return {.status = InventoryObservationStatus::AcceptedNewLineage,
            .error = InventoryError::None,
            .reasonCode = {},
            .stateChanged = true};
}

InventoryObservationResult DisplayServiceModel::observeInventory(
    const InventoryFrame &frame)
{
    if (m_machine == nullptr || frame.uniqueOwner != m_sourceOwner) {
        return establishLineage(frame);
    }
    if (frame.outputGeneration < m_frame.outputGeneration) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::InvalidGeneration,
                .reasonCode = QStringLiteral("regressed-output-generation"),
                .stateChanged = false};
    }
    if (frame.outputGeneration == m_frame.outputGeneration) {
        if (frame != m_frame) {
            return {.status = InventoryObservationStatus::Rejected,
                    .error = InventoryError::InvalidGeneration,
                    .reasonCode = QStringLiteral("changed-equal-output-generation"),
                    .stateChanged = false};
        }
        const DisplayTransaction::MachineState state = m_machine->view().state;
        if (state == DisplayTransaction::MachineState::Ready
            || state == DisplayTransaction::MachineState::Staged) {
            return {.status = InventoryObservationStatus::AcceptedUnchanged,
                    .error = InventoryError::None,
                    .reasonCode = {},
                    .stateChanged = false};
        }
        const DisplayTransaction::CommandResult observed =
            m_machine->observedSnapshot(m_machine->currentSnapshot());
        return {.status = observed.accepted
                    ? InventoryObservationStatus::AcceptedUnchanged
                    : InventoryObservationStatus::Rejected,
                .error = observed.accepted ? InventoryError::None
                                           : InventoryError::ProjectionFailure,
                .reasonCode = observed.accepted ? QString{} : errorReason(observed.error),
                .stateChanged = observed.stateChanged};
    }
    if (frame.outputs == m_frame.outputs) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::InvalidGeneration,
                .reasonCode = QStringLiteral("unchanged-new-output-generation"),
                .stateChanged = false};
    }

    const InventoryProjectionResult projection =
        projectInventory(frame, m_serviceEpoch);
    if (!projection.accepted()) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = projection.error,
                .reasonCode = projection.reasonCode,
                .stateChanged = false};
    }
    const bool setChanged = !sameOutputSet(m_machine->currentSnapshot(),
                                           projection.snapshot);
    const DisplayTransaction::CommandResult observed =
        routeObservation(projection.snapshot, setChanged);
    if (!observed.accepted) {
        return {.status = InventoryObservationStatus::Rejected,
                .error = InventoryError::ProjectionFailure,
                .reasonCode = errorReason(observed.error),
                .stateChanged = false};
    }
    m_frame = frame;
    return {.status = InventoryObservationStatus::AcceptedChanged,
            .error = InventoryError::None,
            .reasonCode = {},
            .stateChanged = observed.stateChanged};
}

DisplayTransaction::CommandResult DisplayServiceModel::routeObservation(
    const Display::Snapshot &nextSnapshot, const bool outputSetChanged)
{
    const DisplayTransaction::MachineState state = m_machine->view().state;
    if (state == DisplayTransaction::MachineState::Ready) {
        return m_machine->observedSnapshot(nextSnapshot);
    }
    if (outputSetChanged) {
        return m_machine->topologyChanged(nextSnapshot);
    }
    if (state == DisplayTransaction::MachineState::Staged) {
        return m_machine->externalIntentObserved(nextSnapshot);
    }
    return m_machine->observedSnapshot(nextSnapshot);
}

bool DisplayServiceModel::transportLost()
{
    const bool changed = m_machine != nullptr;
    m_machine.reset();
    m_frame = {};
    m_sourceOwner.clear();
    return changed;
}

ServiceOperationResult DisplayServiceModel::operation(
    const Display::OperationKind kind, const QString &transactionId,
    const std::function<DisplayTransaction::CommandResult()> &command)
{
    if (m_machine == nullptr) {
        return {};
    }
    const QString epoch = m_machine->currentSnapshot().serviceEpoch;
    const quint64 initiatingRevision = m_machine->currentSnapshot().revision;
    const DisplayTransaction::CommandResult result = command();
    const Display::ErrorCode error = protocolError(result.error);
    const bool noOp = result.accepted
        && result.error == DisplayTransaction::CommandError::NoOp;
    Display::OperationResult operationResult{
        .kind = kind,
        .status = noOp
            ? Display::OperationStatus::Succeeded
            : result.accepted
            ? (kind == Display::OperationKind::Confirm
                   ? Display::OperationStatus::Succeeded
                   : Display::OperationStatus::Accepted)
            : Display::OperationStatus::Rejected,
        .error = result.accepted ? Display::ErrorCode::None : error,
        .initiatingEpoch = epoch,
        .initiatingRevision = initiatingRevision,
        .observedRevision = m_machine->currentSnapshot().revision,
        .transactionId = Display::isBoundedText(transactionId,
                                                Display::kMaxTransactionIdUtf8Bytes)
            ? transactionId
            : QString{},
        .diagnostic = noOp ? QStringLiteral("no-op")
                           : result.accepted ? QString{} : errorReason(result.error),
        .wireValid = true};
    return {.available = true, .command = result, .operation = operationResult};
}

ServiceOperationResult DisplayServiceModel::stage(
    const QString &transactionId, const Display::Candidate &candidate)
{
    return operation(Display::OperationKind::Stage, transactionId,
                     [this, &transactionId, &candidate] {
                         return m_machine->stage(transactionId, candidate);
                     });
}

ServiceOperationResult DisplayServiceModel::preview(const QString &transactionId)
{
    return operation(Display::OperationKind::Preview, transactionId,
                     [this, &transactionId] {
                         return m_machine->preview(transactionId);
                     });
}

ServiceOperationResult DisplayServiceModel::confirm(const QString &transactionId)
{
    return operation(Display::OperationKind::Confirm, transactionId,
                     [this, &transactionId] {
                         return m_machine->confirm(transactionId);
                     });
}

ServiceOperationResult DisplayServiceModel::cancel(const QString &transactionId)
{
    return operation(Display::OperationKind::Cancel, transactionId,
                     [this, &transactionId] {
                         return m_machine->cancel(transactionId);
                     });
}

DisplayTransaction::CommandResult DisplayServiceModel::applyCompleted(
    const quint64 machineLineage, const quint64 token,
    const DisplayTransaction::ApplyOutcome outcome)
{
    if (m_machine == nullptr) {
        return unavailableCommand();
    }
    if (machineLineage != m_machineLineage) {
        return {.accepted = false,
                .stateChanged = false,
                .error = DisplayTransaction::CommandError::CallbackOutOfOrder,
                .state = m_machine->view().state,
                .transactionId = m_machine->view().transactionId};
    }
    return m_machine->applyCompleted(token, outcome);
}

DisplayTransaction::CommandResult DisplayServiceModel::topologySettled()
{
    return m_machine == nullptr
        ? unavailableCommand()
        : m_machine->topologySettled(m_machine->currentSnapshot());
}

DisplayTransaction::CommandResult DisplayServiceModel::safetyChanged(
    const DisplayTransaction::SafetyState safety)
{
    m_safety = safety;
    return m_machine == nullptr ? unavailableCommand()
                                : m_machine->safetyChanged(safety);
}

DisplayTransaction::CommandResult DisplayServiceModel::prepareForSuspend()
{
    return m_machine == nullptr ? unavailableCommand()
                                : m_machine->prepareForSuspend();
}

DisplayTransaction::CommandResult DisplayServiceModel::tick()
{
    return m_machine == nullptr ? unavailableCommand() : m_machine->tick();
}

} // namespace QindaQt::DisplayService
