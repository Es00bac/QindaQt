// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: what the window-management chords do once KWinInteractionFilter
// has recognised them -- modifier + wheel rolls the container (or the window)
// under the pointer up or down, and modifier + right button starts KWin's
// own interactive resize of an ordinary independent window. Grouped windows
// never reach the resize hook: InteractionController claims those presses and
// resizes the whole container through placement.
#include "kwinhybridsession.h"

#include "hybridchromepointerrouter.h"
#include "hybridiconchiprouter.h"
#include "hybridiconifycontroller.h"
#include "kwininteractionfilter.h"
#include "managedwindowregistry.h"

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

} // namespace

void KWinHybridSession::initializeModifierChordInput()
{
    if (!m_inputFilter) {
        return;
    }
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
