// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::DisplayTransaction
{

inline constexpr quint32 kJournalSchemaVersion = 1;
inline constexpr quint32 kMaximumRevertAttempts = Display::kMaximumRevertAttempts;
inline constexpr qsizetype kMaximumJournalBytes = 1'048'576;

enum class MachineState {
    Discovering,
    Ready,
    Staged,
    Applying,
    Observing,
    AwaitingConfirmation,
    ResolvingUncertain,
    SettlingTopology,
    RevertingApply,
    RevertingObserve,
    RevertBackoff,
    Stuck,
};

enum class SafetyState {
    Unknown,
    Safe,
    Locked,
};

enum class ApplyOutcome {
    Applied,
    Rejected,
    TransportUncertain,
};

enum class ApplyScope {
    ForwardCandidate,
    FullPreimage,
    SurvivingOutputProperties,
};

enum class JournalPhase : quint32 {
    Applying = 0,
    AwaitingConfirmation = 1,
    Reverting = 2,
    Stuck = 3,
};

enum class CommandError {
    None,
    InvalidSnapshot,
    InvalidJournal,
    InvalidTransition,
    StaleRevision,
    TransactionActive,
    UnknownTransaction,
    InvalidTransactionId,
    InvalidCandidate,
    NoOp,
    Locked,
    Suspend,
    JournalFailure,
    CallbackOutOfOrder,
    ApplyRejected,
    ApplyUncertain,
    ObservationMismatch,
    ObservationTimeout,
    ExternalChange,
    TopologyChanged,
    RevertFailed,
};

struct Timing {
    quint64 applyTimeoutMilliseconds = 5'000;
    quint64 observationTimeoutMilliseconds = 2'000;
    quint64 confirmationTimeoutMilliseconds = 15'000;
    quint64 firstRevertBackoffMilliseconds = 250;
    quint64 secondRevertBackoffMilliseconds = 500;

    friend bool operator==(const Timing &, const Timing &) = default;
};

struct SurvivingOutputProperties {
    QString stableId;
    QString modeId;
    double scale = 1.0;
    Display::Transform transform = Display::Transform::Normal;

    friend bool operator==(const SurvivingOutputProperties &,
                           const SurvivingOutputProperties &) = default;
};

struct ApplyRequest {
    quint64 token = 0;
    ApplyScope scope = ApplyScope::ForwardCandidate;
    Display::Candidate candidate;
    QList<SurvivingOutputProperties> survivingProperties;

    friend bool operator==(const ApplyRequest &, const ApplyRequest &) = default;
};

struct Journal {
    quint32 schemaVersion = kJournalSchemaVersion;
    QString transactionId;
    JournalPhase phase = JournalPhase::Applying;
    Display::TransactionReason reason = Display::TransactionReason::None;
    Display::Candidate preimage;
    Display::Candidate target;
    quint32 revertAttempt = 0;

    friend bool operator==(const Journal &, const Journal &) = default;
};

struct MachineView {
    MachineState state = MachineState::Discovering;
    SafetyState safety = SafetyState::Unknown;
    QString transactionId;
    Display::TransactionReason reason = Display::TransactionReason::None;
    Display::TransactionReason lastTerminalReason = Display::TransactionReason::None;
    quint64 currentRevision = 0;
    quint64 deadlineMonotonicMilliseconds = 0;
    quint32 revertAttempt = 0;
    bool journalActive = false;

    friend bool operator==(const MachineView &, const MachineView &) = default;
};

struct CommandResult {
    bool accepted = false;
    bool stateChanged = false;
    CommandError error = CommandError::None;
    MachineState state = MachineState::Discovering;
    QString transactionId;

    friend bool operator==(const CommandResult &, const CommandResult &) = default;
};

} // namespace QindaQt::DisplayTransaction
