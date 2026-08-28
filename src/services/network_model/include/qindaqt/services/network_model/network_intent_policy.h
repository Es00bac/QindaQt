// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_scan_lease.h>
#include <qindaqt/services/network_protocol/network_intent.h>
#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QString>

#include <optional>

namespace QindaQt::Network::Model {

struct IntentVerdict final {
  bool allowed = false;
  OperationKind kind = OperationKind::RequestScan;
  QString reasonCode;

  friend bool operator==(const IntentVerdict &, const IntentVerdict &) = default;
};

// AGENT-CONTRACT: These validators are the only admission path from a user
// intent to a transportable Network operation. Every rejection carries a
// stable reason code; a rejected intent changes no model state. Callers must
// not bypass them by transporting raw operation kinds.
[[nodiscard]] IntentVerdict
validateRequestScan(const std::optional<Snapshot> &snapshot,
                    const ScanLeaseTracker &lease,
                    const MonotonicClock &clock,
                    const RequestScanIntent &intent);
[[nodiscard]] IntentVerdict validateConnect(const std::optional<Snapshot> &snapshot,
                                            const ConnectIntent &intent);
[[nodiscard]] IntentVerdict
validateDisconnect(const std::optional<Snapshot> &snapshot,
                   const DisconnectIntent &intent);
[[nodiscard]] IntentVerdict validateSetRadio(const std::optional<Snapshot> &snapshot,
                                             const SetRadioIntent &intent);

} // namespace QindaQt::Network::Model
