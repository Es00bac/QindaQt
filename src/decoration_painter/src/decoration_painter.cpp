// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace QindaQt::Decoration {
namespace {

QColor inkShadow(const QColor &surface)
{
    const QColor source = surface.isValid() ? surface : QColor(Qt::black);
    return QColor::fromRgb(source.red() / 4, source.green() / 4,
                           source.blue() / 4);
}

constexpr qreal kPi = 3.14159265358979323846;

QPointF onBar(const QRectF &bar, QRandomGenerator &generator)
{
    return QPointF(bar.left() + generator.bounded(int(bar.width())),
                   bar.top() + generator.bounded(int(bar.height())));
}

QColor mapColor(const QVariantMap &map, const char *key)
{
    const auto value = map.value(QString::fromLatin1(key));
    if (value.canConvert<QColor>()) {
        const auto color = value.value<QColor>();
        return color.isValid() ? color : QColor();
    }
    return {};
}

QColor themeColor(const Themes::ThemeSpec &theme, const char *key, QColor fallback)
{
    const auto candidate = theme.colors.value(QString::fromLatin1(key));
    return candidate.isValid() ? candidate : fallback;
}

} // namespace

HybridChrome::ChromePalette chromePaletteForTheme(const Themes::ThemeSpec &theme)
{
    HybridChrome::ChromePalette palette;
    palette.surface = themeColor(theme, "surface", palette.surface);
    palette.surfaceRaised = themeColor(theme, "surfaceRaised", palette.surfaceRaised);
    palette.border = themeColor(theme, "border", palette.border);
    palette.text = themeColor(theme, "text", palette.text);
    palette.textMuted = themeColor(theme, "textMuted", palette.textMuted);
    palette.accent = themeColor(theme, "accent", palette.accent);
    palette.close = theme.decoration.closeColor;
    palette.minimize = theme.decoration.minimizeColor;
    palette.maximize = theme.decoration.maximizeColor;
    return palette;
}

DecorationChrome DecorationChrome::fromChromePalette(
    const HybridChrome::ChromePalette &palette, const Themes::ThemeSpec &theme)
{
    DecorationChrome chrome;
    chrome.surface = palette.surface;
    chrome.surfaceRaised = palette.surfaceRaised;
    chrome.border = palette.border;
    chrome.text = palette.text;
    chrome.textMuted = palette.textMuted;
    chrome.close = palette.close;
    chrome.minimize = palette.minimize;
    chrome.maximize = palette.maximize;
    chrome.buttonStyle = theme.decoration.buttonStyle;
    // The worn Luna chrome switches on only when the theme authors it;
    // omitting the keys keeps the classic rendering byte-identical.
    chrome.titleBar = theme.decoration.titleBarColor;
    chrome.titleBarInactive = theme.decoration.titleBarInactiveColor;
    chrome.restore = theme.decoration.restoreColor;
    return chrome;
}

DecorationChrome DecorationChrome::fromTheme(const Themes::ThemeSpec &theme)
{
    return fromChromePalette(chromePaletteForTheme(theme), theme);
}

DecorationChrome DecorationChrome::fromVariantMap(const QVariantMap &map)
{
    DecorationChrome chrome;
    chrome.surface = mapColor(map, "surface");
    chrome.surfaceRaised = mapColor(map, "surfaceRaised");
    chrome.border = mapColor(map, "border");
    chrome.text = mapColor(map, "text");
    chrome.textMuted = mapColor(map, "textMuted");
    chrome.close = mapColor(map, "close");
    chrome.minimize = mapColor(map, "minimize");
    chrome.maximize = mapColor(map, "maximize");
    const auto style = map.value(QStringLiteral("buttonStyle"));
    if (style.metaType().id() == QMetaType::QString && !style.toString().isEmpty()) {
        chrome.buttonStyle = style.toString();
    }
    chrome.titleBar = mapColor(map, "titleBar");
    chrome.titleBarInactive = mapColor(map, "titleBarInactive");
    chrome.restore = mapColor(map, "restore");
    return chrome;
}

