// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_validation.h>

#include <qindaqt/services/display_protocol/display_limits.h>

#include <QtCore/QSet>

#include <cmath>

namespace QindaQt::Display
{
namespace
{

ValidationResult accepted()
{
    return {.accepted = true, .reasonCode = {}};
}

ValidationResult rejected(const char *reasonCode)
{
    return {.accepted = false, .reasonCode = QString::fromLatin1(reasonCode)};
}

bool safeText(const QString &value)
{
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

bool boundedRequiredText(const QString &value, const qsizetype maximum)
{
    return !value.isEmpty() && isBoundedText(value, maximum) && safeText(value);
}

bool validTransform(const Transform value)
{
    return static_cast<quint32>(value) <= static_cast<quint32>(Transform::FlipX270);
}

bool validTransactionState(const TransactionState value)
{
    return static_cast<quint32>(value) <= static_cast<quint32>(TransactionState::Stuck);
}

bool validTransactionReason(const TransactionReason value)
{
    return static_cast<quint32>(value)
        <= static_cast<quint32>(TransactionReason::TransportUncertain);
}

bool validOperationKind(const OperationKind value)
{
    return static_cast<quint32>(value) <= static_cast<quint32>(OperationKind::ImmediatePolicy);
}

bool validOperationStatus(const OperationStatus value)
{
    return static_cast<quint32>(value) <= static_cast<quint32>(OperationStatus::Busy);
}

bool validErrorCode(const ErrorCode value)
{
    return static_cast<quint32>(value) <= static_cast<quint32>(ErrorCode::MalformedPayload);
}

bool validCoordinate(const int value)
{
    return value >= -kCoordinateBound && value <= kCoordinateBound;
}

bool validScale(const double value)
{
    return std::isfinite(value) && value >= kMinimumScale && value <= kMaximumScale;
}

ValidationResult validateCandidateOutput(const CandidateOutput &output)
{
    if (!boundedRequiredText(output.stableId, kMaxStableIdUtf8Bytes)
        || !isBoundedText(output.modeId, kMaxModeIdUtf8Bytes)
        || !isBoundedText(output.replicationSourceStableId, kMaxStableIdUtf8Bytes)) {
        return rejected("invalid-candidate-text");
    }
    if (!validCoordinate(output.position.x()) || !validCoordinate(output.position.y())
        || !validScale(output.scale) || !validTransform(output.transform)
        || output.priority > static_cast<quint32>(kMaxOutputs)) {
        return rejected("invalid-candidate-value");
    }
    if (output.enabled && output.modeId.isEmpty()) {
        return rejected("missing-candidate-mode");
    }
    if (!output.enabled && (output.primary || output.priority != 0)) {
        return rejected("inconsistent-candidate-output");
    }
    return accepted();
}

} // namespace

bool isBoundedText(const QString &value, const qsizetype maximumUtf8Bytes)
{
    return !value.contains(QChar::Null) && value.toUtf8().size() <= maximumUtf8Bytes
        && safeText(value);
}

ValidationResult validateMode(const Mode &mode)
{
    if (!boundedRequiredText(mode.id, kMaxModeIdUtf8Bytes)) {
        return rejected("invalid-mode-id");
    }
    if (mode.pixelSize.width() <= 0 || mode.pixelSize.height() <= 0
        || mode.pixelSize.width() > kMaxPixelDimension
        || mode.pixelSize.height() > kMaxPixelDimension || mode.refreshMilliHertz == 0
        || mode.refreshMilliHertz > kMaxRefreshMilliHertz) {
        return rejected("invalid-mode-value");
    }
    return accepted();
}

ValidationResult validateOutput(const Output &output)
{
    if (!output.wireValid) {
        return rejected("malformed-output");
    }
    const bool validTexts = boundedRequiredText(output.stableId, kMaxStableIdUtf8Bytes)
        && boundedRequiredText(output.connectorName, kMaxConnectorNameUtf8Bytes)
        && isBoundedText(output.runtimeCompositorUuid, kMaxRuntimeUuidUtf8Bytes)
        && isBoundedText(output.label, kMaxLabelUtf8Bytes)
        && isBoundedText(output.manufacturer, kMaxManufacturerUtf8Bytes)
        && isBoundedText(output.model, kMaxModelUtf8Bytes)
        && isBoundedText(output.modeId, kMaxModeIdUtf8Bytes)
        && isBoundedText(output.replicationSourceStableId, kMaxStableIdUtf8Bytes);
    if (!validTexts || !safeText(output.label) || !safeText(output.manufacturer)
        || !safeText(output.model)) {
        return rejected("invalid-output-text");
    }
    if (output.physicalSizeMillimeters.width() < 0
        || output.physicalSizeMillimeters.height() < 0
        || output.physicalSizeMillimeters.width() > kMaxPhysicalDimensionMillimeters
        || output.physicalSizeMillimeters.height() > kMaxPhysicalDimensionMillimeters
        || !validCoordinate(output.position.x()) || !validCoordinate(output.position.y())
        || output.logicalSize.width() < 0 || output.logicalSize.height() < 0
        || output.logicalSize.width() > kCoordinateBound
        || output.logicalSize.height() > kCoordinateBound || !validScale(output.scale)
        || !validTransform(output.transform)
        || output.priority > static_cast<quint32>(kMaxOutputs)) {
        return rejected("invalid-output-value");
    }
    if (output.modes.size() > kMaxModesPerOutput) {
        return rejected("too-many-modes");
    }

    QSet<QString> modeIds;
    bool selectedModeExists = output.modeId.isEmpty();
    for (const Mode &mode : output.modes) {
        if (const auto validation = validateMode(mode); !validation.accepted) {
            return validation;
        }
        if (modeIds.contains(mode.id)) {
            return rejected("duplicate-mode-id");
        }
        modeIds.insert(mode.id);
        selectedModeExists = selectedModeExists || mode.id == output.modeId;
    }
    if ((output.enabled && (output.modeId.isEmpty() || !selectedModeExists
                            || output.logicalSize.isEmpty()))
        || (!output.enabled
            && (output.primary || output.priority != 0 || !output.position.isNull()
                || !output.replicationSourceStableId.isEmpty()))
        || output.replicationSourceStableId == output.stableId) {
        return rejected("inconsistent-output");
    }
    return accepted();
}

ValidationResult validateCandidate(const Candidate &candidate)
{
    if (!candidate.wireValid) {
        return rejected("malformed-payload");
    }
    if (candidate.protocolVersion != kProtocolVersion) {
        return rejected("unsupported-version");
    }
    if (!boundedRequiredText(candidate.baseEpoch, kMaxServiceEpochUtf8Bytes)
        || candidate.baseRevision == 0) {
        return rejected("invalid-candidate-lineage");
    }
    if (candidate.outputs.isEmpty() || candidate.outputs.size() > kMaxCandidateOutputs) {
        return rejected("invalid-candidate-output-count");
    }
    QSet<QString> outputIds;
    for (const CandidateOutput &output : candidate.outputs) {
        if (const auto validation = validateCandidateOutput(output); !validation.accepted) {
            return validation;
        }
        if (outputIds.contains(output.stableId)) {
            return rejected("duplicate-candidate-output");
        }
        outputIds.insert(output.stableId);
    }
    return accepted();
}

ValidationResult validateTransactionSummary(const TransactionSummary &summary)
{
    if (!boundedRequiredText(summary.transactionId, kMaxTransactionIdUtf8Bytes)
        || !boundedRequiredText(summary.initiatingEpoch, kMaxServiceEpochUtf8Bytes)
        || summary.baseRevision == 0 || !validTransactionState(summary.state)
        || !validTransactionReason(summary.reason)
        || summary.revertAttempt > kMaximumRevertAttempts) {
        return rejected("invalid-transaction-summary");
    }
    return accepted();
}

ValidationResult validateSnapshot(const Snapshot &snapshot)
{
    if (!snapshot.wireValid) {
        return rejected("malformed-payload");
    }
    if (snapshot.protocolVersion != kProtocolVersion) {
        return rejected("unsupported-version");
    }
    if (!boundedRequiredText(snapshot.serviceEpoch, kMaxServiceEpochUtf8Bytes)
        || snapshot.revision == 0
        || snapshot.liveFingerprint.size() != kFingerprintBytes) {
        return rejected("invalid-snapshot-lineage");
    }
    if (snapshot.outputs.isEmpty() || snapshot.outputs.size() > kMaxOutputs
        || snapshot.transactions.size() > kMaxTransactions) {
        return rejected("invalid-snapshot-count");
    }

    QSet<QString> outputIds;
    qsizetype enabledCount = 0;
    qsizetype primaryCount = 0;
    for (const Output &output : snapshot.outputs) {
        if (const auto validation = validateOutput(output); !validation.accepted) {
            return validation;
        }
        if (outputIds.contains(output.stableId)) {
            return rejected("duplicate-output-id");
        }
        outputIds.insert(output.stableId);
        enabledCount += output.enabled ? 1 : 0;
        primaryCount += output.primary ? 1 : 0;
    }
    if (enabledCount == 0 || primaryCount != 1) {
        return rejected("invalid-primary-set");
    }
    for (const Output &output : snapshot.outputs) {
        if (!output.replicationSourceStableId.isEmpty()
            && !outputIds.contains(output.replicationSourceStableId)) {
            return rejected("unknown-replication-source");
        }
    }
    for (const TransactionSummary &summary : snapshot.transactions) {
        if (const auto validation = validateTransactionSummary(summary); !validation.accepted) {
            return validation;
        }
        if (summary.initiatingEpoch != snapshot.serviceEpoch
            || summary.observedRevision > snapshot.revision) {
            return rejected("invalid-transaction-lineage");
        }
    }
    return accepted();
}

ValidationResult validateOperationResult(const OperationResult &result)
{
    if (!result.wireValid || !validOperationKind(result.kind)
        || !validOperationStatus(result.status) || !validErrorCode(result.error)
        || !boundedRequiredText(result.initiatingEpoch, kMaxServiceEpochUtf8Bytes)
        || result.initiatingRevision == 0
        || !isBoundedText(result.transactionId, kMaxTransactionIdUtf8Bytes)
        || !isBoundedText(result.diagnostic, kMaxDiagnosticUtf8Bytes)
        || !safeText(result.diagnostic)) {
        return rejected("invalid-operation-result");
    }
    if (result.status == OperationStatus::Succeeded
        && (result.error != ErrorCode::None
            || result.observedRevision < result.initiatingRevision)) {
        return rejected("invalid-success-result");
    }
    if ((result.status == OperationStatus::Rejected
         || result.status == OperationStatus::Uncertain)
        && result.error == ErrorCode::None) {
        return rejected("missing-operation-error");
    }
    return accepted();
}

ConfirmationRequirement confirmationRequirement(const ChangeClass changeClass)
{
    switch (changeClass) {
    case ChangeClass::Topology:
        return ConfirmationRequirement::Required;
    case ChangeClass::Brightness:
    case ChangeClass::Dimming:
    case ChangeClass::SdrBrightness:
    case ChangeClass::IccProfile:
    case ChangeClass::VrrPolicy:
    case ChangeClass::RgbRange:
    case ChangeClass::Overscan:
    case ChangeClass::DdcCiPermission:
    case ChangeClass::MaximumBitsPerColor:
    case ChangeClass::ExtendedDynamicRange:
    case ChangeClass::Sharpness:
    case ChangeClass::AutoRotatePolicy:
    case ChangeClass::CustomModeDefinition:
        return ConfirmationRequirement::BypassedForClosedPolicy;
    }
    // AGENT-GUARD: Unknown future enum values fail safe into confirmation;
    // otherwise wire/version drift could silently turn a class-A change into
    // an immediate mutation.
    return ConfirmationRequirement::Required;
}

} // namespace QindaQt::Display
