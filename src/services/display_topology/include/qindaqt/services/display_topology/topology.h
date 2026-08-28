// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_topology/topology_types.h>

namespace QindaQt::DisplayTopology
{

[[nodiscard]] bool transposesDimensions(Display::Transform transform) noexcept;
[[nodiscard]] QSize logicalSizeForMode(const Display::Mode &mode, double scale,
                                       Display::Transform transform);
[[nodiscard]] bool hasIntegralLogicalExtent(const Display::Mode &mode, double scale,
                                            Display::Transform transform);

// Snapshot and candidate are borrowed for the call. The result owns all
// values, is deterministic, has no side effects, and can be used from any
// thread. Rejection returns no normalized candidate or partial geometry.
[[nodiscard]] ValidationResult validateAndNormalize(const Display::Snapshot &snapshot,
                                                    const Display::Candidate &candidate);
// AGENT-CONTRACT: display_protocol Snapshot::liveFingerprint and
// display_transaction::Machine both use canonicalFingerprint over this
// projection. Callers must first supply a validateSnapshot-accepted snapshot.
[[nodiscard]] Display::Candidate candidateFromSnapshot(const Display::Snapshot &snapshot);
[[nodiscard]] QByteArray canonicalFingerprint(const Display::Candidate &normalizedCandidate);
[[nodiscard]] QList<CandidateDiff> diff(const Display::Snapshot &snapshot,
                                        const Display::Candidate &normalizedCandidate);

} // namespace QindaQt::DisplayTopology
