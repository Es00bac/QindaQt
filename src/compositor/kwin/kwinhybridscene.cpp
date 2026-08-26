// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene.h"

#include "hybridtaskidentitypolicy.h"

#include "qindaqt/hybrid_constraints/constraint_solver.h"

#include <QMargins>
#include <QScopeGuard>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using HybridConstraints::ConstraintSolver;
using HybridConstraints::LayoutMetrics;
using HybridConstraints::MemberSizeConstraints;
using HybridConstraints::WindowRestoreState;

bool fail(QString *error, QString message)
{
    if (error)
        *error = std::move(message);
    return false;
}

QStringList nodeWindows(const Core::LayoutNode &node)
{
    if (node.isLeaf())
        return {node.windowId()};
    auto result = nodeWindows(*node.firstChild());
    result.append(nodeWindows(*node.secondChild()));
    return result;
}

QStringList activeWindows(const Core::WindowContainer &container)
{ const auto *page = container.page(container.activePageId()); return page ? nodeWindows(page->root()) : QStringList{}; }

QHash<QString, QString> ownerPlan(const Hybrid::WindowTopology &topology)
{
    QHash<QString, QString> result;
    for (const auto &id : topology.independentWindowIds())
        result.insert(id, {});
    for (const auto &containerId : topology.containerIds())
        for (const auto &id : topology.windowIds(containerId))
            result.insert(id, containerId);
    return result;
}

QString forgottenWindow(const Hybrid::TopologyCommand &command)
{ const auto *forget = std::get_if<Hybrid::ForgetWindow>(&command); return forget ? forget->windowId : QString{}; }

QString commandAnchor(const Hybrid::TopologyCommand &command, const QString &containerId) {
    if (const auto *value = std::get_if<Hybrid::DockIndependentWindows>(&command);
        value && value->containerId == containerId)
        return value->firstWindowId;
    if (const auto *value = std::get_if<Hybrid::GroupIndependentWindowsAsPages>(&command);
        value && value->containerId == containerId)
        return value->firstWindowId;
    if (const auto *value = std::get_if<Hybrid::RegroupMemberWithIndependent>(&command);
        value && value->newContainerId == containerId)
        return value->independentWindowId;
    return {};
}

QRect expandedContentFrame(const QRect &content, const QMargins &insets) { return content.adjusted(-insets.left(), -insets.top(), insets.right(), insets.bottom()); }

struct WindowChange final { QString id; WindowRestoreState before; WindowRestoreState after; };

bool preservesExternalOrIndependentFocus(
    const QString &activeWindowId,
    const QHash<QString, QString> &candidateOwners,
    const QHash<QString, WindowRestoreState> &current,
    const QHash<QString, WindowRestoreState> &desired)
{
    if (activeWindowId.isEmpty()) {
        return false;
    }
    if (!candidateOwners.contains(activeWindowId)
        && !current.contains(activeWindowId)) {
        // Dialogs, transients, panels, and other KWin windows deliberately
        // absent from Hybrid topology remain valid active windows. A scene
        // transaction must not activate a member behind them.
        return true;
    }
    return candidateOwners.contains(activeWindowId)
        && candidateOwners.value(activeWindowId).isEmpty()
        && current.contains(activeWindowId)
        && !current.value(activeWindowId).minimized
        && !desired.contains(activeWindowId);
}

} // namespace

class KWinHybridSceneTransaction final : public Hybrid::SceneTransaction
{
public:
    explicit KWinHybridSceneTransaction(KWinHybridSceneFactory &factory) : m_factory(&factory) {}

    Hybrid::SceneStepResult prepare(const Hybrid::WindowTopology &before,
                                    const Hybrid::WindowTopology &candidate,
                                    const Hybrid::TopologyCommand &command) override;
    Hybrid::SceneStepResult commit() override;
    void rollback() noexcept override;

private:
    [[nodiscard]] bool planContainers(
        const Hybrid::WindowTopology &before, const Hybrid::WindowTopology &candidate,
        const Hybrid::TopologyCommand &command,
        const QHash<QString, WindowRestoreState> &current,
        QHash<QString, WindowRestoreState> &desired, QSet<QString> &visible, QString *error);
    [[nodiscard]] std::optional<QRect> outerFrame(
        const Hybrid::WindowTopology &before, const Core::WindowContainer &candidateContainer,
        const Hybrid::TopologyCommand &command, QString *error) const;

    KWinHybridSceneFactory *m_factory;
    QVector<WindowChange> m_changes;
    QHash<QString, QString> m_expectedOwners, m_candidateOwners;
    QHash<QString, QRectF> m_targetFrames;
    QSet<QString> m_allowedMissing;
    QHash<QString, WindowRestoreState> m_stagedRestoreStates;
    QHash<QString, CommittedContainerLayout> m_stagedLayouts;
    QString m_originalActiveWindow, m_targetActiveWindow;
    qsizetype m_applied = 0;
    bool m_prepared = false, m_committed = false, m_rolledBack = false;
};

