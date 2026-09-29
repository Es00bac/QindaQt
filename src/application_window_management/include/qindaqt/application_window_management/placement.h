// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <optional>
#include <qindaqt/application_window_management/types.h>
#include <qindaqt/hybrid/topologycommand.h>
#include <qindaqt/hybrid/windowtopology.h>

namespace QindaQt::ApplicationWindowManagement {
struct PlacementPlan final {
  std::optional<Hybrid::TopologyCommand> command;
  QString containerId;
  QString message;
};
// Pure synchronous planning: borrows immutable topology only for this call.
// IDs are server-authenticated managed windows; transport authenticates surface
// ownership and foreground/lock state before planning. `nonce` must be a fresh
// server-generated identifier. A failed plan changes nothing. Execute the
// command through TopologyCoordinator, never mutate topology directly.
[[nodiscard]] PlacementPlan
planPlacement(const Hybrid::WindowTopology &topology, const QString &source,
              const QString &created, Placement placement,
              const QString &nonce);
} // namespace QindaQt::ApplicationWindowManagement
