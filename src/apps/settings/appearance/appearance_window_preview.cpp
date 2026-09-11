// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_window_preview.h"

#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStyleOptionMenuItem>
#include <QStyleOptionProgressBar>
#include <QStyleOptionSlider>
#include <QStyleOptionTab>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

using namespace QindaQt::Decoration;

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

constexpr qreal kBodyInset = 12.0;
constexpr qreal kRowGap = 8.0;

// Shared state for one styled preview pass. Every row painter builds its
// QStyleOption through prepare(), so a preview row never inherits option
// defaults from a widget.
struct ToolkitSampleContext {
    QStyle *style = nullptr;
    QPalette palette;
    QFontMetrics metrics;
    QStyle::State baseState = QStyle::State_None;

    // AGENT-GUARD: never QStyleOption::initFrom(nullptr): it dereferences the
    // widget. Every field the style reads is set explicitly instead.
    void prepare(QStyleOption &option, const QRectF &rect,
                 QStyle::State extra = QStyle::State_None) const
    {
        option.palette = palette;
        option.fontMetrics = metrics;
        option.rect = rect.toRect();
        option.state = baseState | extra;
        option.direction = Qt::LeftToRight;
        option.styleObject = nullptr;
    }
};

qreal paintMenuBarRow(QPainter &painter, const ToolkitSampleContext &context,
                      const QRectF &area, qreal y, int rowHeight)
{
    const QRectF bar(area.left(), y, area.width(), rowHeight);
    QStyleOptionMenuItem empty;
    context.prepare(empty, bar);
    context.style->drawControl(QStyle::CE_MenuBarEmptyArea, &empty, &painter, nullptr);
    qreal x = bar.left() + 2.0;
    int index = 0;
    for (const auto &label : {QStringLiteral("File"), QStringLiteral("Edit"),
                              QStringLiteral("View"), QStringLiteral("Help")}) {
        const qreal itemWidth = context.metrics.horizontalAdvance(label) + 16.0;
        QStyleOptionMenuItem item;
        context.prepare(item, QRectF(x, bar.top(), itemWidth, bar.height()),
                        index == 1 ? QStyle::State_Selected : QStyle::State_None);
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.text = label;
        context.style->drawControl(QStyle::CE_MenuBarItem, &item, &painter, nullptr);
        x += itemWidth;
        ++index;
    }
    return y + rowHeight;
}

qreal paintTabRow(QPainter &painter, const ToolkitSampleContext &context,
                  const QRectF &area, qreal y, int rowHeight)
{
    qreal x = area.left();
    int index = 0;
    for (const auto &label : {QStringLiteral("General"), QStringLiteral("Details")}) {
        const qreal tabWidth = context.metrics.horizontalAdvance(label) + 28.0;
        QStyleOptionTab tab;
        context.prepare(tab, QRectF(x, y, tabWidth, rowHeight),
                        index == 0 ? QStyle::State_Selected : QStyle::State_None);
        tab.text = label;
        tab.shape = QTabBar::RoundedNorth;
        tab.position = index == 0 ? QStyleOptionTab::Beginning : QStyleOptionTab::End;
        tab.selectedPosition = index == 0 ? QStyleOptionTab::NotAdjacent
                                          : QStyleOptionTab::PreviousIsSelected;
        context.style->drawControl(QStyle::CE_TabBarTab, &tab, &painter, nullptr);
        x += tabWidth;
        ++index;
    }
    return y + rowHeight;
}

qreal paintButtonRow(QPainter &painter, const ToolkitSampleContext &context,
                     const QRectF &area, qreal y, int rowHeight)
{
    const qreal buttonWidth = qMin(area.width() / 3.4, 96.0);
    qreal x = area.left();
    struct {
        const char *label;
        QStyle::State state;
        bool defaultButton;
    } buttons[] = {{"OK", QStyle::State_Raised, true},
                   {"Cancel", QStyle::State_Raised, false},
                   {"Apply", QStyle::State_None, false}};
    for (const auto &spec : buttons) {
        QStyleOptionButton button;
        context.prepare(button, QRectF(x, y, buttonWidth, rowHeight), spec.state);
        if (QString::fromLatin1(spec.label) == QStringLiteral("Apply")) {
            button.state &= ~QStyle::State_Enabled;
        }
        button.text = QString::fromLatin1(spec.label);
        if (spec.defaultButton) {
            button.features |= QStyleOptionButton::DefaultButton;
        }
        context.style->drawControl(QStyle::CE_PushButton, &button, &painter, nullptr);
        x += buttonWidth + kRowGap;
    }
    return y + rowHeight;
}

