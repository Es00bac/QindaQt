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
    painter->fillRect(rect(), Qt::transparent);

    // AGENT-CONTRACT: the shared painter is the renderer (ADR-0127); this
    // method only gathers live window state. The Settings preview feeds the
    // same functions, so what it shows is what this paints.
    const auto chrome = chromeState();
    const auto frame = frameState();
    paintDecorationTitle(*painter, chrome, frame);

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
    paintDecorationCaption(*painter, chrome, frame, captionRect);
    painter->restore();
}

DecorationChrome QindaDecoration::chromeState() const
{
    auto chrome = DecorationChrome::fromVariantMap(
        property("qindaqtChromePalette").toMap());
    // Windows without a published chrome map (compositor not yet attached)
    // paint from the window palette, exactly as before the shared painter.
    const auto group = window()->isActive() ? QPalette::Active : QPalette::Inactive;
    const auto &palette = window()->palette();
    if (!chrome.surface.isValid()) {
        chrome.surface = palette.color(group, QPalette::Window);
    }
    if (!chrome.surfaceRaised.isValid()) {
        chrome.surfaceRaised = palette.color(group, QPalette::Window);
    }
    if (!chrome.border.isValid()) {
        chrome.border = palette.color(group, QPalette::Mid);
    }
    if (!chrome.text.isValid()) {
        chrome.text = palette.color(group, QPalette::WindowText);
    }
    if (!chrome.textMuted.isValid()) {
        chrome.textMuted = palette.color(group, QPalette::WindowText);
    }
    for (QColor *button : {&chrome.close, &chrome.minimize, &chrome.maximize}) {
        if (!button->isValid()) {
            *button = palette.color(group, QPalette::Button);
        }
    }
    return chrome;
}

DecorationFrameVisual QindaDecoration::frameState() const
{
    DecorationFrameVisual frame;
    frame.size = size();
    frame.caption = window()->caption();
    frame.font = settings()->font();
    frame.active = window()->isActive();
    frame.maximized = window()->isMaximized();
    frame.controlsHovered = m_controlsHovered;
    frame.restoreGlyph = window()->isMaximized() || memberFocusMaximized();
    return frame;
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
    return decorationTitleColor(chromeState(), window()->isActive());
}

QColor QindaDecoration::captionColor() const
{
    return decorationCaptionColor(chromeState(), window()->isActive());
}

QColor QindaDecoration::textColor() const
{
    return decorationTextColor(chromeState(), window()->isActive());
}

bool QindaDecoration::glyphChrome() const
{
    return chromeState().glyphChrome();
}

bool QindaDecoration::wornLunaChrome() const
{
    return chromeState().wornLuna();
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
    return decorationGlyphChromeColor(chromeState(), buttonKind(type),
                                      window()->isMaximized() || memberFocusMaximized());
}

quint32 QindaDecoration::wearSeed() const
{
    return decorationWearSeed(window()->caption(), size().width());
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
    return decorationButtonFill(chromeState(), buttonKind(type), window()->isActive());
}

QColor QindaDecoration::buttonGlyphColor(KDecoration3::DecorationButtonType type) const
{
    return decorationButtonGlyphColor(chromeState(), buttonKind(type),
                                      window()->isActive());
}

DecorationButtonKind QindaDecoration::buttonKind(KDecoration3::DecorationButtonType type)
{
    switch (type) {
    case KDecoration3::DecorationButtonType::Close:
        return DecorationButtonKind::Close;
    case KDecoration3::DecorationButtonType::Minimize:
        return DecorationButtonKind::Minimize;
    default:
        return DecorationButtonKind::Maximize;
    }
}

} // namespace QindaQt::Decoration

#include "qindadecoration.moc"
