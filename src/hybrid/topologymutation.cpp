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

bool applyAddIndependent(WindowTopology &topology,
                         const AddIndependentWindow &command,
                         QString *error)
{
    if (command.windowId.isEmpty()) {
        return fail(error, QStringLiteral("window ID must be non-empty"));
    }
    auto &independentWindows = TopologyMutationAccess::independentWindows(topology);
    if (independentWindows.contains(command.windowId) || topology.ownerOf(command.windowId)) {
        return fail(error,
                    QStringLiteral("window '%1' already has topology ownership")
                        .arg(command.windowId));
    }
    independentWindows.insert(command.windowId);
    return true;
}

bool applyForget(WindowTopology &topology,
                 const ForgetWindow &command,
                 QString *error)
{
    auto &independentWindows = TopologyMutationAccess::independentWindows(topology);
    if (independentWindows.remove(command.windowId)) {
        return true;
    }
    const auto owner = topology.ownerOf(command.windowId);
    if (!owner) {
        return fail(error, QStringLiteral("unknown window ID '%1'").arg(command.windowId));
    }
    auto *container = mutableContainer(topology, *owner);
    if (!container || !container->removeWindow(command.windowId, error)) {
        return false;
    }
    // AGENT-GUARD: A closed window is absent from the scene candidate. Do not
    // share DetachMember's path, which intentionally makes a live client
    // independent and would leave a dead ID in later docking inventory.
    return true;
}

bool applyDock(WindowTopology &topology,
               const DockIndependentWindows &command,
               QString *error)
{
    if (command.firstWindowId == command.secondWindowId) {
        return fail(error, QStringLiteral("docking requires two different windows"));
    }
    auto &containers = TopologyMutationAccess::containers(topology);
    auto &independentWindows = TopologyMutationAccess::independentWindows(topology);
    if (containers.contains(command.containerId)) {
        return fail(error,
                    QStringLiteral("container ID '%1' already exists")
                        .arg(command.containerId));
    }
    if (!independentWindows.contains(command.firstWindowId)
        || !independentWindows.contains(command.secondWindowId)) {
        return fail(error, QStringLiteral("both docked windows must be independent"));
    }

    Core::WindowContainer container(command.containerId);
    if (!container.addPage(command.pageId,
                           command.firstLeafNodeId,
                           command.firstWindowId,
                           error)) {
        return false;
    }
    const Core::SplitRequest split{
        .targetWindowId = command.firstWindowId,
        .newWindowId = command.secondWindowId,
        .newLeafNodeId = command.secondLeafNodeId,
        .splitNodeId = command.splitNodeId,
        .orientation = command.orientation,
        .ratio = command.ratio,
        .position = command.secondPosition,
    };
    if (!container.splitWindow(split, error)) {
        return false;
    }

    independentWindows.remove(command.firstWindowId);
    independentWindows.remove(command.secondWindowId);
    containers.insert(container.id(), std::move(container));
    return true;
}

bool applyMerge(WindowTopology &topology,
                const MergeContainers &command,
                QString *error)
{
    if (command.targetContainerId == command.sourceContainerId) {
        return fail(error, QStringLiteral("source and target containers must differ"));
    }
    auto *target = mutableContainer(topology, command.targetContainerId);
    const auto *source = topology.container(command.sourceContainerId);
    if (!target || !source) {
        const auto &missing = target ? command.sourceContainerId : command.targetContainerId;
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(missing));
    }
    const qsizetype originalTargetSize = target->pages().size();
    if (command.destinationPageIndex < 0
        || command.destinationPageIndex > originalTargetSize) {
        return fail(error, QStringLiteral("destination page index is out of range"));
    }

    // Copy before mutating target: QMap insertion and container edits may detach
    // storage that backs the source reference.
    const QVector<Core::ContainerPage> sourcePages = source->pages();
    for (const auto &page : sourcePages) {
        if (!target->addPage(page, error)) {
            return false;
        }
    }
    for (qsizetype offset = 0; offset < sourcePages.size(); ++offset) {
        const qsizetype destination = command.destinationPageIndex + offset;
        const QString &pageId = sourcePages[offset].id();
        const auto currentPages = target->pages();
        if (currentPages[destination].id() != pageId
            && !target->movePage(pageId, destination, error)) {
            return false;
        }
    }
    TopologyMutationAccess::containers(topology).remove(command.sourceContainerId);
    return true;
}

