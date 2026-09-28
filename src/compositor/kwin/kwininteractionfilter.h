// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridchrometouchpolicy.h"

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_input/interactioncontroller.h"
#include "qindaqt/hybrid_input/lateshifttakeoverdetector.h"
#include "qindaqt/hybrid_input/tabletpointertranslator.h"
#include "qindaqt/hybrid_input/touchtitlepickup.h"
#include "qindaqt/hybrid_input/wheelrollchord.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

namespace KWin {
class InputRedirection;
class Window;
struct KeyboardKeyEvent;
struct PointerButtonEvent;
struct PointerAxisEvent;
struct PointerMotionEvent;
struct TabletToolAxisEvent;
struct TabletToolButtonEvent;
struct TabletToolProximityEvent;
struct TabletToolTipEvent;
struct TouchDownEvent;
struct TouchMotionEvent;
struct TouchUpEvent;
}

namespace QindaQt::Compositor::KWinIntegration {

class HybridChromePointerRouter;
struct ChromePointerDecision;
class HybridIconChipRouter;
struct IconChipPointerDecision;

// Iconified-window input (ADR-0203). Every member is optional; a missing
// router or resolver simply leaves that path to KWin. The router and the
// callables are borrowed and must outlive the filter.
struct IconifyInputHooks final
{
    HybridIconChipRouter *chipRouter = nullptr;
    std::function<void(const IconChipPointerDecision &)> chipSink;
    // Resolves the independent, server-decorated window whose title bar lies
    // under the pointer, or nothing when another input owner covers it.
    std::function<std::optional<QString>(const QPointF &)> titleWheelTarget;
    // Receives the window a modifier-free wheel away from the user rolls up.
    std::function<void(const QString &windowId)> iconify;
};

// ADR-0282 window-management chords the controller cannot act on alone.
// Every member is optional; a missing callable leaves that chord to KWin.
struct ModifierChordHooks final
{
    // Whether modifier + wheel has something to roll under the pointer: a
    // managed window, container chrome, or an icon chip. False leaves the
    // wheel to KWin (its own Meta+wheel zoom over the bare desktop).
    std::function<bool(const QPointF &)> rollTargetAt;
    // Rolls that target up or down; idempotent (rolling up a rolled-up
    // container does nothing).
    std::function<void(const QPointF &, HybridInput::RollDirection)> roll;
    // modifier + right button over an ordinary independent window: starts
    // KWin's own interactive resize, which QindaQt's CommandAll3=Nothing seed
    // otherwise never does. True when a resize began.
    std::function<bool(const QPointF &)> resizeWindowAt;
};

// ADR-0282 touch pick-up of a window by its own title bar. Both are optional;
// without them a held finger on a title stays KWin's.
struct TouchPickupHooks final
{
    // The manageable window whose native title bar is under a finger landing
    // at this point, or nothing (another input owner covers it, no title).
    std::function<std::optional<QString>(const QPointF &)> titleAt;
    // Called when the long press fires. Returns the dock-drag source when the
    // takeover is safe -- KWin has not started its own move and the finger is
    // on the title, not a title-bar button -- after releasing KWin's pending
    // title press; nothing when the sequence must stay KWin's.
    std::function<std::optional<HybridInput::HitTarget>(const QString &windowId,
                                                        const QPointF &position)>
        takeOver;
};

class KWinInteractionFilter final
{
public:
    using IntentSink = std::function<void(const HybridInput::InteractionIntent &)>;
    using ChromeDecisionSink = std::function<void(const ChromePointerDecision &)>;
    // Maps KWin's interactive-move owner at late-Shift takeover (ADR-0085) to
    // the drag source identity the controller adopts. Must be exact: after the
    // native move is cancelled the window snaps back to its pre-drag frame, so
    // hit-testing the pointer would adopt whichever stranger window the drag
    // was hovering. An unset resolver keeps the legacy pointer hit-test.
    using TakeoverSourceResolver =
        std::function<HybridInput::HitTarget(KWin::Window *window)>;

    KWinInteractionFilter(KWin::InputRedirection *input,
                          HybridInput::InteractionController &controller,
                          IntentSink sink,
                          HybridChromePointerRouter *chromeRouter = nullptr,
                          ChromeDecisionSink chromeSink = {},
                          TakeoverSourceResolver takeoverSource = {});
    ~KWinInteractionFilter();

    // ADR-0205: thresholds from the touch preferences; a gesture in flight
    // keeps the ones it started with.
    void setTouchPolicyConfig(const TouchPolicyConfig &config);

    KWinInteractionFilter(const KWinInteractionFilter &) = delete;
    KWinInteractionFilter &operator=(const KWinInteractionFilter &) = delete;

    [[nodiscard]] bool installed() const;
    // Installs the iconified-window hooks after construction; the session
    // builds its chip router before the filter and resolves chips itself.
    void setIconifyHooks(IconifyInputHooks hooks);
    void setModifierChordHooks(ModifierChordHooks hooks);
    void setTouchPickupHooks(TouchPickupHooks hooks);
    [[nodiscard]] bool beginKeyboardDock(const HybridInput::HitTarget &source);
    [[nodiscard]] bool beginKeyboardMove(const HybridInput::HitTarget &source);
    [[nodiscard]] bool beginKeyboardDividerResize(
        const HybridInput::HitTarget &source);
    [[nodiscard]] bool beginKeyboardContainerResize(
        const HybridInput::HitTarget &source);
    // Cancels only ordinary shared-chrome input. Topology owners use this
    // before an asynchronous overlay replacement without disturbing keyboard
    // or exact-modifier controller state.
    void cancelChrome();
    void invalidateChromeTargets();
    void cancel();
    // Rebinds the pointer docking chord in both places that judge it: the
    // controller (fresh presses) and the late-takeover detector (a native
    // move whose chord completes mid-drag). `std::nullopt` disables both.
    void setDockingModifiers(std::optional<Qt::KeyboardModifiers> modifiers);

private:
    class Filter;
    class EarlyTakeoverFilter;

