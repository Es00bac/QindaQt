// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene.h"

#include "qindaqt/hybrid_constraints/constraint_solver.h"

#include <QScopeGuard>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using HybridConstraints::ConstraintSolver;
using HybridConstraints::MemberSizeConstraints;
using HybridConstraints::WindowRestoreState;

struct ReflowChange final
{
    QString windowId;
    WindowRestoreState before;
    WindowRestoreState after;
};

QStringList nodeWindowIds(const Core::LayoutNode &node)
{
    if (node.isLeaf()) {
        return {node.windowId()};
    }
    auto result = nodeWindowIds(*node.firstChild());
    result.append(nodeWindowIds(*node.secondChild()));
    return result;
}

QStringList containerWindowIds(const Core::WindowContainer &container)
{
    QStringList result;
    for (const auto &page : container.pages()) {
        result.append(nodeWindowIds(page.root()));
    }
    result.sort();
    return result;
}

HybridGroupContext groupContext(const WindowRestoreState &state)
{
    return {
        .outputId = state.outputId,
        .desktopIds = state.desktopIds,
        .activityIds = state.activityIds,
        .keepAbove = state.keepAbove,
        .keepBelow = state.keepBelow,
    };
}

void assignGroupContext(WindowRestoreState *state,
                        const HybridGroupContext &context)
{
    state->outputId = context.outputId;
    state->desktopIds = context.desktopIds;
    state->activityIds = context.activityIds;
    state->keepAbove = context.keepAbove;
    state->keepBelow = context.keepBelow;
}

Hybrid::SceneStepResult failed(QString message)
{
    return Hybrid::SceneStepResult::failure(std::move(message));
}

void rollbackReflow(KWinHybridScenePlatform &platform,
                    const QVector<ReflowChange> &changes,
                    qsizetype applied,
                    const QString &originalActiveWindow) noexcept
{
    while (applied > 0) {
        const auto &change = changes[--applied];
        if (!platform.windowExists(change.windowId)) {
            continue;
        }
        QString ignored;
        platform.applyState(change.windowId, change.before, &ignored);
    }
    QString ignored;
    platform.restoreFocus(originalActiveWindow, &ignored);
}

} // namespace

std::optional<CommittedContainerLayout> KWinHybridSceneFactory::committedLayout(
    const QString &containerId) const
{
    const auto match = m_committedLayouts.constFind(containerId);
    return match == m_committedLayouts.cend()
        ? std::nullopt
        : std::optional<CommittedContainerLayout>(match.value());
}

std::optional<HybridConstraints::OverflowReport> KWinHybridSceneFactory::overflowReport(
    const QString &containerId) const
{
    const auto layout = committedLayout(containerId);
    return layout
        ? std::optional<HybridConstraints::OverflowReport>(layout->activePage.overflow)
        : std::nullopt;
}

Hybrid::SceneStepResult KWinHybridSceneFactory::reflowContainer(
    const Core::WindowContainer &container,
    const QRect &outerFrame)
{
    return reflowContainerWithContext(
        container, outerFrame, std::nullopt, std::nullopt);
}

