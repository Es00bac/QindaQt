// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecoration.h"

#include <QGuiApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QStyleHints>
#include <QWheelEvent>

#include <utility>

// QindaDecoration's pointer input: the context-menu press, the title
// double-click (ADR-0264) and the wheel. Split beside the geometry file for
// size; the class is declared in qindadecoration.h.
namespace QindaQt::Decoration {

void QindaDecoration::wheelEvent(QWheelEvent *event)
{
    // ADR-0203 supersedes ADR-0131's decoration roll-up: a modifier-free wheel
    // over an independent window's title bar is consumed by the QindaQt
    // compositor plugin (KWinInteractionFilter) before it reaches KDecoration
    // and rolls the window up to its icon chip; a wheel over a container
    // member's handlebar is consumed by the chrome router and rolls the whole
    // container. The decoration therefore never shades on wheel any more.
    // KWin 6.6 has no native shade; the roll-up button and a roll-up title
    // double-click (ADR-0264) reuse the compositor's roll-up instead.
    KDecoration3::Decoration::wheelEvent(event);
}

void QindaDecoration::mouseMoveEvent(QMouseEvent *event)
{
    if (m_contextPressPosition.has_value()) {
        event->accept();
        return;
    }
    KDecoration3::Decoration::mouseMoveEvent(event);
}

void QindaDecoration::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        m_contextPressPosition = event->position();
        event->accept();
        return;
    }
    m_contextPressPosition.reset();
    KDecoration3::Decoration::mousePressEvent(event);
    // ADR-0264: a chosen title double-click runs here. Accepting the second
    // press keeps KWin from also running its own double-click command and
    // from starting a move; an unchosen one ("theme") stays KWin's.
    if (event->button() == Qt::LeftButton && !event->isAccepted()
        && runTitleDoubleClick(event->position())) {
        event->accept();
    }
}

bool QindaDecoration::runTitleDoubleClick(const QPointF &position)
{
    const auto chrome = chromeState();
    if (containerMember() || chrome.titleDoubleClick.isEmpty()
        || !titleBar().contains(position)) {
        m_titleClickClock.invalidate();
        return false;
    }
    const auto *hints = QGuiApplication::styleHints();
    const bool second = m_titleClickClock.isValid()
        && m_titleClickClock.elapsed() <= hints->mouseDoubleClickInterval()
        && QLineF(m_titleClickPosition, position).length() <= hints->startDragDistance();
    if (!second) {
        m_titleClickClock.start();
        m_titleClickPosition = position;
        return false;
    }
    m_titleClickClock.invalidate();
    // The window manager's own actions: KWin minimize and maximize, and the
    // compositor's roll-up to the icon (KWin 6.6 has no native shade).
    if (chrome.titleDoubleClick == QLatin1String("minimize")) {
        requestMinimize();
    } else if (chrome.titleDoubleClick == QLatin1String("roll-up")) {
        requestRollUp();
    } else {
        requestToggleMaximization(Qt::LeftButton);
    }
    return true;
}

void QindaDecoration::requestRollUp()
{
    // Deferred: rolling up hides the window, which must not happen inside
    // KWin's dispatch of this decoration's own input event.
    QMetaObject::invokeMethod(this, &QindaDecoration::qindaqtRollUpRequested,
                              Qt::QueuedConnection);
}

void QindaDecoration::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton
        && m_contextPressPosition.has_value()) {
        const auto pressedAt = std::exchange(m_contextPressPosition, std::nullopt);
        if (QLineF(*pressedAt, event->position()).length() <= 8.0) {
            showContextMenu(event->position());
        }
        event->accept();
        return;
    }
    KDecoration3::Decoration::mouseReleaseEvent(event);
}

} // namespace QindaQt::Decoration
