// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QString>

#include <optional>

namespace QindaQt::Network::Model {

struct Lineage final {
  QString owner;
  quint64 epoch = 0;
  quint64 revision = 0;

  friend bool operator==(const Lineage &, const Lineage &) = default;
};

enum class GateDecisionKind {
  Accept,
  RejectStaleRevision,
  RejectStaleEpoch,
  RejectSameOwnerEpochReset,
  RejectInvalidLineage,
};

struct GateDecision final {
  GateDecisionKind kind = GateDecisionKind::Accept;
  QString reasonCode;

  [[nodiscard]] bool accepted() const noexcept {
    return kind == GateDecisionKind::Accept;
  }
};

// AGENT-GUARD: The snapshot gate is the single lineage authority for Network
// N0. Rules: a first snapshot with nonzero epoch/revision is accepted; the
// same owner and epoch must present strictly increasing revisions; any owner
// change or epoch change requires an epoch strictly greater than every
// previously observed epoch. Violating the second rule admits revision
// rollback and out-of-order replay; violating the third admits an A/B/A
// replay of a replaced owner's stale snapshot.
[[nodiscard]] GateDecision gateSnapshot(const std::optional<Lineage> &current,
                                        const Snapshot &incoming);

} // namespace QindaQt::Network::Model
