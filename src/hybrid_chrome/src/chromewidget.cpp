// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromewidget.h"

#include "qindaqt/hybrid_chrome/chromehittest.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <utility>

namespace QindaQt::HybridChrome {
namespace {

bool sameTarget(const ChromeHitTarget &first, const ChromeHitTarget &second)
{
    return first.kind == second.kind && first.stableId == second.stableId
        && first.logicalIndex == second.logicalIndex && first.action == second.action
        && first.resizeEdges == second.resizeEdges;
}

} // namespace

ChromeWidget::ChromeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    qRegisterMetaType<ChromeHitTarget>();
    qRegisterMetaType<ChromeDragEvent>();
}

void ChromeWidget::setRenderPlan(ChromeRenderPlan plan)
{
    m_plan = std::move(plan);
    m_paintState = {};
    update();
}

ChromeHitTarget ChromeWidget::hitTargetAt(const QPointF &logicalPosition) const
{
    return ChromeHitTester::hitTest(m_plan, logicalPosition);
}

bool ChromeWidget::isDragTarget(const ChromeHitTarget &target)
{
    switch (target.kind) {
    case HitKind::Tab:
    case HitKind::Divider:
    case HitKind::MemberTitleDrag:
    case HitKind::OuterTitleDrag:
    case HitKind::OuterResize:
        return true;
    case HitKind::None:
    case HitKind::WindowButton:
    case HitKind::Client:
        return false;
    }
    return false;
}

void ChromeWidget::finishPointerSequence(bool cancel, const QPointF &globalPosition)
{
    if (!m_pointerPressed) {
        return;
    }
    const auto target = m_paintState.pressedTarget;
    const bool completedDrag = m_dragActive;
    m_pointerPressed = false;
    m_dragActive = false;
    m_paintState.pressedTarget = {};
    m_lastGlobalPosition = globalPosition;
    update();
    if (completedDrag) {
        Q_EMIT dragLifecycle({target, cancel ? DragPhase::Cancel : DragPhase::Commit,
                              globalPosition, globalPosition - m_dragOriginGlobal});
    }
}

void ChromeWidget::cancelActiveDrag()
{
    finishPointerSequence(true, m_lastGlobalPosition);
}

bool ChromeWidget::event(QEvent *event)
{
    if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate
        || event->type() == QEvent::Hide) {
        cancelActiveDrag();
    }
    return QWidget::event(event);
}

void ChromeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    ChromeRenderer::paint(painter, m_plan, m_paintState);
}

void ChromeWidget::updateHover(const QPointF &logicalPosition)
{
    setPointerHoverTarget(hitTargetAt(logicalPosition));
}

void ChromeWidget::setPointerHoverTarget(ChromeHitTarget next)
{
    // AGENT-NOTE: Qinda macOS reveals all traffic-light glyphs when any one of
    // the controls is hovered, matching a control-cluster rather than per-icon
    // hover model.
    const bool controlsHovered = next.kind == HitKind::WindowButton;
    if (!sameTarget(next, m_paintState.hoveredTarget)
        || controlsHovered != m_paintState.controlsHovered) {
        m_paintState.hoveredTarget = std::move(next);
        m_paintState.controlsHovered = controlsHovered;
        update();
    }
}

void ChromeWidget::mouseMoveEvent(QMouseEvent *event)
{
    updateHover(event->position());
    if (m_pointerPressed && isDragTarget(m_paintState.pressedTarget)
        && (event->buttons() & Qt::LeftButton)) {
        const auto globalPosition = event->globalPosition();
        if (!m_dragActive
            && QLineF(m_dragOriginGlobal, globalPosition).length()
                >= QApplication::startDragDistance()) {
            m_dragActive = true;
            Q_EMIT dragLifecycle({m_paintState.pressedTarget, DragPhase::Begin,
                                  globalPosition, globalPosition - m_dragOriginGlobal});
        } else if (m_dragActive && globalPosition != m_lastGlobalPosition) {
            Q_EMIT dragLifecycle({m_paintState.pressedTarget, DragPhase::Update,
                                  globalPosition, globalPosition - m_dragOriginGlobal});
        }
        m_lastGlobalPosition = globalPosition;
    }
    QWidget::mouseMoveEvent(event);
}

void ChromeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_pointerPressed) {
            cancelActiveDrag();
        }
        m_paintState.pressedTarget = hitTargetAt(event->position());
        m_dragOriginGlobal = event->globalPosition();
        m_lastGlobalPosition = m_dragOriginGlobal;
        m_pointerPressed = m_paintState.pressedTarget.isInteractive();
        m_dragActive = false;
        update();
    }
    QWidget::mousePressEvent(event);
}

void ChromeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const auto released = hitTargetAt(event->position());
        const auto pressed = m_paintState.pressedTarget;
        const bool completedDrag = m_dragActive;
        finishPointerSequence(false, event->globalPosition());
        // AGENT-GUARD: A drag commit and a click activation are mutually
        // exclusive; emitting both can detach and then immediately activate.
        if (!completedDrag && pressed.isInteractive() && sameTarget(released, pressed)) {
            Q_EMIT targetActivated(released);
        }
        updateHover(event->position());
    }
    QWidget::mouseReleaseEvent(event);
}

void ChromeWidget::leaveEvent(QEvent *event)
{
    m_paintState.hoveredTarget = {};
    m_paintState.controlsHovered = false;
    update();
    QWidget::leaveEvent(event);
}

} // namespace QindaQt::HybridChrome
