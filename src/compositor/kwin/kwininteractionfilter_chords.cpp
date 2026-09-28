// SPDX-License-Identifier: GPL-3.0-or-later
// The ADR-0282 half of KWinInteractionFilter: a stylus speaks the same
// window-management chords as a mouse (tip = left, barrel = right), the
// modifier + wheel rolls containers up and down, and modifier + right button
// resizes an ordinary window. The chord rules live in hybrid_input
// (containerchords.h, wheelrollchord.h, tabletpointertranslator.h); this file
// only moves KWin events in and out of them.
#include "kwininteractionfilter.h"

#include "qindaqt/hybrid_input/containerchords.h"

#include <core/inputdevice.h>
#include <input.h>
#include <input_event.h>
#include <workspace.h>

#include <chrono>

namespace QindaQt::Compositor::KWinIntegration {

Qt::KeyboardModifiers KWinInteractionFilter::keyboardModifiers() const
{
    // Tablet events carry no modifiers; the keyboard's are the chord's.
    return m_input ? m_input->keyboardModifiers() : Qt::NoModifier;
}

bool KWinInteractionFilter::modifierWindowResize(const HybridInput::PointerEvent &event)
{
    if (!m_modifierChords.resizeWindowAt || !m_controller.pointerChordEnabled()) {
        return false;
    }
    // The controller already declined (no container under the pointer), so
    // only the independent-window reading of the chord is left to judge.
    const auto action = HybridInput::classifyPointerChord({
        .button = event.changedButton,
        .modifiers = event.modifiers,
        .dockChord = m_controller.pointerModifiers(),
        .overContainer = false,
        .overIndependentWindow = true,
    });
    return action == HybridInput::PointerChordAction::WindowResize
        && m_modifierChords.resizeWindowAt(event.position);
}

bool KWinInteractionFilter::tabletPointer(const HybridInput::PointerEvent &event, bool pressed)
{
    if (pressed) {
        if (m_tabletGesture) {
            // A second button during our gesture belongs to the gesture.
            return true;
        }
        if (m_controller.active()) {
            return false;
        }
        // AGENT-GUARD: never consume an unclaimed tablet press. Without the
        // window-management modifier the tip, eraser and barrel reach the
        // client exactly as before ADR-0282; only chrome QindaQt draws (tabs,
        // container buttons, icon chips) claims a plain pen press, as it
        // claims a plain mouse press.
        if (!routeButton(event, true)) {
            return false;
        }
        // A KWin-owned modifier resize is driven by KWin's own move/resize
        // filter, which sits before this one and consumes the pen's motion
        // and lift itself. Every other claimed press is ours until its lift,
        // even one a router settles at once, so the lift never reaches a
        // client that did not see the press.
        auto *const activeWorkspace = KWin::workspace();
        m_tabletGesture = !(activeWorkspace && activeWorkspace->moveResizeWindow());
        return true;
    }
    if (!m_tabletGesture) {
        return false;
    }
    static_cast<void>(routeButton(event, false));
    if (m_tablet.buttons() == Qt::NoButton) {
        m_tabletGesture = false;
    }
    return true;
}

bool KWinInteractionFilter::tabletToolTip(KWin::TabletToolTipEvent *event)
{
    if (!event) {
        return false;
    }
    m_penIsLastPointer = true;
    const bool down = event->type == KWin::TabletToolTipEvent::Press;
    const auto tool = event->tool && event->tool->type() == KWin::InputDeviceTabletTool::Eraser
        ? HybridInput::TabletTool::Eraser
        : HybridInput::TabletTool::Pen;
    const auto pointer = m_tablet.contact(tool, down, event->position, keyboardModifiers(),
                                          m_controller.pointerModifiers());
    if (!pointer) {
        // An eraser without the modifier: an ordinary eraser for the client.
        return m_tabletGesture;
    }
    return tabletPointer(*pointer, down);
}

bool KWinInteractionFilter::tabletToolButton(KWin::TabletToolButtonEvent *event)
{
    if (!event) {
        return false;
    }
    const auto pointer = m_tablet.button(event->button, event->pressed, keyboardModifiers());
    if (!pointer) {
        return m_tabletGesture;
    }
    return tabletPointer(*pointer, event->pressed);
}

bool KWinInteractionFilter::tabletToolAxis(KWin::TabletToolAxisEvent *event)
{
    if (!event) {
        return false;
    }
    m_penIsLastPointer = true;
    const auto motion = m_tablet.motion(event->position, keyboardModifiers());
    if (!m_tabletGesture) {
        return false;
    }
    // The gesture holds the pen until its lift even if a router already
    // settled: the client never saw the press, so it must not see the rest.
    static_cast<void>(routeMotion(motion));
    return true;
}

bool KWinInteractionFilter::tabletToolProximity(KWin::TabletToolProximityEvent *event)
{
    if (!event) {
        return false;
    }
    m_penIsLastPointer = true;
    if (event->type == KWin::TabletToolProximityEvent::EnterProximity) {
        static_cast<void>(m_tablet.motion(event->position, keyboardModifiers()));
        return false;
    }
    // A pen lifted away mid-gesture never sends its button-up: release what
    // it still holds so every router ends the gesture (never left active).
    const auto releases = m_tablet.leaveProximity(keyboardModifiers());
    if (m_tabletGesture) {
        for (const auto &release : releases) {
            static_cast<void>(routeButton(release, false));
        }
        if (m_controller.active()) {
            static_cast<void>(dispatch(m_controller.cancel()));
        }
        cancelChrome();
        m_tabletGesture = false;
    }
    return false;
}

bool KWinInteractionFilter::tabletDrivesInput() const
{
    // AGENT-NOTE: not KWin's lastInputHandler(): keyboard input claims it
    // before any filter runs, so a key pressed mid pen-drag would read as
    // keyboard. Pointer and tablet events maintain m_penIsLastPointer.
    return m_penIsLastPointer;
}

bool KWinInteractionFilter::earlyPointerAxis(KWin::PointerAxisEvent *event)
{
    if (!event || event->orientation != Qt::Vertical || m_controller.active()
        || !m_modifierChords.rollTargetAt || !m_modifierChords.roll) {
        return false;
    }
    // Only look for a target when the chord's modifier is actually held: the
    // lookup walks the stacking order, and this runs for every wheel event.
    const bool chordHeld = m_wheelRoll.modifier() != Qt::NoModifier
        && event->modifiers == m_wheelRoll.modifier();
    const auto decision = m_wheelRoll.feed({
        .timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp)
                      .count(),
        .modifiers = event->modifiers,
        .delta = event->delta,
        .deltaV120 = event->deltaV120,
        .inverted = event->inverted,
        .targetUnderPointer = chordHeld && m_modifierChords.rollTargetAt(event->position),
    });
    if (decision.roll) {
        m_modifierChords.roll(event->position, *decision.roll);
    }
    return decision.consumed;
}

} // namespace QindaQt::Compositor::KWinIntegration
