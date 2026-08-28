// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QString>

namespace QindaQt::Display
{

enum class Transform : quint32 {
    Normal = 0,
    Rotate90 = 1,
    Rotate180 = 2,
    Rotate270 = 3,
    FlipX = 4,
    FlipX90 = 5,
    FlipX180 = 6,
    FlipX270 = 7,
};

enum class TransactionState : quint32 {
    Staged = 0,
    PersistingJournal = 1,
    Applying = 2,
    Observing = 3,
    AwaitingConfirmation = 4,
    SettlingTopology = 5,
    Reverting = 6,
    ResolvingUncertain = 7,
    Stuck = 8,
};

enum class TransactionReason : quint32 {
    None = 0,
    Cancelled = 1,
    ConfirmationDeadline = 2,
    Locked = 3,
    Suspend = 4,
    TopologyChanged = 5,
    ExternalChange = 6,
    Recovery = 7,
    ApplyRejected = 8,
    ApplyTimeout = 9,
    ObservationMismatch = 10,
    ObservationTimeout = 11,
    RevertFailed = 12,
    JournalFailure = 13,
    TransportUncertain = 14,
};

enum class OperationKind : quint32 {
    Stage = 0,
    Preview = 1,
    Confirm = 2,
    Cancel = 3,
    Recover = 4,
    RetryRevert = 5,
    ImmediatePolicy = 6,
};

enum class OperationStatus : quint32 {
    Accepted = 0,
    Succeeded = 1,
    Rejected = 2,
    Uncertain = 3,
    Busy = 4,
};

enum class ErrorCode : quint32 {
    None = 0,
    InvalidCandidate = 1,
    StaleRevision = 2,
    TransactionActive = 3,
    UnknownTransaction = 4,
    InvalidTransition = 5,
    CompositorRejected = 6,
    CompositorUnavailable = 7,
    Timeout = 8,
    Locked = 9,
    TopologyChanged = 10,
    ExternalChange = 11,
    JournalFailure = 12,
    RevertFailed = 13,
    RegistryUnavailable = 14,
    UnsupportedVersion = 15,
    MalformedPayload = 16,
};

// This is deliberately closed. Adding a new immediate policy kind requires a
// protocol-version decision and tests proving why confirmation is unnecessary.
enum class ChangeClass : quint32 {
    Topology = 0,
    Brightness = 1,
    Dimming = 2,
    SdrBrightness = 3,
    IccProfile = 4,
    VrrPolicy = 5,
    RgbRange = 6,
    Overscan = 7,
    DdcCiPermission = 8,
    MaximumBitsPerColor = 9,
    ExtendedDynamicRange = 10,
    Sharpness = 11,
    AutoRotatePolicy = 12,
    CustomModeDefinition = 13,
};

enum class ConfirmationRequirement : quint32 {
    Required = 0,
    BypassedForClosedPolicy = 1,
};

struct Mode {
    QString id;
    QSize pixelSize;
    quint32 refreshMilliHertz = 0;
    bool preferred = false;

    friend bool operator==(const Mode &, const Mode &) = default;
};

struct Output {
    QString stableId;
    QString connectorName;
    QString runtimeCompositorUuid;
    QString label;
    QString manufacturer;
    QString model;
    QSize physicalSizeMillimeters;
    bool hasSerial = false;
    bool internal = false;
    bool ambiguousIdentity = false;
    bool enabled = false;
    bool primary = false;
    QString modeId;
    QPoint position;
    QSize logicalSize;
    double scale = 1.0;
    Transform transform = Transform::Normal;
    quint32 priority = 0;
    QString replicationSourceStableId;
    QList<Mode> modes;
    bool wireValid = true;

    friend bool operator==(const Output &, const Output &) = default;
};

struct CandidateOutput {
    QString stableId;
    bool enabled = false;
    bool primary = false;
    QString modeId;
    QPoint position;
    double scale = 1.0;
    Transform transform = Transform::Normal;
    quint32 priority = 0;
    QString replicationSourceStableId;

    friend bool operator==(const CandidateOutput &, const CandidateOutput &) = default;
};

struct Candidate {
    quint32 protocolVersion = 1;
    QString baseEpoch;
    quint64 baseRevision = 0;
    QList<CandidateOutput> outputs;
    bool wireValid = true;

    friend bool operator==(const Candidate &, const Candidate &) = default;
};

struct TransactionSummary {
    QString transactionId;
    TransactionState state = TransactionState::Staged;
    TransactionReason reason = TransactionReason::None;
    QString initiatingEpoch;
    quint64 baseRevision = 0;
    quint64 observedRevision = 0;
    quint64 deadlineMonotonicMilliseconds = 0;
    quint32 revertAttempt = 0;

    friend bool operator==(const TransactionSummary &, const TransactionSummary &) = default;
};

struct Snapshot {
    quint32 protocolVersion = 1;
    QString serviceEpoch;
    quint64 revision = 0;
    // AGENT-CONTRACT: display adapters publish the SHA-256 canonical v1
    // fingerprint of this snapshot's canonical candidate projection. The
    // projection erases derived replica position/scale and disabled-output
    // fields; it does not assert that compositor-authored live topology is a
    // legal new candidate. The transaction machine rejects snapshots that
    // violate this cross-module lineage fence and applies strict topology
    // policy only when staging a mutation.
    QByteArray liveFingerprint;
    QList<Output> outputs;
    QList<TransactionSummary> transactions;
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::Stage;
    OperationStatus status = OperationStatus::Rejected;
    ErrorCode error = ErrorCode::None;
    QString initiatingEpoch;
    quint64 initiatingRevision = 0;
    quint64 observedRevision = 0;
    QString transactionId;
    QString diagnostic;
    bool wireValid = true;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

} // namespace QindaQt::Display

Q_DECLARE_METATYPE(QindaQt::Display::Transform)
Q_DECLARE_METATYPE(QindaQt::Display::TransactionState)
Q_DECLARE_METATYPE(QindaQt::Display::TransactionReason)
Q_DECLARE_METATYPE(QindaQt::Display::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Display::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Display::ErrorCode)
Q_DECLARE_METATYPE(QindaQt::Display::ChangeClass)
Q_DECLARE_METATYPE(QindaQt::Display::Mode)
Q_DECLARE_METATYPE(QindaQt::Display::Output)
Q_DECLARE_METATYPE(QindaQt::Display::CandidateOutput)
Q_DECLARE_METATYPE(QindaQt::Display::Candidate)
Q_DECLARE_METATYPE(QindaQt::Display::TransactionSummary)
Q_DECLARE_METATYPE(QindaQt::Display::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Display::OperationResult)
