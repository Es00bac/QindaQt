// SPDX-License-Identifier: LGPL-3.0-or-later
#include "topologymutation_p.h"
namespace QindaQt::Hybrid {
namespace {
void collectMembers(const Core::LayoutNode &node, QStringList &members) {
  if (node.isLeaf())
    members.append(node.windowId());
  else {
    collectMembers(*node.firstChild(), members);
    collectMembers(*node.secondChild(), members);
  }
}
} // namespace
bool TopologyAdoptionMutation::apply(WindowTopology &candidate,
                                     const AdoptIndependentLayout &command,
                                     QString *error) {
  auto fail = [error](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  const auto validation = command.container.validate();
  if (!validation.valid)
    return fail(validation.message);
  auto &containers = TopologyMutationAccess::containers(candidate);
  if (containers.contains(command.container.id()))
    return fail(
        QStringLiteral("A container with this identity already exists"));
  QStringList members;
  for (const auto &page : command.container.pages())
    collectMembers(page.root(), members);
  if (members.size() < 2)
    return fail(
        QStringLiteral("A restored container needs at least two windows"));
  auto &independent = TopologyMutationAccess::independentWindows(candidate);
  for (const auto &member : members)
    if (!independent.contains(member))
      return fail(QStringLiteral(
          "A chosen window is no longer available. Refresh the window list."));
  // AGENT-GUARD: Check the whole inventory before removing any ownership.
  // The coordinator publishes only after scene prepare/commit, or rolls back.
  for (const auto &member : members)
    independent.remove(member);
  containers.insert(command.container.id(), command.container);
  return true;
}
} // namespace QindaQt::Hybrid
