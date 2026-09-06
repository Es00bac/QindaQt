// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>

namespace QindaQt::HybridChrome {
namespace {

bool targetMatchesButton(const ChromeHitTarget &target, WindowAction action)
{
    return target.kind == HitKind::WindowButton && target.action && *target.action == action;
}

bool targetMatchesControl(const ChromeHitTarget &target, ContainerControl control)
{
    return target.kind == HitKind::ContainerControl
        && target.containerControl && *target.containerControl == control;
}

void paintContainerControlGlyph(QPainter &painter,
                                const ContainerControlGeometry &control,
                                const ChromeRenderPlan &plan)
{
    const auto center = control.rect.center();
    const qreal radius = control.rect.width() * 0.28;
    QPen pen(control.checked ? plan.style.palette.accent
                             : plan.style.palette.textMuted);
    pen.setWidthF(std::max(plan.borderHairline, 1.2));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (control.control == ContainerControl::ToggleMemberTitles) {
        const QRectF frame(center.x() - radius, center.y() - radius,
                           radius * 2.0, radius * 2.0);
        painter.drawRect(frame);
        painter.drawLine(frame.topLeft(), frame.topRight());
        return;
    }
    const qreal dotRadius = std::max(1.0, plan.borderHairline);
    painter.setPen(Qt::NoPen);
    painter.setBrush(plan.style.palette.text);
    for (const qreal offset : {-radius, 0.0, radius}) {
        painter.drawEllipse(QPointF(center.x() + offset, center.y()),
                            dotRadius, dotRadius);
    }
}

void paintActionGlyph(QPainter &painter,
                      const WindowButtonGeometry &button,
                      const ChromeRenderPlan &plan)
{
    const auto center = button.rect.center();
    const qreal radius = button.rect.width() * 0.22;
    QPen pen(plan.style.palette.text);
    pen.setWidthF(std::max(plan.borderHairline, 1.2));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (button.action) {
    case WindowAction::Close:
        painter.drawLine(QPointF(center.x() - radius, center.y() - radius),
                         QPointF(center.x() + radius, center.y() + radius));
        painter.drawLine(QPointF(center.x() + radius, center.y() - radius),
                         QPointF(center.x() - radius, center.y() + radius));
        break;
    case WindowAction::Minimize:
        painter.drawLine(QPointF(center.x() - radius, center.y() + radius * 0.55),
                         QPointF(center.x() + radius, center.y() + radius * 0.55));
        break;
    case WindowAction::Maximize:
        painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                radius * 2.0, radius * 2.0));
        break;
    case WindowAction::Restore:
        painter.drawRect(QRectF(center.x() - radius * 0.75, center.y() - radius * 0.35,
                                radius * 1.45, radius * 1.45));
        painter.drawRect(QRectF(center.x() - radius * 0.35, center.y() - radius * 0.75,
                                radius * 1.45, radius * 1.45));
        break;
    }
}

void paintLabel(QPainter &painter,
                const QRectF &rect,
                const QString &text,
                const QColor &color)
{
    painter.setPen(color);
    const QFontMetricsF metrics(painter.font());
    const auto elided = metrics.elidedText(text, Qt::ElideRight,
                                           std::max(0, qRound(rect.width() - 12.0)));
    painter.drawText(rect.adjusted(6.0, 0.0, -6.0, 0.0), Qt::AlignCenter, elided);
}

} // namespace

