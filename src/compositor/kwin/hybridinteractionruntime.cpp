// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

Hybrid::WindowTopology initialTopology(QStringList windowIds, QString *error)
{
    auto topology = Hybrid::WindowTopology::create(std::move(windowIds), {}, 0, error);
    return topology ? std::move(*topology) : Hybrid::WindowTopology{};
}

HybridRuntimeResult simpleResult(HybridRuntimeStatus status, QString message = {})
{
    return {.status = status, .topologyResult = std::nullopt, .message = std::move(message)};
}

HybridRuntimeResult rejected(QString message)
{
    return simpleResult(HybridRuntimeStatus::Rejected, std::move(message));
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

HybridInteractionRuntime::HybridInteractionRuntime(
    QStringList initialIndependentWindows,
    Hybrid::SceneTransactionFactory &sceneFactory,
    HybridRuntimeCallbacks callbacks)
    : m_initializationError()
    , m_repository(initialTopology(std::move(initialIndependentWindows),
                                   &m_initializationError))
    , m_coordinator(m_repository, sceneFactory)
    , m_callbacks(std::move(callbacks))
{
}

HybridRuntimeResult HybridInteractionRuntime::addWindow(const QString &windowId)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    return execute(Hybrid::AddIndependentWindow{windowId});
}

HybridRuntimeResult HybridInteractionRuntime::forgetWindow(const QString &windowId)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    return execute(Hybrid::ForgetWindow{windowId});
}

HybridRuntimeResult HybridInteractionRuntime::activatePage(const QString &containerId,
                                                           const QString &pageId)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    return execute(Hybrid::ActivatePage{containerId, pageId});
}

HybridRuntimeResult HybridInteractionRuntime::reorderPage(
    const QString &containerId,
    const QString &pageId,
    qsizetype destinationPageIndex)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    // AGENT-CONTRACT: Semantic keyboard and accessibility requests cross the
    // same coordinator transaction as pointer tab reordering. Never mutate a
    // WindowContainer directly or rollback would diverge from scene state.
    return execute(Hybrid::ReorderPage{
        containerId, pageId, destinationPageIndex});
}

HybridRuntimeResult HybridInteractionRuntime::resizeSplit(const QString &containerId,
                                                          const QString &splitNodeId,
                                                          double ratio)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    return execute(Hybrid::ResizeSplit{containerId, splitNodeId, ratio});
}

HybridRuntimeResult HybridInteractionRuntime::handleIntent(
    const HybridInput::InteractionIntent &intent)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    switch (intent.kind) {
    case HybridInput::InteractionKind::ContainerMove:
        return delegate(intent, m_callbacks.containerMove, QStringLiteral("container move"));
    case HybridInput::InteractionKind::ContainerResize:
        return delegate(intent, m_callbacks.containerResize,
                        QStringLiteral("container resize"));
    case HybridInput::InteractionKind::DividerResize:
        if (!m_callbacks.dividerResize) {
            return simpleResult(
                HybridRuntimeStatus::NeedsGeometry,
                QStringLiteral("divider resize requires an adapter-computed ratio"));
        }
        if (intent.phase != HybridInput::IntentPhase::Commit) {
            static_cast<void>(m_callbacks.dividerResize(intent));
            return simpleResult(HybridRuntimeStatus::PreviewOnly);
        }
        if (intent.source.containerId.isEmpty() || intent.source.dividerId.isEmpty()) {
            return rejected(QStringLiteral("divider resize source is invalid"));
        }
        if (const auto geometry = m_callbacks.dividerResize(intent); geometry.ratio) {
            return resizeSplit(intent.source.containerId,
                               intent.source.dividerId,
                               *geometry.ratio);
        } else {
            return simpleResult(HybridRuntimeStatus::NeedsGeometry,
                                geometry.message.isEmpty()
                                    ? QStringLiteral("divider ratio is unavailable")
                                    : geometry.message);
        }
    case HybridInput::InteractionKind::MemberDock:
        if (intent.phase != HybridInput::IntentPhase::Commit) {
            if (m_callbacks.preview) {
                m_callbacks.preview(intent);
            }
            return simpleResult(HybridRuntimeStatus::PreviewOnly);
        }
        return commitMemberDock(intent);
    case HybridInput::InteractionKind::None:
        return rejected(QStringLiteral("interaction kind is missing"));
    }
    return rejected(QStringLiteral("interaction kind is unknown"));
}