qreal paintChoiceRow(QPainter &painter, const ToolkitSampleContext &context,
                     const QRectF &area, qreal y, int rowHeight)
{
    QStyleOptionButton check;
    context.prepare(check, QRectF(area.left(), y, area.width() * 0.45, rowHeight),
                    QStyle::State_On);
    check.text = QStringLiteral("Enabled");
    context.style->drawControl(QStyle::CE_CheckBox, &check, &painter, nullptr);
    QStyleOptionButton radio;
    context.prepare(radio, QRectF(area.left() + area.width() * 0.5, y,
                                  area.width() * 0.45, rowHeight),
                    QStyle::State_On);
    radio.text = QStringLiteral("Option");
    context.style->drawControl(QStyle::CE_RadioButton, &radio, &painter, nullptr);
    return y + rowHeight;
}

qreal paintTextRow(QPainter &painter, const ToolkitSampleContext &context,
                   const QRectF &area, qreal y, int rowHeight)
{
    const QRectF editRect(area.left(), y, area.width() * 0.58, rowHeight);
    QStyleOptionFrame edit;
    context.prepare(edit, editRect, QStyle::State_HasFocus | QStyle::State_Sunken);
    edit.lineWidth = context.style->pixelMetric(QStyle::PM_DefaultFrameWidth, &edit,
                                                nullptr);
    edit.features = QStyleOptionFrame::None;
    context.style->drawPrimitive(QStyle::PE_PanelLineEdit, &edit, &painter, nullptr);
    const QRectF textRect = editRect.adjusted(6.0, 0.0, -6.0, 0.0);
    const qreal selectionWidth =
        context.metrics.horizontalAdvance(QStringLiteral("Selected"));
    painter.fillRect(QRectF(textRect.left(), textRect.top() + 4.0, selectionWidth,
                            textRect.height() - 8.0),
                     context.palette.color(QPalette::Highlight));
    painter.setPen(context.palette.color(QPalette::HighlightedText));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Selected"));
    painter.setPen(context.palette.color(QPalette::Text));
    painter.drawText(textRect.adjusted(selectionWidth, 0.0, 0.0, 0.0),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral(" text"));

    QStyleOptionComboBox combo;
    context.prepare(combo, QRectF(area.left() + area.width() * 0.62, y,
                                  area.width() * 0.38, rowHeight),
                    QStyle::State_Raised);
    combo.currentText = QStringLiteral("Fusion");
    combo.editable = false;
    combo.frame = true;
    combo.subControls = QStyle::SC_All;
    context.style->drawComplexControl(QStyle::CC_ComboBox, &combo, &painter, nullptr);
    context.style->drawControl(QStyle::CE_ComboBoxLabel, &combo, &painter, nullptr);
    return y + rowHeight;
}

void paintProgressRow(QPainter &painter, const ToolkitSampleContext &context,
                      const QRectF &area, qreal y, int rowHeight)
{
    QStyleOptionSlider slider;
    context.prepare(slider, QRectF(area.left(), y, area.width() * 0.48, rowHeight));
    slider.orientation = Qt::Horizontal;
    slider.minimum = 0;
    slider.maximum = 100;
    slider.sliderPosition = 40;
    slider.sliderValue = 40;
    slider.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
    context.style->drawComplexControl(QStyle::CC_Slider, &slider, &painter, nullptr);

    QStyleOptionProgressBar progress;
    context.prepare(progress, QRectF(area.left() + area.width() * 0.52, y + 2.0,
                                     area.width() * 0.48, rowHeight - 4.0));
    progress.minimum = 0;
    progress.maximum = 100;
    progress.progress = 60;
    progress.textVisible = true;
    progress.text = QStringLiteral("60%");
    progress.textAlignment = Qt::AlignCenter;
    context.style->drawControl(QStyle::CE_ProgressBar, &progress, &painter, nullptr);
}

void paintScrollBarColumn(QPainter &painter, const ToolkitSampleContext &context,
                          const QRectF &area)
{
    const int extent =
        context.style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, nullptr);
    QStyleOptionSlider bar;
    context.prepare(bar, QRectF(area.right() + kBodyInset - extent - 1.0, area.top(),
                                extent, area.height()));
    bar.orientation = Qt::Vertical;
    bar.minimum = 0;
    bar.maximum = 100;
    bar.pageStep = 30;
    bar.sliderPosition = 20;
    bar.sliderValue = 20;
    bar.subControls = QStyle::SC_All;
    context.style->drawComplexControl(QStyle::CC_ScrollBar, &bar, &painter, nullptr);
}

} // namespace