void ChromeRenderer::paint(QPainter &painter,
                           const ChromeRenderPlan &plan,
                           const ChromePaintState &state)
{
    painter.save();
    // AGENT-GUARD: Scene chrome is an ARGB image that can be reused after a
    // reflow. Clear with Source before painting so pixels that become member
    // holes cannot retain an opaque surface from the previous plan.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(plan.outerFrame, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath framePath;
    framePath.addRoundedRect(plan.outerFrame, plan.metrics.cornerRadius,
                             plan.metrics.cornerRadius);
    QPainterPath paintClip = framePath;
    for (const auto &member : plan.members) {
        QPainterPath nativeWindow;
        nativeWindow.addRect(member.windowRect);
        paintClip = paintClip.subtracted(nativeWindow);
    }
    // AGENT-CONTRACT: Every native member frame is a complete transparent
    // hole. The scene item may cover the group's whole outer geometry, but it
    // must never blend over application content or KDecoration pixels.
    painter.setClipPath(paintClip);
    painter.fillPath(framePath, plan.style.palette.surface);
    painter.fillRect(plan.outerTitleBar, plan.style.palette.surfaceRaised);
    if (plan.tabStrip.isValid() && !plan.tabStrip.isEmpty()) {
        painter.fillRect(plan.tabStrip, plan.style.palette.surface);
    }

    for (const auto &tab : plan.tabs) {
        const auto fill = tab.active ? plan.style.palette.surfaceRaised
                                     : plan.style.palette.surface;
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(tab.rect.adjusted(0.0, 2.0, 0.0, -2.0), 6.0, 6.0);
        paintLabel(painter, tab.rect, tab.title,
                   tab.active ? plan.style.palette.text : plan.style.palette.textMuted);
        // AGENT-CONTRACT: The active page cue is resolved from the same theme
        // accent as dividers and controls. Keep it inside the tab geometry so
        // native member frames remain transparent and KWin owns focus/input.
        if (tab.active) {
            const qreal inset = std::min(4.0, tab.rect.width() / 4.0);
            const qreal thickness = std::min(2.0, tab.rect.height());
            if (tab.rect.width() > inset * 2.0 && thickness > 0.0) {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setBrush(plan.style.palette.accent);
                painter.drawRect(QRectF(tab.rect.left() + inset,
                                       tab.rect.bottom() - thickness,
                                       tab.rect.width() - inset * 2.0,
                                       thickness));
                painter.restore();
            }
        }
    }

    // AGENT-GUARD: Member title bars are painted by KDecoration. Drawing the
    // plan's locator here duplicates captions and obscures native close,
    // minimize, maximize/restore, and decoration drag handling.
    for (const auto &button : plan.buttons) {
        painter.setPen(QPen(plan.style.palette.border, plan.borderHairline));
        painter.setBrush(button.fillColor);
        if (plan.style.buttonStyle == ButtonStyle::TrafficLights) {
            painter.drawEllipse(button.rect);
        } else {
            painter.drawRoundedRect(button.rect, 3.0, 3.0);
        }
        const bool glyphVisible = button.glyphVisibleWhenIdle || state.controlsHovered
            || targetMatchesButton(state.hoveredTarget, button.action)
            || targetMatchesButton(state.pressedTarget, button.action);
        if (glyphVisible) {
            paintActionGlyph(painter, button, plan);
        }
    }
    for (const auto &control : plan.controls) {
        const bool highlighted = targetMatchesControl(state.hoveredTarget,
                                                       control.control)
            || targetMatchesControl(state.pressedTarget, control.control);
        painter.setPen(QPen(highlighted ? plan.style.palette.accent
                                       : plan.style.palette.border,
                            plan.borderHairline));
        painter.setBrush(highlighted ? plan.style.palette.surfaceRaised
                                     : plan.style.palette.surface);
        painter.drawRoundedRect(control.rect, 4.0, 4.0);
        paintContainerControlGlyph(painter, control, plan);
    }

    painter.setClipping(false);
    // Divider visuals intentionally bridge the transparent native-window
    // holes. They are the shared seam between adjacent members, not a fill
    // over either member's decoration or client surface.
    painter.setPen(Qt::NoPen);
    painter.setBrush(plan.style.palette.accent);
    for (const auto &divider : plan.dividers) {
        painter.drawRect(divider.visualRect);
    }
    painter.setBrush(Qt::NoBrush);
    const qreal frameThickness = plan.containerFocused
        ? std::max(plan.borderHairline * 2.0, 2.0)
        : plan.borderHairline;
    painter.setPen(QPen(plan.style.palette.border, frameThickness));
    painter.drawPath(framePath);

    // AGENT-CONTRACT: A focused container is marked on its shared top edge;
    // the complete neutral frame remains visible so a focused member's ring
    // can distinguish the left/right or top/bottom sibling boundary.
    if (plan.containerFocused) {
        const qreal radius = plan.metrics.cornerRadius;
        painter.setPen(QPen(plan.style.palette.accent, frameThickness));
        painter.drawLine(QPointF(plan.outerFrame.left() + radius, plan.outerFrame.top()),
                         QPointF(plan.outerFrame.right() - radius, plan.outerFrame.top()));
    }

    // AGENT-CONTRACT: A focused member cue is clipped to the paintable side
    // of its native frame. This keeps client content and KDecoration pixels
    // transparent while still identifying one tiled member when titles are
    // hidden or visually compressed.
    for (const auto &member : plan.members) {
        if (!member.focused) {
            continue;
        }
        const qreal ring = std::max(plan.borderHairline * 2.0, 2.0);
        QPainterPath ringPath;
        ringPath.addRect(member.windowRect.adjusted(-ring, -ring, ring, ring));
        QPainterPath nativeFrame;
        nativeFrame.addRect(member.windowRect);
        painter.setPen(Qt::NoPen);
        painter.fillPath(ringPath.subtracted(nativeFrame).intersected(paintClip),
                         plan.style.palette.accent);
    }
    painter.restore();
}

} // namespace QindaQt::HybridChrome
