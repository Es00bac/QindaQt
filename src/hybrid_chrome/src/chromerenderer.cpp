// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"

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
    if (control.control == ContainerControl::ToggleShade) {
        // A short chevron: pointing up (roll up) when not yet shaded, down
        // (unroll) when checked/shaded, mirroring the outer window's own
        // maximize-or-restore glyph convention.
        const qreal dy = control.checked ? -radius * 0.5 : radius * 0.5;
        painter.drawLine(QPointF(center.x() - radius, center.y() - dy),
                         QPointF(center.x(), center.y() + dy));
        painter.drawLine(QPointF(center.x() + radius, center.y() - dy),
                         QPointF(center.x(), center.y() + dy));
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

// Theming v2 material helpers (ADR-0207).
QColor withMaterialOpacity(QColor color, const ChromeMaterial &material)
{
    color.setAlphaF(static_cast<float>(color.alphaF()
                                       * std::clamp(material.opacity, 0.0, 1.0)));
    return color;
}

QColor materialTint(const ChromeMaterial &material)
{
    QColor tint = material.tint;
    if (tint.alphaF() >= 1.0F) {
        tint.setAlphaF(0.35F);
    }
    return tint;
}

// The title row and tab strip fills. A translucent row repaints its rect
// from transparent (inside the frame clip, so corners stay round) instead of
// stacking on the frame fill, which would double the alpha wherever rows
// overlap; the catch light sits directly under the three-row identity
// stripe that paintIdentityFrame lays along the title row's top edge.
void paintMaterialRows(QPainter &painter, const ChromeRenderPlan &plan, qreal frameRadius)
{
    const auto &material = plan.style.material;
    const auto fillRow = [&](const QRectF &rect, const QColor &color) {
        if (material.opacity < 1.0) {
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            if (material.tint.isValid()) {
                painter.fillRect(rect, materialTint(material));
            }
        }
        painter.fillRect(rect, withMaterialOpacity(color, material));
    };
    fillRow(plan.outerTitleBar, plan.style.palette.surfaceRaised);
    if (plan.tabStrip.isValid() && !plan.tabStrip.isEmpty()) {
        fillRow(plan.tabStrip, plan.style.palette.surface);
    }
    if (material.highlight && plan.outerTitleBar.height() > 4.0) {
        const QColor raised = plan.style.palette.surfaceRaised;
        painter.fillRect(QRectF(plan.outerTitleBar.left() + frameRadius / 2.0,
                                plan.outerTitleBar.top() + 3.0,
                                plan.outerTitleBar.width() - frameRadius, 1.0),
                         QColor(255, 255, 255, qGray(raised.rgb()) < 128 ? 46 : 120));
    }
}

// Identity frame, focus glow, title-row stripe, and the keyboard selection
// chip (ADR-0139). Called after the row content so the stripe stays crisp.
void paintIdentityFrame(QPainter &painter, const ChromeRenderPlan &plan,
                        const QPainterPath &framePath)
{
    painter.setBrush(Qt::NoBrush);
    const qreal frameThickness = plan.containerFocused && !plan.shaded
        ? std::max(plan.borderHairline * 3.0, 3.0)
        : std::max(plan.borderHairline * 2.0, 2.0);
    // The material's border strength scales the identity frame's alpha
    // (ADR-0207); 1.0 keeps the shipped frame.
    QColor frameColor = plan.containerFocused || plan.shaded
        ? plan.identity.border : plan.identity.borderDimmed;
    frameColor.setAlphaF(static_cast<float>(
        frameColor.alphaF() * std::clamp(plan.style.material.border, 0.0, 1.0)));
    painter.setPen(QPen(frameColor, frameThickness));
    painter.drawPath(framePath);
    if (plan.containerFocused && !plan.shaded) {
        auto glowPen = QPen(plan.identity.glow, std::max(frameThickness * 2.0, 6.0));
        glowPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(glowPen);
        painter.drawPath(framePath);
    }

    if (!plan.shaded) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::NoPen);
        painter.setBrush(plan.identity.border);
        const qreal radius = plan.metrics.cornerRadius;
        painter.drawRect(QRectF(plan.outerTitleBar.left() + radius,
                                plan.outerTitleBar.top(),
                                plan.outerTitleBar.width() - radius * 2.0,
                                std::min(3.0, plan.outerTitleBar.height())));
        painter.restore();
    }

    if (plan.indexBadgeRect.isValid()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(plan.identity.border);
        painter.drawRoundedRect(plan.indexBadgeRect, 9.0, 9.0);
        painter.setPen(plan.identity.indexBadgeInk);
        painter.drawText(plan.indexBadgeRect, Qt::AlignCenter,
                         QString::number(plan.indexBadge));
        painter.restore();
    }
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
    // Theming v2 material (ADR-0207): the surface carries the document's
    // opacity and optional tint; a square badge style flattens the shaded
    // frame. Defaults reproduce the shipped chrome pixel for pixel.
    const auto &material = plan.style.material;
    const qreal frameRadius = plan.shaded && material.squareBadge
        ? std::min<qreal>(3.0, plan.metrics.cornerRadius) : plan.metrics.cornerRadius;
    QPainterPath framePath;
    framePath.addRoundedRect(plan.outerFrame, frameRadius, frameRadius);
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
    if (material.tint.isValid() && material.opacity < 1.0) {
        painter.fillPath(framePath, materialTint(material));
    }
    painter.fillPath(framePath, withMaterialOpacity(plan.style.palette.surface, material));
    if (!plan.shaded) {
        paintMaterialRows(painter, plan, frameRadius);
        // AGENT-CONTRACT: containerTitle is the user's rename override (see
        // ContainerAppearance); it paints in the leftover outer-title drag
        // region beside tabs/controls, in the resolved identity text color.
        // Empty title is a silent no-op, matching every container that never
        // renamed.
        if (!plan.containerTitle.isEmpty() && plan.outerTitleDragRect.width() > 0.0) {
            paintLabel(painter, plan.outerTitleDragRect, plan.containerTitle,
                      plan.identity.textOnFill);
        }
    }

    if (!plan.shaded) {
        for (const auto &tab : plan.tabs) {
            const auto fill = tab.active ? plan.identity.tabTint
                                         : plan.style.palette.surface;
            // An inactive pill is the strip's own surface color: invisible
            // on an opaque strip, and on a translucent one it would only
            // stack alpha, so it is skipped there. The active pill keeps its
            // opaque identity tint.
            if (tab.active || material.opacity >= 1.0) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(fill);
                painter.drawRoundedRect(tab.rect.adjusted(0.0, 2.0, 0.0, -2.0), 6.0, 6.0);
            }
            paintLabel(painter, tab.rect, tab.title,
                       tab.active ? plan.identity.textOnFill
                                  : plan.style.palette.textMuted);
            // AGENT-CONTRACT: The active page underline derives from the same
            // identity shades as the frame and stripe (ADR-0139). Keep it
            // inside the tab geometry so native member frames remain
            // transparent and KWin owns focus/input.
            if (tab.active) {
                const qreal inset = std::min(4.0, tab.rect.width() / 4.0);
                const qreal thickness = std::min(3.0, tab.rect.height());
                if (tab.rect.width() > inset * 2.0 && thickness > 0.0) {
                    painter.save();
                    painter.setRenderHint(QPainter::Antialiasing, false);
                    painter.setBrush(plan.identity.border);
                    painter.drawRect(QRectF(tab.rect.left() + inset,
                                           tab.rect.bottom() - thickness,
                                           tab.rect.width() - inset * 2.0,
                                           thickness));
                    painter.restore();
                }
            }
        }
    } else {
        // ADR-0139: the shaded plan's tabs are the badge pills.
        ChromeShadedBadge::paint(painter, plan);
    }

    // AGENT-GUARD: Member title bars are painted by KDecoration. Drawing the
    // plan's locator here duplicates captions and obscures native close,
    // minimize, maximize/restore, and decoration drag handling.
    for (const auto &button : plan.buttons) {
        const bool hovered = targetMatchesButton(state.hoveredTarget, button.action);
        const bool pressed = targetMatchesButton(state.pressedTarget, button.action);
        const bool glyphVisible = button.glyphVisibleWhenIdle || state.controlsHovered
            || hovered || pressed;
        if (plan.style.buttonPainter) {
            // ADR-0264: a named style paints through the decoration painter.
            plan.style.buttonPainter(painter, plan, button, hovered, pressed, glyphVisible);
            continue;
        }
        painter.setPen(QPen(plan.style.palette.border, plan.borderHairline));
        painter.setBrush(button.fillColor);
        if (plan.style.buttonStyle == ButtonStyle::TrafficLights) {
            painter.drawEllipse(button.rect);
        } else {
            painter.drawRoundedRect(button.rect, 3.0, 3.0);
        }
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
    paintIdentityFrame(painter, plan, framePath);

    // AGENT-CONTRACT: A focused member cue is clipped to the paintable side
    // of its native frame. This keeps client content and KDecoration pixels
    // transparent while still identifying one tiled member when titles are
    // hidden or visually compressed.
    for (const auto &member : plan.members) {
        if (!member.focused) {
            continue;
        }
        const qreal ring = std::max(plan.borderHairline * 3.0, 3.0);
        QPainterPath ringPath;
        ringPath.addRect(member.windowRect.adjusted(-ring, -ring, ring, ring));
        QPainterPath nativeFrame;
        nativeFrame.addRect(member.windowRect);
        painter.setPen(Qt::NoPen);
        painter.fillPath(ringPath.subtracted(nativeFrame).intersected(paintClip),
                         plan.identity.border);
    }
    painter.restore();
}

} // namespace QindaQt::HybridChrome
