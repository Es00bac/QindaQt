// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_client/power_client.h>

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

namespace QindaQt::Power {
namespace {

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

// Local preflight mirrors the service's validation so obvious rejections and
// stale handles never cross the bus. The service remains authoritative; a
// divergent transport reply is completed Uncertain, never trusted.
QString preflightOperation(const Snapshot &snapshot,
                           const PowerClientRequest &request)
{
    if (snapshot.availability != Availability::Ready
        && snapshot.availability != Availability::Degraded) {
        return QStringLiteral("unavailable");
    }
    switch (request.kind) {
    case OperationKind::SetProfile:
        if (!snapshot.capabilities.testFlag(Capability::Profiles)) {
            return QStringLiteral("unsupported");
        }
        if (!isBoundedText(request.profileId, kMaxProfileIdUtf8Bytes)
            || !supportedProfile(snapshot, request.profileId)) {
            return QStringLiteral("unknown-profile");
        }
        return {};
    case OperationKind::AcquireProfileHold:
        if (!snapshot.capabilities.testFlag(Capability::ProfileHolds)) {
            return QStringLiteral("unsupported");
        }
        if (!isBoundedText(request.profileId, kMaxProfileIdUtf8Bytes)
            || !supportedProfile(snapshot, request.profileId)) {
            return QStringLiteral("unknown-profile");
        }
        if (!isBoundedText(request.applicationName, kMaxNameUtf8Bytes)
            || !isBoundedText(request.reason, kMaxDiagnosticUtf8Bytes)) {
            return QStringLiteral("malformed-request");
        }
        if (snapshot.profiles.holds.size() >= kMaxProfileHolds) {
            return QStringLiteral("too-many-holds");
        }
        return {};
    case OperationKind::ReleaseProfileHold:
        if (!snapshot.capabilities.testFlag(Capability::ProfileHolds)) {
            return QStringLiteral("unsupported");
        }
        if (!request.handle.isValid() || request.handle.epoch != snapshot.epoch
            || findHold(snapshot, request.handle) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return {};
    case OperationKind::SetKeyboardBrightness: {
        if (!request.handle.isValid() || request.handle.epoch != snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const KeyboardBacklight *device =
            findKeyboardDevice(snapshot, request.handle);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::KeyboardBacklight)
            || !device->canSet || device->maximum == 0 || request.value > device->maximum) {
            return QStringLiteral("unsupported");
        }
        return {};
    }
    }
    return QStringLiteral("malformed-request");
}

} // namespace

OperationResult PowerClient::localResult(const PowerClientRequest &request,
                                         const OperationStatus status,
                                         const QString &reasonCode) const
{
    const quint64 epoch = m_snapshot.has_value() ? m_snapshot->epoch : 1;
    const quint64 revision = m_snapshot.has_value() ? m_snapshot->revision : 1;
    return OperationResult{.kind = request.kind,
                           .status = status,
                           .initiatingEpoch = epoch,
                           .initiatingRevision = revision,
                           .observedEpoch = epoch,
                           .observedRevision = revision,
                           .reasonCode = reasonCode,
                           .diagnostic = {},
                           .wireValid = true};
}

quint64 PowerClient::beginOperation(const PowerClientRequest &request)
{
    if (m_nextRequestId == 0 || m_nextRequestId == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    const quint64 requestId = m_nextRequestId++;
    if (m_operation.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Busy,
                        QStringLiteral("operation-busy")));
        return requestId;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Rejected,
                        QStringLiteral("unavailable")));
        return requestId;
    }
    const QString rejection = preflightOperation(*m_snapshot, request);
    if (!rejection.isEmpty()) {
        queueOperationCompletion(
            requestId,
            localResult(request,
                        rejection == QStringLiteral("unsupported")
                            ? OperationStatus::Unsupported
                            : OperationStatus::Rejected,
                        rejection));
        return requestId;
    }

    m_operation = PendingOperation{.requestId = requestId,
                                   .request = request,
                                   .epoch = m_snapshot->epoch,
                                   .revision = m_snapshot->revision};
    m_operationTimer.start(m_requestTimeoutMs);
    m_transport->submitOperation(m_owner, requestId, request);
    return requestId;
}

