// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_input/interactioncontroller.h"

#include <QPointer>

#include <functional>
#include <memory>

namespace KWin {
class InputRedirection;
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

    [[nodiscard]] bool pointerMotion(KWin::PointerMotionEvent *event);
    [[nodiscard]] bool pointerButton(KWin::PointerButtonEvent *event);
    [[nodiscard]] bool keyboardKey(KWin::KeyboardKeyEvent *event);
    [[nodiscard]] bool dispatch(HybridInput::InteractionDecision decision);
    [[nodiscard]] bool dispatchChrome(ChromePointerDecision decision);

    QPointer<KWin::InputRedirection> m_input;
    HybridInput::InteractionController &m_controller;
    IntentSink m_sink;
    HybridChromePointerRouter *m_chromeRouter = nullptr;
    ChromeDecisionSink m_chromeSink;
    std::unique_ptr<Filter> m_filter;
};

} // namespace QindaQt::Compositor::KWinIntegration