QVariantMap DecorationChrome::toVariantMap() const
{
    QVariantMap map{{QStringLiteral("surface"), surface},
                    {QStringLiteral("surfaceRaised"), surfaceRaised},
                    {QStringLiteral("border"), border},
                    {QStringLiteral("text"), text},
                    {QStringLiteral("textMuted"), textMuted},
                    {QStringLiteral("close"), close},
                    {QStringLiteral("minimize"), minimize},
                    {QStringLiteral("maximize"), maximize},
                    {QStringLiteral("buttonStyle"), buttonStyle}};
    if (titleBar.isValid()) {
        map.insert(QStringLiteral("titleBar"), titleBar);
    }
    if (titleBarInactive.isValid()) {
        map.insert(QStringLiteral("titleBarInactive"), titleBarInactive);
    }
    if (restore.isValid()) {
        map.insert(QStringLiteral("restore"), restore);
    }
    return map;
}

bool DecorationChrome::glyphChrome() const
{
    return buttonStyle == QStringLiteral("glyph");
}

bool DecorationChrome::wornLuna() const
{
    return titleBar.isValid();
}

DecorationVisualStyle decorationVisualStyle(const QColor &border,
                                            const QColor &surface,
                                            bool maximized)
{
    DecorationVisualStyle style;
    style.frameColor = border;
    style.shadowColor = inkShadow(surface);
    style.framed = !maximized;
    return style;
}

QMarginsF decorationBorders(bool maximized)
{
    return maximized ? QMarginsF(0.0, DecorationTitleHeight, 0.0, 0.0)
                     : QMarginsF(1.0, DecorationTitleHeight, 1.0, 1.0);
}

QMarginsF decorationResizeOnlyBorders(bool maximized, bool containerMember)
{
    return maximized || containerMember ? QMarginsF{}
                                        : QMarginsF(5.0, 5.0, 5.0, 5.0);
}

QList<DecorationButtonVisual> layoutDecorationButtons(const DecorationChrome &chrome,
                                                      const QSizeF &size)
{
    QList<DecorationButtonVisual> buttons;
    if (chrome.glyphChrome()) {
        // Luna order on the physical right: minimize, maximize, close.
        const qreal edge = DecorationGlyphButtonSize;
        const qreal groupWidth = 3.0 * edge + 2.0 * DecorationButtonSpacing;
        qreal x = size.width() - groupWidth - 14.0;
        const qreal y = (DecorationTitleHeight - edge) / 2.0;
        for (const auto kind : {DecorationButtonKind::Minimize,
                                DecorationButtonKind::Maximize,
                                DecorationButtonKind::Close}) {
            buttons.append({kind, QRectF(x, y, edge, edge)});
            x += edge + DecorationButtonSpacing;
        }
        return buttons;
    }
    const qreal edge = DecorationClassicButtonSize;
    qreal x = 12.0;
    for (const auto kind : {DecorationButtonKind::Close,
                            DecorationButtonKind::Minimize,
                            DecorationButtonKind::Maximize}) {
        buttons.append({kind, QRectF(x, 5.0, edge, edge)});
        x += edge + DecorationButtonSpacing;
    }
    return buttons;
}

QRectF decorationCaptionRect(const DecorationChrome &chrome, const QSizeF &size,
                             const QList<DecorationButtonVisual> &buttons)
{
    qreal left = 12.0;
    qreal right = size.width() - 18.0;
    if (!buttons.isEmpty()) {
        QRectF group = buttons.first().geometry;
        for (const auto &button : buttons) {
            group = group.united(button.geometry);
        }
        if (chrome.glyphChrome()) {
            right = group.left() - 10.0;
        } else {
            left = group.right() + 18.0;
        }
    }
    return QRectF(left, 0.0, qMax(0.0, right - left), DecorationTitleHeight);
}

QColor decorationTitleColor(const DecorationChrome &chrome, bool active)
{
    if (active && chrome.titleBar.isValid()) {
        return chrome.titleBar;
    }
    if (!active && chrome.titleBarInactive.isValid()) {
        return chrome.titleBarInactive;
    }
    return active ? chrome.surfaceRaised : chrome.surface;
}

QColor decorationCaptionColor(const DecorationChrome &chrome, bool active)
{
    // White captions ride the dark Luna paint; light title surfaces keep the
    // theme's own text color so contrast never regresses.
    const QColor title = decorationTitleColor(chrome, active);
    if (title.isValid() && qGray(title.rgb()) < 128) {
        return Qt::white;
    }
    return decorationTextColor(chrome, active);
}

QColor decorationTextColor(const DecorationChrome &chrome, bool active)
{
    return active ? chrome.text : chrome.textMuted;
}

