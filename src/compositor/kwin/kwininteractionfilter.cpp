// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwininteractionfilter.h"

#include "hybridchromepointerrouter.h"

#include <core/inputdevice.h>
#include <input.h>
#include <input_event.h>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

class KWinInteractionFilter::Filter final : public KWin::InputEventFilter
{
public:
    explicit Filter(KWinInteractionFilter &owner)
        : KWin::InputEventFilter(KWin::InputFilterOrder::Decoration)
        , m_owner(owner)
    {
    }

    bool pointerMotion(KWin::PointerMotionEvent *event) override
    {
        return m_owner.pointerMotion(event);
    }

    bool pointerButton(KWin::PointerButtonEvent *event) override
    {
        return m_owner.pointerButton(event);
    }

    bool keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        return m_owner.keyboardKey(event);
    }

private:
    KWinInteractionFilter &m_owner;
};

KWinInteractionFilter::KWinInteractionFilter(KWin::InputRedirection *input,
                                             HybridInput::InteractionController &controller,
                                             IntentSink sink,
                                             HybridChromePointerRouter *chromeRouter,
                                             ChromeDecisionSink chromeSink)
    : m_input(input)
    , m_controller(controller)
    , m_sink(std::move(sink))
    , m_chromeRouter(chromeRouter)
    , m_chromeSink(std::move(chromeSink))
{
    if (!m_input) {
        return;
    }
    m_filter = std::make_unique<Filter>(*this);
    // AGENT-CONTRACT: Decoration order places this filter after KWin's Popup
    // filter and before native Decoration (lower_bound inserts equal weights
    // first). Thus an outside press dismisses a popup grab without also
    // activating/mutating Hybrid chrome, while shared/native decoration input
    // still reaches QindaQt before KWin begins its own titlebar operation.
    m_input->installInputEventFilter(m_filter.get());
}

KWinInteractionFilter::~KWinInteractionFilter()
{
    // InputEventFilter's destructor unregisters itself. Destroy it while KWin
    // input and the controller/sink collaborators are still valid.
    m_filter.reset();
    m_input = nullptr;
}

bool KWinInteractionFilter::installed() const
{
    return m_filter != nullptr;
}

bool KWinInteractionFilter::beginKeyboardDock(const HybridInput::HitTarget &source)
{
    if (m_chromeRouter && m_chromeRouter->active()) {
        return false;
    }
    return dispatch(m_controller.beginKeyboardDock(source));
}

bool KWinInteractionFilter::beginKeyboardMove(const HybridInput::HitTarget &source)
{
    if (m_chromeRouter && m_chromeRouter->active()) {
        return false;
    }
    return dispatch(m_controller.beginKeyboardMove(source));
}

bool KWinInteractionFilter::beginKeyboardDividerResize(
    const HybridInput::HitTarget &source)
{
    if (m_chromeRouter && m_chromeRouter->active()) {
        return false;
    }
    return dispatch(m_controller.beginKeyboardDividerResize(source));
}

bool KWinInteractionFilter::beginKeyboardContainerResize(
    const HybridInput::HitTarget &source)
{
    if (m_chromeRouter && m_chromeRouter->active()) {
        return false;
    }
    return dispatch(m_controller.beginKeyboardContainerResize(source));
}

void KWinInteractionFilter::cancelChrome()
{
    if (m_chromeRouter) {
        static_cast<void>(dispatchChrome(m_chromeRouter->cancel()));
    }
}

void KWinInteractionFilter::invalidateChromeTargets()
{
    if (m_chromeRouter) {
        static_cast<void>(dispatchChrome(m_chromeRouter->invalidateTargets()));
    }
}

void KWinInteractionFilter::cancel()
{
    cancelChrome();
    static_cast<void>(dispatch(m_controller.cancel()));
}

bool KWinInteractionFilter::pointerMotion(KWin::PointerMotionEvent *event)
{
    if (!event) {
        return false;
    }
    const HybridInput::PointerEvent normalized{
        .position = event->position,
        .changedButton = Qt::NoButton,
        .buttons = event->buttons,
        .modifiers = event->modifiers,
    };
    if (!m_controller.active() && m_chromeRouter
        && dispatchChrome(m_chromeRouter->pointerMove(normalized))) {
        return true;
    }
    return dispatch(m_controller.pointerMove(normalized));
}

bool KWinInteractionFilter::pointerButton(KWin::PointerButtonEvent *event)
{
    if (!event) {
        return false;
    }
    const HybridInput::PointerEvent normalized{
        .position = event->position,
        .changedButton = event->button,
        .buttons = event->buttons,
        .modifiers = event->modifiers,
    };
    if (!m_controller.active() && m_chromeRouter) {
        const auto chromeDecision = event->state == KWin::PointerButtonState::Pressed
            ? m_chromeRouter->pointerPress(normalized)
            : m_chromeRouter->pointerRelease(normalized);
        if (dispatchChrome(chromeDecision)) {
            return true;
        }
    }
    return dispatch(event->state == KWin::PointerButtonState::Pressed
                        ? m_controller.pointerPress(normalized)
                        : m_controller.pointerRelease(normalized));
}

bool KWinInteractionFilter::keyboardKey(KWin::KeyboardKeyEvent *event)
{
    if (!event) {
        return false;
    }
    if (m_chromeRouter && m_chromeRouter->active()
        && event->state != KWin::KeyboardKeyState::Released
        && event->key == Qt::Key_Escape) {
        return dispatchChrome(m_chromeRouter->cancel());
    }
    return dispatch(m_controller.keyEvent(
        {.key = event->key,
         .modifiers = event->modifiers,
         .pressed = event->state != KWin::KeyboardKeyState::Released,
         .autoRepeat = event->state == KWin::KeyboardKeyState::Repeated}));
}

bool KWinInteractionFilter::dispatchChrome(ChromePointerDecision decision)
{
    if (m_chromeSink && hasChromeDecisionOutput(decision)) {
        m_chromeSink(decision);
    }
    return decision.consumed;
}

bool KWinInteractionFilter::dispatch(HybridInput::InteractionDecision decision)
{
    if (m_sink) {
        for (const auto &intent : std::as_const(decision.intents)) {
            m_sink(intent);
        }
    }
    return decision.consumed;
}

} // namespace QindaQt::Compositor::KWinIntegration
