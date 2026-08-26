// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridgroupstacking.h"

#include "hybridchromeexposure.h"
#include "kwinchromemanager.h"
#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

#include <QHash>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

void collectMembers(const Core::LayoutNode &node, QStringList *members)
{
    if (node.isLeaf()) {
        members->append(node.windowId());
        return;
    }
    if (node.firstChild()) {
        collectMembers(*node.firstChild(), members);
    }
    if (node.secondChild()) {
        collectMembers(*node.secondChild(), members);
    }
}

QString stackId(const KWin::Window *window)
{
    return window ? window->internalId().toString(QUuid::WithoutBraces)
                  : QString{};
}

bool inputEligibleAt(const KWin::Window *window, const QPointF &position)
{
    // Mirrors InputRedirection::findToplevel's ordinary unlocked-session
    // eligibility. In particular, internal windows and transients are valid
    // occluders even though neither can become a Hybrid topology leaf.
    return window && !window->isDeleted()
        && window->isOnCurrentActivity() && window->isOnCurrentDesktop()
        && !window->isMinimized() && !window->isHidden()
        && !window->isHiddenByShowDesktop() && window->readyForPainting()
        && window->hitTest(position);
}

bool paintableInCurrentContext(const KWin::Window *window)
{
    return window && !window->isDeleted()
        && window->isOnCurrentActivity() && window->isOnCurrentDesktop()
        && !window->isMinimized() && !window->isHidden()
        && !window->isHiddenByShowDesktop() && window->readyForPainting();
}

QMap<QString, HybridGroupStackingInput> stackingGroups(
    const Hybrid::WindowTopology &topology)
{
    QMap<QString, HybridGroupStackingInput> result;
    QHash<QString, QString> activeOwners;
    QSet<QString> groupedMembers;
    for (const auto &containerId : topology.containerIds()) {
        const auto *container = topology.container(containerId);
        const auto *page = container
            ? container->page(container->activePageId()) : nullptr;
        QStringList members;
        if (page) {
            collectMembers(page->root(), &members);
        }
        for (const auto &memberId : members) {
            activeOwners.insert(memberId, containerId);
        }
        const auto allMembers = topology.windowIds(containerId);
        for (const auto &memberId : allMembers) {
            groupedMembers.insert(memberId);
        }
        result.insert(containerId,
                      HybridGroupStackingInput{
                          .activeMembers = std::move(members),
                          .associatedTransients = {},
                          .transientOwnerById = {},
                      });
    }
    for (auto *window : KWin::workspace()->stackingOrder()) {
        const auto windowId = stackId(window);
        if (!window || window->isDeleted() || window->isInternal()
            || groupedMembers.contains(windowId)
            || (!window->isTransient() && !window->isDialog())) {
            continue;
        }
        QSet<const KWin::Window *> visited;
        auto *candidate = window->transientFor();
        QString containerId;
        QString ownerWindowId;
        while (candidate && !visited.contains(candidate)) {
            visited.insert(candidate);
            ownerWindowId = stackId(candidate);
            containerId = activeOwners.value(ownerWindowId);
            if (!containerId.isEmpty()) {
                break;
            }
            candidate = candidate->transientFor();
        }
        if (containerId.isEmpty()) {
            for (auto *main : window->allMainWindows()) {
                ownerWindowId = stackId(main);
                containerId = activeOwners.value(ownerWindowId);
                if (!containerId.isEmpty()) {
                    break;
                }
            }
        }
        if (!containerId.isEmpty()) {
            result[containerId].associatedTransients.append(windowId);
            result[containerId].transientOwnerById.insert(windowId,
                                                          ownerWindowId);
        }
    }
    return result;
}

} // namespace

KWinHybridGroupStacking::KWinHybridGroupStacking(
    ManagedWindowRegistry &registry, KWinChromeManager &chrome)
    : m_registry(registry)
    , m_chrome(chrome)
{
}