ReleaseAllResult HybridInteractionRuntime::releaseAll()
{
    ReleaseAllResult result;
    if (!ready()) {
        result.complete = false;
        result.message = m_initializationError;
        return result;
    }

    QStringList failures;
    const QStringList containerIds = topology().containerIds();
    for (const QString &containerId : containerIds) {
        auto commandResult = m_coordinator.execute(Hybrid::ReleaseContainer{containerId});
        if (!commandResult.committed()) {
            failures.append(containerId);
        }
        result.commands.append(std::move(commandResult));
    }
    result.complete = failures.isEmpty();
    if (!result.complete) {
        result.message = QStringLiteral("failed to release containers: %1")
                             .arg(failures.join(QStringLiteral(", ")));
    }
    return result;
}

HybridRuntimeResult HybridInteractionRuntime::releaseContainer(
    const QString &containerId)
{
    if (!ready()) {
        return rejected(m_initializationError);
    }
    return execute(Hybrid::ReleaseContainer{containerId});
}

HybridRuntimeResult HybridInteractionRuntime::commitMemberDock(
    const HybridInput::InteractionIntent &intent)
{
    if ((intent.source.kind != HybridInput::HitKind::MemberTitle
        && intent.source.kind != HybridInput::HitKind::Tab)
        || intent.source.memberId.isEmpty()) {
        return rejected(QStringLiteral("member dock source is invalid"));
    }

    const Placement sourcePlacement = placement(intent.source.memberId);
    if (!sourcePlacement.known) {
        return rejected(QStringLiteral("source window is not in topology"));
    }
    if (intent.source.containerId != sourcePlacement.containerId) {
        return rejected(QStringLiteral("source ownership is stale"));
    }

    if (!intent.target.isValid()) {
        if (sourcePlacement.containerId.isEmpty()) {
            return simpleResult(HybridRuntimeStatus::NoChange,
                                QStringLiteral("independent source has nothing to detach"));
        }
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            const auto sourcePage = pageLocation(sourcePlacement.containerId,
                                                 intent.source.memberId);
            if (!sourcePage || intent.source.pageId.isEmpty()
                || sourcePage->pageId != intent.source.pageId) {
                return rejected(QStringLiteral("dragged tab page identity is stale"));
            }
            return execute(Hybrid::DetachPage{
                .sourceContainerId = sourcePlacement.containerId,
                .pageId = intent.source.pageId,
                .newContainerId = structuralId(QLatin1StringView("container")),
            });
        }
        return execute(Hybrid::DetachMember{sourcePlacement.containerId,
                                            intent.source.memberId});
    }
    if (intent.target.memberId == intent.source.memberId) {
        return simpleResult(HybridRuntimeStatus::NoChange,
                            QStringLiteral("source already occupies the target"));
    }

    Placement targetPlacement;
    if (!intent.target.memberId.isEmpty()) {
        targetPlacement = placement(intent.target.memberId);
        if (!targetPlacement.known) {
            return rejected(QStringLiteral("target window is not in topology"));
        }
        if (intent.target.containerId != targetPlacement.containerId) {
            return rejected(QStringLiteral("target ownership is stale"));
        }
    } else {
        targetPlacement.containerId = intent.target.containerId;
        targetPlacement.known = topology().container(intent.target.containerId) != nullptr;
        if (!targetPlacement.known) {
            return rejected(QStringLiteral("target container is not in topology"));
        }
    }

    if (sourcePlacement.containerId.isEmpty()) {
        return commitIndependentDock(intent, targetPlacement);
    }
    return commitGroupedDock(intent, sourcePlacement.containerId, targetPlacement);
}

