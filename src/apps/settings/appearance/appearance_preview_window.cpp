// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_window_preview.h"
#include "qindaqt/decoration_painter/decoration_shadow.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegion>

namespace QindaQt::Apps::SettingsAppearance {
using namespace QindaQt::Decoration;
namespace { constexpr qreal kBodyInset = 12.0; }

void AppearanceWindowPreview::paintWindow(QPainter &painter, const QRectF &frame,
                                          const QString &caption, bool active) const
{
    const auto chrome = DecorationChrome::fromVariantMap(m_chrome);
    const QPalette palette = resolvedPalette();
    painter.save();
    painter.translate(frame.topLeft());
    const QSizeF size = frame.size();
    DecorationFrameVisual state;
    state.size = size;
    state.caption = caption;
    state.font = m_toolkitFont;
    state.active = active;
    state.icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
    paintDecorationShadow(painter, decorationVisualStyleFor(chrome, false), chrome, state);
    // Client area first: the decoration's rounded title sits on top of it
    // exactly as KWin composes a decorated window.
    // The chrome's own radius and title height (ADR-0207, ADR-0264), so the
    // client area meets the title exactly where the live window's does.
    const qreal radius = decorationFrameRadius(chrome, false);
    const qreal titleHeight = decorationTitleHeight(chrome);
    QPainterPath body;
    body.addRoundedRect(QRectF(QPointF(0.0, 0.0), size), radius, radius);
    const QRectF client(1.0, titleHeight, size.width() - 2.0,
                        size.height() - titleHeight - 1.0);
    painter.save();
    painter.setClipPath(body, Qt::IntersectClip);
    painter.setClipRect(client, Qt::IntersectClip);
    // AGENT-GUARD: client backing must never fill transparent decoration
    // pixels. The live client begins below the title, and the shared cutout
    // supplies the exact Corner Bar exclusion. Keeping the title unbacked
    // also lets its authored opacity and rounded corners reveal wallpaper.
    painter.setClipRegion(QRegion(QRect(QPoint(), size.toSize()))
        - decorationTransparentTitleRegion(chrome, state), Qt::IntersectClip);
    painter.fillPath(body, palette.color(QPalette::Window));
    if (active) {
        paintToolkitSample(painter, client.adjusted(kBodyInset, kBodyInset - 2.0,
                                                    -kBodyInset, -kBodyInset), active);
    } else {
        // The background window shows content rows in muted text so the
        // inactive chrome reads against a realistic client area.
        painter.setPen(palette.color(QPalette::Disabled, QPalette::Text));
        QFont font = m_toolkitFont;
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        qreal y = client.top() + kBodyInset;
        for (const auto &line : {QStringLiteral("Report.pdf"), QStringLiteral("Notes.txt"),
                                 QStringLiteral("Photos")}) {
            painter.drawText(QPointF(client.left() + kBodyInset, y + metrics.ascent()), line);
            y += metrics.height() + 6.0;
        }
    }
    painter.restore();
    paintDecoration(painter, chrome, state, layoutDecorationButtons(chrome, size));
    painter.restore();
}

} // namespace QindaQt::Apps::SettingsAppearance