bool KWinHybridGroupStacking::synchronize(
    const Hybrid::WindowTopology &topology, QString *error)
{
    if (error) {
        error->clear();
    }
    auto *const workspace = KWin::workspace();
    if (!workspace) {
        return fail(error, QStringLiteral("KWin workspace is unavailable"));
    }
    const auto hideAll = [this, &topology] {
        for (const auto &containerId : topology.containerIds()) {
            m_chrome.setOverlayVisible(containerId, false);
        }
        // AGENT-GUARD: A failed publication invalidates every cached member,
        // transient, and activation identity. A later chrome press must not
        // raise the previous topology while its scene item is hidden.
        clear();
    };

    QStringList stack;
    QHash<QString, KWin::Window *> windows;
    for (auto *window : workspace->stackingOrder()) {
        const auto id = stackId(window);
        if (id.isEmpty() || windows.contains(id)) {
            continue;
        }
        stack.append(id);
        windows.insert(id, window);
    }
    const auto groups = stackingGroups(topology);
    const auto plan = planHybridGroupStacking(stack, groups, error);
    if (!plan) {
        hideAll();
        return false;
    }

    for (const auto &block : plan->blocksBottomToTop) {
        const auto first = m_registry.window(block.membersBottomToTop.constFirst());
        if (!first) {
            hideAll();
            return fail(error, QStringLiteral("group '%1' has a dead stack member")
                                   .arg(block.containerId));
        }
        for (const auto &memberId : block.membersBottomToTop) {
            const auto *member = m_registry.window(memberId);
            if (!member || member->layer() != first->layer()) {
                hideAll();
                return fail(error,
                            QStringLiteral("group '%1' members do not share one KWin layer")
                                .arg(block.containerId));
            }
        }
    }

    {
        KWin::StackingUpdatesBlocker blocker(workspace);
        for (const auto &block : plan->blocksBottomToTop) {
            KWin::Window *reference = nullptr;
            if (!block.transientsBottomToTop.isEmpty()) {
                reference = windows.value(block.transientsBottomToTop.constLast());
                for (qsizetype index = block.transientsBottomToTop.size() - 1;
                     index-- > 0;) {
                    auto *window = windows.value(block.transientsBottomToTop[index]);
                    workspace->stackBelow(window, reference);
                    reference = window;
                }
            }
            auto *topMember = m_registry.window(block.topMemberId());
            if (reference) {
                workspace->stackBelow(topMember, reference);
            }
            reference = topMember;
            for (qsizetype index = block.membersBottomToTop.size() - 1;
                 index-- > 0;) {
                auto *window = m_registry.window(block.membersBottomToTop[index]);
                workspace->stackBelow(window, reference);
                reference = window;
            }
        }
    }

    QHash<const KWin::Window *, int> finalIndices;
    const auto &finalStack = workspace->stackingOrder();
    for (int index = 0; index < finalStack.size(); ++index) {
        finalIndices.insert(finalStack[index], index);
    }
    QMap<QString, QStringList> published;
    QMap<QString, QStringList> publishedTransients;
    QMap<QString, QString> representatives;
    for (const auto &block : plan->blocksBottomToTop) {
        int previous = -1;
        for (const auto &memberId : block.membersBottomToTop) {
            const int index = finalIndices.value(m_registry.window(memberId), -1);
            if (index < 0 || (previous >= 0 && index != previous + 1)) {
                hideAll();
                return fail(error,
                            QStringLiteral("group '%1' could not form a contiguous stack block")
                                .arg(block.containerId));
            }
            previous = index;
        }
        int transientIndex = previous;
        for (const auto &transientId : block.transientsBottomToTop) {
            const int index = finalIndices.value(windows.value(transientId), -1);
            if (index <= transientIndex) {
                hideAll();
                return fail(error,
                            QStringLiteral("group '%1' transient is not above its member block")
                                .arg(block.containerId));
            }
            transientIndex = index;
        }
        if (!m_chrome.setStackingAnchor(block.containerId,
                                        block.topMemberId(), error)) {
            hideAll();
            return false;
        }
        published.insert(block.containerId, block.membersBottomToTop);
        publishedTransients.insert(block.containerId,
                                   block.transientsBottomToTop);
        representatives.insert(
            block.containerId,
            groups.value(block.containerId).activeMembers.constFirst());
    }
    m_membersBottomToTop = std::move(published);
    m_transientsBottomToTop = std::move(publishedTransients);
    m_activationRepresentatives = std::move(representatives);
    return true;
}

