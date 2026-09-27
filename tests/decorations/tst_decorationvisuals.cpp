// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecorationvisuals.h"

#include <KDecoration3/DecorationShadow>

#include <QImage>
#include <QPainter>
#include <QTest>

using namespace QindaQt::Decoration;

class DecorationVisualsTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void normalStyleUsesThemeBoundary();
    void framePaintsOnlyTheOuterRing();
    void shadowHasBoundedNinePatchGeometry();
    void maximizedStyleHasNoOuterMaterial();
    void groupedMembersHaveNoResizeOnlyBorders();
    void cornerTabShadowFollowsSilhouette();
    void cornerTabShadowTracksGeometry();
    void rectangularAndMemberShadowsStayCompact();
};

void DecorationVisualsTest::normalStyleUsesThemeBoundary()
{
    const QColor border(QStringLiteral("#526170"));
    const QColor surface(QStringLiteral("#192939"));
    const auto style = decorationVisualStyle(border, surface, false);

    QVERIFY(style.framed);
    QCOMPARE(style.frameColor, border);
    QCOMPARE(style.shadowColor, QColor(QStringLiteral("#060a0e")));
    QCOMPARE(style.frameWidth, 1.0);
    QCOMPARE(style.cornerRadius, 10.0);
    QCOMPARE(style.shadowExtent, 12.0);
    QCOMPARE(style.shadowOpacity, 0.30);
}

void DecorationVisualsTest::framePaintsOnlyTheOuterRing()
{
    QImage image(QSize(80, 60), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintDecorationFrame(
        painter, image.rect(),
        decorationVisualStyle(QColor(QStringLiteral("#526170")),
                              QColor(QStringLiteral("#192939")), false));
    painter.end();

    QVERIFY(image.pixelColor(40, 0).alpha() >= 240);
    QVERIFY(image.pixelColor(0, 30).alpha() >= 240);
    QVERIFY(image.pixelColor(79, 30).alpha() >= 240);
    QVERIFY(image.pixelColor(40, 59).alpha() >= 240);
    QCOMPARE(image.pixelColor(40, 30), QColor(Qt::transparent));
}

void DecorationVisualsTest::shadowHasBoundedNinePatchGeometry()
{
    const auto shadow = createDecorationShadow(
        decorationVisualStyle(QColor(QStringLiteral("#526170")),
                              QColor(QStringLiteral("#192939")), false));
    QVERIFY(shadow);
    QCOMPARE(shadow->padding(), QMarginsF(12.0, 12.0, 12.0, 12.0));
    QCOMPARE(shadow->innerShadowRect(), QRectF(22.0, 22.0, 1.0, 1.0));
    const QImage texture = shadow->shadow();
    QCOMPARE(texture.size(), QSize(45, 45));
    QCOMPARE(texture.pixelColor(22, 22), QColor(Qt::transparent));
    QVERIFY(texture.pixelColor(22, 8).alpha() >= 45);
    QVERIFY(texture.pixelColor(22, 0).alpha() <= 2);
    QVERIFY(texture.pixelColor(22, 8).red() <
            QColor(QStringLiteral("#192939")).red());
}

void DecorationVisualsTest::maximizedStyleHasNoOuterMaterial()
{
    const auto style =
        decorationVisualStyle(QColor(QStringLiteral("#526170")),
                              QColor(QStringLiteral("#192939")), true);
    QVERIFY(!style.framed);
    QVERIFY(!createDecorationShadow(style));

    QImage image(QSize(20, 20), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintDecorationFrame(painter, image.rect(), style);
    painter.end();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QCOMPARE(image.pixelColor(x, y), QColor(Qt::transparent));
        }
    }
}

void DecorationVisualsTest::groupedMembersHaveNoResizeOnlyBorders()
{
    // Membership reaches the decoration through the qindaqtContainerMember
    // property; the compositor vetoes native member resize, so the grip is
    // removed instead of advertising a capability that cannot work.
    QCOMPARE(decorationResizeOnlyBorders(false, false), QMarginsF(5.0, 5.0, 5.0, 5.0));
    QCOMPARE(decorationResizeOnlyBorders(false, true), QMarginsF{});
    QCOMPARE(decorationResizeOnlyBorders(true, false), QMarginsF{});
    QCOMPARE(decorationResizeOnlyBorders(true, true), QMarginsF{});
}