AppearanceWindowPreview::AppearanceWindowPreview(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setImplicitWidth(420);
    setImplicitHeight(280);
    // AGENT-GUARD: QStyle painting needs the widgets application class. A
    // plain QGuiApplication (headless tests) keeps the item usable through
    // the flat palette rendering instead of crashing inside the style.
    if (qobject_cast<QApplication *>(QCoreApplication::instance()) != nullptr) {
        m_style = QStyleFactory::create(QStringLiteral("Fusion"));
    }
}

AppearanceWindowPreview::~AppearanceWindowPreview()
{
    delete m_style;
}

bool AppearanceWindowPreview::nativeStyleAvailable() const
{
    return m_style != nullptr;
}

void AppearanceWindowPreview::setChrome(const QVariantMap &chrome)
{
    if (m_chrome == chrome) {
        return;
    }
    m_chrome = chrome;
    Q_EMIT chromeChanged();
    update();
}

void AppearanceWindowPreview::setToolkitPalette(const QVariantMap &palette)
{
    if (m_toolkitPalette == palette) {
        return;
    }
    m_toolkitPalette = palette;
    Q_EMIT toolkitPaletteChanged();
    update();
}

void AppearanceWindowPreview::setToolkitFont(const QFont &font)
{
    if (m_toolkitFont == font) {
        return;
    }
    m_toolkitFont = font;
    Q_EMIT toolkitFontChanged();
    update();
}

void AppearanceWindowPreview::setCanvas(const QColor &canvas)
{
    if (m_canvas == canvas) {
        return;
    }
    m_canvas = canvas;
    Q_EMIT canvasChanged();
    update();
}

void AppearanceWindowPreview::setCaption(const QString &caption)
{
    if (m_caption == caption) {
        return;
    }
    m_caption = caption;
    Q_EMIT captionChanged();
    update();
}

void AppearanceWindowPreview::setShowInactiveWindow(bool show)
{
    if (m_showInactiveWindow == show) {
        return;
    }
    m_showInactiveWindow = show;
    Q_EMIT showInactiveWindowChanged();
    update();
}

QPalette AppearanceWindowPreview::resolvedPalette() const
{
    QPalette palette;
    const QColor window = roleColor(m_toolkitPalette, "window", palette.color(QPalette::Window));
    const QColor windowText = roleColor(m_toolkitPalette, "windowText",
                                        palette.color(QPalette::WindowText));
    const QColor base = roleColor(m_toolkitPalette, "base", palette.color(QPalette::Base));
    const QColor text = roleColor(m_toolkitPalette, "text", palette.color(QPalette::Text));
    const QColor button = roleColor(m_toolkitPalette, "button", palette.color(QPalette::Button));
    const QColor buttonText = roleColor(m_toolkitPalette, "buttonText",
                                        palette.color(QPalette::ButtonText));
    const QColor highlight = roleColor(m_toolkitPalette, "highlight",
                                       palette.color(QPalette::Highlight));
    const QColor highlightedText = roleColor(m_toolkitPalette, "highlightedText",
                                             palette.color(QPalette::HighlightedText));
    const QColor mid = roleColor(m_toolkitPalette, "mid", palette.color(QPalette::Mid));
    const QColor dark = roleColor(m_toolkitPalette, "dark", palette.color(QPalette::Dark));
    const QColor light = roleColor(m_toolkitPalette, "light", palette.color(QPalette::Light));
    const QColor placeholder = roleColor(m_toolkitPalette, "placeholderText",
                                         palette.color(QPalette::PlaceholderText));
    const QColor disabledText = roleColor(m_toolkitPalette, "disabledText", placeholder);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, windowText);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, light);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, buttonText);
    palette.setColor(QPalette::BrightText, windowText);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Accent, highlight);
    palette.setColor(QPalette::HighlightedText, highlightedText);
    palette.setColor(QPalette::Mid, mid);
    palette.setColor(QPalette::Dark, dark);
    palette.setColor(QPalette::Shadow, dark);
    palette.setColor(QPalette::Light, light);
    palette.setColor(QPalette::Midlight, button);
    palette.setColor(QPalette::ToolTipBase, light);
    palette.setColor(QPalette::ToolTipText, windowText);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::PlaceholderText, placeholder);
    for (const auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, disabledText);
    }
    return palette;
}

