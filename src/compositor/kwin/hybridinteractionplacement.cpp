// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// AGENT-NOTE: Same-container and cross-container placement commits live in
// this file so hybridinteractionruntime.cpp stays within the source-shape
// review threshold; both are private members of HybridInteractionRuntime.

HybridRuntimeResult rejected(QString message)
{
    return {.status = HybridRuntimeStatus::Rejected,
            .topologyResult = std::nullopt,
            .message = std::move(message)};
}

HybridRuntimeResult noChange(QString message)
{
    return {.status = HybridRuntimeStatus::NoChange,
            .topologyResult = std::nullopt,
            .message = std::move(message)};
}

struct SplitPlacement final
{
    Core::SplitOrientation orientation = Core::SplitOrientation::Horizontal;
    Core::InsertPosition position = Core::InsertPosition::Second;
};

std::optional<SplitPlacement> splitPlacement(HybridInput::DockZone zone)
{
    switch (zone) {
    case HybridInput::DockZone::Left:
        return SplitPlacement{Core::SplitOrientation::Horizontal,
                              Core::InsertPosition::First};
    case HybridInput::DockZone::Right:
        return SplitPlacement{Core::SplitOrientation::Horizontal,
                              Core::InsertPosition::Second};
    case HybridInput::DockZone::Top:
        return SplitPlacement{Core::SplitOrientation::Vertical,
                              Core::InsertPosition::First};
    case HybridInput::DockZone::Bottom:
        return SplitPlacement{Core::SplitOrientation::Vertical,
                              Core::InsertPosition::Second};
    case HybridInput::DockZone::None:
    case HybridInput::DockZone::Tab:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

HybridRuntimeResult HybridInteractionRuntime::commitAcrossContainers(
    const HybridInput::InteractionIntent &intent,
    const QString &sourceContainerId,
    const QString &targetContainerId)
{
    const auto targetPage = intent.target.memberId.isEmpty()
        ? std::optional<PageLocation>{}
        : pageLocation(targetContainerId, intent.target.memberId);
    const auto *targetContainer = topology().container(targetContainerId);
    if (!targetContainer || (!intent.target.memberId.isEmpty() && !targetPage)) {
        return rejected(QStringLiteral("cross-container target page is missing"));
    }
    const qsizetype pageInsertion = targetPage
        ? targetPage->index + 1
        : targetContainer->pages().size();

    if (intent.target.zone == HybridInput::DockZone::Tab) {
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            const auto sourcePage = pageLocation(sourceContainerId,
                                                 intent.source.memberId);
            if (!sourcePage || intent.source.pageId.isEmpty()
                || sourcePage->pageId != intent.source.pageId) {
                return rejected(QStringLiteral("dragged tab page identity is stale"));
            }
            return execute(Hybrid::MovePage{
                .sourceContainerId = sourceContainerId,
                .targetContainerId = targetContainerId,
                .pageId = intent.source.pageId,
                .destinationPageIndex = pageInsertion,
            });
        }
        return execute(Hybrid::MoveMember{
            .sourceContainerId = sourceContainerId,
            .targetContainerId = targetContainerId,
            .windowId = intent.source.memberId,
            .destination = Hybrid::MoveAsPage{
                structuralId(QLatin1StringView("page")), pageInsertion},
        });
    }

    if (intent.source.kind == HybridInput::HitKind::Tab) {
        return rejected(QStringLiteral("tabs can only move to a tab destination"));
    }
    const auto split = splitPlacement(intent.target.zone);
    if (!split) {
        return rejected(QStringLiteral("dock zone cannot create a split"));
    }
    if (intent.target.memberId.isEmpty()) {
        // Container-level edge drop: the moved member takes a whole side of
        // the target's active page instead of splitting one member tile.
        if (targetContainer->activePageId().isEmpty()) {
            return rejected(QStringLiteral("cross-container target has no active page"));
        }
        return execute(Hybrid::MoveMember{
            .sourceContainerId = sourceContainerId,
            .targetContainerId = targetContainerId,
            .windowId = intent.source.memberId,
            .destination = Hybrid::MoveAsRootSplit{
                .pageId = targetContainer->activePageId(),
                .splitNodeId = structuralId(QLatin1StringView("split")),
                .orientation = split->orientation,
                .ratio = 0.5,
                .position = split->position,
            },
        });
    }
    return execute(Hybrid::MoveMember{
        .sourceContainerId = sourceContainerId,
        .targetContainerId = targetContainerId,
        .windowId = intent.source.memberId,
        .destination = Hybrid::MoveAsSplit{
            .targetWindowId = intent.target.memberId,
            .splitNodeId = structuralId(QLatin1StringView("split")),
            .orientation = split->orientation,
            .ratio = 0.5,
            .position = split->position,
        },
    });
}

HybridRuntimeResult HybridInteractionRuntime::commitWithinContainer(
    const HybridInput::InteractionIntent &intent, const QString &containerId)
{
    if (intent.target.zone != HybridInput::DockZone::Tab) {
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            return rejected(QStringLiteral("tabs can only move to a tab destination"));
        }
        const auto split = splitPlacement(intent.target.zone);
        if (!split) {
            return rejected(QStringLiteral("dock zone cannot create a split"));
        }
        if (intent.target.memberId.isEmpty()) {
            // Container-level edge drop inside the member's own container:
            // the member leaves its tile and takes a whole side of the active
            // page beside everything that remains.
            const auto *container = topology().container(containerId);
            if (!container || container->activePageId().isEmpty()) {
                return rejected(QStringLiteral("container target has no active page"));
            }
            return execute(Hybrid::ReparentMemberToPageRoot{
                .containerId = containerId,
                .windowId = intent.source.memberId,
                .pageId = container->activePageId(),
                .splitNodeId = structuralId(QLatin1StringView("split")),
                .orientation = split->orientation,
                .ratio = 0.5,
                .position = split->position,
            });
        }
        return execute(Hybrid::ReparentMember{
            .containerId = containerId,
            .windowId = intent.source.memberId,
            .targetWindowId = intent.target.memberId,
            .splitNodeId = structuralId(QLatin1StringView("split")),
            .orientation = split->orientation,
            .ratio = 0.5,
            .position = split->position,
        });
    }
    if (intent.target.memberId.isEmpty()) {
        return noChange(QStringLiteral("container target has no member anchor"));
    }

    const auto sourcePage = pageLocation(containerId, intent.source.memberId);
    const auto targetPage = pageLocation(containerId, intent.target.memberId);
    if (!sourcePage || !targetPage) {
        return rejected(QStringLiteral("same-container page anchor is missing"));
    }
    if (intent.source.kind == HybridInput::HitKind::Tab
        && (intent.source.pageId.isEmpty()
            || intent.source.pageId != sourcePage->pageId)) {
        return rejected(QStringLiteral("dragged tab page identity is stale"));
    }
    if (sourcePage->pageId == targetPage->pageId) {
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            return noChange(QStringLiteral("tab already occupies the target page"));
        }
        return execute(Hybrid::ReorderMembers{containerId,
                                              intent.source.memberId,
                                              intent.target.memberId});
    }

    if (intent.source.kind != HybridInput::HitKind::Tab) {
        return execute(Hybrid::MoveMemberToPage{
            .containerId = containerId,
            .windowId = intent.source.memberId,
            .newPageId = structuralId(QLatin1StringView("page")),
            .targetPageId = targetPage->pageId,
        });
    }

    const auto *container = topology().container(containerId);
    const qsizetype lastIndex = container->pages().size() - 1;
    const qsizetype afterTarget = sourcePage->index < targetPage->index
        ? targetPage->index
        : targetPage->index + 1;
    const qsizetype destination = std::min(afterTarget, lastIndex);
    if (destination == sourcePage->index) {
        return noChange(QStringLiteral("source page is already after target page"));
    }
    return execute(Hybrid::ReorderPage{containerId, sourcePage->pageId, destination});
}

HybridInteractionRuntime::Placement HybridInteractionRuntime::placement(
    const QString &windowId) const
{
    if (topology().isIndependent(windowId)) {
        return {true, {}};
    }
    const auto owner = topology().ownerOf(windowId);
    return owner ? Placement{true, *owner} : Placement{};
}

std::optional<HybridInteractionRuntime::PageLocation>
HybridInteractionRuntime::pageLocation(const QString &containerId,
                                       const QString &windowId) const
{
    const auto *container = topology().container(containerId);
    if (!container) {
        return std::nullopt;
    }
    for (qsizetype index = 0; index < container->pages().size(); ++index) {
        const auto &page = container->pages()[index];
        if (page.root().findWindow(windowId)) {
            return PageLocation{page.id(), index};
        }
    }
    return std::nullopt;
}

} // namespace QindaQt::Compositor::KWinIntegration
