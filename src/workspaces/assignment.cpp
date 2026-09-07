// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces/workspace.h"
#include <QSet>

namespace QindaQt::Workspaces {
AssignmentPlan
assignWindows(const Workspace &workspace, const QList<AvailableWindow> &windows,
              const QMap<QString, QString> &explicitAssignments) {
  AssignmentPlan result;
  if (!workspace.validate(&result.error))
    return result;
  QMap<QString, AvailableWindow> byId;
  for (const auto &window : windows) {
    if (window.id.isEmpty() || byId.contains(window.id)) {
      result.error = QStringLiteral(
          "Window inventory contains an invalid or duplicate identity");
      return result;
    }
    byId.insert(window.id, window);
  }
  QSet<QString> slotIds;
  for (const auto &slot : workspace.applicationSlots)
    slotIds.insert(slot.id);
  QSet<QString> used;
  for (auto it = explicitAssignments.cbegin(); it != explicitAssignments.cend();
       ++it) {
    if (!slotIds.contains(it.key()) || !byId.contains(it.value()) ||
        !byId.value(it.value()).eligible || used.contains(it.value())) {
      result.error = QStringLiteral(
          "Selected window is unavailable or assigned more than once");
      return result;
    }
    used.insert(it.value());
    result.windowsBySlot.insert(it.key(), it.value());
  }
  QMap<QString, QStringList> slotsByApp;
  QMap<QString, QStringList> windowsByApp;
  for (const auto &slot : workspace.applicationSlots)
    if (!result.windowsBySlot.contains(slot.id))
      slotsByApp[slot.desktopEntryId].append(slot.id);
  for (const auto &window : windows)
    if (window.eligible && !used.contains(window.id))
      windowsByApp[window.desktopEntryId].append(window.id);
  for (const auto &slot : workspace.applicationSlots) {
    if (result.windowsBySlot.contains(slot.id))
      continue;
    const auto candidates = windowsByApp.value(slot.desktopEntryId);
    if (slotsByApp.value(slot.desktopEntryId).size() == 1 &&
        candidates.size() == 1)
      result.windowsBySlot.insert(slot.id, candidates.first());
    else {
      result.unassignedSlots.append(slot.id);
      if (!candidates.isEmpty())
        result.ambiguousSlots.append(slot.id);
    }
  }
  return result;
}
namespace {
Core::LayoutNode bindNode(const Core::LayoutNode &node,
                          const QMap<QString, QString> &bindings) {
  if (node.isLeaf())
    return Core::LayoutNode::makeLeaf(node.id(),
                                      bindings.value(node.windowId()));
  return Core::LayoutNode::makeSplit(node.id(), *node.orientation(),
                                     *node.ratio(),
                                     bindNode(*node.firstChild(), bindings),
                                     bindNode(*node.secondChild(), bindings));
}
} // namespace
std::optional<Core::WindowContainer> instantiate(const Workspace &workspace,
                                                 const AssignmentPlan &plan,
                                                 const QString &containerId,
                                                 QString *error) {
  auto fail =
      [error](const QString &message) -> std::optional<Core::WindowContainer> {
    if (error)
      *error = message;
    return std::nullopt;
  };
  if (!workspace.validate(error))
    return std::nullopt;
  if (!plan.complete() ||
      plan.windowsBySlot.size() != workspace.applicationSlots.size())
    return fail(QStringLiteral(
        "Choose a window for every application slot before reopening"));
  QSet<QString> used;
  for (const auto &slot : workspace.applicationSlots) {
    const auto window = plan.windowsBySlot.value(slot.id);
    if (window.isEmpty() || used.contains(window))
      return fail(
          QStringLiteral("Window assignments are incomplete or duplicated"));
    used.insert(window);
  }
  Core::WindowContainer result(containerId);
  for (const auto &page : workspace.layout.pages())
    if (!result.addPage(
            Core::ContainerPage(page.id(),
                                bindNode(page.root(), plan.windowsBySlot)),
            error))
      return std::nullopt;
  if (!result.activatePage(workspace.layout.activePageId(), error))
    return std::nullopt;
  if (!result.validate().valid)
    return fail(QStringLiteral("Restored layout is invalid"));
  return result;
}
} // namespace QindaQt::Workspaces
