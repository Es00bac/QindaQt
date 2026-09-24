// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QString>

namespace QindaQt::Shell::NetworkApplet
{

enum class RequestAction {
    Scan,
    ConnectKnown,
    ConnectVisible,
    Disconnect,
    SetRadio,
};

// Pending: dispatched, no reply yet. Confirming: Network1 accepted the
// mutation and the applet waits for a newer same-owner snapshot to show the
// requested state. Only that snapshot produces Succeeded.
enum class RequestPhase {
    Idle,
    Pending,
    Confirming,
    Succeeded,
    Failed,
    Uncertain,
};

struct RequestTarget {
    RequestAction action = RequestAction::Scan;
    // Known-network id, visible access-point id, or device interface.
    QString id;
    // Presentation name only; never used for matching except ConnectVisible,
    // whose created profile is recognized by its public SSID and security.
    QString label;
    QString ssid;
    Network::SecuritySuite security = Network::SecuritySuite::Open;
    Network::RadioKind radio = Network::RadioKind::Wifi;
    bool enable = false;

    friend bool operator==(const RequestTarget &, const RequestTarget &) = default;
};

struct RequestState {
    RequestPhase phase = RequestPhase::Idle;
    RequestTarget target;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
    QString feedback;

    [[nodiscard]] bool active() const noexcept
    {
        return phase == RequestPhase::Pending || phase == RequestPhase::Confirming;
    }

    friend bool operator==(const RequestState &, const RequestState &) = default;
};

// A locally refused intent (model verdict or client admission). Nothing was
// sent; the stable reason code becomes user text.
[[nodiscard]] RequestState refuseNetworkRequest(const RequestTarget &target,
                                                const QString &reasonCode);

// The client accepted and dispatched the intent against this exact lineage.
[[nodiscard]] RequestState beginNetworkRequest(const RequestTarget &target,
                                               const QString &owner,
                                               quint64 epoch,
                                               quint64 revision);

// Only a reply of the same kind and initiating lineage advances the request.
// Anything else is uncertainty: never success, never an automatic replay.
[[nodiscard]] RequestState applyNetworkResult(const RequestState &request,
                                              const Network::OperationResult &result);

[[nodiscard]] RequestState applyNetworkUncertain(const RequestState &request);

// Confirms or retires an active request from newly published truth. `state`
// must be the client's current projection; `current` is snapshotCurrent().
[[nodiscard]] RequestState observeNetworkState(const RequestState &request,
                                               const Network::Model::ModelState &state,
                                               bool current);

// The bounded confirmation window elapsed without confirming truth.
[[nodiscard]] RequestState expireNetworkRequest(const RequestState &request);

[[nodiscard]] QString networkReasonText(const QString &reasonCode);

} // namespace QindaQt::Shell::NetworkApplet
