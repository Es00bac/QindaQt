// SPDX-License-Identifier: LGPL-3.0-or-later
#include "topologymutation_p.h"

#include <type_traits>
#include <utility>

namespace QindaQt::Hybrid {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

Core::WindowContainer *mutableContainer(WindowTopology &topology,
                                        const QString &containerId)
{
    auto &containers = TopologyMutationAccess::containers(topology);
    const auto match = containers.find(containerId);
    return match == containers.end() ? nullptr : &match.value();
}

bool addIndependentToContainer(Core::WindowContainer &target,
                               const QString &windowId,
                               const QString &leafNodeId,
                               const MemberDestination &destination,
                               QString *error)
{
    return std::visit(
        [&](const auto &placement) {
            using Placement = std::decay_t<decltype(placement)>;
            if constexpr (std::is_same_v<Placement, MoveAsPage>) {
                const qsizetype oldSize = target.pages().size();
                if (placement.destinationPageIndex < 0
                    || placement.destinationPageIndex > oldSize) {
                    return fail(error,
                                QStringLiteral("destination page index is out of range"));
                }
                if (!target.addPage(placement.pageId, leafNodeId, windowId, error)) {
                    return false;
                }
                return placement.destinationPageIndex == oldSize
                    || target.movePage(placement.pageId,
                                       placement.destinationPageIndex,
                                       error);
            } else {
                return target.splitWindow(
                    {.targetWindowId = placement.targetWindowId,
                     .newWindowId = windowId,
                     .newLeafNodeId = leafNodeId,
                     .splitNodeId = placement.splitNodeId,
                     .orientation = placement.orientation,
                     .ratio = placement.ratio,
                     .position = placement.position},
                    error);
            }
        },
        destination);
}

bool isLeafWindow(const Core::LayoutNode *node, const QString &windowId)
{
    return node && node->isLeaf() && node->windowId() == windowId;
}

bool isDirectSiblingNoOp(const Core::LayoutNode &node,
                         const ReparentMember &command)
{
    if (node.isLeaf()) {
        return false;
    }
    const bool sourceFirst = command.position == Core::InsertPosition::First;
    const auto *expectedFirst = sourceFirst ? node.findWindow(command.windowId)
                                            : node.findWindow(command.targetWindowId);
    const auto *expectedSecond = sourceFirst ? node.findWindow(command.targetWindowId)
                                             : node.findWindow(command.windowId);
    const bool directPair = expectedFirst == node.firstChild()
        && expectedSecond == node.secondChild()
        && isLeafWindow(node.firstChild(), sourceFirst ? command.windowId
                                                       : command.targetWindowId)
        && isLeafWindow(node.secondChild(), sourceFirst ? command.targetWindowId
                                                        : command.windowId);
    if (directPair && node.orientation() == command.orientation
        && node.ratio() == command.ratio) {
        return true;
    }
    return isDirectSiblingNoOp(*node.firstChild(), command)
        || isDirectSiblingNoOp(*node.secondChild(), command);
}

bool containerHasDirectSiblingNoOp(const Core::WindowContainer &container,
                                   const ReparentMember &command)
{
    for (const auto &page : container.pages()) {
        if (isDirectSiblingNoOp(page.root(), command)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool TopologyPlacementMutation::apply(WindowTopology &candidate,
                                      const InsertIndependentWindow &command,
                                      QString *error)
{
    auto &independent = TopologyMutationAccess::independentWindows(candidate);
    if (!independent.contains(command.windowId)) {
        return fail(error,
                    QStringLiteral("window '%1' is not independent")
                        .arg(command.windowId));
    }
    auto *target = mutableContainer(candidate, command.targetContainerId);
    if (!target) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.targetContainerId));
    }
    if (!addIndependentToContainer(*target,
                                   command.windowId,
                                   command.leafNodeId,
                                   command.destination,
                                   error)) {
        return false;
    }
    independent.remove(command.windowId);
    return true;
}

bool TopologyPlacementMutation::apply(
    WindowTopology &candidate,
    const GroupIndependentWindowsAsPages &command,
    QString *error)
{
    if (command.firstWindowId == command.secondWindowId) {
        return fail(error, QStringLiteral("grouping requires two different windows"));
    }
    auto &containers = TopologyMutationAccess::containers(candidate);
    auto &independent = TopologyMutationAccess::independentWindows(candidate);
    if (containers.contains(command.containerId)) {
        return fail(error,
                    QStringLiteral("container ID '%1' already exists")
                        .arg(command.containerId));
    }
    if (!independent.contains(command.firstWindowId)
        || !independent.contains(command.secondWindowId)) {
        return fail(error, QStringLiteral("both grouped windows must be independent"));
    }

    Core::WindowContainer group(command.containerId);
    if (!group.addPage(command.firstPageId,
                       command.firstLeafNodeId,
                       command.firstWindowId,
                       error)
        || !group.addPage(command.secondPageId,
                          command.secondLeafNodeId,
                          command.secondWindowId,
                          error)) {
        return false;
    }
    independent.remove(command.firstWindowId);
    independent.remove(command.secondWindowId);
    containers.insert(group.id(), std::move(group));
    return true;
}

bool TopologyPlacementMutation::apply(
    WindowTopology &candidate,
    const RegroupMemberWithIndependent &command,
    QString *error)
{
    if (command.memberWindowId == command.independentWindowId) {
        return fail(error, QStringLiteral("regrouping requires two different windows"));
    }
    auto &containers = TopologyMutationAccess::containers(candidate);
    auto &independent = TopologyMutationAccess::independentWindows(candidate);
    if (containers.contains(command.newContainerId)) {
        return fail(error,
                    QStringLiteral("container ID '%1' already exists")
                        .arg(command.newContainerId));
    }
    auto *source = mutableContainer(candidate, command.sourceContainerId);
    if (!source) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.sourceContainerId));
    }
    if (!source->findWindow(command.memberWindowId)) {
        return fail(error,
                    QStringLiteral("window '%1' is not owned by source container")
                        .arg(command.memberWindowId));
    }
    if (!independent.contains(command.independentWindowId)) {
        return fail(error,
                    QStringLiteral("window '%1' is not independent")
                        .arg(command.independentWindowId));
    }

    const auto detached = source->detachWindow(command.memberWindowId, error);
    if (!detached) {
        return false;
    }

    Core::WindowContainer group(command.newContainerId);
    const bool grouped = std::visit(
        [&](const auto &layout) {
            using Layout = std::decay_t<decltype(layout)>;
            if constexpr (std::is_same_v<Layout, RegroupAsSplit>) {
                if (!group.addPage(layout.pageId,
                                   layout.independentLeafNodeId,
                                   command.independentWindowId,
                                   error)) {
                    return false;
                }
                return group.splitWindow(
                    {.targetWindowId = command.independentWindowId,
                     .newWindowId = detached->windowId,
                     .newLeafNodeId = detached->leafNodeId,
                     .splitNodeId = layout.splitNodeId,
                     .orientation = layout.orientation,
                     .ratio = layout.ratio,
                     .position = layout.memberPosition},
                    error);
            } else {
                return group.addPage(layout.independentPageId,
                                     layout.independentLeafNodeId,
                                     command.independentWindowId,
                                     error)
                    && group.addPage(layout.memberPageId,
                                     detached->leafNodeId,
                                     detached->windowId,
                                     error);
            }
        },
        command.layout);
    if (!grouped) {
        return false;
    }

    // AGENT-GUARD: The independent target changes ownership only after the new
    // group is valid. The coordinator discards this entire candidate if source
    // detachment or group construction fails, so no partial topology publishes.
    independent.remove(command.independentWindowId);
    containers.insert(group.id(), std::move(group));
    return true;
}