    [[nodiscard]] bool pointerMotion(KWin::PointerMotionEvent *event);
    [[nodiscard]] bool pointerButton(KWin::PointerButtonEvent *event);
    [[nodiscard]] bool pointerAxis(KWin::PointerAxisEvent *event);
    [[nodiscard]] bool keyboardKey(KWin::KeyboardKeyEvent *event);
    void hideOnScreenKeyboardForHardwareKey(KWin::KeyboardKeyEvent *event);
    // Touch over shared chrome (ADR-0193): the first finger stands in for
    // the left button through the chrome router, a held finger opens the
    // container menu, a second finger's vertical swipe rolls the container.
    // Touches that land on nothing the router owns pass through untouched.
    [[nodiscard]] bool touchDown(KWin::TouchDownEvent *event);
    [[nodiscard]] bool touchMotion(KWin::TouchMotionEvent *event);
    [[nodiscard]] bool touchUp(KWin::TouchUpEvent *event);
    [[nodiscard]] bool touchCancel();
    void expireTouchLongPress();
    // ADR-0282 native-title pick-up (kwininteractionfilter_touch.cpp).
    [[nodiscard]] bool titlePickupDown(KWin::TouchDownEvent *event, qint64 now);
    [[nodiscard]] bool titlePickupMotion(KWin::TouchMotionEvent *event);
    [[nodiscard]] bool titlePickupUp(KWin::TouchUpEvent *event);
    void titlePickupCancel();
    void expireTitlePickup();
    [[nodiscard]] bool applyTitlePickup(const HybridInput::TouchPickupDecision &decision);
    void finishTouchGesture();
    // ADR-0282 (kwininteractionfilter_chords.cpp): the pen speaks the mouse
    // chords (tip = left, barrel = right) through TabletPointerTranslator,
    // modifier + wheel rolls containers, and modifier + right button resizes
    // an ordinary window. Tablet events stay the client's unless the
    // controller claimed the press that began the gesture.
    [[nodiscard]] bool tabletToolTip(KWin::TabletToolTipEvent *event);
    [[nodiscard]] bool tabletToolButton(KWin::TabletToolButtonEvent *event);
    [[nodiscard]] bool tabletToolAxis(KWin::TabletToolAxisEvent *event);
    [[nodiscard]] bool tabletToolProximity(KWin::TabletToolProximityEvent *event);
    [[nodiscard]] bool tabletPointer(const HybridInput::PointerEvent &event, bool pressed);
    [[nodiscard]] bool earlyPointerAxis(KWin::PointerAxisEvent *event);
    [[nodiscard]] bool modifierWindowResize(const HybridInput::PointerEvent &event);
    [[nodiscard]] Qt::KeyboardModifiers keyboardModifiers() const;
    [[nodiscard]] bool dispatch(HybridInput::InteractionDecision decision);
    [[nodiscard]] bool dispatchChrome(ChromePointerDecision decision);
    [[nodiscard]] bool dispatchChip(IconChipPointerDecision decision);

    // Early (pre-InteractiveMoveResize) observation. Watches for our exact
    // takeover chord newly becoming satisfied while KWin is mid a *native*
    // interactive move (never a resize - see ADR-0085) so it can cancel that
    // move and adopt our own gesture before KWin's own Shift-adds-Custom-tile
    // default ever sees the completed chord. Returns true (consumed) only on
    // the single triggering event; every other event of an ordinary native
    // move (with or without Shift from the start, which our own exact-chord
    // press already claims before any native move begins) passes through
    // untouched.
    [[nodiscard]] bool earlyKeyboardKey(KWin::KeyboardKeyEvent *event);
    [[nodiscard]] bool earlyPointerMotion(KWin::PointerMotionEvent *event);
    [[nodiscard]] bool earlyPointerButton(KWin::PointerButtonEvent *event);
    [[nodiscard]] bool observeLateShiftTakeover(Qt::KeyboardModifiers modifiers,
                                                const QPointF &position);

    QPointer<KWin::InputRedirection> m_input;
    HybridInput::InteractionController &m_controller;
    IntentSink m_sink;
    HybridChromePointerRouter *m_chromeRouter = nullptr;
    ChromeDecisionSink m_chromeSink;
    IconifyInputHooks m_iconify;
    TakeoverSourceResolver m_takeoverSource;
    std::unique_ptr<Filter> m_filter;
    std::unique_ptr<EarlyTakeoverFilter> m_earlyFilter;
    HybridInput::LateShiftTakeoverDetector m_lateShiftDetector;
    ModifierChordHooks m_modifierChords;
    HybridInput::TabletPointerTranslator m_tablet;
    // True from a tablet press the controller claimed until its release.
    bool m_tabletGesture = false;
    HybridInput::WheelRollChord m_wheelRoll;
    TouchPickupHooks m_touchPickupHooks;
    HybridInput::TouchTitlePickup m_titlePickup;
    QTimer m_titlePickupTimer;
    bool m_titlePickupTimerConnected = false;
    QString m_titlePickupWindowId;
    HybridChromeTouchPolicy m_touch;
    QTimer m_touchTimer;
    bool m_touchTimerConnected = false;
    QPointF m_touchOffset;
    QString m_touchContainerId;
    HybridChrome::ChromeHitTarget m_touchTarget;
};

} // namespace QindaQt::Compositor::KWinIntegration