HybridRuntimeResult HybridInteractionRuntime::commitIndependentDock(
    const HybridInput::InteractionIntent &intent, const Placement &targetPlacement)
{
    if (!targetPlacement.containerId.isEmpty()) {
        const auto targetPage = intent.target.memberId.isEmpty()
            ? std::optional<PageLocation>{}
            : pageLocation(targetPlacement.containerId, intent.target.memberId);
        const auto *target = topology().container(targetPlacement.containerId);
        if (!target || (!intent.target.memberId.isEmpty() && !targetPage)) {
            return rejected(QStringLiteral("target page anchor is missing"));
        }
        if (intent.target.zone == HybridInput::DockZone::Tab) {
            const qsizetype insertion = targetPage
                ? targetPage->index + 1
                : target->pages().size();
            return execute(Hybrid::InsertIndependentWindow{
                .targetContainerId = targetPlacement.containerId,
                .windowId = intent.source.memberId,
                .leafNodeId = structuralId(QLatin1StringView("source-leaf")),
                .destination = Hybrid::MoveAsPage{
                    structuralId(QLatin1StringView("page")), insertion},
            });
        }
        if (intent.target.memberId.isEmpty()) {
            return rejected(QStringLiteral("split target member is missing"));
        }
        const auto split = splitPlacement(intent.target.zone);
        if (!split) {
            return rejected(QStringLiteral("dock zone cannot create a split"));
        }
        return execute(Hybrid::InsertIndependentWindow{
            .targetContainerId = targetPlacement.containerId,
            .windowId = intent.source.memberId,
            .leafNodeId = structuralId(QLatin1StringView("source-leaf")),
            .destination = Hybrid::MoveAsSplit{
                .targetWindowId = intent.target.memberId,
                .splitNodeId = structuralId(QLatin1StringView("split")),
                .orientation = split->orientation,
                .ratio = 0.5,
                .position = split->position,
            },
        });
    }
    if (intent.target.memberId.isEmpty()) {
        return rejected(QStringLiteral("independent dock target is missing"));
    }
    if (intent.target.zone == HybridInput::DockZone::Tab) {
        return execute(Hybrid::GroupIndependentWindowsAsPages{
            .containerId = structuralId(QLatin1StringView("container")),
            .firstWindowId = intent.target.memberId,
            .firstPageId = structuralId(QLatin1StringView("target-page")),
            .firstLeafNodeId = structuralId(QLatin1StringView("target-leaf")),
            .secondWindowId = intent.source.memberId,
            .secondPageId = structuralId(QLatin1StringView("source-page")),
            .secondLeafNodeId = structuralId(QLatin1StringView("source-leaf")),
        });
    }
    const auto split = splitPlacement(intent.target.zone);
    if (!split) {
        return rejected(QStringLiteral("dock zone cannot create a split"));
    }
    return execute(Hybrid::DockIndependentWindows{
        .containerId = structuralId(QLatin1StringView("container")),
        .pageId = structuralId(QLatin1StringView("page")),
        .firstWindowId = intent.target.memberId,
        .firstLeafNodeId = structuralId(QLatin1StringView("target-leaf")),
        .secondWindowId = intent.source.memberId,
        .secondLeafNodeId = structuralId(QLatin1StringView("source-leaf")),
        .splitNodeId = structuralId(QLatin1StringView("split")),
        .orientation = split->orientation,
        .ratio = 0.5,
        .secondPosition = split->position,
    });
}