bool applyMovePage(WindowTopology &topology,
                   const MovePage &command,
                   QString *error)
{
    if (command.sourceContainerId == command.targetContainerId) {
        return fail(error,
                    QStringLiteral("cross-container page move requires different containers"));
    }
    auto *source = mutableContainer(topology, command.sourceContainerId);
    auto *target = mutableContainer(topology, command.targetContainerId);
    if (!source || !target) {
        const auto &missing = source ? command.targetContainerId
                                     : command.sourceContainerId;
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(missing));
    }
    const qsizetype targetSize = target->pages().size();
    if (command.destinationPageIndex < 0
        || command.destinationPageIndex > targetSize) {
        return fail(error, QStringLiteral("destination page index is out of range"));
    }

    const auto page = source->detachPage(command.pageId, error);
    if (!page || !target->addPage(*page, error)) {
        return false;
    }
    return command.destinationPageIndex == targetSize
        || target->movePage(command.pageId, command.destinationPageIndex, error);
}

bool applyDetachPage(WindowTopology &topology,
                     const DetachPage &command,
                     QString *error)
{
    auto &containers = TopologyMutationAccess::containers(topology);
    auto *source = mutableContainer(topology, command.sourceContainerId);
    if (!source) {
        return fail(error,
                    QStringLiteral("unknown container ID '%1'")
                        .arg(command.sourceContainerId));
    }
    if (source->pages().size() <= 1) {
        return fail(error, QStringLiteral("the only page already represents the container"));
    }
    const auto *page = source->page(command.pageId);
    if (!page) {
        return fail(error, QStringLiteral("unknown page ID '%1'").arg(command.pageId));
    }
    const bool createsContainer = page->root().isSplit();
    if (createsContainer
        && (command.newContainerId.isEmpty()
            || containers.contains(command.newContainerId))) {
        return fail(error, QStringLiteral("detached page needs a unique container ID"));
    }

    auto detached = source->detachPage(command.pageId, error);
    if (!detached) {
        return false;
    }
    if (!createsContainer) {
        TopologyMutationAccess::independentWindows(topology).insert(
            detached->root().windowId());
        return true;
    }

    Core::WindowContainer separated(command.newContainerId);
    if (!separated.addPage(std::move(*detached), error)) {
        return false;
    }
    containers.insert(separated.id(), std::move(separated));
    return true;
}

bool addMovedMember(Core::WindowContainer &target,
                    const Core::DetachedWindow &detached,
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
                    return fail(error, QStringLiteral("destination page index is out of range"));
                }
                if (!target.addPage(placement.pageId,
                                    detached.leafNodeId,
                                    detached.windowId,
                                    error)) {
                    return false;
                }
                return placement.destinationPageIndex == oldSize
                    || target.movePage(placement.pageId,
                                       placement.destinationPageIndex,
                                       error);
            } else {
                const Core::SplitRequest split{
                    .targetWindowId = placement.targetWindowId,
                    .newWindowId = detached.windowId,
                    .newLeafNodeId = detached.leafNodeId,
                    .splitNodeId = placement.splitNodeId,
                    .orientation = placement.orientation,
                    .ratio = placement.ratio,
                    .position = placement.position,
                };
                return target.splitWindow(split, error);
            }
        },
        destination);
}

bool applyMove(WindowTopology &topology, const MoveMember &command, QString *error)
{
    if (command.sourceContainerId == command.targetContainerId) {
        return fail(error, QStringLiteral("cross-container move requires different containers"));
    }
    auto *source = mutableContainer(topology, command.sourceContainerId);
    auto *target = mutableContainer(topology, command.targetContainerId);
    if (!source || !target) {
        const auto &missing = source ? command.targetContainerId : command.sourceContainerId;
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(missing));
    }
    if (!source->findWindow(command.windowId)) {
        return fail(error,
                    QStringLiteral("window '%1' is not owned by source container")
                        .arg(command.windowId));
    }

    const auto detached = source->detachWindow(command.windowId, error);
    if (!detached) {
        return false;
    }
    return addMovedMember(*target, *detached, command.destination, error);
}

bool applyReorderPage(WindowTopology &topology,
                      const ReorderPage &command,
                      QString *error)
{
    auto *container = mutableContainer(topology, command.containerId);
    return container
        ? container->movePage(command.pageId, command.destinationPageIndex, error)
        : fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
}