quint64 PowerClient::setProfile(const QString &profileId)
{
    return beginOperation({.kind = OperationKind::SetProfile,
                           .profileId = profileId,
                           .applicationName = {},
                           .reason = {},
                           .handle = {},
                           .value = 0});
}

quint64 PowerClient::acquireProfileHold(const QString &profileId,
                                        const QString &applicationName,
                                        const QString &reason)
{
    return beginOperation({.kind = OperationKind::AcquireProfileHold,
                           .profileId = profileId,
                           .applicationName = applicationName,
                           .reason = reason,
                           .handle = {},
                           .value = 0});
}

quint64 PowerClient::releaseProfileHold(const Handle &hold)
{
    return beginOperation({.kind = OperationKind::ReleaseProfileHold,
                           .profileId = {},
                           .applicationName = {},
                           .reason = {},
                           .handle = hold,
                           .value = 0});
}

quint64 PowerClient::setKeyboardBrightness(const Handle &device, const quint32 value)
{
    return beginOperation({.kind = OperationKind::SetKeyboardBrightness,
                           .profileId = {},
                           .applicationName = {},
                           .reason = {},
                           .handle = device,
                           .value = value});
}

void PowerClient::completeUncertain(const QString &reasonCode)
{
    if (!m_operation.has_value()) {
        return;
    }
    const PendingOperation pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();
    const quint64 observedEpoch =
        m_snapshot.has_value() ? m_snapshot->epoch : pending.epoch;
    const quint64 observedRevision =
        m_snapshot.has_value() ? m_snapshot->revision : pending.revision;
    queueOperationCompletion(
        pending.requestId,
        OperationResult{.kind = pending.request.kind,
                        .status = OperationStatus::Uncertain,
                        .initiatingEpoch = pending.epoch,
                        .initiatingRevision = pending.revision,
                        .observedEpoch = observedEpoch,
                        .observedRevision = observedRevision,
                        .reasonCode = reasonCode,
                        .diagnostic = {},
                        .wireValid = true});
}

void PowerClient::acceptOperationReply(const QString &owner, const quint64 requestId,
                                       const bool transportSuccess,
                                       const OperationResult &result,
                                       const QString &reasonCode)
{
    if (!m_operation.has_value() || owner != m_owner
        || requestId != m_operation->requestId) {
        return;
    }
    const PendingOperation pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();

    const ValidationResult validation = validateOperationResult(result);
    const bool exactInitiator = result.kind == pending.request.kind
        && result.initiatingEpoch == pending.epoch
        && result.initiatingRevision == pending.revision;
    const bool currentSuccessLineage =
        result.status != OperationStatus::Succeeded
        || (m_snapshot.has_value() && result.observedEpoch == m_snapshot->epoch);
    if (!transportSuccess || !validation.accepted || !exactInitiator
        || !currentSuccessLineage) {
        // AGENT-GUARD: A malformed, late, or divergent reply is Uncertain, not
        // Failed: the mutation was dispatched, so its outcome cannot be
        // proven. The client never replays it and immediately resnapshots.
        queueOperationCompletion(
            requestId,
            OperationResult{.kind = pending.request.kind,
                            .status = OperationStatus::Uncertain,
                            .initiatingEpoch = pending.epoch,
                            .initiatingRevision = pending.revision,
                            .observedEpoch =
                                m_snapshot.has_value() ? m_snapshot->epoch : pending.epoch,
                            .observedRevision = m_snapshot.has_value()
                                ? m_snapshot->revision
                                : pending.revision,
                            .reasonCode = transportSuccess
                                ? QStringLiteral("malformed-result")
                                : reasonCode,
                            .diagnostic = {},
                            .wireValid = true});
        requestSnapshot();
        return;
    }

    queueOperationCompletion(requestId, result);
    requestSnapshot();
}

} // namespace QindaQt::Power
