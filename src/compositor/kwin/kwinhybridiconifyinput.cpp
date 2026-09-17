// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridiconchiprouter.h"
#include "hybridiconifycontroller.h"
#include "kwiniconchippresenter.h"
#include "kwininteractionfilter.h"
#include "managedwindowregistry.h"

#include "qindaqt/hybrid_chrome/chromeiconchip.h"

#include <KDecoration3/Decoration>

#include <window.h>
#include <workspace.h>

#include <QAction>
#include <QMenu>
#include <QTimer>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool inputEligibleAt(const KWin::Window *window, const QPointF &position)
{
    return window && !window->isDeleted()
        && window->isOnCurrentActivity() && window->isOnCurrentDesktop()
        && !window->isMinimized() && !window->isHidden()
        && !window->isHiddenByShowDesktop() && window->readyForPainting()
        && window->hitTest(position);
}

} // namespace

// Wires the iconified-window input paths into the interaction filter once
// the filter exists (ADR-0191); the chip router was built by ensureIconify().
void KWinHybridSession::initializeIconifyInput()
{
    if (!m_inputFilter || !m_iconChipRouter) {
        return;
    }
    m_inputFilter->setIconifyHooks(IconifyInputHooks{
        .chipRouter = m_iconChipRouter.get(),
        .chipSink = [this](const IconChipPointerDecision &decision) {
            dispatchIconChipDecision(decision);
        },
        .titleWheelTarget = [this](const QPointF &position) {
            return iconifyWheelTargetAt(position);
        },
        .iconify = [this](const QString &windowId) {
            QString error;
            if (!iconifyWindow(windowId, &error)) {
                qWarning("QindaQt wheel roll-up of '%s' failed: %s",
                         qPrintable(windowId), qPrintable(error));
            }
        },
    });
}

std::optional<QString> KWinHybridSession::iconChipSourceAt(const QPointF &position) const
{
    const auto hit = iconChipHitAt(position);
    return hit ? std::optional(hit->windowId) : std::nullopt;
}