std::optional<QRect> KWinHybridSceneTransaction::outerFrame(
    const Hybrid::WindowTopology &before, const Core::WindowContainer &candidateContainer,
    const Hybrid::TopologyCommand &command, QString *error) const
{
    if (const auto match = m_stagedLayouts.constFind(candidateContainer.id());
        match != m_stagedLayouts.cend()) {
        return match->outerFrame;
    }

    QStringList anchors;
    bool existingContainer = before.container(candidateContainer.id()) != nullptr;
    if (const auto commandAnchorId = commandAnchor(command, candidateContainer.id());
        !commandAnchorId.isEmpty()) {
        anchors = {commandAnchorId};
        existingContainer = false;
    } else if (const auto *old = before.container(candidateContainer.id())) {
        anchors = activeWindows(*old);
    } else {
        anchors = activeWindows(candidateContainer);
    }

    QRectF frame;
    for (const auto &id : anchors) {
        const auto memberFrame = m_factory->m_platform->currentFrame(id, error);
        if (!memberFrame) {
            return std::nullopt;
        }
        frame = frame.isValid() ? frame.united(*memberFrame) : *memberFrame;
    }
    if (!frame.isValid()) {
        fail(error, QStringLiteral("container '%1' has no usable anchor frame")
                        .arg(candidateContainer.id()));
        return std::nullopt;
    }
    auto result = frame.toRect();
    if (existingContainer) {
        result = expandedContentFrame(result, m_factory->m_metrics.contentInsets);
    }
    if (!result.isValid()) {
        fail(error, QStringLiteral("container '%1' outer frame is invalid")
                        .arg(candidateContainer.id()));
        return std::nullopt;
    }
    return result;
}

bool KWinHybridSceneTransaction::planContainers(
    const Hybrid::WindowTopology &before, const Hybrid::WindowTopology &candidate,
    const Hybrid::TopologyCommand &command,
    const QHash<QString, WindowRestoreState> &current,
    QHash<QString, WindowRestoreState> &desired, QSet<QString> &visible, QString *error)
{
    const auto candidateContainers = candidate.containerIds();
    for (const auto &oldId : std::as_const(m_stagedLayouts).keys()) {
        if (!candidateContainers.contains(oldId)) {
            m_stagedLayouts.remove(oldId);
        }
    }

    for (const auto &containerId : candidateContainers) {
        const auto *container = candidate.container(containerId);
        const auto frame = outerFrame(before, *container, command, error);
        if (!frame) {
            return false;
        }

        const auto activeIds = activeWindows(*container);
        if (activeIds.isEmpty()) {
            return fail(error, QStringLiteral("container '%1' has no active members")
                                   .arg(containerId));
        }
        const auto taskIdentity = HybridTaskIdentityPolicy::planContainer(
            *container, m_originalActiveWindow, error);
        if (!taskIdentity) {
            return false;
        }
        auto anchorId = commandAnchor(command, containerId);
        if (anchorId.isEmpty()) {
            const auto *old = before.container(containerId);
            const auto priorIds = old ? activeWindows(*old) : QStringList{};
            for (const auto &priorId : priorIds) {
                if (current.contains(priorId)) {
                    anchorId = priorId;
                    break;
                }
            }
        }
        if (anchorId.isEmpty()) {
            anchorId = activeIds.first();
        }
        const auto anchorState = current.value(anchorId);
        std::optional<HybridConstraints::ConstraintSolution> activeSolution;
        for (const auto &page : container->pages()) {
            QHash<QString, MemberSizeConstraints> constraints;
            for (const auto &id : nodeWindows(page.root())) {
                const auto value = m_factory->m_platform->sizeConstraints(id, error);
                if (!value) {
                    return false;
                }
                constraints.insert(id, *value);
            }
            const auto solution = ConstraintSolver::solve(
                page.root(), *frame, constraints, m_factory->m_metrics, error);
            if (!solution) {
                return false;
            }
            if (page.id() == container->activePageId()) {
                activeSolution = *solution;
            }
            for (auto iterator = solution->members.cbegin();
                 iterator != solution->members.cend(); ++iterator) {
                auto state = current.value(iterator.key());
                state.geometry = iterator->windowFrame;
                state.minimized = page.id() != container->activePageId();
                state.maximizedAxes = {};
                state.quickTileEdges = {};
                state.fullscreen = false;
                state.outputId = anchorState.outputId;
                state.desktopIds = anchorState.desktopIds;
                state.activityIds = anchorState.activityIds;
                state.keepAbove = anchorState.keepAbove;
                state.keepBelow = anchorState.keepBelow;
                state.focused = false;
                const auto *taskMember = taskIdentity->member(iterator.key());
                if (!taskMember) {
                    return fail(error, QStringLiteral(
                                           "container '%1' task identity lost member '%2'")
                                           .arg(containerId, iterator.key()));
                }
                state.skipTaskbar = taskMember->skipTaskbar;
                state.skipSwitcher = taskMember->skipSwitcher;
                desired.insert(iterator.key(), state);
                m_targetFrames.insert(iterator.key(), state.geometry);
                if (!state.minimized) {
                    visible.insert(iterator.key());
                }
            }
        }
        if (!activeSolution) {
            return fail(error, QStringLiteral("container '%1' has no active page solution")
                                   .arg(containerId));
        }
        m_stagedLayouts.insert(
            containerId, CommittedContainerLayout{*frame, std::move(*activeSolution)});
    }
    return true;
}