QColor decorationButtonFill(const DecorationChrome &chrome, DecorationButtonKind kind,
                            bool active)
{
    const QColor color = kind == DecorationButtonKind::Close ? chrome.close
        : kind == DecorationButtonKind::Minimize ? chrome.minimize
                                                 : chrome.maximize;
    return active ? color : color.darker(112);
}

QColor decorationButtonGlyphColor(const DecorationChrome &chrome,
                                  DecorationButtonKind kind, bool active)
{
    const QColor fill = kind == DecorationButtonKind::Close ? chrome.close
        : kind == DecorationButtonKind::Minimize ? chrome.minimize
                                                 : chrome.maximize;
    Q_UNUSED(active)
    return qGray(fill.rgb()) >= 128 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor decorationGlyphChromeColor(const DecorationChrome &chrome,
                                  DecorationButtonKind kind, bool restoreGlyph)
{
    switch (kind) {
    case DecorationButtonKind::Close:
        return chrome.close;
    case DecorationButtonKind::Minimize:
        return chrome.minimize;
    case DecorationButtonKind::Maximize:
        if (restoreGlyph && chrome.restore.isValid()) {
            return chrome.restore;
        }
        return chrome.maximize;
    }
    return chrome.maximize;
}

quint32 decorationWearSeed(const QString &caption, qreal width)
{
    // AGENT-NOTE: focus state is deliberately excluded. The same window text
    // and width always reproduce the same wear; only the palette dims.
    return quint32(qHash(caption) ^ (quint64(width) << 32));
}

void paintWornLunaTitle(QPainter &painter, const QRectF &bar,
                        const QColor &paintColor, quint32 seed)
{
    if (!paintColor.isValid() || bar.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = DecorationCornerRadius;

    // Rust undercoat: what the Luna paint flaked away from.
    QLinearGradient undercoat(bar.topLeft(), bar.bottomLeft());
    undercoat.setColorAt(0.0, QColor(QStringLiteral("#4a2f1b")));
    undercoat.setColorAt(1.0, QColor(QStringLiteral("#6f4526")));
    QPainterPath underPath;
    underPath.addRoundedRect(bar, radius, radius);
    painter.fillPath(underPath, undercoat);

    // Surviving Luna paint keeps its original vertical sheen.
    QLinearGradient sheen(bar.topLeft(), bar.bottomLeft());
    sheen.setColorAt(0.0, paintColor.lighter(122));
    sheen.setColorAt(0.55, paintColor);
    sheen.setColorAt(1.0, paintColor.darker(126));
    QPainterPath paintPath;
    paintPath.addRoundedRect(bar, radius, radius);
    painter.fillPath(paintPath, sheen);

    // Thin surviving gloss line under the top edge.
    painter.fillRect(QRectF(bar.left(), bar.top() + 1.0, bar.width(), 1.2),
                     QColor(255, 255, 255, 70));

    QRandomGenerator generator(seed);
    painter.setPen(Qt::NoPen);

    // Edge chips: paint flaked off along the top and bottom seams. Every
    // chip reveals the undercoat with a slightly darker worn rim.
    const QColor chipFill(QStringLiteral("#5f3d22"));
    const QColor chipRim(QStringLiteral("#3e2716"));
    const int chipCount = qBound(4, qRound(bar.width() / 95.0), 24);
    for (int i = 0; i < chipCount; ++i) {
        const bool topEdge = generator.bounded(2) == 0;
        const qreal cx = bar.left() + generator.bounded(int(bar.width()));
        const qreal cy = topEdge ? bar.top() + generator.bounded(3)
                                 : bar.bottom() - 1.0 - generator.bounded(3);
        const qreal span = 1.8 + generator.bounded(40) / 10.0;
        QPolygonF chip;
        const int points = 3 + generator.bounded(3);
        for (int p = 0; p < points; ++p) {
            const qreal angle = (p / qreal(points)) * 2.0 * kPi
                + generator.bounded(628) / 100.0;
            const qreal extent = span * (0.6 + generator.bounded(100) / 160.0);
            chip.append(QPointF(cx + std::cos(angle) * extent,
                                cy + std::sin(angle) * extent * 0.7));
        }
        painter.setBrush(chipFill);
        painter.drawPolygon(chip);
        painter.setBrush(Qt::NoBrush);
        QColor rimColor = chipRim;
        rimColor.setAlpha(150);
        QPen rim(rimColor, 0.6);
        painter.setPen(rim);
        painter.drawPolygon(chip);
        painter.setPen(Qt::NoPen);
    }

    // Rust speckles bloom through the paint, denser toward the bottom seam.
    const int speckles = qBound(6, qRound(bar.width() / 42.0), 60);
    for (int i = 0; i < speckles; ++i) {
        const QPointF spot = onBar(bar, generator);
        QColor rust(QStringLiteral("#8c5427"));
        rust.setAlpha(90 + generator.bounded(110));
        painter.setBrush(rust);
        const qreal extent = 0.6 + generator.bounded(90) / 90.0;
        painter.drawEllipse(spot, extent, extent);
    }

    // Weather streaks run from the bottom seam downward and fade out.
    const int drips = qBound(2, qRound(bar.width() / 240.0), 8);
    for (int i = 0; i < drips; ++i) {
        const qreal cx = bar.left() + 8.0
            + generator.bounded(qMax(1, int(bar.width()) - 16));
        const qreal length = 3.0 + generator.bounded(90) / 10.0;
        QLinearGradient drip(cx, qMax(bar.top(), bar.bottom() - length), cx,
                             bar.bottom());
        QColor stain(QStringLiteral("#74451f"));
        QColor faded = stain;
        faded.setAlpha(0);
        drip.setColorAt(0.0, faded);
        drip.setColorAt(1.0, stain);
        painter.setBrush(drip);
        painter.drawRect(QRectF(cx, bar.bottom() - length, 1.1, length));
    }
    painter.restore();
}

void paintDecorationFrame(QPainter &painter, const QRectF &bounds,
                          const DecorationVisualStyle &style)
{
    if (!style.framed || !style.frameColor.isValid() || bounds.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(style.frameColor, style.frameWidth));
    const qreal inset = style.frameWidth / 2.0;
    const QRectF frame = bounds.adjusted(inset, inset, -inset, -inset);
    const qreal radius = std::max(0.0, style.cornerRadius - inset);
    painter.drawRoundedRect(frame, radius, radius);
    painter.restore();
}

void paintDecorationTitle(QPainter &painter, const DecorationChrome &chrome,
                          const DecorationFrameVisual &frame)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds(QPointF(0.0, 0.0), frame.size);
    const qreal titleHeight = DecorationTitleHeight;
    const qreal radius = frame.maximized ? 0.0 : DecorationCornerRadius;
    const QColor title = decorationTitleColor(chrome, frame.active);
    const bool worn = chrome.wornLuna();
    if (worn) {
        paintWornLunaTitle(painter, QRectF(0.0, 0.0, frame.size.width(), titleHeight),
                           title, decorationWearSeed(frame.caption, frame.size.width()));
    } else {
        QPainterPath titlePath;
        titlePath.addRoundedRect(QRectF(0.0, 0.0, frame.size.width(),
                                        titleHeight + radius),
                                 radius, radius);
        painter.fillPath(titlePath, title);
        painter.fillRect(QRectF(0.0, titleHeight - radius, frame.size.width(), radius),
                         title);
        painter.setPen(QPen(chrome.border, 0.75));
        painter.drawLine(QPointF(0.0, titleHeight - 0.5),
                         QPointF(frame.size.width(), titleHeight - 0.5));
    }
    paintDecorationFrame(painter, bounds,
                         decorationVisualStyle(chrome.border, chrome.surface,
                                               frame.maximized));
    painter.restore();
}

namespace {

void paintClassicGlyph(QPainter &painter, const DecorationChrome &chrome,
                       const DecorationFrameVisual &frame,
                       const DecorationButtonVisual &button, const QRectF &circle)
{
    QPen pen(decorationButtonGlyphColor(chrome, button.kind, frame.active));
    pen.setWidthF(1.15);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.20;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-radius, radius * 0.45),
                         center + QPointF(radius, radius * 0.45));
        break;
    case DecorationButtonKind::Maximize:
        if (frame.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius * 0.55,
                                    radius * 1.45, radius * 1.45));
            painter.drawRect(QRectF(center.x() - radius * 0.45, center.y() - radius,
                                    radius * 1.45, radius * 1.45));
        } else {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        }
        break;
    }
}