bool applyActivatePage(WindowTopology &topology,
                       const ActivatePage &command,
                       QString *error)
{
    auto *container = mutableContainer(topology, command.containerId);
    if (!container) {
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
    }
    if (container->activePageId() == command.pageId) {
        return fail(error, QStringLiteral("page is already active"));
    }
    return container->activatePage(command.pageId, error);
}

bool applyResizeSplit(WindowTopology &topology,
                      const ResizeSplit &command,
                      QString *error)
{
    auto *container = mutableContainer(topology, command.containerId);
    if (!container) {
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
    }
    const auto *node = container->findNode(command.splitNodeId);
    if (node && node->isSplit() && node->ratio() == std::optional(command.ratio)) {
        return fail(error, QStringLiteral("split already has the requested ratio"));
    }
    return container->setSplitRatio(command.splitNodeId, command.ratio, error);
}

bool applyReorderMembers(WindowTopology &topology,
                         const ReorderMembers &command,
                         QString *error)
{
    auto *container = mutableContainer(topology, command.containerId);
    return container
        ? container->swapWindows(command.firstWindowId, command.secondWindowId, error)
        : fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
}

bool applyDetach(WindowTopology &topology,
                 const DetachMember &command,
                 QString *error)
{
    auto *container = mutableContainer(topology, command.containerId);
    if (!container) {
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
    }
    const auto detached = container->detachWindow(command.windowId, error);
    if (!detached) {
        return false;
    }
    TopologyMutationAccess::independentWindows(topology).insert(detached->windowId);
    return true;
}

bool applyRelease(WindowTopology &topology,
                  const ReleaseContainer &command,
                  QString *error)
{
    const auto *container = topology.container(command.containerId);
    if (!container) {
        return fail(error, QStringLiteral("unknown container ID '%1'").arg(command.containerId));
    }
    for (const QString &windowId : topology.windowIds(command.containerId)) {
        TopologyMutationAccess::independentWindows(topology).insert(windowId);
    }
    TopologyMutationAccess::containers(topology).remove(command.containerId);
    return true;
}

} // namespace

bool TopologyMutation::apply(WindowTopology &candidate,
                             const TopologyCommand &command,
                             QString *error)
{
    return std::visit(
        [&](const auto &typedCommand) {
            using Command = std::decay_t<decltype(typedCommand)>;
            if constexpr (std::is_same_v<Command, AddIndependentWindow>) {
                return applyAddIndependent(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ForgetWindow>) {
                return applyForget(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, DockIndependentWindows>) {
                return applyDock(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, MergeContainers>) {
                return applyMerge(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, MovePage>) {
                return applyMovePage(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, DetachPage>) {
                return applyDetachPage(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, MoveMemberToPage>
                                 || std::is_same_v<Command,
                                                   RegroupPageWithIndependent>
                                 || std::is_same_v<Command,
                                                   InsertIndependentWindow>
                                 || std::is_same_v<Command,
                                                   GroupIndependentWindowsAsPages>
                                 || std::is_same_v<Command,
                                                   RegroupMemberWithIndependent>) {
                return TopologyPlacementMutation::apply(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, MoveMember>) {
                return applyMove(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ReorderPage>) {
                return applyReorderPage(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ActivatePage>) {
                return applyActivatePage(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ResizeSplit>) {
                return applyResizeSplit(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ReorderMembers>) {
                return applyReorderMembers(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, ReparentMember>) {
                return TopologyPlacementMutation::apply(candidate, typedCommand, error);
            } else if constexpr (std::is_same_v<Command, DetachMember>) {
                return applyDetach(candidate, typedCommand, error);
            } else {
                return applyRelease(candidate, typedCommand, error);
            }
        },
        command);
}

void TopologyMutation::normalize(WindowTopology &candidate)
{
    QStringList containersToRemove;
    for (auto match = candidate.m_containers.cbegin();
         match != candidate.m_containers.cend();
         ++match) {
        if (match.value().pages().isEmpty()) {
            containersToRemove.append(match.key());
        } else if (const auto singleton = match.value().singleWindowId()) {
            candidate.m_independentWindows.insert(*singleton);
            containersToRemove.append(match.key());
        }
    }
    for (const QString &containerId : containersToRemove) {
        candidate.m_containers.remove(containerId);
    }
}

} // namespace QindaQt::Hybrid