Hybrid::SceneStepResult KWinHybridSceneTransaction::prepare(
    const Hybrid::WindowTopology &before, const Hybrid::WindowTopology &candidate,
    const Hybrid::TopologyCommand &command)
{
    if (m_prepared) {
        return Hybrid::SceneStepResult::failure(
            QStringLiteral("scene transaction was already prepared"));
    }

    const auto beforeOwners = ownerPlan(before);
    const auto afterOwners = ownerPlan(candidate);
    auto allIds = beforeOwners.keys();
    for (const auto &id : afterOwners.keys()) {
        if (!allIds.contains(id)) {
            allIds.append(id);
        }
    }
    allIds.sort();

    const auto forgotten = forgottenWindow(command);
    QHash<QString, WindowRestoreState> current;
    QString error;
    for (const auto &id : allIds) {
        m_expectedOwners.insert(id, beforeOwners.value(id));
        m_candidateOwners.insert(id, afterOwners.value(id));
        if (!m_factory->m_platform->windowExists(id)) {
            if (id == forgotten && !afterOwners.contains(id)) {
                m_allowedMissing.insert(id);
                continue;
            }
            return Hybrid::SceneStepResult::failure(
                QStringLiteral("window '%1' closed before scene preparation").arg(id));
        }
        const auto state = m_factory->m_platform->captureState(id, &error);
        if (!state) {
            return Hybrid::SceneStepResult::failure(std::move(error));
        }
        current.insert(id, *state);
    }

    m_originalActiveWindow = m_factory->m_platform->activeWindowId();
    m_stagedRestoreStates = m_factory->m_restoreStates;
    m_stagedLayouts = m_factory->m_committedLayouts;

    for (const auto &id : allIds) {
        const auto beforeOwner = beforeOwners.value(id);
        const auto afterOwner = afterOwners.value(id);
        if (beforeOwner.isEmpty() && !afterOwner.isEmpty()) {
            m_stagedRestoreStates.insert(id, current.value(id));
        } else if (!beforeOwner.isEmpty() && !afterOwner.isEmpty()
                   && !m_stagedRestoreStates.contains(id)) {
            return Hybrid::SceneStepResult::failure(
                QStringLiteral("window '%1' has no independent restore state").arg(id));
        }
    }

    QHash<QString, WindowRestoreState> desired;
    QSet<QString> visible;
    if (!planContainers(before, candidate, command, current, desired, visible, &error)) {
        return Hybrid::SceneStepResult::failure(std::move(error));
    }

    for (const auto &id : allIds) {
        const auto beforeOwner = beforeOwners.value(id);
        const auto afterOwner = afterOwners.value(id);
        if (!beforeOwner.isEmpty() && afterOwner.isEmpty()) {
            if (current.contains(id)) {
                const auto restore = m_stagedRestoreStates.constFind(id);
                if (restore == m_stagedRestoreStates.cend()) {
                    return Hybrid::SceneStepResult::failure(
                        QStringLiteral("window '%1' has no independent restore state").arg(id));
                }
                desired.insert(id, restore.value());
            }
            m_stagedRestoreStates.remove(id);
        } else if (!afterOwners.contains(id)) {
            m_stagedRestoreStates.remove(id);
        }
    }

    if (preservesExternalOrIndependentFocus(
            m_originalActiveWindow, m_candidateOwners, current, desired)) {
        // AGENT-GUARD: An independent or non-topology active window is outside
        // this scene mutation. Do not manufacture a group focus target:
        // Add/Forget and releaseAll routinely re-plan other groups and must
        // remain focus-transparent to the untouched KWin window.
        m_targetActiveWindow.clear();
    } else if (visible.contains(m_originalActiveWindow)) {
        m_targetActiveWindow = m_originalActiveWindow;
    } else if (desired.contains(m_originalActiveWindow)
               && !desired.value(m_originalActiveWindow).minimized) {
        m_targetActiveWindow = m_originalActiveWindow;
    } else {
        for (const auto &id : allIds) {
            if (desired.value(id).focused || visible.contains(id)) {
                m_targetActiveWindow = id;
                break;
            }
        }
    }
    for (auto iterator = desired.begin(); iterator != desired.end(); ++iterator) {
        iterator->focused = iterator.key() == m_targetActiveWindow;
        if (!m_factory->m_platform->validateState(iterator.key(), iterator.value(), &error)) {
            return Hybrid::SceneStepResult::failure(std::move(error));
        }
        if (current.value(iterator.key()) != iterator.value()) {
            m_changes.append({iterator.key(), current.value(iterator.key()), iterator.value()});
        }
    }
    std::sort(m_changes.begin(), m_changes.end(), [](const WindowChange &first,
                                                     const WindowChange &second) {
        return first.id < second.id;
    });
    m_prepared = true;
    return Hybrid::SceneStepResult::ready();
}

