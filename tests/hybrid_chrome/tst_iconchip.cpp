// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromeiconchip.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

#include <cmath>

using namespace QindaQt::HybridChrome;

namespace {

IconChipRequest request(const QPointF &anchor = QPointF(100.0, 80.0),
                        const QRectF &bounds = QRectF(0.0, 0.0, 1920.0, 1080.0))
{
    IconChipRequest request;
    request.windowId = QStringLiteral("window-a");
    request.title = QStringLiteral("Quarterly report.odt");
    request.anchor = anchor;
    request.bounds = bounds;
    request.devicePixelRatio = 2.0;
    request.identityColor = QColor(QStringLiteral("#b65447"));
    return request;
}

QImage render(const IconChipPlan &plan, const IconChipPaintState &state = {})
{
    const QSize physical(qCeil(plan.imageRect.width() * plan.devicePixelRatio),
                         qCeil(plan.imageRect.height() * plan.devicePixelRatio));
    QImage image(physical, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(plan.devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(-plan.imageRect.topLeft());
    ChromeIconChip::paint(painter, plan, state);
    return image;
}

QColor sample(const QImage &image, const IconChipPlan &plan, const QPointF &logical)
{
    const QPointF local = (logical - plan.imageRect.topLeft()) * plan.devicePixelRatio;
    return image.pixelColor(qRound(local.x()), qRound(local.y()));
}

bool near(const QColor &actual, const QColor &expected, int tolerance = 24)
{
    return std::abs(actual.red() - expected.red()) <= tolerance
        && std::abs(actual.green() - expected.green()) <= tolerance
        && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

} // namespace

class IconChipTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void laysOutThePillIconCloseAndLabel();
    void clampsIntoBoundsAndFlipsTheLabelAtTheEdge();
    void scalesEveryExtentAndClampsTheScale();
    void rejectsInvalidRequests();
    void hitTestsTheCircleAndTheCloseGlyph();
    void paintsIconInkAndOnlyHoverGlyphsWhileHovered();
    void paintsAPlaceholderGlyphWithoutAnIcon();
};

void IconChipTests::laysOutThePillIconCloseAndLabel()
{
    QString error;
    const auto plan = ChromeIconChip::layout(request(), &error);
    QVERIFY2(plan.has_value(), qPrintable(error));
    QVERIFY(plan->isValid());
    QCOMPARE(plan->frame, QRectF(100.0, 80.0, 48.0, 48.0));
    QCOMPARE(plan->iconRect.size(), QSizeF(28.0, 28.0));
    QCOMPARE(plan->iconRect.center(), plan->frame.center());
    QCOMPARE(plan->closeRect.size(), QSizeF(16.0, 16.0));
    // The close glyph sits on the pill's top-right edge: its center is one
    // pill radius from the pill center, and it pokes outside the frame.
    const auto offset = plan->closeRect.center() - plan->frame.center();
    QVERIFY(std::abs(std::hypot(offset.x(), offset.y()) - 24.0) < 0.01);
    QVERIFY(offset.x() > 0.0 && offset.y() < 0.0);
    QVERIFY(plan->closeRect.right() > plan->frame.right());
    QVERIFY(plan->labelRect.isValid());
    QVERIFY(plan->labelRect.left() > plan->frame.right());
    QVERIFY(plan->imageRect.contains(plan->frame));
    QVERIFY(plan->imageRect.contains(plan->closeRect));
    QVERIFY(plan->imageRect.contains(plan->labelRect));
    QCOMPARE(plan->identity.base, QColor(QStringLiteral("#b65447")));

    auto untitled = request();
    untitled.title.clear();
    const auto quiet = ChromeIconChip::layout(untitled);
    QVERIFY(quiet.has_value());
    QVERIFY(!quiet->labelRect.isValid());
    QCOMPARE(quiet->imageRect, quiet->frame.united(quiet->closeRect));
}

void IconChipTests::clampsIntoBoundsAndFlipsTheLabelAtTheEdge()
{
    const QRectF bounds(0.0, 0.0, 800.0, 600.0);
    const auto plan = ChromeIconChip::layout(request(QPointF(790.0, 590.0), bounds));
    QVERIFY(plan.has_value());
    QCOMPARE(plan->frame, QRectF(752.0, 552.0, 48.0, 48.0));
    QVERIFY(plan->labelRect.right() < plan->frame.left());
    QVERIFY(plan->labelRect.right() <= bounds.right());

    const auto negative = ChromeIconChip::layout(request(QPointF(-30.0, -30.0), bounds));
    QVERIFY(negative.has_value());
    QCOMPARE(negative->frame.topLeft(), QPointF(0.0, 0.0));

    // Without bounds the anchor is honored verbatim.
    const auto free = ChromeIconChip::layout(request(QPointF(-30.0, 5000.0), QRectF()));
    QVERIFY(free.has_value());
    QCOMPARE(free->frame.topLeft(), QPointF(-30.0, 5000.0));

    // Bounds narrower than the chip keep the leading edge.
    const auto narrow = ChromeIconChip::layout(request(QPointF(10.0, 10.0),
                                                       QRectF(20.0, 20.0, 30.0, 30.0)));
    QVERIFY(narrow.has_value());
    QCOMPARE(narrow->frame.topLeft(), QPointF(20.0, 20.0));
}

void IconChipTests::scalesEveryExtentAndClampsTheScale()
{
    auto doubled = request();
    doubled.scale = 2.0;
    const auto plan = ChromeIconChip::layout(doubled);
    QVERIFY(plan.has_value());
    QCOMPARE(plan->frame.size(), QSizeF(96.0, 96.0));
    QCOMPARE(plan->iconRect.size(), QSizeF(56.0, 56.0));
    QCOMPARE(plan->closeRect.size(), QSizeF(32.0, 32.0));

    auto huge = request();
    huge.scale = 40.0;
    QCOMPARE(ChromeIconChip::layout(huge)->scale, ChromeIconChip::MaximumScale);
    auto tiny = request();
    tiny.scale = 0.0;
    QCOMPARE(ChromeIconChip::layout(tiny)->scale, ChromeIconChip::MinimumScale);
    auto nan = request();
    nan.scale = std::nan("");
    QCOMPARE(ChromeIconChip::layout(nan)->scale, 1.0);
}

void IconChipTests::rejectsInvalidRequests()
{
    QString error;
    auto anonymous = request();
    anonymous.windowId.clear();
    QVERIFY(!ChromeIconChip::layout(anonymous, &error));
    QVERIFY(error.contains(QStringLiteral("names no window")));

    auto lost = request();
    lost.anchor = QPointF(std::nan(""), 0.0);
    QVERIFY(!ChromeIconChip::layout(lost, &error));
    QVERIFY(error.contains(QStringLiteral("anchor")));

    auto flat = request();
    flat.devicePixelRatio = 0.0;
    QVERIFY(!ChromeIconChip::layout(flat, &error));
    QVERIFY(error.contains(QStringLiteral("pixel ratio")));

    auto unpainted = request();
    unpainted.palette.text = QColor();
    QVERIFY(!ChromeIconChip::layout(unpainted, &error));
    QVERIFY(error.contains(QStringLiteral("palette")));
}

void IconChipTests::hitTestsTheCircleAndTheCloseGlyph()
{
    const auto plan = ChromeIconChip::layout(request());
    QVERIFY(plan.has_value());
    QCOMPARE(ChromeIconChip::hitTest(*plan, plan->frame.center()), IconChipHitKind::Body);
    QCOMPARE(ChromeIconChip::hitTest(*plan, plan->closeRect.center()), IconChipHitKind::Close);
    // The bounding-rect corners lie outside the painted circle.
    QCOMPARE(ChromeIconChip::hitTest(*plan, plan->frame.topLeft() + QPointF(2.0, 2.0)),
             IconChipHitKind::None);
    QCOMPARE(ChromeIconChip::hitTest(*plan, plan->frame.bottomRight() - QPointF(2.0, 2.0)),
             IconChipHitKind::None);
    // The hover label is not an input target.
    QCOMPARE(ChromeIconChip::hitTest(*plan, plan->labelRect.center()), IconChipHitKind::None);
    QCOMPARE(ChromeIconChip::hitTest(*plan, QPointF(std::nan(""), 0.0)),
             IconChipHitKind::None);
    QCOMPARE(ChromeIconChip::hitTest(IconChipPlan{}, plan->frame.center()),
             IconChipHitKind::None);
}

void IconChipTests::paintsIconInkAndOnlyHoverGlyphsWhileHovered()
{
    auto iconified = request();
    QImage icon(16, 16, QImage::Format_ARGB32_Premultiplied);
    icon.fill(QColor(255, 0, 0));
    iconified.icon = icon;
    const auto plan = ChromeIconChip::layout(iconified);
    QVERIFY(plan.has_value());

    const auto idle = render(*plan);
    QVERIFY(near(sample(idle, *plan, plan->iconRect.center()), QColor(255, 0, 0)));
    // A pill pixel between the icon and the edge carries the raised surface.
    const QPointF pillPoint(plan->frame.center().x(), plan->frame.top() + 6.0);
    QVERIFY(near(sample(idle, *plan, pillPoint), plan->palette.surfaceRaised, 40));
    // Idle: no close glyph outside the pill and no label.
    const QPointF closeOutside = plan->closeRect.center() + QPointF(4.0, -4.0);
    QCOMPARE(sample(idle, *plan, closeOutside).alpha(), 0);
    QCOMPARE(sample(idle, *plan, plan->labelRect.center()).alpha(), 0);

    const auto hovered = render(*plan, {.hovered = true});
    QVERIFY(near(sample(hovered, *plan, closeOutside), plan->palette.close, 40));
    QVERIFY(near(sample(hovered, *plan, plan->labelRect.center()),
                 plan->palette.surfaceRaised, 40));
    QVERIFY(near(sample(hovered, *plan, plan->iconRect.center()), QColor(255, 0, 0)));

    const auto pressed = render(*plan, {.hovered = true, .pressed = true});
    QVERIFY(near(sample(pressed, *plan, pillPoint), plan->palette.surface, 40));
}

void IconChipTests::paintsAPlaceholderGlyphWithoutAnIcon()
{
    const auto plan = ChromeIconChip::layout(request());
    QVERIFY(plan.has_value());
    QVERIFY(plan->icon.isNull());
    const auto image = render(*plan);
    // The placeholder fills the icon rect with the identity color, so the
    // corner of that rect is neither transparent nor the pill surface.
    const QPointF glyphPoint = plan->iconRect.topLeft() + QPointF(3.0, 3.0);
    const auto pixel = sample(image, *plan, glyphPoint);
    QVERIFY(pixel.alpha() > 0);
    QVERIFY(near(pixel, plan->identity.handlebarFill, 40));
    QVERIFY(!near(pixel, plan->palette.surfaceRaised, 8));
}

QTEST_MAIN(IconChipTests)
#include "tst_iconchip.moc"