bool KWinHybridGroupStacking::raiseContainer(const QString &containerId,
                                              QString *error)
{
    if (error) {
        error->clear();
    }
    const auto members = m_membersBottomToTop.value(containerId);
    auto *const workspace = KWin::workspace();
    const auto invalidatePublishedContainer = [this, &containerId] {
        // AGENT-GUARD: Published stacking is an input-safety capability, not
        // historical telemetry. Once a raise cannot prove its live block,
        // remove every cached identity before hiding chrome so diagnostics and
        // later hit tests cannot claim that the unsafe group remains usable.
        m_membersBottomToTop.remove(containerId);
        m_transientsBottomToTop.remove(containerId);
        m_activationRepresentatives.remove(containerId);
        m_chrome.setOverlayVisible(containerId, false);
    };
    if (members.isEmpty() || !workspace) {
        if (!members.isEmpty()) {
            invalidatePublishedContainer();
        }
        return fail(error, QStringLiteral("unknown stack group '%1'").arg(containerId));
    }
    const auto representativeId = m_activationRepresentatives.value(containerId);
    auto *representative = m_registry.window(representativeId);
    if (!representative) {
        invalidatePublishedContainer();
        return fail(error, QStringLiteral("group '%1' has no activation representative")
                               .arg(containerId));
    }

    QVector<KWin::Window *> memberWindows;
    memberWindows.reserve(members.size());
    const auto expectedLayer = representative->layer();
    for (const auto &memberId : members) {
        auto *window = m_registry.window(memberId);
        if (!window || window->isDeleted() || window->layer() != expectedLayer) {
            invalidatePublishedContainer();
            return fail(error,
                        QStringLiteral("group '%1' has a dead or incompatible stack member")
                            .arg(containerId));
        }
        memberWindows.append(window);
    }
    QHash<QString, KWin::Window *> liveStack;
    for (auto *window : workspace->stackingOrder()) {
        liveStack.insert(stackId(window), window);
    }
    QVector<KWin::Window *> transientWindows;
    const auto transientIds = m_transientsBottomToTop.value(containerId);
    transientWindows.reserve(transientIds.size());
    for (const auto &transientId : transientIds) {
        auto *window = liveStack.value(transientId);
        if (!window || window->isDeleted()) {
            invalidatePublishedContainer();
            return fail(error, QStringLiteral("group '%1' has a dead stack transient")
                                   .arg(containerId));
        }
        transientWindows.append(window);
    }
    const auto raisedOrder = members;

    // AGENT-CONTRACT: Shared scene chrome cannot receive native activation.
    // Activate one stable active-page leaf so task/focus policy sees a real
    // client, then replay the synchronized member order independently of that
    // focus choice. Its top slot may deliberately be a different member that
    // owns a native transient; changing that order lets KWin interleave the
    // transient and split the supposedly contiguous group block.
    {
        KWin::StackingUpdatesBlocker blocker(workspace);
        workspace->activateWindow(representative);
        for (auto *window : memberWindows) {
            workspace->raiseWindow(window, true);
        }
        for (auto *window : transientWindows) {
            workspace->raiseWindow(window, true);
        }
    }

    QHash<const KWin::Window *, int> finalIndices;
    const auto &finalStack = workspace->stackingOrder();
    for (int index = 0; index < finalStack.size(); ++index) {
        finalIndices.insert(finalStack[index], index);
    }
    int previous = -1;
    for (auto *window : memberWindows) {
        const int index = finalIndices.value(window, -1);
        if (index < 0 || (previous >= 0 && index != previous + 1)) {
            invalidatePublishedContainer();
            return fail(error,
                        QStringLiteral("group '%1' did not raise as one contiguous block")
                            .arg(containerId));
        }
        previous = index;
    }
    for (auto *window : transientWindows) {
        const int index = finalIndices.value(window, -1);
        if (index <= previous) {
            invalidatePublishedContainer();
            return fail(error,
                        QStringLiteral("group '%1' transient did not remain above its owner block")
                            .arg(containerId));
        }
        previous = index;
    }
    if (workspace->activeWindow() != representative
        && !transientWindows.contains(workspace->activeWindow())) {
        invalidatePublishedContainer();
        return fail(error,
                    QStringLiteral("group '%1' could not activate its representative or dialog")
                        .arg(containerId));
    }
    if (!m_chrome.setStackingAnchor(containerId,
                                    raisedOrder.constLast(), error)) {
        invalidatePublishedContainer();
        return false;
    }
    m_membersBottomToTop.insert(containerId, std::move(raisedOrder));
    return true;
}

bool KWinHybridGroupStacking::chromeExposedAt(
    const QString &containerId,
    const QPointF &position,
    const QString &excludedWindowId) const
{
    const auto members = m_membersBottomToTop.value(containerId);
    if (members.isEmpty()) {
        return false;
    }
    const auto &anchorId = members.constLast();
    if (!paintableInCurrentContext(m_registry.window(anchorId))) {
        return false;
    }

    QVector<HybridChromeExposureEntry> stack;
    const auto &liveStack = KWin::workspace()->stackingOrder();
    stack.reserve(liveStack.size());
    for (auto *window : liveStack) {
        stack.append({stackId(window), inputEligibleAt(window, position)});
    }
    return sceneChromeExposed(anchorId, excludedWindowId, stack);
}

void KWinHybridGroupStacking::clear() noexcept
{
    m_membersBottomToTop.clear();
    m_transientsBottomToTop.clear();
    m_activationRepresentatives.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