void DecorationVisualsTest::cornerTabShadowFollowsSilhouette()
{
    DecorationChrome chrome;
    chrome.buttonStyle = QStringLiteral("tab");
    DecorationFrameVisual frame;
    frame.size = QSizeF(640, 480);
    frame.caption = QStringLiteral("Short title");
    const auto style = decorationVisualStyle(QColor("#526170"), QColor("#192939"), false);
    const auto shadow = createDecorationShadow(style, chrome, frame);
    QVERIFY(shadow);
    const QImage texture = shadow->shadow();
    const int extent = qCeil(style.shadowExtent);
    const int tab = qCeil(decorationTitleTabWidth(chrome, frame));
    const int title = qCeil(decorationTitleHeight(chrome));
    QVERIFY(tab < 500);
    // The empty strip has no phantom full-width upper edge. The real tab,
    // its vertical edge, and the body shoulder each cast their own shadow.
    QCOMPARE(texture.pixelColor(extent + 500, extent - 2).alpha(), 0);
    QVERIFY(texture.pixelColor(extent + tab / 2, extent - 2).alpha() > 0);
    QVERIFY(texture.pixelColor(extent + tab + 2, extent + title / 2).alpha() > 0);
    QVERIFY(texture.pixelColor(extent + 500, extent + title - 2).alpha() > 0);
    QCOMPARE(texture.pixelColor(extent + 30, extent + title + 2).alpha(), 0);
    QCOMPARE(texture.width(), 640 + 2 * extent);
    QVERIFY(texture.height() < 120);
    // Fixed corners consume exactly the physical width: no horizontal
    // stretching can smear the notch; only the opaque body row stretches.
    QCOMPARE(shadow->topLeftGeometry().width() + shadow->topGeometry().width()
                 + shadow->topRightGeometry().width(), qreal(texture.width()));
    QVERIFY(shadow->innerShadowRect().top() >= extent + title);
}

void DecorationVisualsTest::cornerTabShadowTracksGeometry()
{
    DecorationChrome chrome;
    chrome.buttonStyle = QStringLiteral("tab");
    DecorationFrameVisual frame;
    frame.size = QSizeF(640, 480);
    frame.caption = QStringLiteral("A");
    const auto style = decorationVisualStyle(QColor("#526170"), QColor("#192939"), false);
    const auto first = createDecorationShadow(style, chrome, frame);
    frame.caption = QStringLiteral("A substantially longer caption changes the tab");
    const auto wider = createDecorationShadow(style, chrome, frame);
    QVERIFY(first->shadow() != wider->shadow());
    frame.size.setWidth(800);
    QCOMPARE(createDecorationShadow(style, chrome, frame)->shadow().width(), 824);
    frame.maximized = true;
    QVERIFY(!createDecorationShadow(decorationVisualStyle(QColor("#526170"),
        QColor("#192939"), true), chrome, frame));
}

void DecorationVisualsTest::rectangularAndMemberShadowsStayCompact()
{
    DecorationChrome chrome;
    DecorationFrameVisual frame;
    frame.size = QSizeF(640, 480);
    const auto style = decorationVisualStyle(QColor("#526170"), QColor("#192939"), false);
    const auto ordinary = createDecorationShadow(style);
    QCOMPARE(createDecorationShadow(style, chrome, frame)->shadow(), ordinary->shadow());
    chrome.buttonStyle = QStringLiteral("tab");
    chrome.buttonSide = QStringLiteral("right");
    QCOMPARE(createDecorationShadow(style, chrome, frame)->shadow(), ordinary->shadow());
    chrome.buttonSide.clear();
    frame.memberHandle = true;
    QCOMPARE(createDecorationShadow(style, chrome, frame)->shadow(), ordinary->shadow());
}

QTEST_MAIN(DecorationVisualsTest)
#include "tst_decorationvisuals.moc"
