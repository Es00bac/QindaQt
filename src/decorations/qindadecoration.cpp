// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecoration.h"

#include "qindabutton.h"
#include "qindadecorationvisuals.h"
#include "qindawindowcontextmenu.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>

#include <QDynamicPropertyChangeEvent>
#include <QFontMetricsF>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

#include <utility>

K_PLUGIN_FACTORY_WITH_JSON(
    QindaDecorationFactory,
    "qindaqt-decoration.json",
    registerPlugin<QindaQt::Decoration::QindaDecoration>();
    registerPlugin<QindaQt::Decoration::QindaButton>();)

namespace QindaQt::Decoration {

QindaDecoration::QindaDecoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
{
}

QindaDecoration::~QindaDecoration() = default;

bool QindaDecoration::memberFocusMaximized() const
{
    // AGENT-CONTRACT: The compositor owns this process-local presentation
    // property. It deliberately does not alter KWin's maximize state because
    // that would let one grouped member escape the committed layout.
    return property("qindaqtMemberFocusMode").toString()
        == QStringLiteral("maximized");
}

bool QindaDecoration::init()
{
    createButtons();
    createContextMenu();
    m_initialized = true;
    updateGeometry();

    connect(window(), &KDecoration3::DecoratedWindow::widthChanged,
            this, &QindaDecoration::updateGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedChanged,
            this, &QindaDecoration::updateGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::captionChanged,
            this, qOverload<>(&QindaDecoration::update));
    connect(window(), &KDecoration3::DecoratedWindow::activeChanged,
            this, &QindaDecoration::updateVisualStyle);
    connect(window(), &KDecoration3::DecoratedWindow::paletteChanged,
            this, &QindaDecoration::updateVisualStyle);
    connect(window(), &KDecoration3::DecoratedWindow::scaleChanged,
            this, &QindaDecoration::updateGeometry);
    return true;
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

bool QindaDecoration::event(QEvent *event)
{
    const bool handled = KDecoration3::Decoration::event(event);
    if (m_initialized && event->type() == QEvent::DynamicPropertyChange) {
        const auto *change = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (change->propertyName() == QByteArrayLiteral("qindaqtChromePalette")) {
            // AGENT-NOTE: the palette map arrives after init(), so the glyph
            // button group is (re)built here rather than in createButtons.
            reconcileButtons();
            updateVisualStyle();
        } else if (change->propertyName() == QByteArrayLiteral("qindaqtContainerMember")) {
            updateGeometry();
        }
    }
    return handled;
}

void QindaDecoration::paint(QPainter *painter, const QRectF &repaintArea)
{
    Q_UNUSED(repaintArea)
    if (!painter) {
        return;
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(rect(), Qt::transparent);

    const qreal radius = window()->isMaximized() ? 0.0 : 10.0;
    const bool worn = wornLunaChrome();
    if (worn) {
        paintWornLunaTitle(*painter,
                           QRectF(0.0, 0.0, size().width(), borderTop()),
                           titleColor(), wearSeed());
    } else {
        QPainterPath titlePath;
        titlePath.addRoundedRect(QRectF(0.0, 0.0, size().width(), borderTop() + radius),
                                 radius, radius);
        painter->fillPath(titlePath, titleColor());
        painter->fillRect(QRectF(0.0, borderTop() - radius,
                                 size().width(), radius), titleColor());
    }
    if (!worn) {
        painter->setPen(QPen(paletteColor("border", QPalette::Mid,
                                         window()->isActive() ? QPalette::Active
                                                              : QPalette::Inactive),
                             0.75));
        painter->drawLine(QPointF(0.0, borderTop() - 0.5),
                          QPointF(size().width(), borderTop() - 0.5));
    }
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    paintDecorationFrame(
        *painter, rect(),
        decorationVisualStyle(paletteColor("border", QPalette::Mid, group),
                              paletteColor("surface", QPalette::Window, group),
                              window()->isMaximized()));

    if (m_leftButtons) {
        m_leftButtons->paint(painter, repaintArea);
    }
    if (m_rightButtons) {
        m_rightButtons->paint(painter, repaintArea);
    }

    const qreal captionLeft = m_leftButtons
        ? m_leftButtons->geometry().right() + 18.0
        : 12.0;
    const qreal captionRight = m_rightButtons
        ? m_rightButtons->pos().x() - 10.0
        : size().width() - 18.0;
    const QRectF captionRect(captionLeft, 0.0,
                             qMax(0.0, captionRight - captionLeft),
                             borderTop());
    auto font = settings()->font();
    if (worn) {
        // Luna captions carried the era's humanist title face; the family is
        // advisory and falls back through fontconfig when it is not installed.
        font.setFamily(QStringLiteral("Trebuchet MS"));
        font.setWeight(QFont::Bold);
        painter->setFont(font);
        const QFontMetricsF metrics(font);
        const auto caption = metrics.elidedText(window()->caption(), Qt::ElideRight,
                                                qFloor(captionRect.width()));
        painter->setPen(QPen(QColor(0, 0, 0, 140)));
        painter->drawText(captionRect.translated(0.0, 1.0), Qt::AlignCenter, caption);
        painter->setPen(QPen(captionColor()));
        painter->drawText(captionRect, Qt::AlignCenter, caption);
    } else {
        painter->setPen(textColor());
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        const QFontMetricsF metrics(font);
        const auto caption = metrics.elidedText(window()->caption(), Qt::ElideRight,
                                                qFloor(captionRect.width()));
        painter->drawText(captionRect, Qt::AlignCenter, caption);
    }
    painter->restore();
}

void QindaDecoration::updateControlHover()
{
    bool hovered = false;
    if (m_leftButtons) {
        for (const auto *button : m_leftButtons->buttons()) {
            hovered = hovered || button->isHovered();
        }
    }
    if (m_rightButtons) {
        for (const auto *button : m_rightButtons->buttons()) {
            hovered = hovered || button->isHovered();
        }
    }
    if (hovered != m_controlsHovered) {
        m_controlsHovered = hovered;
        update();
    }
}

void QindaDecoration::createButtons()
{
    if (glyphChrome()) {
        m_rightButtons = new KDecoration3::DecorationButtonGroup(
            KDecoration3::DecorationButtonGroup::Position::Right,
            this, &QindaButton::create);
        // Luna order on the physical right: minimize, maximize, close.
        for (const auto action : {KDecoration3::DecorationButtonType::Minimize,
                                  KDecoration3::DecorationButtonType::Maximize,
                                  KDecoration3::DecorationButtonType::Close}) {
            if (auto *button = QindaButton::create(action, this, m_rightButtons)) {
                m_rightButtons->addButton(button);
            }
        }
        return;
    }
    m_leftButtons = new KDecoration3::DecorationButtonGroup(
        KDecoration3::DecorationButtonGroup::Position::Left,
        this, &QindaButton::create);
    // AGENT-CONTRACT: Qinda macOS uses stable logical action order on the
    // physical left. The separate outer-chrome model reverses only tab visual
    // placement, never these actions or member identity.
    for (const auto action : {KDecoration3::DecorationButtonType::Close,
                              KDecoration3::DecorationButtonType::Minimize,
                              KDecoration3::DecorationButtonType::Maximize}) {
        if (auto *button = QindaButton::create(action, this, m_leftButtons)) {
            m_leftButtons->addButton(button);
        }
    }
}

void QindaDecoration::reconcileButtons()
{
    const bool glyph = glyphChrome();
    if (glyph == (m_rightButtons != nullptr)) {
        return;
    }
    delete m_leftButtons;
    m_leftButtons = nullptr;
    delete m_rightButtons;
    m_rightButtons = nullptr;
    createButtons();
    updateGeometry();
}

void QindaDecoration::createContextMenu()
{
    m_contextMenu = std::make_unique<QindaWindowContextMenu>(
        [this](WindowContextCommand command) {
            switch (command) {
            case WindowContextCommand::Minimize:
                requestMinimize();
                break;
            case WindowContextCommand::ToggleMaximized:
                requestToggleMaximization(Qt::LeftButton);
                break;
            case WindowContextCommand::ToggleShaded:
                requestToggleShade();
                break;
            case WindowContextCommand::ToggleAllWorkspaces:
                requestToggleOnAllDesktops();
                break;
            case WindowContextCommand::ToggleKeepAbove:
                requestToggleKeepAbove();
                break;
            case WindowContextCommand::ToggleKeepBelow:
                requestToggleKeepBelow();
                break;
            case WindowContextCommand::Close:
                requestClose();
                break;
            }
        });
}

void QindaDecoration::showContextMenu(const QPointF &position)
{
    if (!m_contextMenu) {
        return;
    }
    QPalette menuPalette = m_contextMenu->palette();
    menuPalette.setColor(QPalette::Window,
                         paletteColor("surface", QPalette::Window, QPalette::Active));
    menuPalette.setColor(QPalette::Base,
                         paletteColor("surfaceRaised", QPalette::Base, QPalette::Active));
    menuPalette.setColor(QPalette::Text,
                         paletteColor("text", QPalette::Text, QPalette::Active));
    menuPalette.setColor(QPalette::WindowText,
                         paletteColor("text", QPalette::WindowText, QPalette::Active));
    menuPalette.setColor(QPalette::ButtonText,
                         paletteColor("text", QPalette::ButtonText, QPalette::Active));
    menuPalette.setColor(QPalette::PlaceholderText,
                         paletteColor("textMuted", QPalette::PlaceholderText,
                                      QPalette::Active));
    menuPalette.setColor(QPalette::Highlight,
                         paletteColor("accent", QPalette::Highlight, QPalette::Active));
    menuPalette.setColor(QPalette::HighlightedText,
                         paletteColor("accentText", QPalette::HighlightedText,
                                      QPalette::Active));
    m_contextMenu->setPalette(menuPalette);
    m_contextMenu->prepare({
        .canMinimize = window()->isMinimizeable(),
        .canMaximize = window()->isMaximizeable(),
        .maximized = window()->isMaximized(),
        .canShade = window()->isShadeable(),
        .shaded = window()->isShaded(),
        .onAllWorkspaces = window()->isOnAllDesktops(),
        .keepAbove = window()->isKeepAbove(),
        .keepBelow = window()->isKeepBelow(),
        .canClose = window()->isCloseable(),
    });

    KDecoration3::Positioner positioner;
    positioner.setAnchorRect(QRectF(position, QSizeF(1.0, 1.0)));
    popup(positioner, m_contextMenu.get());
}

void QindaDecoration::updateGeometry()
{
    const bool maximized = window()->isMaximized();
    // AGENT-CONTRACT: The compositor's member policy owns this process-local
    // marker (qindaqtContainerMember). Grouped members expose no resize grip
    // because their frames change only through container reflow; the veto in
    // KWinMemberPolicyManager is the enforcement side of the same contract.
    const bool containerMember = property("qindaqtContainerMember").toBool();
    // Grouped leaves retain a native title for ordinary detach and per-window
    // controls, but it is intentionally a compact strip below shared chrome.
    const qreal titleHeight = 24.0;
    setBorders(maximized ? QMarginsF(0.0, titleHeight, 0.0, 0.0)
                         : QMarginsF(1.0, titleHeight, 1.0, 1.0));
    setResizeOnlyBorders(decorationResizeOnlyBorders(maximized, containerMember));
    setTitleBar(QRectF(0.0, 0.0, size().width(), titleHeight));
    setBorderRadius(KDecoration3::BorderRadius(maximized ? 0.0 : 10.0));

    if (m_leftButtons) {
        m_leftButtons->setSpacing(8.0);
        for (auto *button : m_leftButtons->buttons()) {
            button->setGeometry(QRectF(0.0, 0.0, 14.0, 14.0));
        }
        m_leftButtons->setPos(QPointF(12.0, 5.0));
    }
    if (m_rightButtons) {
        m_rightButtons->setSpacing(8.0);
        for (auto *button : m_rightButtons->buttons()) {
            button->setGeometry(QRectF(0.0, 0.0, 16.0, 16.0));
        }
        const qreal groupWidth = m_rightButtons->geometry().width();
        m_rightButtons->setPos(QPointF(size().width() - groupWidth - 14.0,
                                       (titleHeight - 16.0) / 2.0));
    }
    updateVisualStyle();
}

void QindaDecoration::updateVisualStyle()
{
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    const auto style = decorationVisualStyle(
        paletteColor("border", QPalette::Mid, group),
        paletteColor("surface", QPalette::Window, group),
        window()->isMaximized());
    setShadow(createDecorationShadow(style));
    update();
}

QColor QindaDecoration::titleColor() const
{
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    if (window()->isActive()) {
        if (const QColor authored = authoredColor("titleBar"); authored.isValid()) {
            return authored;
        }
    } else {
        if (const QColor authored = authoredColor("titleBarInactive");
            authored.isValid()) {
            return authored;
        }
    }
    return paletteColor(window()->isActive() ? "surfaceRaised" : "surface",
                        QPalette::Window, group);
}

QColor QindaDecoration::captionColor() const
{
    // White captions ride the dark Luna paint; light title surfaces keep the
    // theme's own text color so contrast never regresses.
    const QColor title = titleColor();
    if (qGray(title.rgb()) < 128) {
        return Qt::white;
    }
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    return paletteColor(window()->isActive() ? "text" : "textMuted",
                        QPalette::WindowText, group);
}

QColor QindaDecoration::textColor() const
{
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    return paletteColor(window()->isActive() ? "text" : "textMuted",
                        QPalette::WindowText, group);
}

bool QindaDecoration::glyphChrome() const
{
    return property("qindaqtChromePalette").toMap()
               .value(QStringLiteral("buttonStyle"))
               .toString() == QStringLiteral("glyph");
}

bool QindaDecoration::wornLunaChrome() const
{
    return authoredColor("titleBar").isValid();
}

QColor QindaDecoration::authoredColor(const char *key) const
{
    const auto map = property("qindaqtChromePalette").toMap();
    const auto color = map.value(QString::fromLatin1(key)).value<QColor>();
    return color.isValid() ? color : QColor();
}

QColor QindaDecoration::glyphChromeColor(
    KDecoration3::DecorationButtonType type) const
{
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    switch (type) {
    case KDecoration3::DecorationButtonType::Close:
        return paletteColor("close", QPalette::Button, group);
    case KDecoration3::DecorationButtonType::Minimize:
        return paletteColor("minimize", QPalette::Button, group);
    case KDecoration3::DecorationButtonType::Maximize:
        if ((window()->isMaximized() || memberFocusMaximized())) {
            if (const QColor restore = authoredColor("restore"); restore.isValid()) {
                return restore;
            }
        }
        return paletteColor("maximize", QPalette::Button, group);
    default:
        return paletteColor("maximize", QPalette::Button, group);
    }
}

quint32 QindaDecoration::wearSeed() const
{
    // AGENT-NOTE: focus state is deliberately excluded. The same window text
    // and width always reproduce the same wear; only the palette dims.
    return quint32(qHash(window()->caption()) ^ (quint64(size().width()) << 32));
}

QColor QindaDecoration::paletteColor(const char *key,
                                     QPalette::ColorRole fallbackRole,
                                     QPalette::ColorGroup group) const
{
    const auto map = property("qindaqtChromePalette").toMap();
    const auto color = map.value(QString::fromLatin1(key)).value<QColor>();
    return color.isValid() ? color : window()->palette().color(group, fallbackRole);
}

QColor QindaDecoration::buttonColor(KDecoration3::DecorationButtonType type) const
{
    const char *key = type == KDecoration3::DecorationButtonType::Close ? "close"
        : type == KDecoration3::DecorationButtonType::Minimize ? "minimize"
                                                               : "maximize";
    return paletteColor(key, QPalette::Button,
                        window()->isActive() ? QPalette::Active : QPalette::Inactive);
}

QColor QindaDecoration::buttonGlyphColor(KDecoration3::DecorationButtonType type) const
{
    const QColor fill = buttonColor(type);
    return qGray(fill.rgb()) >= 128 ? QColor(Qt::black) : QColor(Qt::white);
}

} // namespace QindaQt::Decoration

#include "qindadecoration.moc"