HybridRuntimeResult HybridInteractionRuntime::commitGroupedDock(
    const HybridInput::InteractionIntent &intent,
    const QString &sourceContainerId,
    const Placement &targetPlacement)
{
    if (targetPlacement.containerId.isEmpty()) {
        if (intent.target.memberId.isEmpty()) {
            return rejected(QStringLiteral("independent target is missing"));
        }
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            if (intent.target.zone != HybridInput::DockZone::Tab) {
                return rejected(QStringLiteral("tabs can only move to a tab destination"));
            }
            const auto sourcePage = pageLocation(sourceContainerId,
                                                 intent.source.memberId);
            if (!sourcePage || intent.source.pageId.isEmpty()
                || sourcePage->pageId != intent.source.pageId) {
                return rejected(QStringLiteral("dragged tab page identity is stale"));
            }
            return execute(Hybrid::RegroupPageWithIndependent{
                .sourceContainerId = sourceContainerId,
                .pageId = intent.source.pageId,
                .independentWindowId = intent.target.memberId,
                .newContainerId = structuralId(QLatin1StringView("container")),
                .independentPageId = structuralId(
                    QLatin1StringView("target-page")),
                .independentLeafNodeId = structuralId(
                    QLatin1StringView("target-leaf")),
            });
        }
        Hybrid::RegroupLayout layout;
        if (intent.target.zone == HybridInput::DockZone::Tab) {
            layout = Hybrid::RegroupAsPages{
                structuralId(QLatin1StringView("target-page")),
                structuralId(QLatin1StringView("source-page")),
                structuralId(QLatin1StringView("target-leaf")),
            };
        } else if (const auto split = splitPlacement(intent.target.zone)) {
            layout = Hybrid::RegroupAsSplit{
                .pageId = structuralId(QLatin1StringView("page")),
                .independentLeafNodeId = structuralId(QLatin1StringView("target-leaf")),
                .splitNodeId = structuralId(QLatin1StringView("split")),
                .orientation = split->orientation,
                .ratio = 0.5,
                .memberPosition = split->position,
            };
        } else {
            return rejected(QStringLiteral("dock zone cannot form a group"));
        }
        return execute(Hybrid::RegroupMemberWithIndependent{
            .sourceContainerId = sourceContainerId,
            .memberWindowId = intent.source.memberId,
            .independentWindowId = intent.target.memberId,
            .newContainerId = structuralId(QLatin1StringView("container")),
            .layout = std::move(layout),
        });
    }
    if (sourceContainerId == targetPlacement.containerId) {
        return commitWithinContainer(intent, sourceContainerId);
    }
    return commitAcrossContainers(intent,
                                  sourceContainerId,
                                  targetPlacement.containerId);
}

HybridRuntimeResult HybridInteractionRuntime::commitWithinContainer(
    const HybridInput::InteractionIntent &intent, const QString &containerId)
{
    if (intent.target.memberId.isEmpty()) {
        return simpleResult(HybridRuntimeStatus::NoChange,
                            QStringLiteral("container target has no member anchor"));
    }
    if (intent.target.zone != HybridInput::DockZone::Tab) {
        if (intent.source.kind == HybridInput::HitKind::Tab) {
            return rejected(QStringLiteral("tabs can only move to a tab destination"));
        }
        const auto split = splitPlacement(intent.target.zone);
        if (!split) {
            return rejected(QStringLiteral("dock zone cannot create a split"));
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
            return simpleResult(HybridRuntimeStatus::NoChange,
                                QStringLiteral("tab already occupies the target page"));
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
        return simpleResult(HybridRuntimeStatus::NoChange,
                            QStringLiteral("source page is already after target page"));
    }
    return execute(Hybrid::ReorderPage{containerId, sourcePage->pageId, destination});
}

HybridRuntimeResult HybridInteractionRuntime::execute(Hybrid::TopologyCommand command)
{
    auto result = m_coordinator.execute(command);
    HybridRuntimeResult runtimeResult;
    runtimeResult.status = result.committed()
        ? HybridRuntimeStatus::TopologyCommitted
        : HybridRuntimeStatus::Rejected;
    runtimeResult.message = result.message;
    runtimeResult.topologyResult = std::move(result);
    return runtimeResult;
}

HybridRuntimeResult HybridInteractionRuntime::delegate(
    const HybridInput::InteractionIntent &intent,
    const DirectInteractionHandler &handler,
    QString operationName)
{
    if (!handler) {
        return simpleResult(HybridRuntimeStatus::Unsupported,
                            operationName + QStringLiteral(" has no installed handler"));
    }
    const auto result = handler(intent);
    return simpleResult(result.accepted ? HybridRuntimeStatus::Delegated
                                        : HybridRuntimeStatus::Rejected,
                        result.message);
}

QString HybridInteractionRuntime::structuralId(QLatin1StringView role) const
{
    // The next published revision is unique for this runtime. Failed scene
    // transactions retain the revision, so retries reproduce the same IDs.
    const quint64 revision = topology().revision();
    const quint64 nextRevision = revision == std::numeric_limits<quint64>::max()
        ? revision
        : revision + 1;
    return QStringLiteral("hybrid-r%1-%2")
        .arg(nextRevision)
        .arg(role);
}

} // namespace QindaQt::Compositor::KWinIntegration
