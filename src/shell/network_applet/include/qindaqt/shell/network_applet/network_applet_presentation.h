// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/shell/network_applet/network_applet_types.h>

namespace QindaQt::Shell::NetworkApplet
{

// Client facts the pure projection cannot read from the value model itself.
// The runtime controller fills this from its NetworkClient; tests fill it
// directly.
struct ProjectionContext {
    bool readGranted = false;
    bool controlGranted = false;
    // NetworkClient::snapshotCurrent(): false after owner loss, a failed
    // refresh, or an uncertain operation even when last-known rows remain.
    bool snapshotCurrent = false;
    // The client lifecycle mapped onto the presentation vocabulary.
    ServicePhase clientPhase = ServicePhase::Unavailable;
    // NetworkClient::operationAdmissionReady() and no applet request pending.
    bool admissionOpen = false;
    QString clientDiagnostic;
};

// Projects one bounded, owned value model from the client's NetworkModel.
// Every `can*` flag is the model's own intent verdict gated by the control
// grant and `admissionOpen`, so QML can never advertise an action the client
// would refuse. No secret-bearing field exists on any input or output.
[[nodiscard]] NetworkAppletModel projectNetworkApplet(
    const Network::Model::NetworkModel &model,
    const ProjectionContext &context);

} // namespace QindaQt::Shell::NetworkApplet
