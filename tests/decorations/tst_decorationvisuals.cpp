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

QTEST_GUILESS_MAIN(DecorationVisualsTest)
#include "tst_decorationvisuals.moc"
