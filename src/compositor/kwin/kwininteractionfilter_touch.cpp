// SPDX-License-Identifier: GPL-3.0-or-later
// The touch half of KWinInteractionFilter (ADR-0193): fingers over shared
// chrome become the router's left button, a held finger becomes the
// container's right click, and a second finger's swipe becomes the wheel.
#include "kwininteractionfilter.h"

#include "hybridchromepointerrouter.h"

#include <core/output.h>
#include <input.h>
#include <input_event.h>
#include <window.h>
#include <workspace.h>

#include <algorithm>
#include <chrono>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// Chrome hit targets grow to this radius for a finger (hit test only).
constexpr qreal kTouchHitRadius = 40.0;

qint64 milliseconds(std::chrono::microseconds time)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
}

// AGENT-CONTRACT (ADR-0193): the ring only widens chrome into empty space.
// The chrome resolver cannot tell empty desktop from a window it does not
// manage (both are "no hit"), so a finger over any real window that is not
// the desktop keeps the exact test only.
bool nothingUnderFinger(const QPointF &position)
{
    auto *const input = KWin::input();
    KWin::Window *const under = input ? input->findToplevel(position) : nullptr;
    return under == nullptr || under->isDeleted() || under->isDesktop();
}

std::optional<QRectF> outputClip(const QPointF &position)
{
    auto *const workspace = KWin::workspace();
    KWin::LogicalOutput *const output = workspace ? workspace->outputAt(position) : nullptr;
    if (output == nullptr) {
        return std::nullopt;
    }
    return QRectF(output->geometry());
}

HybridInput::PointerEvent syntheticPointer(const QPointF &position, Qt::MouseButton changed,
                                           Qt::MouseButtons buttons)
{
    HybridInput::PointerEvent event;
    event.position = position;
    event.changedButton = changed;
    event.buttons = buttons;
    event.modifiers = Qt::NoModifier;
    return event;
}

} // namespace

bool KWinInteractionFilter::touchDown(KWin::TouchDownEvent *event)
{
    if (event == nullptr || m_chromeRouter == nullptr) {
        return false;
    }
    const qint64 now = milliseconds(event->time);
    if (m_touch.isStale(now)) {
        // The grab went elsewhere and no up or cancel ever came: abandon the
        // old gesture so this finger is judged fresh instead of eaten.
        (void)m_touch.cancel();
        finishTouchGesture();
    }
    // A finger has to land on a chrome target the router owns; anything else
    // is KWin's (decorations, clients, other windows) exactly as before touch
    // existed here. The ring applies only where nothing is underneath, and
    // its probes stay on the finger's own output.
    const qreal radius = nothingUnderFinger(event->pos) ? kTouchHitRadius : 0.0;
    const auto hit = m_chromeRouter->hitNear(event->pos, radius, outputClip(event->pos));
    if (m_touch.active()) {
        // A second finger joins the gesture only on the same container's
        // chrome; anywhere else it is KWin's and is not consumed.
        const bool joins = hit.has_value() && hit->hit.containerId == m_touchContainerId;
        return m_touch.down(event->id, event->pos, now, joins, false).consumed;
    }
    if (!hit) {
        return false;
    }
    const auto decision = m_touch.down(event->id, event->pos, now, true,
                                       HybridChromePointerRouter::rollTarget(hit->hit.target));
    if (!decision.consumed || decision.gesture != TouchGesture::Press) {
        return decision.consumed;
    }
    m_touchOffset = hit->position - event->pos;
    m_touchContainerId = hit->hit.containerId;
    m_touchTarget = hit->hit.target;
    (void)dispatchChrome(m_chromeRouter->pointerPress(
        syntheticPointer(hit->position, Qt::LeftButton, Qt::LeftButton)));
    if (!m_touchTimerConnected) {
        m_touchTimer.setSingleShot(true);
        QObject::connect(&m_touchTimer, &QTimer::timeout, &m_touchTimer,
                         [this] { expireTouchLongPress(); });
        m_touchTimerConnected = true;
    }
    if (const auto due = m_touch.longPressDueMs()) {
        m_touchTimer.start(static_cast<int>(std::max<qint64>(0, *due - now)));
    }
    return true;
}