Hybrid::SceneStepResult KWinHybridSceneTransaction::commit()
{
    if (!m_prepared || m_committed || m_rolledBack) {
        return Hybrid::SceneStepResult::failure(
            QStringLiteral("scene transaction is not committable"));
    }
    ++m_factory->m_windowStateMutationDepth;
    const auto mutationGuard = qScopeGuard([this] {
        --m_factory->m_windowStateMutationDepth;
    });
    QString error;
    for (const auto &change : std::as_const(m_changes)) {
        if (!m_factory->m_platform->windowExists(change.id)
            || !m_factory->m_platform->applyState(change.id, change.after, &error)) {
            return Hybrid::SceneStepResult::failure(error.isEmpty()
                ? QStringLiteral("window '%1' closed during scene commit").arg(change.id)
                : std::move(error));
        }
        ++m_applied;
    }
    if (!m_targetActiveWindow.isEmpty()
        && !m_factory->m_platform->activateWindow(m_targetActiveWindow, &error)) {
        return Hybrid::SceneStepResult::failure(std::move(error));
    }
    if (!m_factory->m_platform->finalizeOwners(m_expectedOwners, m_candidateOwners,
                                               m_targetFrames, m_allowedMissing, &error)) {
        return Hybrid::SceneStepResult::failure(std::move(error));
    }

    // AGENT-GUARD: Registry finalization is an all-or-nothing validation/edit.
    // Swap already-built maps only afterward; no allocation or fallible work may
    // occur between live ownership and the matching restore/layout state.
    m_factory->m_restoreStates.swap(m_stagedRestoreStates);
    m_factory->m_committedLayouts.swap(m_stagedLayouts);
    m_committed = true;
    return Hybrid::SceneStepResult::ready();
}

void KWinHybridSceneTransaction::rollback() noexcept
{
    if (m_committed || m_rolledBack) {
        return;
    }
    ++m_factory->m_windowStateMutationDepth;
    const auto mutationGuard = qScopeGuard([this] {
        --m_factory->m_windowStateMutationDepth;
    });
    while (m_applied > 0) {
        const auto &change = m_changes[--m_applied];
        if (!m_factory->m_platform->windowExists(change.id)) {
            continue;
        }
        QString ignored;
        m_factory->m_platform->applyState(change.id, change.before, &ignored);
    }
    if (m_prepared) {
        // AGENT-GUARD: An empty original ID is observable state, not an absent
        // rollback request. Conversely, failed prepare has not captured focus
        // or mutated the scene, so clearing focus there would create damage
        // while handling an otherwise side-effect-free validation failure.
        QString ignored;
        m_factory->m_platform->restoreFocus(m_originalActiveWindow, &ignored);
    }
    m_rolledBack = true;
}

KWinHybridSceneFactory::KWinHybridSceneFactory(
    ManagedWindowRegistry &registry, LayoutMetrics metrics)
    : m_ownedPlatform(makeRegistryHybridScenePlatform(registry))
    , m_platform(m_ownedPlatform.get())
    , m_metrics(std::move(metrics))
{
}

KWinHybridSceneFactory::KWinHybridSceneFactory(
    KWinHybridScenePlatform &platform, LayoutMetrics metrics)
    : m_platform(&platform)
    , m_metrics(std::move(metrics))
{
}

KWinHybridSceneFactory::~KWinHybridSceneFactory() = default;

std::unique_ptr<Hybrid::SceneTransaction> KWinHybridSceneFactory::create() { return std::make_unique<KWinHybridSceneTransaction>(*this); }

} // namespace QindaQt::Compositor::KWinIntegration