bool TopologyPlacementMutation::apply(WindowTopology &candidate,
                                      const MoveMemberToPage &command,
                                      QString *error)
{
    auto *container = mutableContainer(candidate, command.containerId);
    if (!container) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.containerId));
    }
    const Core::ContainerPage *sourcePage = nullptr;
    for (const auto &page : container->pages()) {
        if (page.root().findWindow(command.windowId)) {
            sourcePage = &page;
            break;
        }
    }
    if (!sourcePage) {
        return fail(error,
                    QStringLiteral("unknown window ID '%1'").arg(command.windowId));
    }
    if (!container->page(command.targetPageId)) {
        return fail(error,
                    QStringLiteral("unknown target page ID '%1'")
                        .arg(command.targetPageId));
    }
    if (sourcePage->id() == command.targetPageId) {
        return fail(error, QStringLiteral("source member is already on target page"));
    }

    const auto detached = container->detachWindow(command.windowId, error);
    if (!detached) {
        return false;
    }
    qsizetype targetIndex = -1;
    for (qsizetype index = 0; index < container->pages().size(); ++index) {
        if (container->pages()[index].id() == command.targetPageId) {
            targetIndex = index;
            break;
        }
    }
    if (targetIndex < 0) {
        return fail(error, QStringLiteral("target page disappeared during extraction"));
    }
    if (!container->addPage(command.newPageId,
                            detached->leafNodeId,
                            detached->windowId,
                            error)) {
        return false;
    }
    const qsizetype destination = targetIndex + 1;
    // addPage appends, so avoid turning an already-correct append into a
    // rejected no-op movePage command.
    return destination == container->pages().size() - 1
        || container->movePage(command.newPageId, destination, error);
}

