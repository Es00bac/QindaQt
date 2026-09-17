// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QString>

namespace QindaQt::Shell::SmartLightsApplet
{

enum class RequestPhase {
    Idle,
    Pending,
    Succeeded,
    Failed,
    Uncertain,
};

struct RequestState {
    RequestPhase phase = RequestPhase::Idle;
    Wiz::OperationRequest operation;
    quint64 initiatingEpoch = 0;
    quint64 initiatingRevision = 0;
    // User-facing sentence for a request that did not plainly succeed. Empty
    // while pending and on success: a light that did what it was told needs no
    // announcement.
    QString feedback;

    [[nodiscard]] bool pending() const noexcept { return phase == RequestPhase::Pending; }

    friend bool operator==(const RequestState &, const RequestState &) = default;
};

// Admission is intentionally duplicated at the final presentation boundary: it
// consumes only a validated current snapshot and never trusts QML row state.
// A request that this function refuses is never dispatched.
[[nodiscard]] RequestState beginSmartLightRequest(const Wiz::Snapshot &snapshot,
                                                  const Wiz::OperationRequest &operation,
                                                  bool controlGranted);

// Only a result for the exact operation and initiating lineage can complete a
// pending request. A foreign or stale result is uncertainty, never success.
//
// AGENT-GUARD: an Uncertain result must not be replayed. The light may already
// have applied it; repeating it would fight the user's next action.
[[nodiscard]] RequestState applySmartLightResult(const RequestState &request,
                                                 const Wiz::OperationResult &result);

// Losing the service, or a restart that bumps the epoch, ends a pending
// request as uncertain rather than leaving it pending forever.
[[nodiscard]] RequestState observeSmartLightAuthority(const RequestState &request,
                                                      bool serviceAvailable,
                                                      quint64 currentEpoch);

} // namespace QindaQt::Shell::SmartLightsApplet
