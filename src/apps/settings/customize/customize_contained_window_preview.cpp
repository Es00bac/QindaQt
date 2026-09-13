// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_contained_window_preview.h"

#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QPainter>
#include <QPainterPath>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

using namespace QindaQt::Decoration;

QColor mapColor(const QVariantMap &map, const char *key, const QColor &fallback)
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

CustomizeContainedWindowPreview::CustomizeContainedWindowPreview(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setImplicitWidth(300);
    setImplicitHeight(180);
}

void CustomizeContainedWindowPreview::setChrome(const QVariantMap &chrome)
{
    if (chrome == m_chrome) {
        return;
    }
    m_chrome = chrome;
    Q_EMIT chromeChanged();
    update();
}

void CustomizeContainedWindowPreview::paint(QPainter *painter)
{
    if (painter == nullptr || width() <= 0.0 || height() <= 0.0) {
        return;
    }
    const DecorationChrome chrome = DecorationChrome::fromVariantMap(m_chrome);
    const QSizeF size(width(), height());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Client area first, exactly the order a real contained window composes
    // in: the handlebar paints its own frame/seam over this on top.
    QPainterPath body;
    body.addRoundedRect(QRectF(QPointF(0.0, 0.0), size),
                        DecorationMemberCornerRadius, DecorationMemberCornerRadius);
    painter->fillPath(body, mapColor(m_chrome, "surface", chrome.surface));
    painter->save();
    painter->setClipPath(body, Qt::IntersectClip);
    painter->setPen(mapColor(m_chrome, "textMuted", chrome.textMuted));
    qreal y = DecorationMemberHandleHeight + 14.0;
    for (const qreal fraction : {0.72, 0.5}) {
        const QRectF line(12.0, y, (size.width() - 24.0) * fraction, 6.0);
        painter->fillRect(line, painter->pen().color());
        y += 16.0;
    }
    painter->restore();

    DecorationFrameVisual frame;
    frame.size = size;
    frame.active = true;
    frame.memberHandle = true;
    paintMemberHandle(*painter, chrome, frame);
    for (const auto &button : layoutMemberHandleButtons(chrome, size)) {
        paintDecorationButton(*painter, chrome, frame, button);
    }

    painter->restore();
}

} // namespace QindaQt::Apps::SettingsCustomize
