// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QRectF>
#include <Qt>

#include <optional>

namespace QindaQt::HybridInput {

// ADR-0282: what a modifier + pointer-button press means over a window.
// The window-management modifier is the configured docking chord without
// Shift (windowManagement.dockingModifier: Meta by default, Alt or Control,
// or disabled):
//   modifier + Shift + left  -> MemberDock: drag the one window under the
//                               pointer out of / into containers (ADR-0085).
//   modifier + left          -> ContainerMove over a container; KWin's own
//                               window move everywhere else (None here).
//   modifier + right         -> ContainerResize over a container, WindowResize
//                               over an ordinary independent window.
// A pen reaches this with its tip as the left button and its barrel button as
// the right button (TabletPointerTranslator).
enum class PointerChordAction {
    None,
    MemberDock,
    ContainerMove,
    ContainerResize,
    WindowResize,
};

struct PointerChordFacts final
{
    Qt::MouseButton button = Qt::NoButton;
    Qt::KeyboardModifiers modifiers;
    // The docking chord (modifier + Shift); nullopt when the user disabled
    // modifier gestures, which disables every chord here.
    std::optional<Qt::KeyboardModifiers> dockChord;
    bool overContainer = false;
    bool overIndependentWindow = false;
};

// The modifier alone: the docking chord without Shift. Empty when the chord
// is disabled or would degrade to a bare button.
[[nodiscard]] Qt::KeyboardModifiers windowManagementModifier(
    const std::optional<Qt::KeyboardModifiers> &dockChord) noexcept;

// AGENT-CONTRACT: exact modifier equality, like the docking chord. A chord
// that merely contains the modifier (an accessibility or user shortcut) is
// never claimed. The right-button chord over an independent window is
// WindowResize because QindaQt seeds KWin's CommandAll3 to Nothing (the
// shell's own Meta+right-click customization chord on panels needs it), so
// KWin would otherwise never resize there.
[[nodiscard]] PointerChordAction classifyPointerChord(const PointerChordFacts &facts) noexcept;

// KWin's modifier-resize rule: the press picks the nearest corner, or the
// nearest side from the middle band, of the frame it lands in (thirds on
// each axis). A press outside the frame resizes from the bottom-right.
[[nodiscard]] Qt::Edges nearestResizeEdges(const QRectF &frame, const QPointF &position) noexcept;

} // namespace QindaQt::HybridInput