void paintGlyphChrome(QPainter &painter, const DecorationChrome &chrome,
                      const DecorationFrameVisual &frame,
                      const DecorationButtonVisual &button, const QRectF &circle)
{
    // AGENT-NOTE: glyph chrome draws the console-style outline glyphs in their
    // own colors directly on the Luna paint. The dash-pattern stroke reads as
    // chipped paint at 16 px; the shapes stay recognizable when eroded.
    QColor stroke = decorationGlyphChromeColor(chrome, button.kind, frame.restoreGlyph);
    if (!frame.active) {
        auto dimmed = stroke.toHsl();
        dimmed.setHslF(dimmed.hslHueF(),
                       dimmed.saturationF() * 0.45f,
                       qBound(0.0f, static_cast<float>(dimmed.lightnessF() * 0.9), 1.0f),
                       stroke.alphaF());
        stroke = dimmed;
    }
    if (button.pressed) {
        stroke = stroke.darker(130);
    } else if (button.hovered) {
        stroke = stroke.lighter(115);
    }

    if (button.hovered || button.pressed) {
        const QColor halo = button.pressed ? QColor(0, 0, 0, 60)
                                           : QColor(255, 255, 255, 46);
        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawRoundedRect(circle.adjusted(-1.0, -1.0, -1.0, -1.0), 3.5, 3.5);
    }

    QPen pen(QBrush(stroke), 1.8);
    pen.setDashPattern({5.0, 1.6, 3.0, 1.2});
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.30;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawEllipse(center, radius, radius);
        break;
    case DecorationButtonKind::Maximize:
        if (frame.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        } else {
            QPainterPath triangle;
            triangle.moveTo(center + QPointF(0.0, -radius));
            triangle.lineTo(center + QPointF(radius * 1.1, radius * 0.8));
            triangle.lineTo(center + QPointF(-radius * 1.1, radius * 0.8));
            triangle.closeSubpath();
            painter.drawPath(triangle);
        }
        break;
    }
}

} // namespace

