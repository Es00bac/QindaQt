// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <utility>

namespace QindaQt::Power {
namespace {

bool hasCapability(const Snapshot &snapshot, const Capability capability)
{
    return snapshot.capabilities.testFlag(capability);
}

const KeyboardBacklight *findKeyboardDevice(const Snapshot &snapshot,
                                            const Handle &handle)
{
    for (const KeyboardBacklight &device : snapshot.keyboardBacklights) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}

const ProfileHold *findHold(const Snapshot &snapshot, const Handle &handle)
{
    for (const ProfileHold &hold : snapshot.profiles.holds) {
        if (hold.handle == handle) {
            return &hold;
        }
    }
    return nullptr;
}

bool supportedProfile(const Snapshot &snapshot, const QString &profileId)
{
    for (const Profile &profile : snapshot.profiles.supported) {
        if (profile.id == profileId) {
            return true;
        }
    }
    return false;
}

bool validOutcomeReasonCode(const QString &reasonCode)
{
    if (reasonCode.isEmpty() || !isBoundedText(reasonCode, kMaxReasonCodeUtf8Bytes)) {
        return false;
    }
    for (const QChar character : reasonCode) {
        const bool lowerAscii = character >= QLatin1Char('a')
            && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!lowerAscii && !digit && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

} // namespace

OperationResult PowerServiceCoordinator::immediateResult(
    const PowerServiceRequest &request, const OperationStatus status,
    const QString &reasonCode) const
{
    return OperationResult{
        .kind = request.kind,
        .status = status,
        .initiatingEpoch = m_snapshot.epoch,
        .initiatingRevision = m_snapshot.revision,
        .observedEpoch = m_snapshot.epoch,
        .observedRevision = m_snapshot.revision,
        .reasonCode = reasonCode,
        .diagnostic = {},
        .wireValid = true};
}

QString PowerServiceCoordinator::validateRequest(
    const PowerServiceRequest &request) const
{
    if (!m_running || (m_snapshot.availability != Availability::Ready
                       && m_snapshot.availability != Availability::Degraded)) {
        return QStringLiteral("unavailable");
    }
    switch (request.kind) {
    case OperationKind::SetProfile: {
        if (!hasCapability(m_snapshot, Capability::Profiles)) {
            return QStringLiteral("unsupported");
        }
        if (!isBoundedText(request.profileId, kMaxProfileIdUtf8Bytes)
            || !supportedProfile(m_snapshot, request.profileId)) {
            return QStringLiteral("unknown-profile");
        }
        return {};
    }
    case OperationKind::AcquireProfileHold: {
        if (!hasCapability(m_snapshot, Capability::ProfileHolds)) {
            return QStringLiteral("unsupported");
        }
        if (!isBoundedText(request.profileId, kMaxProfileIdUtf8Bytes)
            || !supportedProfile(m_snapshot, request.profileId)) {
            return QStringLiteral("unknown-profile");
        }
        if (!isBoundedText(request.applicationName, kMaxNameUtf8Bytes)
            || !isBoundedText(request.reason, kMaxDiagnosticUtf8Bytes)) {
            return QStringLiteral("malformed-request");
        }
        if (m_snapshot.profiles.holds.size() >= kMaxProfileHolds) {
            return QStringLiteral("too-many-holds");
        }
        return {};
    }
    case OperationKind::ReleaseProfileHold: {
        if (!hasCapability(m_snapshot, Capability::ProfileHolds)) {
            return QStringLiteral("unsupported");
        }
        if (!request.handle.isValid() || request.handle.epoch != m_snapshot.epoch
            || findHold(m_snapshot, request.handle) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return {};
    }
    case OperationKind::SetKeyboardBrightness: {
        if (!hasCapability(m_snapshot, Capability::KeyboardBacklight)) {
            return QStringLiteral("unsupported");
        }
        if (!request.handle.isValid() || request.handle.epoch != m_snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const KeyboardBacklight *device = findKeyboardDevice(m_snapshot, request.handle);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!device->canSet || device->maximum == 0
            || request.value > device->maximum) {
            return QStringLiteral("unsupported");
        }
        return {};
    }
    }
    return QStringLiteral("malformed-request");
}

OperationSubmission PowerServiceCoordinator::submit(
    const PowerServiceRequest &request)
{
    const QString rejection = validateRequest(request);
    if (!rejection.isEmpty()) {
        const OperationStatus status =
            rejection == QStringLiteral("unsupported") ? OperationStatus::Unsupported
                                                       : OperationStatus::Rejected;
        return OperationSubmission{.pending = false,
                                   .operationId = 0,
                                   .immediateResult =
                                       immediateResult(request, status, rejection)};
    }
    if (m_pending.size() >= kMaxServicePendingOperations || m_nextOperationId == 0
        || m_nextOperationId == std::numeric_limits<quint64>::max()) {
        return OperationSubmission{
            .pending = false,
            .operationId = 0,
            .immediateResult = immediateResult(request, OperationStatus::Busy,
                                               QStringLiteral("too-many-operations"))};
    }

    const quint64 operationId = m_nextOperationId++;
    m_pending.insert(operationId,
                     PendingOperation{.kind = request.kind,
                                      .epoch = m_snapshot.epoch,
                                      .revision = m_snapshot.revision});
    // AGENT-GUARD: Text crossing the collaborator seam is sanitized here, not
    // trusted from the bus; the coordinator owns the only sanitized copy.
    const QString profileId = sanitizeText(request.profileId, kMaxProfileIdUtf8Bytes);
    const QString applicationName =
        sanitizeText(request.applicationName, kMaxNameUtf8Bytes);
    const QString reason = sanitizeText(request.reason, kMaxDiagnosticUtf8Bytes);
    switch (request.kind) {
    case OperationKind::SetProfile:
        m_profiles->submitSetProfile(operationId, profileId);
        break;
    case OperationKind::AcquireProfileHold:
        m_profiles->submitAcquireProfileHold(operationId, profileId, applicationName,
                                             reason);
        break;
    case OperationKind::ReleaseProfileHold:
        m_profiles->submitReleaseProfileHold(operationId, request.handle);
        break;
    case OperationKind::SetKeyboardBrightness:
        m_battery->submitSetKeyboardBrightness(operationId, request.handle,
                                               request.value);
        break;
    }
    return OperationSubmission{.pending = true, .operationId = operationId,
                               .immediateResult = {}};
}

void PowerServiceCoordinator::deliverOutcome(const quint64 operationId,
                                             const PendingOperation &pending,
                                             const CollaboratorOutcome &outcome)
{
    OperationStatus status = OperationStatus::Failed;
    bool knownStatus = true;
    switch (outcome.status) {
    case CollaboratorStatus::Succeeded:
        status = OperationStatus::Succeeded;
        break;
    case CollaboratorStatus::Unsupported:
        status = OperationStatus::Unsupported;
        break;
    case CollaboratorStatus::Failed:
        status = OperationStatus::Failed;
        break;
    case CollaboratorStatus::Uncertain:
        status = OperationStatus::Uncertain;
        break;
    default:
        knownStatus = false;
        break;
    }
    const bool authorityReplaced = pending.epoch != m_snapshot.epoch;
    if (authorityReplaced) {
        status = OperationStatus::Uncertain;
    }

    OperationResult result{
        .kind = pending.kind,
        .status = status,
        .initiatingEpoch = pending.epoch,
        .initiatingRevision = pending.revision,
        .observedEpoch = m_snapshot.epoch,
        .observedRevision = m_snapshot.revision,
        .reasonCode = sanitizeReasonCode(outcome.reasonCode),
        .diagnostic = sanitizeText(outcome.diagnostic, kMaxDiagnosticUtf8Bytes),
        .wireValid = true};
    if (authorityReplaced) {
        result.reasonCode = QStringLiteral("authority-replaced");
        result.diagnostic.clear();
    }
    // AGENT-GUARD: Collaborators are an untrusted platform boundary. One
    // invalid field replaces the whole classification with a stable
    // protocol-valid failure; raw upstream text never reaches D-Bus.
    if (!knownStatus || !validOutcomeReasonCode(outcome.reasonCode)
        || !validateOperationResult(result).accepted) {
        result.status = OperationStatus::Failed;
        result.reasonCode = QStringLiteral("upstream-malformed");
        result.diagnostic.clear();
    }
    Q_ASSERT(validateOperationResult(result).accepted);
    Q_EMIT operationCompleted(operationId, result);
}

void PowerServiceCoordinator::acceptBatteryOutcome(
    const quint64 generation, const quint64 operationId,
    const CollaboratorOutcome &outcome)
{
    if (!generationCurrent(m_batteryDomain.state, generation)) {
        return;
    }
    const auto it = m_pending.find(operationId);
    if (it == m_pending.end()) {
        return;
    }
    const PendingOperation pending = it.value();
    // AGENT-GUARD: Exactly-once completion — the pending entry is removed
    // before emission so a duplicated or late upstream reply is dropped.
    m_pending.erase(it);
    deliverOutcome(operationId, pending, outcome);
}

void PowerServiceCoordinator::acceptProfileOutcome(
    const quint64 generation, const quint64 operationId,
    const CollaboratorOutcome &outcome)
{
    if (!generationCurrent(m_profileDomain.state, generation)) {
        return;
    }
    const auto it = m_pending.find(operationId);
    if (it == m_pending.end()) {
        return;
    }
    const PendingOperation pending = it.value();
    m_pending.erase(it);
    deliverOutcome(operationId, pending, outcome);
}

} // namespace QindaQt::Power
