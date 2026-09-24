// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/decoration_painter/decoration_button_style.h"
#include "qindaqt/hybrid_chrome/chromeidentity.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
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

QColor themeColor(const Themes::ThemeSpec &theme, const char *key, QColor fallback)
{
    const auto candidate = theme.colors.value(QString::fromLatin1(key));
    return candidate.isValid() ? candidate : fallback;
}

QColor actionColor(const DecorationChrome &chrome, DecorationButtonKind kind)
{
    switch (kind) {
    case DecorationButtonKind::Close:
        return chrome.close;
    case DecorationButtonKind::Minimize:
        return chrome.minimize;
    case DecorationButtonKind::RollUp:
        // ADR-0264: a neutral light, never mistaken for close or zoom.
        return chrome.textMuted;
    case DecorationButtonKind::Maximize:
    case DecorationButtonKind::More:
        break;
    }
    return chrome.maximize;
}

// The caption weight: the title-weight option, else the shipped weight.
QFont::Weight captionWeight(const DecorationChrome &chrome)
{
    if (chrome.titleWeight > 0) {
        return static_cast<QFont::Weight>(std::clamp(chrome.titleWeight, 100, 900));
    }
    return chrome.wornLuna() ? QFont::Bold : QFont::DemiBold;
}

// ADR-0264: the application icon's edge in the caption row.
qreal captionIconExtent(const QRectF &captionRect)
{
    return std::min(16.0, captionRect.height() - 6.0);
}

// ADR-0264: a tab style's title reaches from the left edge past its buttons
// and caption; every other style spans the whole width. With the buttons
// moved to the right edge the tab spans the bar, so they stay on it.
qreal titleTabWidth(const DecorationChrome &chrome, const DecorationFrameVisual &frame)
{
    const qreal width = frame.size.width();
    if (!decorationButtonStyle(chrome.buttonStyle).titleTab
        || effectiveButtonSide(chrome) == DecorationButtonSide::Right) {
        return width;
    }
    const QRectF caption = decorationCaptionRect(chrome, frame.size,
                                                 layoutDecorationButtons(chrome, frame.size));
    QFont font = frame.font;
    font.setWeight(captionWeight(chrome));
    qreal right = caption.left() + QFontMetricsF(font).horizontalAdvance(frame.caption) + 14.0;
    if (chrome.appIcon && !frame.icon.isNull()) {
        right += captionIconExtent(caption) + 6.0;
    }
    return std::clamp(right, std::min(width, 120.0), width);
}

// A tab title's outline: the tab over the body, one closed path, so no
// frame line crosses the transparent strip beside the tab.
void paintTabFrame(QPainter &painter, const QRectF &bounds, qreal titleHeight, qreal tabWidth,
                   const DecorationVisualStyle &style)
{
    if (!style.framed || !style.frameColor.isValid() || bounds.isEmpty()) {
        return;
    }
    const qreal inset = style.frameWidth / 2.0;
    const qreal radius = std::max(0.0, style.cornerRadius - inset);
    QPainterPath tab;
    tab.addRoundedRect(QRectF(inset, inset, tabWidth - 2.0 * inset, titleHeight + radius),
                       radius, radius);
    QPainterPath body;
    body.addRoundedRect(QRectF(inset, titleHeight, bounds.width() - 2.0 * inset,
                               bounds.height() - titleHeight - inset),
                        radius, radius);
    QPainterPath shoulder;
    shoulder.addRect(QRectF(inset, titleHeight, bounds.width() - 2.0 * inset, radius));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(style.frameColor, style.frameWidth));
    painter.drawPath(tab.united(body.united(shoulder)).simplified());
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
    // An authored decoration block states its button side; unauthored themes
    // keep the legacy rule so their published map stays byte-identical.
    if (theme.decoration.authored) {
        chrome.buttonSide = theme.decoration.buttonPlacement;
    }
    return chrome;
}

DecorationChrome DecorationChrome::fromTheme(const Themes::ThemeSpec &theme)
{
    return fromChromePalette(chromePaletteForTheme(theme), theme);
}

bool DecorationChrome::glyphChrome() const
{
    return buttonStyle == QStringLiteral("glyph");
}

bool DecorationChrome::flatChrome() const
{
    return buttonStyle == QStringLiteral("flat");
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

DecorationVisualStyle decorationVisualStyleFor(const DecorationChrome &chrome, bool maximized)
{
    auto style = decorationVisualStyle(chrome.border, chrome.surface, maximized);
    style.cornerRadius = chrome.cornerRadius;
    style.shadowExtent = chrome.shadowExtent;
    style.shadowOpacity = chrome.shadowOpacity;
    return style;
}

qreal decorationFrameRadius(const DecorationChrome &chrome, bool maximized)
{
    return maximized ? 0.0 : chrome.cornerRadius;
}

QColor decorationMemberHandleFillColor(const DecorationChrome &chrome,
                                       const DecorationFrameVisual &frame)
{
    // ADR-0139: only the one focused member wears the container identity;
    // every other handlebar keeps the neutral title surface.
    if (frame.memberFocused && chrome.identityColor.isValid()) {
        return chrome.identityColor;
    }
    return decorationTitleColor(chrome, frame.active);
}

QColor decorationMemberHandleInkColor(const DecorationChrome &chrome,
                                      const DecorationFrameVisual &frame)
{
    const auto fill = decorationMemberHandleFillColor(chrome, frame);
    // The shared identity derivation picks whichever of the theme text,
    // white, and black holds 4.5:1 against the fill (ADR-0139), so glyphs
    // switch light/dark automatically. The neutral path keeps today's
    // caption color rule unchanged.
    if (frame.memberFocused && chrome.identityColor.isValid()) {
        return HybridChrome::identityInk(fill, chrome.text);
    }
    return decorationCaptionColor(chrome, frame.active);
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
    const QColor color = actionColor(chrome, kind);
    return active ? color : color.darker(112);
}

QColor decorationButtonGlyphColor(const DecorationChrome &chrome,
                                  DecorationButtonKind kind, bool active)
{
    const QColor fill = actionColor(chrome, kind);
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
    case DecorationButtonKind::More:
    case DecorationButtonKind::RollUp:
        return chrome.textMuted;
    }
    return chrome.maximize;
}

