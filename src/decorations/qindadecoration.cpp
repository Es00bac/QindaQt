// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecoration.h"

#include "qindabutton.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

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
    updateGeometry();

    connect(window(), &KDecoration3::DecoratedWindow::widthChanged,
            this, &QindaDecoration::updateGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedChanged,
            this, &QindaDecoration::updateGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::captionChanged,
            this, qOverload<>(&QindaDecoration::update));
    connect(window(), &KDecoration3::DecoratedWindow::activeChanged,
            this, qOverload<>(&QindaDecoration::update));
    connect(window(), &KDecoration3::DecoratedWindow::scaleChanged,
            this, &QindaDecoration::updateGeometry);
    return true;
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
    QPainterPath titlePath;
    titlePath.addRoundedRect(QRectF(0.0, 0.0, size().width(), borderTop() + radius),
                             radius, radius);
    painter->fillPath(titlePath, titleColor());
    painter->fillRect(QRectF(0.0, borderTop() - radius,
                             size().width(), radius), titleColor());
    painter->setPen(QPen(QColor(QStringLiteral("#82958e")), 0.75));
    painter->drawLine(QPointF(0.0, borderTop() - 0.5),
                      QPointF(size().width(), borderTop() - 0.5));

    if (m_leftButtons) {
        m_leftButtons->paint(painter, repaintArea);
    }

    const auto buttonRight = m_leftButtons
        ? m_leftButtons->geometry().right() + 18.0
        : 18.0;
    const QRectF captionRect(buttonRight, 0.0,
                             qMax(0.0, size().width() - buttonRight - 18.0),
                             borderTop());
    painter->setPen(textColor());
    auto font = settings()->font();
    font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    const QFontMetricsF metrics(font);
    const auto caption = metrics.elidedText(window()->caption(), Qt::ElideRight,
                                            qFloor(captionRect.width()));
    painter->drawText(captionRect, Qt::AlignCenter, caption);
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
    if (hovered != m_controlsHovered) {
        m_controlsHovered = hovered;
        update();
    }
}

void QindaDecoration::createButtons()
{
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

void QindaDecoration::updateGeometry()
{
    const bool maximized = window()->isMaximized();
    const qreal titleHeight = 36.0;
    setBorders(maximized ? QMarginsF(0.0, titleHeight, 0.0, 0.0)
                         : QMarginsF(1.0, titleHeight, 1.0, 1.0));
    setResizeOnlyBorders(maximized ? QMarginsF{} : QMarginsF(5.0, 5.0, 5.0, 5.0));
    setTitleBar(QRectF(0.0, 0.0, size().width(), titleHeight));
    setBorderRadius(KDecoration3::BorderRadius(maximized ? 0.0 : 10.0));

    if (m_leftButtons) {
        m_leftButtons->setSpacing(8.0);
        for (auto *button : m_leftButtons->buttons()) {
            button->setGeometry(QRectF(0.0, 0.0, 14.0, 14.0));
        }
        m_leftButtons->setPos(QPointF(12.0, 11.0));
    }
    update();
}

QColor QindaDecoration::titleColor() const
{
    return window()->isActive() ? QColor(QStringLiteral("#e8f1ee"))
                                : QColor(QStringLiteral("#d7e1de"));
}

QColor QindaDecoration::textColor() const
{
    return window()->isActive() ? QColor(QStringLiteral("#17231f"))
                                : QColor(QStringLiteral("#60716c"));
}

} // namespace QindaQt::Decoration

#include "qindadecoration.moc"
