// SPDX-License-Identifier: GPL-3.0-or-later
//
// Focus integrity for KWinHybridSession (ADR-0384): a hidden container member
// never stays the active window, and a dissolved container never leaves
// per-container state behind.
//
// AGENT-NOTE: a translation unit of KWinHybridSession like the other
// kwinhybrid*.cpp files; it owns no state beyond the session's members.

#include "kwinhybridsession.h"

#include "containerlifetimeledger.h"
#include "hybridactivationpolicy.h"
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"
#include "kwinhybridgroupstacking.h"
#include "kwinmemberpolicy.h"
#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

#include <QPointer>
#include <QTimer>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void appendLeaves(const Core::LayoutNode &node, QStringList *windowIds)
{
    if (node.isLeaf()) {
        windowIds->append(node.windowId());
        return;
    }
    if (node.firstChild()) {
        appendLeaves(*node.firstChild(), windowIds);
    }
    if (node.secondChild()) {
        appendLeaves(*node.secondChild(), windowIds);
    }
}

} // namespace

void KWinHybridSession::forgetContainerState(const QString &containerId)
{
    // AGENT-CONTRACT: the complete per-container state outside topology. A
    // new per-container store must be forgotten here, or a dissolved
    // container's state outlives it (the "no valid maximize area" storm).
    forgetShadedContainer(containerId);
    if (m_placement) {
        m_placement->forgetContainer(containerId);
    }
    m_minimizedContainers.remove(containerId);
    m_appearance.forgetContainer(containerId);
}

void KWinHybridSession::guardHiddenActivation(KWin::Window *window)
{
    if (m_shutdown || !ready() || !window || !window->isHidden()) {
        return;
    }
    const QString windowId = m_registry.windowId(window);
    const QString containerId = m_registry.owner(windowId);
    // Independent hidden windows are iconified chips whose activation is a
    // deliberate unroll (ADR-0203). A rolled-up container's shade controller
    // and member focus mode own their members' hidden state and focus.
    if (windowId.isEmpty() || containerId.isEmpty()
        || (m_placement && m_placement->isShaded(containerId))) {
        return;
    }
    if (m_memberPolicy) {
        for (const auto &state : m_memberPolicy->focusStates()) {
            if (state.containerId == containerId) {
                return;
            }
        }
    }
    // AGENT-GUARD: KWin emits windowActivated before activateWindow()
    // returns; re-activating synchronously would nest focus changes. Settle
    // one turn later, and only if the hidden member still holds focus.
    QTimer::singleShot(0, this, [this, target = QPointer<KWin::Window>(window), containerId] {
        auto *const workspace = KWin::workspace();
        if (m_shutdown || !ready() || !target || !workspace
            || workspace->activeWindow() != target || !target->isHidden()) {
            return;
        }
        QStringList activePage;
        if (const auto *container = m_runtime->topology().container(containerId)) {
            if (const auto *page = container->page(container->activePageId())) {
                appendLeaves(page->root(), &activePage);
            }
        }
        const QString preferred =
            m_groupStacking ? m_groupStacking->anchorMemberId(containerId) : QString{};
        const auto replacement = visibleMemberToActivate(
            activePage, preferred, [this](const QString &memberId) {
                const auto *member = m_registry.window(memberId);
                return member && !member->isDeleted() && !member->isHidden()
                    && !member->isMinimized();
            });
        if (replacement) {
            workspace->activateWindow(m_registry.window(*replacement));
        } else {
            workspace->activateNextWindow(target);
        }
    });
}

} // namespace QindaQt::Compositor::KWinIntegration
