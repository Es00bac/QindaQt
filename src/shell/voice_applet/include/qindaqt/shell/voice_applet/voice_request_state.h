// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QString>

namespace QindaQt::Shell::VoiceApplet {

enum class RequestPhase {
    Idle,
    Pending,
    Succeeded,
    Failed,
    Uncertain,
};

struct RequestState {
    RequestPhase phase = RequestPhase::Idle;
    Services::Voice::OperationKind kind = Services::Voice::OperationKind::StartDictation;
    quint64 requestId = 0;
    quint64 initiatingRevision = 0;
    // A sentence for a request that did not plainly succeed. Empty while
    // pending and on success: dictation that worked speaks for itself.
    QString feedback;

    [[nodiscard]] bool pending() const noexcept { return phase == RequestPhase::Pending; }

    friend bool operator==(const RequestState &, const RequestState &) = default;
};

// Admission is deliberately duplicated here, at the presentation boundary, and
// again inside VoiceClient. This copy consumes only a validated snapshot and
// never trusts a QML-supplied action id, so a request refused here is never
// dispatched at all.
[[nodiscard]] RequestState beginVoiceRequest(const Services::Voice::Snapshot &snapshot,
                                             bool hasSnapshot,
                                             Services::Voice::OperationKind kind,
                                             bool controlGranted);

// Only a result for the exact request id and kind can complete a pending
// request. A foreign or stale result is uncertainty, never success.
//
// AGENT-GUARD: an Uncertain voice request must never be replayed. StartDictation
// is not idempotent — a silent retry opens a second microphone session and can
// deliver the same utterance twice.
[[nodiscard]] RequestState applyVoiceResult(const RequestState &request,
                                            const Services::Voice::OperationResult &result);

// Losing the provider ends a pending request as uncertain rather than leaving
// it pending until the panel is rebuilt.
[[nodiscard]] RequestState observeVoiceAuthority(const RequestState &request,
                                                 bool serviceAvailable);

} // namespace QindaQt::Shell::VoiceApplet
