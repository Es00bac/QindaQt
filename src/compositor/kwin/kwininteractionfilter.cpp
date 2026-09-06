// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwininteractionfilter.h"

#include "hybridchromepointerrouter.h"

#include <core/inputdevice.h>
#include <input.h>
#include <input_event.h>
#include <window.h>
#include <workspace.h>

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

// ADR-0085. Installed at GlobalShortcut order - strictly before
// InteractiveMoveResize (KWin's own MoveResizeFilter) and therefore before
// this class's own `Filter` above, which sits at Decoration order. Watches
// only for the late-Shift-mid-native-move takeover; every other event it
// sees passes through unconsumed so KWin's own move continues exactly as
// today whenever the takeover chord never completes.
class KWinInteractionFilter::EarlyTakeoverFilter final : public KWin::InputEventFilter
{
public:
    explicit EarlyTakeoverFilter(KWinInteractionFilter &owner)
        : KWin::InputEventFilter(KWin::InputFilterOrder::GlobalShortcut)
        , m_owner(owner)
    {
    }

    bool pointerMotion(KWin::PointerMotionEvent *event) override
    {
        return m_owner.earlyPointerMotion(event);
    }

    bool pointerButton(KWin::PointerButtonEvent *event) override
    {
        return m_owner.earlyPointerButton(event);
    }

    bool keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        return m_owner.earlyKeyboardKey(event);
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
    , m_lateShiftDetector(controller.pointerModifiers())
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
    m_earlyFilter = std::make_unique<EarlyTakeoverFilter>(*this);
    m_input->installInputEventFilter(m_earlyFilter.get());
}

KWinInteractionFilter::~KWinInteractionFilter()
{
    // InputEventFilter's destructor unregisters itself. Destroy it while KWin
    // input and the controller/sink collaborators are still valid.
    m_earlyFilter.reset();
    m_filter.reset();
    m_input = nullptr;
}

bool KWinInteractionFilter::installed() const
{
    return m_filter != nullptr && m_earlyFilter != nullptr;
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

bool KWinInteractionFilter::earlyKeyboardKey(KWin::KeyboardKeyEvent *event)
{
    if (!event || !m_input) {
        return false;
    }
    return observeLateShiftTakeover(event->modifiers, m_input->globalPointer());
}

bool KWinInteractionFilter::earlyPointerMotion(KWin::PointerMotionEvent *event)
{
    if (!event) {
        return false;
    }
    return observeLateShiftTakeover(event->modifiers, event->position);
}

bool KWinInteractionFilter::earlyPointerButton(KWin::PointerButtonEvent *event)
{
    if (!event) {
        return false;
    }
    return observeLateShiftTakeover(event->modifiers, event->position);
}

bool KWinInteractionFilter::observeLateShiftTakeover(Qt::KeyboardModifiers modifiers,
                                                     const QPointF &position)
{
    // AGENT-GUARD: `isInteractiveMove()` excludes resizes (gravity != None) -
    // a resize's own finishInteractiveMoveResize path never applies
    // Shift-adds-Custom-tile (that branch is guarded by `wasMove`, per
    // window.cpp), so a resize is never a competing gesture here and must
    // never be handed an identity, or the detector would arm against it.
    auto *const activeWorkspace = KWin::workspace();
    auto *const movingWindow = activeWorkspace ? activeWorkspace->moveResizeWindow() : nullptr;
    auto *const competingMove = (movingWindow && movingWindow->isInteractiveMove())
        ? movingWindow : nullptr;
    if (!m_lateShiftDetector.observe(competingMove, modifiers)) {
        return false;
    }
    // AGENT-CONTRACT: consuming this event here - strictly before
    // InteractiveMoveResize - is what keeps KWin's own tracked
    // `m_interactiveMoveResize.modifiers` frozen at whatever it was a moment
    // ago: KWin::MoveResizeFilter (the only code that ever updates it) never
    // gets to see this event at all once we return true. cancel() below runs
    // finishInteractiveMoveResize(cancel=true) synchronously, whose second,
    // unconditional `wasMove && Shift` check therefore reads the frozen
    // (pre-takeover) value and never applies Custom-tile.
    competingMove->cancelInteractiveMoveResize();
    static_cast<void>(dispatch(m_controller.adoptDrag(position)));
    return true;
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
