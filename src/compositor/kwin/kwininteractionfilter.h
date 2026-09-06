// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactioncontroller.h"
#include "qindaqt/hybrid_input/lateshifttakeoverdetector.h"

#include <QPointer>

#include <functional>
#include <memory>

namespace KWin {
class InputRedirection;
class Window;
struct KeyboardKeyEvent;
struct PointerButtonEvent;
struct PointerMotionEvent;
}

namespace QindaQt::Compositor::KWinIntegration {

class HybridChromePointerRouter;
struct ChromePointerDecision;

class KWinInteractionFilter final
{
public:
    using IntentSink = std::function<void(const HybridInput::InteractionIntent &)>;
    using ChromeDecisionSink = std::function<void(const ChromePointerDecision &)>;

    KWinInteractionFilter(KWin::InputRedirection *input,
                          HybridInput::InteractionController &controller,
                          IntentSink sink,
                          HybridChromePointerRouter *chromeRouter = nullptr,
                          ChromeDecisionSink chromeSink = {});
    ~KWinInteractionFilter();

    KWinInteractionFilter(const KWinInteractionFilter &) = delete;
    KWinInteractionFilter &operator=(const KWinInteractionFilter &) = delete;

    [[nodiscard]] bool installed() const;
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

private:
    class Filter;
    class EarlyTakeoverFilter;

    [[nodiscard]] bool pointerMotion(KWin::PointerMotionEvent *event);
    [[nodiscard]] bool pointerButton(KWin::PointerButtonEvent *event);
    [[nodiscard]] bool keyboardKey(KWin::KeyboardKeyEvent *event);
    [[nodiscard]] bool dispatch(HybridInput::InteractionDecision decision);
    [[nodiscard]] bool dispatchChrome(ChromePointerDecision decision);

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
    std::unique_ptr<Filter> m_filter;
    std::unique_ptr<EarlyTakeoverFilter> m_earlyFilter;
    HybridInput::LateShiftTakeoverDetector m_lateShiftDetector;
};

} // namespace QindaQt::Compositor::KWinIntegration
