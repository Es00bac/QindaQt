// SPDX-License-Identifier: LGPL-3.0-or-later

#include "display_brightness_authority_p.h"

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::DisplayService::Private
{
namespace
{

using Display::ErrorCode;
using Display::OperationStatus;
using DisplayTransaction::MachineState;
using DisplayTransaction::SafetyState;

const Display::Output *findOutput(const Display::Snapshot &topology, const QString &stableId)
{
    const auto found = std::ranges::find(topology.outputs, stableId, &Display::Output::stableId);
    return found == topology.outputs.cend() ? nullptr : &*found;
}

const Display::OutputBrightness *findRow(const Display::BrightnessSnapshot &published,
                                         const QString &stableId)
{
    const auto found =
        std::ranges::find(published.outputs, stableId, &Display::OutputBrightness::stableId);
    return found == published.outputs.cend() ? nullptr : &*found;
}

} // namespace

BrightnessAuthority::BrightnessAuthority(DisplayTransaction::MonotonicClock &clock,
                                         TransactionPort &port,
                                         const quint64 deadlineMilliseconds)
    : m_clock(clock)
    , m_port(port)
    , m_deadlineMilliseconds(std::max<quint64>(deadlineMilliseconds, 1))
{
}

const Display::BrightnessSnapshot *BrightnessAuthority::snapshot() const noexcept
{
    return m_published ? &*m_published : nullptr;
}

bool BrightnessAuthority::pending() const noexcept
{
    return m_pending.has_value();
}

quint64 BrightnessAuthority::deadlineMonotonicMilliseconds() const noexcept
{
    return m_pending ? m_pending->deadline : 0;
}

void BrightnessAuthority::refresh(const Display::Snapshot *topology)
{
    republish(topology);
}

void BrightnessAuthority::devicesObserved(const DeviceBrightnessFrame &frame,
                                          const Display::Snapshot *topology)
{
    // AGENT-GUARD: A request belongs to one exact compositor connection and
    // device set. Any replacement, even one that later publishes the same
    // value, may have dropped or re-targeted the submitted configuration.
    if (m_pending && frame.ownerGeneration != m_pending->ownerGeneration) {
        finish(OperationStatus::Uncertain, ErrorCode::CompositorUnavailable,
               QStringLiteral("compositor-owner-changed"));
    }
    m_devices = frame;
    republish(topology);
}

const DeviceBrightness *BrightnessAuthority::joinedDevice(const Display::Output &output) const
{
    // AGENT-GUARD: Topology and device facts travel on different compositor
    // channels. A row joins only one exact connector whose runtime UUID also
    // occurs exactly once; anything else publishes no brightness authority.
    if (m_devices.ownerGeneration == 0 || output.connectorName.isEmpty()
        || output.runtimeCompositorUuid.isEmpty()) {
        return nullptr;
    }
    const DeviceBrightness *match = nullptr;
    qsizetype uuidMatches = 0;
    for (const DeviceBrightness &device : m_devices.devices) {
        uuidMatches += device.runtimeUuid == output.runtimeCompositorUuid ? 1 : 0;
        if (device.connectorName == output.connectorName) {
            if (match != nullptr) {
                return nullptr;
            }
            match = &device;
        }
    }
    return match != nullptr && uuidMatches == 1
            && match->runtimeUuid == output.runtimeCompositorUuid
        ? match
        : nullptr;
}

void BrightnessAuthority::republish(const Display::Snapshot *topology)
{
    if (topology == nullptr || topology->serviceEpoch == m_exhaustedEpoch) {
        finish(OperationStatus::Uncertain, ErrorCode::CompositorUnavailable,
               QStringLiteral("service-lineage-lost"));
        if (m_published) {
            m_published.reset();
            m_publicationChanged = true;
        }
        m_joinRevision = 0;
        return;
    }

    QList<Display::OutputBrightness> rows;
    rows.reserve(topology->outputs.size());
    for (const Display::Output &output : topology->outputs) {
        Display::OutputBrightness row{.stableId = output.stableId};
        if (const DeviceBrightness *device = joinedDevice(output)) {
            row.capable = device->capable;
            row.observed = device->observed && device->value <= Display::kMaxBrightness;
            row.value = row.observed ? device->value : 0;
        }
        rows.push_back(std::move(row));
    }

    if (!m_published || m_published->serviceEpoch != topology->serviceEpoch) {
        finish(OperationStatus::Uncertain, ErrorCode::CompositorUnavailable,
               QStringLiteral("service-lineage-lost"));
        m_published = Display::BrightnessSnapshot{.protocolVersion = Display::kProtocolVersion,
                                                  .serviceEpoch = topology->serviceEpoch,
                                                  .topologyRevision = topology->revision,
                                                  .revision = 1,
                                                  .outputs = std::move(rows),
                                                  .wireValid = true};
        m_joinRevision = 1;
        m_publicationChanged = true;
        return;
    }

    const bool topologyChanged = m_published->topologyRevision != topology->revision;
    if (!topologyChanged && rows == m_published->outputs) {
        return;
    }
    if (topologyChanged) {
        // AGENT-CONTRACT: Any accepted topology revision may have replaced the
        // connector behind the pinned stable ID. The request is uncertain and
        // is never replayed against the new topology.
        finish(OperationStatus::Uncertain, ErrorCode::TopologyChanged,
               QStringLiteral("topology-changed"));
    }
    if (m_published->revision == std::numeric_limits<quint64>::max()) {
        // Revisions never wrap inside an epoch; withdraw it instead.
        m_exhaustedEpoch = m_published->serviceEpoch;
        republish(nullptr);
        return;
    }
    m_published->revision += 1;
    m_published->topologyRevision = topology->revision;
    m_published->outputs = std::move(rows);
    if (topologyChanged) {
        m_joinRevision = m_published->revision;
    }
    m_publicationChanged = true;
    resolvePending();
}

BrightnessRequestResult BrightnessAuthority::finalResult(const OperationStatus status,
                                                         const ErrorCode error,
                                                         const QString &diagnostic) const
{
    const quint64 revision = m_published->revision;
    return {.available = true,
            .final = true,
            .requestId = 0,
            .operation = {.kind = Display::OperationKind::ImmediatePolicy,
                          .status = status,
                          .error = error,
                          .initiatingEpoch = m_published->serviceEpoch,
                          .initiatingRevision = revision,
                          .observedRevision = revision,
                          .transactionId = {},
                          .diagnostic = diagnostic,
                          .wireValid = true}};
}

BrightnessRequestResult BrightnessAuthority::request(const Display::BrightnessRequest &request,
                                                     const Display::Snapshot *topology,
                                                     const MachineState state,
                                                     const SafetyState safety)
{
    if (topology == nullptr || !m_published) {
        return {};
    }
    const auto rejected = [this](const ErrorCode error, const QString &diagnostic) {
        return finalResult(OperationStatus::Rejected, error, diagnostic);
    };
    if (const auto validation = Display::validateBrightnessRequest(request);
        !validation.accepted) {
        return rejected(ErrorCode::InvalidCandidate, validation.reasonCode);
    }
    if (request.baseEpoch != m_published->serviceEpoch || request.baseRevision < m_joinRevision
        || request.baseRevision > m_published->revision) {
        return rejected(ErrorCode::StaleRevision, QStringLiteral("stale-revision"));
    }
    if (m_pending) {
        return finalResult(OperationStatus::Busy, ErrorCode::TransactionActive,
                           QStringLiteral("brightness-request-pending"));
    }
    // AGENT-CONTRACT: Class-A coexistence. Brightness is admitted only while
    // D1 is Ready, and the model refuses Preview while this request is
    // pending, so no topology apply or revert can overlap a brightness write.
    if (state != MachineState::Ready) {
        return rejected(ErrorCode::TransactionActive, QStringLiteral("transaction-active"));
    }
    if (safety == SafetyState::Locked) {
        return rejected(ErrorCode::Locked, QStringLiteral("locked"));
    }
    if (safety != SafetyState::Safe) {
        return rejected(ErrorCode::CompositorUnavailable,
                        QStringLiteral("mutation-authority-unavailable"));
    }

    const Display::Output *output = findOutput(*topology, request.stableId);
    if (output == nullptr) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("unknown-output"));
    }
    if (output->ambiguousIdentity) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("ambiguous-output"));
    }
    if (output->internal) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("internal-output"));
    }
    if (!output->enabled) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("output-disabled"));
    }
    if (!output->replicationSourceStableId.isEmpty()) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("replica-output"));
    }
    const DeviceBrightness *device = joinedDevice(*output);
    const Display::OutputBrightness *row = findRow(*m_published, request.stableId);
    if (device == nullptr || row == nullptr || !row->capable) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("brightness-unsupported"));
    }
    if (!device->enabled) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("output-disabled"));
    }
    if (!row->observed) {
        return rejected(ErrorCode::InvalidCandidate, QStringLiteral("brightness-unobserved"));
    }
    if (row->value == request.value) {
        return finalResult(OperationStatus::Succeeded, ErrorCode::None, QStringLiteral("no-op"));
    }

    const quint64 requestId = m_nextRequestId;
    m_nextRequestId = requestId == std::numeric_limits<quint64>::max() ? 1 : requestId + 1;
    const quint64 now = m_clock.nowMilliseconds();
    const BrightnessApplyRequest submitted{.requestId = requestId,
                                           .ownerGeneration = m_devices.ownerGeneration,
                                           .connectorName = device->connectorName,
                                           .runtimeUuid = device->runtimeUuid,
                                           .value = request.value};
    // AGENT-GUARD: Publish the fence before the port call. A port that
    // violates its asynchronous contract must still match this exact request.
    m_pending = Pending{.requestId = requestId,
                        .stableId = request.stableId,
                        .epoch = m_published->serviceEpoch,
                        .initiatingRevision = m_published->revision,
                        .ownerGeneration = m_devices.ownerGeneration,
                        .value = request.value,
                        .deadline = now > std::numeric_limits<quint64>::max() - m_deadlineMilliseconds
                            ? std::numeric_limits<quint64>::max()
                            : now + m_deadlineMilliseconds,
                        .acknowledged = false};
    const BrightnessRequestResult accepted{
        .available = true,
        .final = false,
        .requestId = requestId,
        .operation = finalResult(OperationStatus::Accepted, ErrorCode::None, {}).operation};
    switch (m_port.requestBrightness(submitted)) {
    case BrightnessSubmitStatus::Accepted:
        return accepted;
    case BrightnessSubmitStatus::Busy:
        m_pending.reset();
        return finalResult(OperationStatus::Busy, ErrorCode::TransactionActive,
                           QStringLiteral("compositor-busy"));
    case BrightnessSubmitStatus::Unavailable:
        m_pending.reset();
        return rejected(ErrorCode::CompositorUnavailable, QStringLiteral("compositor-unavailable"));
    case BrightnessSubmitStatus::Unsupported:
        m_pending.reset();
        return rejected(ErrorCode::CompositorRejected, QStringLiteral("compositor-unsupported"));
    case BrightnessSubmitStatus::Malformed:
        m_pending.reset();
        return rejected(ErrorCode::MalformedPayload, QStringLiteral("compositor-malformed"));
    }
    m_pending.reset();
    return rejected(ErrorCode::MalformedPayload, QStringLiteral("compositor-malformed"));
}