Hybrid::SceneStepResult KWinHybridSceneFactory::recontextualizeContainer(
    const Core::WindowContainer &container,
    const QString &sourceWindowId)
{
    const auto memberIds = containerWindowIds(container);
    if (!memberIds.contains(sourceWindowId) || memberIds.size() < 2) {
        return failed(QStringLiteral(
            "context source must belong to a live multi-member container"));
    }
    const auto layout = committedLayout(container.id());
    if (!layout) {
        return failed(QStringLiteral("container '%1' has no committed scene layout")
                          .arg(container.id()));
    }

    QString error;
    const auto sourceState = m_platform->captureState(sourceWindowId, &error);
    if (!sourceState) {
        return failed(std::move(error));
    }
    const auto desiredContext = groupContext(*sourceState);
    if (!desiredContext.isValid(&error)) {
        return failed(std::move(error));
    }

    std::optional<HybridGroupContext> rollbackContext;
    for (const auto &windowId : memberIds) {
        if (windowId == sourceWindowId) {
            continue;
        }
        const auto state = m_platform->captureState(windowId, &error);
        if (!state) {
            return failed(std::move(error));
        }
        rollbackContext = groupContext(*state);
        break;
    }
    if (!rollbackContext || !rollbackContext->isValid(&error)) {
        return failed(error.isEmpty()
                          ? QStringLiteral("container has no rollback context")
                          : std::move(error));
    }

    const QString originalActiveWindow = m_platform->activeWindowId();
    QVector<ReflowChange> externalRollback;
    externalRollback.reserve(memberIds.size());
    for (const auto &windowId : memberIds) {
        const auto state = m_platform->captureState(windowId, &error);
        const auto frame = m_platform->currentFrame(windowId, &error);
        if (!state || !frame) {
            return failed(std::move(error));
        }
        auto before = *state;
        assignGroupContext(&before, *rollbackContext);
        before.geometry = *frame;
        externalRollback.append({windowId, std::move(before), {}});
    }
    const auto restoreExternalMutation =
        [this, &externalRollback, &originalActiveWindow](QString message) {
            QStringList recoveryFailures;
            ++m_windowStateMutationDepth;
            const auto mutationGuard = qScopeGuard([this] {
                --m_windowStateMutationDepth;
            });
            // AGENT-GUARD: Try every member even after one restore failure. The
            // triggering source may sort after a failed peer and must never be
            // left alone on its newly requested desktop/output/layer.
            for (const auto &change : std::as_const(externalRollback)) {
                QString recoveryError;
                if (!m_platform->windowExists(change.windowId)
                    || !m_platform->applyState(
                        change.windowId, change.before, &recoveryError)) {
                    recoveryFailures.append(
                        recoveryError.isEmpty() ? change.windowId : recoveryError);
                }
            }
            QString focusError;
            if (!m_platform->restoreFocus(originalActiveWindow, &focusError)) {
                recoveryFailures.append(focusError.isEmpty()
                                            ? QStringLiteral("focus")
                                            : focusError);
            }
            if (!recoveryFailures.isEmpty()) {
                message += QStringLiteral("; context rollback incomplete: %1")
                               .arg(recoveryFailures.join(QStringLiteral(", ")));
            }
            return failed(std::move(message));
        };

    QRect targetFrame = layout->outerFrame;
    if (desiredContext.outputId != rollbackContext->outputId) {
        if (desiredContext.outputId.isEmpty() || rollbackContext->outputId.isEmpty()) {
            return restoreExternalMutation(QStringLiteral(
                "cannot map a group between unidentified outputs"));
        }
        const auto oldArea = m_platform->placementArea(
            sourceWindowId, rollbackContext->outputId, &error);
        const auto newArea = m_platform->placementArea(
            sourceWindowId, desiredContext.outputId, &error);
        if (!oldArea || !newArea) {
            return restoreExternalMutation(std::move(error));
        }
        const auto mapped = mapHybridGroupFrameToArea(
            targetFrame, *oldArea, *newArea, &error);
        if (!mapped) {
            return restoreExternalMutation(std::move(error));
        }
        targetFrame = *mapped;
    }
    const auto result = reflowContainerWithContext(
        container, targetFrame, desiredContext, rollbackContext);
    return result.succeeded
        ? result
        : restoreExternalMutation(result.message);
}

