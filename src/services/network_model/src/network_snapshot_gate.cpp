// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_model/network_snapshot_gate.h>

namespace QindaQt::Network::Model {

GateDecision gateSnapshot(const std::optional<Lineage> &current,
                          const Snapshot &incoming) {
  if (incoming.epoch == 0 || incoming.revision == 0) {
    return {GateDecisionKind::RejectInvalidLineage,
            QStringLiteral("snapshot-lineage-invalid")};
  }
  if (!current.has_value()) {
    return {GateDecisionKind::Accept, {}};
  }
  if (current->owner == incoming.owner && current->epoch == incoming.epoch) {
    if (incoming.revision <= current->revision) {
      return {GateDecisionKind::RejectStaleRevision,
              QStringLiteral("snapshot-revision-not-newer")};
    }
    return {GateDecisionKind::Accept, {}};
  }
  if (current->owner == incoming.owner && current->epoch != incoming.epoch) {
    // AGENT-NOTE: One bus owner cannot legitimately change its service epoch
    // while keeping the unique name; that shape is a replayed or forged
    // restart, so it is refused instead of reset.
    return {GateDecisionKind::RejectSameOwnerEpochReset,
            QStringLiteral("same-owner-epoch-reset")};
  }
  if (incoming.epoch <= current->epoch) {
    return {GateDecisionKind::RejectStaleEpoch,
            QStringLiteral("snapshot-epoch-not-newer")};
  }
  return {GateDecisionKind::Accept, {}};
}

} // namespace QindaQt::Network::Model