std::optional<IconChipPointerHit> KWinHybridSession::iconChipHitAt(
    const QPointF &position) const
{
    if (!m_iconify || !m_iconChips || m_iconify->count() == 0) {
        return std::nullopt;
    }
    // Top-down: an exposed chip wins, any real input owner above it covers it.
    const auto &stack = KWin::workspace()->stackingOrder();
    for (auto iterator = stack.crbegin(); iterator != stack.crend(); ++iterator) {
        auto *const window = *iterator;
        const auto windowId = m_registry.windowId(window);
        if (!windowId.isEmpty() && m_iconify->isIconified(windowId)) {
            if (!m_iconChips->isVisible(windowId)) {
                continue;
            }
            const auto plan = m_iconChips->plan(windowId);
            const auto kind = plan
                ? HybridChrome::ChromeIconChip::hitTest(*plan, position)
                : HybridChrome::IconChipHitKind::None;
            if (kind != HybridChrome::IconChipHitKind::None) {
                return IconChipPointerHit{windowId, kind};
            }
            continue;
        }
        if (inputEligibleAt(window, position)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool KWinHybridSession::iconChipCoversAbove(const QString &anchorWindowId,
                                            const QPointF &position) const
{
    if (!m_iconify || !m_iconChips || m_iconify->count() == 0 || anchorWindowId.isEmpty()) {
        return false;
    }
    const auto &stack = KWin::workspace()->stackingOrder();
    for (auto iterator = stack.crbegin(); iterator != stack.crend(); ++iterator) {
        const auto windowId = m_registry.windowId(*iterator);
        if (windowId == anchorWindowId) {
            return false;
        }
        if (!windowId.isEmpty() && m_iconify->isIconified(windowId)
            && m_iconChips->isVisible(windowId)) {
            const auto plan = m_iconChips->plan(windowId);
            if (plan && HybridChrome::ChromeIconChip::hitTest(*plan, position)
                    != HybridChrome::IconChipHitKind::None) {
                return true;
            }
        }
    }
    return false;
}

std::optional<QString> KWinHybridSession::iconifyWheelTargetAt(const QPointF &position) const
{
    if (!ready()) {
        return std::nullopt;
    }
    const auto &stack = KWin::workspace()->stackingOrder();
    for (auto iterator = stack.crbegin(); iterator != stack.crend(); ++iterator) {
        auto *const window = *iterator;
        if (!inputEligibleAt(window, position)) {
            continue;
        }
        // AGENT-CONTRACT: stop at the first real input owner. Popups, panels,
        // dialogs, and grouped members (whose handlebar wheel the chrome
        // router owns) never tunnel to a title bar below them.
        const auto windowId = m_registry.windowId(window);
        if (windowId.isEmpty() || m_registry.window(windowId) != window
            || !m_registry.owner(windowId).isEmpty() || !window->isNormalWindow()
            || (m_iconify && m_iconify->isIconified(windowId))) {
            return std::nullopt;
        }
        auto *const decoration = window->decoration();
        if (!decoration) {
            return std::nullopt;
        }
        const QRectF frame = window->frameGeometry();
        const QRectF titleBar = decoration->titleBar().translated(frame.topLeft());
        return titleBar.contains(position) ? std::optional(windowId) : std::nullopt;
    }
    return std::nullopt;
}

void KWinHybridSession::dispatchIconChipDecision(const IconChipPointerDecision &decision)
{
    if (!ready() || !m_iconify || !m_iconChips) {
        return;
    }
    if (decision.hoverChanged) {
        m_iconChips->setPointerHover(decision.hovered);
    }
    auto *const workspace = KWin::workspace();
    for (const auto &windowId : decision.raiseRequests) {
        auto *const window = m_registry.window(windowId);
        if (window && !window->isDeleted() && workspace) {
            workspace->raiseWindow(window);
        }
    }
    for (const auto &drag : decision.drags) {
        handleIconChipDrag(drag);
    }
    for (const auto &windowId : decision.unrollRequests) {
        QString error;
        if (!restoreIconifiedWindow(windowId, true, &error)) {
            qWarning("QindaQt chip unroll of '%s' failed: %s",
                     qPrintable(windowId), qPrintable(error));
        }
    }
    for (const auto &windowId : decision.closeRequests) {
        if (auto *const window = m_registry.window(windowId); window && !window->isDeleted()) {
            window->closeWindow();
        }
    }
    for (const auto &request : decision.contextMenus) {
        showIconChipMenu(request.windowId, request.globalPosition);
    }
}

void KWinHybridSession::handleIconChipDrag(const IconChipDrag &drag)
{
    const auto record = m_iconify->record(drag.windowId);
    if (!record) {
        return;
    }
    if (drag.phase == HybridChrome::DragPhase::Begin
        || !m_iconChipDragBaselines.contains(drag.windowId)) {
        m_iconChipDragBaselines.insert(drag.windowId, record->chipFrame.topLeft());
    }
    const QPointF baseline = m_iconChipDragBaselines.value(drag.windowId);
    const QPointF target = drag.phase == HybridChrome::DragPhase::Cancel
        ? baseline : baseline + drag.delta;
    QString error;
    if (!m_iconify->relocateChip(drag.windowId, target, iconChipBounds(drag.windowId), &error)
        || !publishIconChip(drag.windowId, &error)) {
        qWarning("QindaQt chip drag failed for '%s': %s",
                 qPrintable(drag.windowId), qPrintable(error));
    }
    if (drag.phase == HybridChrome::DragPhase::Commit
        || drag.phase == HybridChrome::DragPhase::Cancel) {
        m_iconChipDragBaselines.remove(drag.windowId);
    }
}

void KWinHybridSession::showIconChipMenu(const QString &windowId, const QPointF &globalPosition)
{
    if (!m_iconChipMenu) {
        m_iconChipMenu = std::make_unique<QMenu>();
        m_iconChipMenu->setObjectName(QStringLiteral("qindaqtIconChipMenu"));
    }
    m_iconChipMenu->clear();
    auto *const unroll = m_iconChipMenu->addAction(tr("Unroll"));
    unroll->setObjectName(QStringLiteral("iconChipUnroll"));
    // Commands run after the popup hides so an action that unrolls or closes
    // the window cannot invalidate the open menu (same rule as the group menu).
    connect(unroll, &QAction::triggered, this, [this, windowId] {
        QTimer::singleShot(0, this, [this, windowId] {
            QString error;
            if (!restoreIconifiedWindow(windowId, true, &error)) {
                qWarning("QindaQt chip menu unroll failed: %s", qPrintable(error));
            }
        });
    });
    auto *const close = m_iconChipMenu->addAction(tr("Close"));
    close->setObjectName(QStringLiteral("iconChipClose"));
    connect(close, &QAction::triggered, this, [this, windowId] {
        QTimer::singleShot(0, this, [this, windowId] {
            if (auto *const window = m_registry.window(windowId);
                window && !window->isDeleted()) {
                window->closeWindow();
            }
        });
    });
    m_iconChipMenu->popup(globalPosition.toPoint());
}

} // namespace QindaQt::Compositor::KWinIntegration
