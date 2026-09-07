// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/workspaces/workspace.h"
namespace QindaQt::Workspaces::Detail {
// Shared capture/instantiate transform: preserve structural identity and
// ratios; callers validate that every leaf has a binding before this private
// traversal.
inline Core::LayoutNode bindNode(const Core::LayoutNode &node,
                                 const QMap<QString, QString> &bindings) {
  if (node.isLeaf())
    return Core::LayoutNode::makeLeaf(node.id(),
                                      bindings.value(node.windowId()));
  return Core::LayoutNode::makeSplit(node.id(), *node.orientation(),
                                     *node.ratio(),
                                     bindNode(*node.firstChild(), bindings),
                                     bindNode(*node.secondChild(), bindings));
}
} // namespace QindaQt::Workspaces::Detail