void BrightnessAuthority::completed(const quint64 requestId, const BrightnessApplyOutcome outcome)
{
    if (!m_pending || m_pending->requestId != requestId) {
        return;
    }
    switch (outcome) {
    case BrightnessApplyOutcome::Applied:
        // The compositor acknowledgement alone is not proof: KWin 6.6.6
        // acknowledges set_brightness on outputs that ignore it.
        m_pending->acknowledged = true;
        resolvePending();
        return;
    case BrightnessApplyOutcome::Rejected:
        finish(OperationStatus::Rejected, ErrorCode::CompositorRejected,
               QStringLiteral("compositor-rejected"));
        return;
    case BrightnessApplyOutcome::TransportUncertain:
        finish(OperationStatus::Uncertain, ErrorCode::CompositorUnavailable,
               QStringLiteral("transport-uncertain"));
        return;
    }
}

void BrightnessAuthority::tick()
{
    if (m_pending && m_clock.nowMilliseconds() >= m_pending->deadline) {
        finish(OperationStatus::Uncertain, ErrorCode::Timeout,
               m_pending->acknowledged ? QStringLiteral("brightness-observation-timeout")
                                       : QStringLiteral("brightness-apply-timeout"));
    }
}

void BrightnessAuthority::resolvePending()
{
    if (!m_pending || !m_pending->acknowledged || !m_published
        || m_published->serviceEpoch != m_pending->epoch) {
        return;
    }
    const Display::OutputBrightness *row = findRow(*m_published, m_pending->stableId);
    if (row != nullptr && row->observed && row->value == m_pending->value) {
        finish(OperationStatus::Succeeded, ErrorCode::None, {});
    }
}

void BrightnessAuthority::finish(const OperationStatus status, const ErrorCode error,
                                 const QString &diagnostic)
{
    if (!m_pending) {
        return;
    }
    const Pending completed = *std::exchange(m_pending, std::nullopt);
    const quint64 observed = m_published && m_published->serviceEpoch == completed.epoch
        ? std::max(m_published->revision, completed.initiatingRevision)
        : completed.initiatingRevision;
    m_finishes.push_back({.requestId = completed.requestId,
                          .operation = {.kind = Display::OperationKind::ImmediatePolicy,
                                        .status = status,
                                        .error = error,
                                        .initiatingEpoch = completed.epoch,
                                        .initiatingRevision = completed.initiatingRevision,
                                        .observedRevision = observed,
                                        .transactionId = {},
                                        .diagnostic = diagnostic,
                                        .wireValid = true}});
}

QList<BrightnessFinish> BrightnessAuthority::takeFinishes()
{
    return std::exchange(m_finishes, {});
}

bool BrightnessAuthority::takePublicationChanged()
{
    return std::exchange(m_publicationChanged, false);
}

} // namespace QindaQt::DisplayService::Private