quint32 decorationWearSeed(const QString &caption, qreal width)
{
    // AGENT-NOTE: focus state is deliberately excluded. The same window text
    // and width always reproduce the same wear; only the palette dims.
    return quint32(qHash(caption) ^ (quint64(width) << 32));
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
    const qreal titleHeight = decorationTitleHeight(chrome);
    const qreal radius = decorationFrameRadius(chrome, frame.maximized);
    const QColor title = decorationTitleColor(chrome, frame.active);
    const bool worn = chrome.wornLuna();
    // ADR-0264: a tab style fills only its tab; the strip beside it stays
    // clear. Every other style's bar spans the full width, as it shipped.
    const qreal barWidth = titleTabWidth(chrome, frame);
    if (worn) {
        paintWornLunaTitle(painter, QRectF(0.0, 0.0, barWidth, titleHeight),
                           title, decorationWearSeed(frame.caption, frame.size.width()));
    } else {
        // Theming v2 (ADR-0207): the title fill carries the document's
        // material opacity, an optional vibrancy tint beneath it, and a
        // one-pixel catch light under the top edge. At the defaults (opaque,
        // no tint, no highlight) this paints exactly what shipped before.
        QColor fill = title;
        fill.setAlphaF(static_cast<float>(std::clamp(chrome.titleOpacity, 0.0, 1.0)
                                          * title.alphaF()));
        QPainterPath titlePath;
        titlePath.addRoundedRect(QRectF(0.0, 0.0, barWidth, titleHeight + radius),
                                 radius, radius);
        const QRectF seam(0.0, titleHeight - radius, barWidth, radius);
        if (chrome.titleTint.isValid() && chrome.titleOpacity < 1.0) {
            QColor tint = chrome.titleTint;
            if (tint.alphaF() >= 1.0F) {
                tint.setAlphaF(0.35F);
            }
            painter.fillPath(titlePath, tint);
            painter.fillRect(seam, tint);
        }
        painter.fillPath(titlePath, fill);
        painter.fillRect(seam, fill);
        if (chrome.titleHighlight) {
            const QColor highlight(255, 255, 255, qGray(title.rgb()) < 128 ? 46 : 120);
            painter.fillRect(QRectF(radius / 2.0, 1.0, barWidth - radius, 1.0), highlight);
        }
        painter.setPen(QPen(chrome.border, 0.75));
        painter.drawLine(QPointF(0.0, titleHeight - 0.5),
                         QPointF(frame.size.width(), titleHeight - 0.5));
    }
    if (barWidth < frame.size.width()) {
        paintTabFrame(painter, bounds, titleHeight, barWidth,
                      decorationVisualStyleFor(chrome, frame.maximized));
    } else {
        paintDecorationFrame(painter, bounds, decorationVisualStyleFor(chrome, frame.maximized));
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
    // A tab title always reads from its buttons outward (ADR-0264).
    const bool leftAligned = chrome.titleAlignment == QLatin1String("left")
        || decorationButtonStyle(chrome.buttonStyle).titleTab;
    int alignment = leftAligned ? static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter)
                                : static_cast<int>(Qt::AlignCenter);
    QFont font = frame.font;
    if (chrome.wornLuna()) {
        // Luna captions carried the era's humanist title face; the family is
        // advisory and falls back through fontconfig when it is not installed.
        font.setFamily(QStringLiteral("Trebuchet MS"));
    }
    font.setWeight(captionWeight(chrome));
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    QRectF textRect = captionRect;
    if (chrome.appIcon && !frame.icon.isNull()) {
        // ADR-0264: the application icon leads the caption; a centered
        // caption centers icon and text together.
        const qreal extent = captionIconExtent(captionRect);
        constexpr qreal gap = 6.0;
        if (extent >= 8.0 && captionRect.width() > extent + gap) {
            qreal iconLeft = captionRect.left();
            if (!leftAligned) {
                const qreal textWidth = std::min(metrics.horizontalAdvance(frame.caption),
                                                 captionRect.width() - extent - gap);
                iconLeft = std::max(captionRect.left(),
                                    captionRect.center().x() - (extent + gap + textWidth) / 2.0);
            }
            const QRectF iconRect(iconLeft, captionRect.center().y() - extent / 2.0, extent,
                                  extent);
            frame.icon.paint(&painter, iconRect.toAlignedRect());
            textRect.setLeft(iconRect.right() + gap);
            alignment = static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    const auto caption = metrics.elidedText(frame.caption, Qt::ElideRight,
                                            qFloor(textRect.width()));
    if (chrome.wornLuna()) {
        painter.setPen(QPen(QColor(0, 0, 0, 140)));
        painter.drawText(textRect.translated(0.0, 1.0), alignment, caption);
        painter.setPen(QPen(decorationCaptionColor(chrome, frame.active)));
        painter.drawText(textRect, alignment, caption);
    } else {
        painter.setPen(decorationTextColor(chrome, frame.active));
        painter.drawText(textRect, alignment, caption);
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