bool TopologyPlacementMutation::apply(
    WindowTopology &candidate,
    const RegroupPageWithIndependent &command,
    QString *error)
{
    auto &containers = TopologyMutationAccess::containers(candidate);
    auto &independent = TopologyMutationAccess::independentWindows(candidate);
    if (command.newContainerId.isEmpty()
        || containers.contains(command.newContainerId)) {
        return fail(error,
                    QStringLiteral("new container ID must be non-empty and unique"));
    }
    auto *source = mutableContainer(candidate, command.sourceContainerId);
    if (!source) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.sourceContainerId));
    }
    if (!source->page(command.pageId)) {
        return fail(error,
                    QStringLiteral("unknown page ID '%1'").arg(command.pageId));
    }
    if (!independent.contains(command.independentWindowId)) {
        return fail(error,
                    QStringLiteral("window '%1' is not independent")
                        .arg(command.independentWindowId));
    }

    auto sourcePage = source->detachPage(command.pageId, error);
    if (!sourcePage) {
        return false;
    }
    Core::WindowContainer group(command.newContainerId);
    if (!group.addPage(command.independentPageId,
                       command.independentLeafNodeId,
                       command.independentWindowId,
                       error)
        || !group.addPage(std::move(*sourcePage), error)) {
        return false;
    }

    // AGENT-CONTRACT: A tab source transfers its complete page tree. The
    // representative window ID is never a mutation boundary on this path.
    independent.remove(command.independentWindowId);
    containers.insert(group.id(), std::move(group));
    return true;
}

bool TopologyPlacementMutation::apply(WindowTopology &candidate,
                                      const ReparentMember &command,
                                      QString *error)
{
    if (command.windowId == command.targetWindowId) {
        return fail(error, QStringLiteral("reparent source and target must differ"));
    }
    auto *container = mutableContainer(candidate, command.containerId);
    if (!container) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.containerId));
    }
    if (!container->findWindow(command.windowId)
        || !container->findWindow(command.targetWindowId)) {
        const QString &missing = container->findWindow(command.windowId)
            ? command.targetWindowId
            : command.windowId;
        return fail(error, QStringLiteral("unknown window ID '%1'").arg(missing));
    }
    if (container->findNode(command.splitNodeId)) {
        return fail(error,
                    QStringLiteral("split node ID '%1' already exists")
                        .arg(command.splitNodeId));
    }
    if (containerHasDirectSiblingNoOp(*container, command)) {
        return fail(error, QStringLiteral("member already has the requested placement"));
    }

    const auto detached = container->detachWindow(command.windowId, error);
    if (!detached) {
        return false;
    }
    // AGENT-CONTRACT: Drag reparenting moves the original leaf below a new
    // split. Scene adapters and restore-state caches may retain that leaf ID.
    return container->splitWindow(
        {.targetWindowId = command.targetWindowId,
         .newWindowId = detached->windowId,
         .newLeafNodeId = detached->leafNodeId,
         .splitNodeId = command.splitNodeId,
         .orientation = command.orientation,
         .ratio = command.ratio,
         .position = command.position},
        error);
}

} // namespace QindaQt::Hybrid