bool KWinInteractionFilter::touchMotion(KWin::TouchMotionEvent *event)
{
    if (event == nullptr || m_chromeRouter == nullptr || !m_touch.active()) {
        return false;
    }
    const auto decision = m_touch.motion(event->id, event->pos, milliseconds(event->time));
    switch (decision.gesture) {
    case TouchGesture::Move:
        (void)dispatchChrome(m_chromeRouter->pointerMove(
            syntheticPointer(decision.position + m_touchOffset, Qt::NoButton, Qt::LeftButton)));
        break;
    case TouchGesture::SwipeUp:
    case TouchGesture::SwipeDown: {
        // The wheel equivalent: the press is abandoned, the roll requested.
        m_touchTimer.stop();
        (void)dispatchChrome(m_chromeRouter->cancel());
        ChromePointerDecision roll;
        roll.consumed = true;
        roll.shadeRequests.append(
            {m_touchContainerId, decision.gesture == TouchGesture::SwipeUp});
        (void)dispatchChrome(roll);
        break;
    }
    case TouchGesture::None:
    case TouchGesture::Press:
    case TouchGesture::Release:
    case TouchGesture::LongPress:
    case TouchGesture::Cancel:
        break;
    }
    return decision.consumed;
}

bool KWinInteractionFilter::touchUp(KWin::TouchUpEvent *event)
{
    if (event == nullptr || m_chromeRouter == nullptr || !m_touch.active()) {
        return false;
    }
    const auto decision = m_touch.up(event->id, milliseconds(event->time));
    if (decision.gesture == TouchGesture::Release) {
        m_touchTimer.stop();
        (void)dispatchChrome(m_chromeRouter->pointerRelease(
            syntheticPointer(decision.position + m_touchOffset, Qt::LeftButton, Qt::NoButton)));
        m_touchContainerId.clear();
        m_touchTarget = {};
        m_touchOffset = {};
    } else if (!m_touch.active()) {
        // The gesture ended without a release the router should see (a long
        // press or a swipe already spent it): leave nothing pressed behind.
        finishTouchGesture();
    }
    return decision.consumed;
}

bool KWinInteractionFilter::touchCancel()
{
    if (m_chromeRouter == nullptr || !m_touch.active()) {
        return false;
    }
    (void)m_touch.cancel();
    finishTouchGesture();
    // A cancel is a seat-wide reset, not a consumed down: every later filter
    // and the seat must see it too.
    return false;
}

void KWinInteractionFilter::expireTouchLongPress()
{
    if (m_chromeRouter == nullptr) {
        return;
    }
    // The timer fired at the due time the policy asked for, so that is the
    // clock reading it needs; the finger's own timestamps do not tick while
    // it holds still.
    const auto due = m_touch.longPressDueMs();
    if (!due) {
        return;
    }
    const auto expired = m_touch.expire(*due);
    if (!expired || expired->gesture != TouchGesture::LongPress) {
        return;
    }
    // The held finger is the container's right click: the press is
    // abandoned so the lift never activates, then the menu opens where the
    // finger rests. Targets without a menu (buttons, dividers) just cancel.
    (void)dispatchChrome(m_chromeRouter->cancel());
    if (HybridChromePointerRouter::contextMenuTarget(m_touchTarget)) {
        ChromePointerDecision menu;
        menu.consumed = true;
        menu.contextMenus.append({m_touchContainerId, expired->position + m_touchOffset});
        (void)dispatchChrome(menu);
    }
}

void KWinInteractionFilter::finishTouchGesture()
{
    m_touchTimer.stop();
    if (m_chromeRouter != nullptr && m_chromeRouter->active()) {
        (void)dispatchChrome(m_chromeRouter->cancel());
    }
    m_touchContainerId.clear();
    m_touchTarget = {};
    m_touchOffset = {};
}

} // namespace QindaQt::Compositor::KWinIntegration
