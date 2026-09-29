// SPDX-License-Identifier: GPL-3.0-or-later

// Per-device latency offsets (ADR-0288): remembered by node.name through the
// latency store and declared whole to the backend, which applies them.
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include "audio_operation_admission_p.h"

namespace QindaQt::Audio
{

OperationSubmission AudioOperationCoordinator::submitLatencyOffset(
    const OperationRequest &request)
{
    const QString rejection = validateRequest(request);
    if (!rejection.isEmpty()) {
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request,
                                             rejection == QStringLiteral("unsupported")
                                                 ? OperationStatus::Unsupported
                                                 : OperationStatus::Rejected,
                                             rejection)};
    }
    // validateRequest proved the device is in the retained snapshot, has a
    // node.name and admits this value; the name is what survives a reboot.
    const Device *const device = Admission::findDevice(m_snapshot, request.primary);
    QString reasonCode = QStringLiteral("stale-handle");
    if (device == nullptr || !m_latency.set(device->nodeName, request.latencyOffsetNs,
                                            &reasonCode)) {
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(
                    request,
                    reasonCode == QStringLiteral("latency-store-unwritable")
                        ? OperationStatus::Failed
                        : OperationStatus::Rejected,
                    reasonCode)};
    }
    publishLatency();
    // AGENT-NOTE: success means remembered and declared. The graph reports
    // the applied value through the device's latencyOffsetNs in a later
    // snapshot; a caller confirms from there, never from this result.
    return {.pending = false,
            .operationId = 0,
            .immediateResult = immediate(request, OperationStatus::Succeeded, {})};
}

void AudioOperationCoordinator::publishLatency()
{
    QList<BackendLatencyOffset> wanted;
    const QMap<QString, qint64> stored = m_latency.load();
    for (auto it = stored.cbegin(); it != stored.cend(); ++it) {
        wanted.append({.nodeName = it.key(), .offsetNs = it.value()});
    }
    if (wanted == m_publishedLatency) {
        return;
    }
    m_publishedLatency = wanted;
    if (m_running) {
        m_backend->applyLatencyOffsets(m_publishedLatency);
    }
}

} // namespace QindaQt::Audio