void AppearanceWindowPreview::paint(QPainter *painter)
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
    const qreal margin = 12.0;
    if (m_showInactiveWindow) {
        const QRectF back(bounds.left() + bounds.width() * 0.22, bounds.top() + margin,
                          bounds.width() * 0.72, bounds.height() * 0.58);
        paintWindow(*painter, back, QStringLiteral("Documents"), false);
    }
    const QRectF front(bounds.left() + margin, bounds.top() + bounds.height() * 0.24,
                       bounds.width() * 0.76, bounds.height() * 0.72);
    paintWindow(*painter, front, m_caption, true);
    painter->restore();
}

void AppearanceWindowPreview::paintWindow(QPainter &painter, const QRectF &frame,
                                          const QString &caption, bool active) const
{
    const auto chrome = DecorationChrome::fromVariantMap(m_chrome);
    const QPalette palette = resolvedPalette();
    painter.save();
    painter.translate(frame.topLeft());
    const QSizeF size = frame.size();
    // Client area first: the decoration's rounded title sits on top of it
    // exactly as KWin composes a decorated window.
    QPainterPath body;
    body.addRoundedRect(QRectF(QPointF(0.0, 0.0), size), DecorationCornerRadius,
                        DecorationCornerRadius);
    painter.fillPath(body, palette.color(QPalette::Window));
    const QRectF client(1.0, DecorationTitleHeight, size.width() - 2.0,
                        size.height() - DecorationTitleHeight - 1.0);
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
    DecorationFrameVisual state;
    state.size = size;
    state.caption = caption;
    state.font = m_toolkitFont;
    state.active = active;
    paintDecoration(painter, chrome, state, layoutDecorationButtons(chrome, size));
    painter.restore();
}

void AppearanceWindowPreview::paintToolkitSample(QPainter &painter, const QRectF &area,
                                                 bool active) const
{
    const QPalette palette = resolvedPalette();
    if (m_style == nullptr || area.width() < 120.0 || area.height() < 80.0) {
        paintFlatSample(painter, area, palette);
        return;
    }
    painter.save();
    painter.setFont(m_toolkitFont);
    const ToolkitSampleContext context{
        m_style, palette, QFontMetrics(m_toolkitFont),
        QStyle::State_Enabled | (active ? QStyle::State_Active : QStyle::State_None)};
    const int rowHeight = qMax(24, context.metrics.height() + 10);
    qreal y = area.top();
    y = paintMenuBarRow(painter, context, area, y, rowHeight) + kRowGap;
    y = paintTabRow(painter, context, area, y, rowHeight) + kRowGap;
    y = paintButtonRow(painter, context, area, y, rowHeight) + kRowGap;
    y = paintChoiceRow(painter, context, area, y, rowHeight) + kRowGap;
    y = paintTextRow(painter, context, area, y, rowHeight) + kRowGap;
    if (y + rowHeight <= area.bottom()) {
        paintProgressRow(painter, context, area, y, rowHeight);
    }
    paintScrollBarColumn(painter, context, area);
    painter.restore();
}

void AppearanceWindowPreview::paintFlatSample(QPainter &painter, const QRectF &area,
                                              const QPalette &palette) const
{
    // Headless stand-in: palette rectangles in the same rows the styled
    // sample would occupy, so layout-dependent tests stay deterministic.
    painter.save();
    painter.setPen(Qt::NoPen);
    const qreal rowHeight = 22.0;
    qreal y = area.top();
    painter.fillRect(QRectF(area.left(), y, area.width(), rowHeight),
                     palette.color(QPalette::Button));
    y += rowHeight + 8.0;
    painter.fillRect(QRectF(area.left(), y, area.width() * 0.4, rowHeight),
                     palette.color(QPalette::Highlight));
    painter.fillRect(QRectF(area.left() + area.width() * 0.45, y, area.width() * 0.4,
                            rowHeight),
                     palette.color(QPalette::Base));
    y += rowHeight + 8.0;
    painter.setPen(palette.color(QPalette::Text));
    painter.setFont(m_toolkitFont);
    painter.drawText(QRectF(area.left(), y, area.width(), rowHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Sample text"));
    painter.restore();
}

} // namespace QindaQt::Apps::SettingsAppearance
