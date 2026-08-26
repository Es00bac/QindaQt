// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene.h"

#include <QScopeGuard>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

Hybrid::SceneStepResult failure(QString message)
{
    return Hybrid::SceneStepResult::failure(std::move(message));
}

QStringList groupedWindowIds(const Hybrid::WindowTopology &topology)
{
    QStringList result;
    for (const auto &containerId : topology.containerIds()) {
        result.append(topology.windowIds(containerId));
    }
    result.removeDuplicates();
    result.sort();
    return result;
}

} // namespace

Hybrid::SceneStepResult KWinHybridSceneFactory::emergencyReleaseAll(
    const Hybrid::WindowTopology &topology)
{
    const auto validation = topology.validate();
    if (!validation.valid) {
        return failure(QStringLiteral("invalid emergency topology: %1")
                           .arg(validation.message));
    }

    const auto windowIds = groupedWindowIds(topology);
    QHash<QString, HybridConstraints::WindowRestoreState> restorePlan;
    QHash<QString, QString> expectedOwners;
    QHash<QString, QString> independentOwners;
    QHash<QString, QRectF> targetFrames;
    QSet<QString> allowedMissing;
    QString error;

    for (const auto &windowId : windowIds) {
        const auto restore = m_restoreStates.constFind(windowId);
        if (!m_platform->windowExists(windowId)) {
            allowedMissing.insert(windowId);
            expectedOwners.insert(windowId, m_platform->owner(windowId));
            independentOwners.insert(windowId, {});
            continue;
        }
        if (restore == m_restoreStates.cend()) {
            return failure(QStringLiteral(
                               "live grouped window '%1' has no emergency restore state")
                               .arg(windowId));
        }
        if (!m_platform->validateState(windowId, restore.value(), &error)) {
            return failure(error.isEmpty()
                               ? QStringLiteral("invalid emergency restore for '%1'")
                                     .arg(windowId)
                               : std::move(error));
        }
        restorePlan.insert(windowId, restore.value());
        expectedOwners.insert(windowId, m_platform->owner(windowId));
        independentOwners.insert(windowId, {});
        targetFrames.insert(windowId, restore->geometry);
    }

    const QString originalActiveWindow = m_platform->activeWindowId();
    QString focusTarget;
    if (const auto restore = restorePlan.constFind(originalActiveWindow);
        restore != restorePlan.cend() && !restore->minimized) {
        focusTarget = originalActiveWindow;
    } else {
        for (const auto &windowId : windowIds) {
            const auto candidate = restorePlan.constFind(windowId);
            if (candidate != restorePlan.cend() && candidate->focused
                && !candidate->minimized) {
                focusTarget = windowId;
                break;
            }
        }
    }

    QStringList stateFailures;
    ++m_windowStateMutationDepth;
    const auto mutationGuard = qScopeGuard([this] {
        --m_windowStateMutationDepth;
    });
    for (const auto &windowId : windowIds) {
        const auto restoreState = restorePlan.constFind(windowId);
        if (restoreState == restorePlan.cend()) {
            continue;
        }
        error.clear();
        if (m_platform->applyState(windowId, restoreState.value(), &error)) {
            continue;
        }
        if (!m_platform->windowExists(windowId)) {
            allowedMissing.insert(windowId);
            continue;
        }
        stateFailures.append(error.isEmpty()
                                 ? QStringLiteral("could not restore '%1'").arg(windowId)
                                 : error);
    }

    if (originalActiveWindow.isEmpty()
        || topology.ownerOf(originalActiveWindow).has_value()) {
        error.clear();
        if (!m_platform->restoreFocus(focusTarget, &error)) {
            stateFailures.append(error.isEmpty()
                                     ? QStringLiteral("could not restore emergency focus")
                                     : error);
        }
    }

    error.clear();
    if (!m_platform->finalizeOwners(expectedOwners, independentOwners,
                                    targetFrames, allowedMissing, &error)) {
        return failure(error.isEmpty()
                           ? QStringLiteral("emergency owner cleanup failed")
                           : std::move(error));
    }

    // Ownership is gone, so retaining these maps would falsely imply that a
    // later retry can still address grouped clients. Clear them even when a
    // best-effort state operation reported an error, and return that error
    // honestly to the shutdown coordinator.
    m_restoreStates.clear();
    m_committedLayouts.clear();
    if (!stateFailures.isEmpty()) {
        return failure(stateFailures.join(QStringLiteral("; ")));
    }
    return Hybrid::SceneStepResult::ready();
}

} // namespace QindaQt::Compositor::KWinIntegration
