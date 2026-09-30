// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/hybrid/topologycommand.h>
#include <qindaqt/hybrid/windowtopology.h>
#include <optional>
namespace QindaQt::WindowManagement {
enum class GroupingMode { Tab, Tile };
enum class GroupingDirection { Left, Right, Above, Below };
struct GroupingRequest final {
    QString movedWindowId;
    QString targetWindowId;
    GroupingMode mode = GroupingMode::Tab;
    GroupingDirection direction = GroupingDirection::Right;
    // First split child's share, regardless of the moved member's side.
    double ratio = 0.5;
    QString nonce;
};
struct GroupingPlan final {
    QString containerId;
    std::optional<Hybrid::TopologyCommand> command;
    QString error;
    [[nodiscard]] bool valid() const noexcept { return command.has_value(); }
};
// Pure, synchronous value planning; no scene, transport or borrowed state is
// retained. Caller resolves two live leaves and supplies a fresh canonical
// QUuid::WithoutBraces nonce, then serializes execution against this topology.
// Checks syntax/current structural collisions, not historical nonce freshness.
// Exactly one command: tab activation is part of its candidate transaction.
// Whole-container moves are outside this leaf-only boundary. Rejection returns
// no command and never mutates topology; ownership is rechecked by coordinator.
[[nodiscard]] GroupingPlan planGrouping(const Hybrid::WindowTopology &topology,
                                        const GroupingRequest &request);
}