Hybrid::SceneStepResult KWinHybridSceneFactory::reflowContainerWithContext(
    const Core::WindowContainer &container,
    const QRect &outerFrame,
    const std::optional<HybridGroupContext> &context,
    const std::optional<HybridGroupContext> &rollbackContext)
{
    const auto validity = container.validate();
    if (!validity.valid) {
        return failed(QStringLiteral("invalid container snapshot: %1").arg(validity.message));
    }
    if (!outerFrame.isValid()) {
        return failed(QStringLiteral("container outer frame must have positive dimensions"));
    }
    QString error;
    if ((context && !context->isValid(&error))
        || (rollbackContext && !rollbackContext->isValid(&error))) {
        return failed(std::move(error));
    }
    if (!m_committedLayouts.contains(container.id())) {
        return failed(QStringLiteral("container '%1' has no committed scene layout")
                          .arg(container.id()));
    }

    const auto memberIds = containerWindowIds(container);
    QStringList ownedIds;
    for (const auto &windowId : m_platform->windowIds()) {
        if (m_platform->owner(windowId) == container.id()) {
            ownedIds.append(windowId);
        }
    }
    ownedIds.sort();
    if (memberIds != ownedIds) {
        return failed(QStringLiteral("container snapshot does not match live ownership"));
    }

    QHash<QString, WindowRestoreState> current;
    QHash<QString, MemberSizeConstraints> constraints;
    for (const auto &windowId : memberIds) {
        const auto state = m_platform->captureState(windowId, &error);
        if (!state) {
            return failed(std::move(error));
        }
        const auto size = m_platform->sizeConstraints(windowId, &error);
        if (!size) {
            return failed(std::move(error));
        }
        current.insert(windowId, *state);
        constraints.insert(windowId, *size);
    }

    QHash<QString, WindowRestoreState> desired;
    QSet<QString> visible;
    std::optional<HybridConstraints::ConstraintSolution> activeSolution;
    for (const auto &page : container.pages()) {
        const auto solution = ConstraintSolver::solve(
            page.root(), outerFrame, constraints, m_metrics, &error);
        if (!solution) {
            return failed(std::move(error));
        }
        const bool activePage = page.id() == container.activePageId();
        if (activePage) {
            activeSolution = *solution;
        }
        for (auto iterator = solution->members.cbegin();
             iterator != solution->members.cend(); ++iterator) {
            auto state = current.value(iterator.key());
            state.geometry = iterator->windowFrame;
            state.minimized = !activePage;
            state.maximizedAxes = {};
            state.quickTileEdges = {};
            state.fullscreen = false;
            state.focused = false;
            if (context) {
                assignGroupContext(&state, *context);
            }
            desired.insert(iterator.key(), state);
            if (activePage) {
                visible.insert(iterator.key());
            }
        }
    }
    if (!activeSolution) {
        return failed(QStringLiteral("container has no active page solution"));
    }

    const QString originalActiveWindow = m_platform->activeWindowId();
    QString targetActiveWindow;
    if (visible.contains(originalActiveWindow)) {
        targetActiveWindow = originalActiveWindow;
    } else if (current.contains(originalActiveWindow)) {
        auto visibleIds = visible.values();
        visibleIds.sort();
        targetActiveWindow = visibleIds.value(0);
    }

    QVector<ReflowChange> changes;
    QHash<QString, QString> owners;
    QHash<QString, QRectF> targetFrames;
    for (const auto &windowId : memberIds) {
        auto after = desired.value(windowId);
        after.focused = windowId == targetActiveWindow;
        if (!m_platform->validateState(windowId, after, &error)) {
            return failed(std::move(error));
        }
        auto before = current.value(windowId);
        if (rollbackContext) {
            assignGroupContext(&before, *rollbackContext);
            const auto priorFrame = m_platform->currentFrame(windowId, &error);
            if (!priorFrame) {
                return failed(std::move(error));
            }
            before.geometry = *priorFrame;
        }
        if (before != after) {
            changes.append({windowId, std::move(before), after});
        }
        owners.insert(windowId, container.id());
        targetFrames.insert(windowId, after.geometry);
    }

    auto stagedLayouts = m_committedLayouts;
    stagedLayouts.insert(container.id(),
                         CommittedContainerLayout{outerFrame,
                                                  std::move(*activeSolution)});

    qsizetype applied = 0;
    ++m_windowStateMutationDepth;
    const auto mutationGuard = qScopeGuard([this] {
        --m_windowStateMutationDepth;
    });
    for (const auto &change : std::as_const(changes)) {
        if (!m_platform->windowExists(change.windowId)
            || !m_platform->applyState(change.windowId, change.after, &error)) {
            if (error.isEmpty()) {
                error = QStringLiteral("window '%1' closed during container reflow")
                            .arg(change.windowId);
            }
            rollbackReflow(*m_platform, changes, applied,
                           originalActiveWindow);
            return failed(std::move(error));
        }
        ++applied;
    }
    if (!targetActiveWindow.isEmpty()
        && !m_platform->activateWindow(targetActiveWindow, &error)) {
        rollbackReflow(*m_platform, changes, applied, originalActiveWindow);
        return failed(std::move(error));
    }
    if (context) {
        for (const auto &windowId : memberIds) {
            const auto effective = m_platform->captureState(windowId, &error);
            if (!effective || groupContext(*effective) != *context) {
                if (error.isEmpty()) {
                    error = QStringLiteral(
                        "window '%1' rejected the container context").arg(windowId);
                }
                rollbackReflow(*m_platform, changes, applied, originalActiveWindow);
                return failed(std::move(error));
            }
        }
    }
    if (!m_platform->finalizeOwners(owners, owners, targetFrames, {}, &error)) {
        rollbackReflow(*m_platform, changes, applied, originalActiveWindow);
        return failed(std::move(error));
    }

    // AGENT-GUARD: All allocation happened before registry finalization. This
    // noexcept swap is the publication point shared with committedLayout().
    m_committedLayouts.swap(stagedLayouts);
    return Hybrid::SceneStepResult::ready();
}

} // namespace QindaQt::Compositor::KWinIntegration