void paintDecorationButton(QPainter &painter, const DecorationChrome &chrome,
                           const DecorationFrameVisual &frame,
                           const DecorationButtonVisual &button)
{
    if (!button.visible || button.geometry.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF circle = button.geometry.adjusted(1.0, 1.0, -1.0, -1.0);
    if (chrome.glyphChrome()) {
        paintGlyphChrome(painter, chrome, frame, button, circle);
        painter.restore();
        return;
    }
    QColor fill = decorationButtonFill(chrome, button.kind, frame.active);
    if (button.pressed) {
        fill = fill.darker(125);
    } else if (button.hovered) {
        fill = fill.lighter(108);
    }
    painter.setPen(QPen(fill.darker(118), 0.75));
    painter.setBrush(fill);
    painter.drawEllipse(circle);
    if (frame.controlsHovered) {
        paintClassicGlyph(painter, chrome, frame, button, circle);
    }
    painter.restore();
}

void paintDecorationCaption(QPainter &painter, const DecorationChrome &chrome,
                            const DecorationFrameVisual &frame,
                            const QRectF &captionRect)
{
    if (captionRect.isEmpty()) {
        return;
    }
    painter.save();
    QFont font = frame.font;
    if (chrome.wornLuna()) {
        // Luna captions carried the era's humanist title face; the family is
        // advisory and falls back through fontconfig when it is not installed.
        font.setFamily(QStringLiteral("Trebuchet MS"));
        font.setWeight(QFont::Bold);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const auto caption = metrics.elidedText(frame.caption, Qt::ElideRight,
                                                qFloor(captionRect.width()));
        painter.setPen(QPen(QColor(0, 0, 0, 140)));
        painter.drawText(captionRect.translated(0.0, 1.0), Qt::AlignCenter, caption);
        painter.setPen(QPen(decorationCaptionColor(chrome, frame.active)));
        painter.drawText(captionRect, Qt::AlignCenter, caption);
    } else {
        painter.setPen(decorationTextColor(chrome, frame.active));
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const auto caption = metrics.elidedText(frame.caption, Qt::ElideRight,
                                                qFloor(captionRect.width()));
        painter.drawText(captionRect, Qt::AlignCenter, caption);
    }
    painter.restore();
}

void paintDecoration(QPainter &painter, const DecorationChrome &chrome,
                     const DecorationFrameVisual &frame,
                     const QList<DecorationButtonVisual> &buttons)
{
    paintDecorationTitle(painter, chrome, frame);
    for (const auto &button : buttons) {
        paintDecorationButton(painter, chrome, frame, button);
    }
    paintDecorationCaption(painter, chrome, frame,
                           decorationCaptionRect(chrome, frame.size, buttons));
}

} // namespace QindaQt::Decoration
