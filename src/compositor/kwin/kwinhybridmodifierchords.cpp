// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: what the window-management chords do once KWinInteractionFilter
// has recognised them -- modifier + wheel rolls the container (or the window)
// under the pointer up or down, and modifier + right button starts KWin's
// own interactive resize of an ordinary independent window -- and the touch
// pick-up of a window held by its own title bar. Grouped windows
// never reach the resize hook: InteractionController claims those presses and
// resizes the whole container through placement.
#include "kwinhybridsession.h"

#include "hybridchromepointerrouter.h"
#include "hybridiconchiprouter.h"
#include "hybridiconifycontroller.h"
#include "kwininteractionfilter.h"
#include "managedwindowregistry.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>

#include <input.h>
#include <options.h>
#include <window.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// The topmost window KWin would give pointer input to at `position`.
KWin::Window *windowUnder(const QPointF &position)
{
    auto *const input = KWin::input();
    auto *const window = input ? input->findToplevel(position) : nullptr;
    return window && !window->isDeleted() ? window : nullptr;
}

// A pick-up may only replace a press KWin read as a title press: never a
// title-bar button (its release would still click it), never a move KWin
// already started.
bool titlePressIsQuiet(KWin::Window *window, const QPointF &position)
{
    auto *const decoration = window ? window->decoration() : nullptr;
    if (!decoration || window->isDeleted() || window->isInteractiveMove()
        || window->isInteractiveResize()
        || decoration->sectionUnderMouse() != Qt::TitleBarArea) {
        return false;
    }
    const QPointF local = position - window->pos();
    const auto buttons = decoration->findChildren<KDecoration3::DecorationButton *>();
    for (const auto *button : buttons) {
        if (button->isVisible() && (button->isPressed() || button->geometry().contains(local))) {
            return false;
        }
    }
    return true;
}

} // namespace

void KWinHybridSession::initializeModifierChordInput()
{
    if (!m_inputFilter) {
        return;
    }
    m_inputFilter->setTouchPickupHooks(TouchPickupHooks{
        .titleAt = [this](const QPointF &position) -> std::optional<QString> {
            auto *const window = windowUnder(position);
            const auto id = m_registry.windowId(window);
            auto *const decoration = window ? window->decoration() : nullptr;
            if (id.isEmpty() || m_registry.window(id) != window || !window->isNormalWindow()
                || !decoration || (m_iconify && m_iconify->isIconified(id))) {
                return std::nullopt;
            }
            const QRectF titleBar = decoration->titleBar().translated(window->frameGeometry().topLeft());
            return titleBar.contains(position) ? std::optional(id) : std::nullopt;
        },
        .takeOver = [this](const QString &windowId,
                           const QPointF &position) -> std::optional<HybridInput::HitTarget> {
            auto *const window = m_registry.window(windowId);
            if (!titlePressIsQuiet(window, position)) {
                return std::nullopt;
            }
            // AGENT-GUARD: KWin recorded a title press on touch down. Release
            // it now so nothing (a mouse moving over this title meanwhile)
            // can turn it into KWin's own move while QindaQt owns the drag;
            // the finger's lift still reaches KWin's decoration filter, which
            // clears its touch-press id (a swallowed lift would make every
            // later title touch ignored).
            window->processDecorationButtonRelease(Qt::LeftButton);
            return HybridInput::HitTarget{HybridInput::HitKind::MemberTitle,
                                          m_registry.owner(windowId), windowId, {}};
        },
    });
    // Container chrome under the pointer, or the container of the managed
    // window under it; empty for an independent window or nothing managed.
    const auto containerAt = [this](const QPointF &position) -> QString {
        if (m_chromePointerRouter) {
            if (const auto hit = m_chromePointerRouter->hitNear(position, 0.0, std::nullopt)) {
                return hit->hit.containerId;
            }
        }
        const auto id = m_registry.windowId(windowUnder(position));
        return id.isEmpty() ? QString{} : m_registry.owner(id);
    };
    const auto managedIndependentAt = [this](const QPointF &position) -> QString {
        auto *const window = windowUnder(position);
        const auto id = m_registry.windowId(window);
        if (id.isEmpty() || m_registry.window(id) != window || !window->isNormalWindow()
            || !m_registry.owner(id).isEmpty()) {
            return {};
        }
        return id;
    };
    m_inputFilter->setModifierChordHooks(ModifierChordHooks{
        .rollTargetAt = [this, containerAt, managedIndependentAt](const QPointF &position) {
            return !containerAt(position).isEmpty() || iconChipHitAt(position).has_value()
                || !managedIndependentAt(position).isEmpty();
        },
        .roll = [this, containerAt, managedIndependentAt](const QPointF &position,
                                                          HybridInput::RollDirection direction) {
            const bool up = direction == HybridInput::RollDirection::Up;
            QString error;
            // An icon chip stands in for its hidden window: down unrolls it.
            if (const auto chip = iconChipHitAt(position)) {
                if (!up && !restoreIconifiedWindow(chip->windowId, true, &error)) {
                    qWarning("QindaQt modifier-wheel unroll of '%s' failed: %s",
                             qPrintable(chip->windowId), qPrintable(error));
                }
                return;
            }
            // AGENT-CONTRACT: applyWheelShade is idempotent (rolling up a
            // rolled-up container does nothing) and leaves the container's
            // member focus first; a maximized container rolls up too.
            if (const auto containerId = containerAt(position); !containerId.isEmpty()) {
                applyWheelShade(containerId, up);
                return;
            }
            const auto windowId = managedIndependentAt(position);
            if (up && !windowId.isEmpty()
                && !(m_iconify && m_iconify->isIconified(windowId))
                && !iconifyWindow(windowId, &error)) {
                qWarning("QindaQt modifier-wheel roll-up of '%s' failed: %s",
                         qPrintable(windowId), qPrintable(error));
            }
        },
        .resizeWindowAt = [managedIndependentAt, this](const QPointF &position) {
            const auto windowId = managedIndependentAt(position);
            auto *const window = windowId.isEmpty() ? nullptr : m_registry.window(windowId);
            if (!window || !window->isResizable()
                || (m_iconify && m_iconify->isIconified(windowId))) {
                return false;
            }
            // KWin's own modifier-resize command: nearest-corner gravity from
            // the press, driven by KWin's move/resize filter until release.
            static_cast<void>(window->performMousePressCommand(
                KWin::Options::MouseResize, position));
            return window->isInteractiveResize();
        },
    });
}

} // namespace QindaQt::Compositor::KWinIntegration
