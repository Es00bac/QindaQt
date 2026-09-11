// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>

// Contained-window handlebar (ADR-0131). A container already names its pages
// in its tabs, so a member keeps only a quiet bar to grab, with miniature
// stoplights and a "more" control, instead of a full title bar.
namespace QindaQt::Decoration {

QList<DecorationButtonVisual> layoutMemberHandleButtons(const DecorationChrome &chrome,
                                                        const QSizeF &size)
{
    QList<DecorationButtonVisual> buttons;
    const qreal cell = DecorationMiniButtonCell;
    const qreal top = (DecorationMemberHandleHeight - cell) / 2.0;
    const bool right = effectiveButtonSide(chrome) == DecorationButtonSide::Right;
    const auto kinds = decorationButtonKinds(chrome);
    const auto count = static_cast<qreal>(kinds.size());
    const qreal groupWidth = count * cell + std::max(0.0, count - 1.0) * DecorationMiniButtonSpacing;
    qreal x = right ? size.width() - groupWidth - DecorationMiniButtonInset
                    : DecorationMiniButtonInset;
    for (const auto kind : kinds) {
        buttons.append({kind, QRectF(x, top, cell, cell)});
        x += cell + DecorationMiniButtonSpacing;
    }
    const qreal moreX = right ? DecorationMiniButtonInset
                              : size.width() - cell - DecorationMiniButtonInset;
    buttons.append({DecorationButtonKind::More, QRectF(moreX, top, cell, cell)});
    return buttons;
}

void paintMemberHandle(QPainter &painter, const DecorationChrome &chrome,
                       const DecorationFrameVisual &frame)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal width = frame.size.width();
    const qreal height = DecorationMemberHandleHeight;
    const qreal radius = frame.maximized ? 0.0 : DecorationMemberCornerRadius;
    const QColor bar = decorationTitleColor(chrome, frame.active);

    QPainterPath top;
    top.addRoundedRect(QRectF(0.0, 0.0, width, height + radius), radius, radius);
    QPainterPath body;
    body.addRect(QRectF(0.0, 0.0, width, height));
    const QPainterPath handle = top.intersected(body);
    if (chrome.wornLuna()) {
        // Luna paint keeps its vertical sheen at handlebar scale.
        QLinearGradient sheen(QPointF(0.0, 0.0), QPointF(0.0, height));
        sheen.setColorAt(0.0, bar.lighter(118));
        sheen.setColorAt(1.0, bar.darker(112));
        painter.fillPath(handle, sheen);
    } else {
        painter.fillPath(handle, bar);
    }
    painter.setPen(QPen(chrome.border, 0.75));
    painter.drawLine(QPointF(0.0, height - 0.5), QPointF(width, height - 0.5));

    auto style = decorationVisualStyle(chrome.border, chrome.surface, frame.maximized);
    style.cornerRadius = DecorationMemberCornerRadius;
    paintDecorationFrame(painter, QRectF(QPointF(0.0, 0.0), frame.size), style);

    // Grip: a short pill centered on the bar in caption ink.
    QColor grip = decorationCaptionColor(chrome, frame.active);
    grip.setAlphaF(frame.active ? 0.55f : 0.35f);
    const qreal gripWidth = std::min(36.0, width * 0.2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(grip);
    painter.drawRoundedRect(QRectF((width - gripWidth) / 2.0, height / 2.0 - 1.5, gripWidth, 3.0),
                            1.5, 1.5);
    painter.restore();
}

} // namespace QindaQt::Decoration
