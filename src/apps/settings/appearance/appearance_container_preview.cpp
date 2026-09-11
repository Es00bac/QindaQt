// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_container_preview.h"

#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>

#include <cmath>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

constexpr qreal kFrameInset = 14.0;

QColor roleColor(const QVariantMap &map, const char *key, const QColor &fallback)
{
    const auto value = map.value(QString::fromLatin1(key));
    if (value.canConvert<QColor>()) {
        const auto color = value.value<QColor>();
        if (color.isValid()) {
            return color;
        }
    }
    return fallback;
}

} // namespace

AppearanceContainerPreview::AppearanceContainerPreview(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setImplicitWidth(420);
    setImplicitHeight(240);
}

void AppearanceContainerPreview::setContainerStyle(const QVariantMap &style)
{
    if (m_containerStyle == style) {
        return;
    }
    m_containerStyle = style;
    Q_EMIT containerStyleChanged();
    update();
}

void AppearanceContainerPreview::setChrome(const QVariantMap &chrome)
{
    if (m_chrome == chrome) {
        return;
    }
    m_chrome = chrome;
    Q_EMIT chromeChanged();
    update();
}

void AppearanceContainerPreview::setToolkitPalette(const QVariantMap &palette)
{
    if (m_toolkitPalette == palette) {
        return;
    }
    m_toolkitPalette = palette;
    Q_EMIT toolkitPaletteChanged();
    update();
}

void AppearanceContainerPreview::setToolkitFont(const QFont &font)
{
    if (m_toolkitFont == font) {
        return;
    }
    m_toolkitFont = font;
    Q_EMIT toolkitFontChanged();
    update();
}

void AppearanceContainerPreview::setCanvas(const QColor &canvas)
{
    if (m_canvas == canvas) {
        return;
    }
    m_canvas = canvas;
    Q_EMIT canvasChanged();
    update();
}

HybridChrome::ChromeLayoutRequest AppearanceContainerPreview::layoutRequest() const
{
    HybridChrome::ChromeLayoutRequest request;
    request.containerId = QStringLiteral("appearance-preview");
    request.outerRect = QRectF(0.0, 0.0, width(), height())
                            .adjusted(kFrameInset, kFrameInset, -kFrameInset, -kFrameInset);
    request.containerFocused = true;
    request.style = Decoration::containerStyleFromVariantMap(m_containerStyle);
    // Members keep the handlebar the compositor lays out for them (ADR-0131).
    request.metrics.memberTitleHeight = Decoration::DecorationMemberHandleHeight;
    const auto &metrics = request.metrics;
    const QRectF inner = request.outerRect.adjusted(metrics.outerBorder, metrics.outerBorder,
                                                    -metrics.outerBorder, -metrics.outerBorder);
    const qreal top = inner.top() + metrics.titleBarHeight;
    const qreal gap = metrics.dividerVisualThickness;
    const qreal split = std::round(inner.left() + inner.width() * 0.52);
    request.tabs = {{QStringLiteral("page-documents"), tr("Documents"), true},
                    {QStringLiteral("page-terminal"), tr("Terminal"), false}};
    request.members = {
        {QStringLiteral("member-report"), QStringLiteral("Report.txt"),
         QRectF(inner.left(), top, split - gap / 2.0 - inner.left(), inner.bottom() - top), true},
        {QStringLiteral("member-files"), tr("Files"),
         QRectF(split + gap / 2.0, top, inner.right() - (split + gap / 2.0),
                inner.bottom() - top), false},
    };
    request.dividers = {{QStringLiteral("divider-main"),
                         HybridChrome::DividerOrientation::Vertical, split, top,
                         inner.bottom()}};
    return request;
}

bool AppearanceContainerPreview::layoutBuilds() const
{
    return HybridChrome::ChromeLayoutEngine::build(layoutRequest()).has_value();
}

void AppearanceContainerPreview::paint(QPainter *painter)
{
    if (painter == nullptr || width() <= 0.0 || height() <= 0.0) {
        return;
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds(0.0, 0.0, width(), height());
    if (m_canvas.isValid()) {
        QPainterPath canvas;
        canvas.addRoundedRect(bounds, 8.0, 8.0);
        painter->fillPath(canvas, m_canvas);
    }
    const auto request = layoutRequest();
    const auto plan = HybridChrome::ChromeLayoutEngine::build(request);
    if (!plan) {
        painter->restore();
        return;
    }
    const auto chrome = Decoration::DecorationChrome::fromVariantMap(m_chrome);
    const QColor client = roleColor(m_toolkitPalette, "window", plan->style.palette.surface);
    const QColor muted = roleColor(m_toolkitPalette, "placeholderText",
                                   plan->style.palette.textMuted);
    // Members first: client surfaces with their native member title bars,
    // exactly where the compositor maps member windows into the holes.
    for (const auto &member : request.members) {
        painter->save();
        painter->translate(member.windowRect.topLeft());
        const QSizeF size = member.windowRect.size();
        painter->fillRect(QRectF(QPointF(0.0, 0.0), size), client);
        painter->setPen(muted);
        painter->setFont(m_toolkitFont);
        const QFontMetricsF metrics(m_toolkitFont);
        qreal y = Decoration::DecorationMemberHandleHeight + 10.0;
        for (const auto &line : {tr("Quarterly summary"), tr("Draft notes")}) {
            if (y + metrics.height() > size.height()) {
                break;
            }
            painter->drawText(QPointF(12.0, y + metrics.ascent()),
                              metrics.elidedText(line, Qt::ElideRight, size.width() - 24.0));
            y += metrics.height() + 6.0;
        }
        // Contained windows draw the handlebar the decoration plugin paints
        // for container members (ADR-0131).
        Decoration::DecorationFrameVisual frame;
        frame.size = size;
        frame.caption = member.title;
        frame.font = m_toolkitFont;
        frame.active = member.focused;
        frame.memberHandle = true;
        Decoration::paintMemberHandle(*painter, chrome, frame);
        for (const auto &button : Decoration::layoutMemberHandleButtons(chrome, size)) {
            Decoration::paintDecorationButton(*painter, chrome, frame, button);
        }
        painter->restore();
    }
    // AGENT-NOTE: ChromeRenderer clears its whole frame before painting, so
    // the container chrome gets its own layer and composites over members
    // through the transparent member holes it leaves.
    const qreal dpr = window() != nullptr ? window()->effectiveDevicePixelRatio() : 1.0;
    QImage layer((bounds.size() * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    layer.setDevicePixelRatio(dpr);
    layer.fill(Qt::transparent);
    {
        QPainter chromePainter(&layer);
        chromePainter.setFont(m_toolkitFont);
        HybridChrome::ChromeRenderer::paint(chromePainter, *plan);
    }
    painter->drawImage(QPointF(0.0, 0.0), layer);
    painter->restore();
}

} // namespace QindaQt::Apps::SettingsAppearance
