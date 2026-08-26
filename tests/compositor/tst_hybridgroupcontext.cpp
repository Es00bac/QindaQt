// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupcontext.h"

#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridGroupContextTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void validatesContextIdentityAndLayer();
    void mapsOuterFrameAsOneUnitAcrossOutputs();
    void keepsPreviouslyContainedFrameInsideSmallerArea();
    void rejectsInvalidGeometry();
    void retainsOnlyStillOwnedQueuedSources();
};

void HybridGroupContextTest::validatesContextIdentityAndLayer()
{
    HybridGroupContext context{
        .outputId = QStringLiteral("right"),
        .desktopIds = {QStringLiteral("desk-2")},
        .activityIds = {QStringLiteral("writing")},
        .keepAbove = true,
    };
    QVERIFY(context.isValid());

    context.keepBelow = true;
    QVERIFY(!context.isValid());
    context.keepBelow = false;
    context.desktopIds.append(QStringLiteral("desk-2"));
    QVERIFY(!context.isValid());
}

void HybridGroupContextTest::mapsOuterFrameAsOneUnitAcrossOutputs()
{
    const auto mapped = mapHybridGroupFrameToArea(
        QRect(100, 100, 800, 500),
        QRectF(0, 0, 1920, 1080),
        QRectF(1920, 0, 2560, 1440));
    QVERIFY(mapped.has_value());
    QCOMPARE(mapped->size(), QSize(800, 500));
    QCOMPARE(mapped->center(), QPoint(2586, 466));
}

void HybridGroupContextTest::keepsPreviouslyContainedFrameInsideSmallerArea()
{
    const auto mapped = mapHybridGroupFrameToArea(
        QRect(1200, 700, 700, 350),
        QRectF(0, 0, 1920, 1080),
        QRectF(1920, 0, 1280, 720));
    QVERIFY(mapped.has_value());
    QVERIFY(QRect(1920, 0, 1280, 720).contains(*mapped));
    QCOMPARE(mapped->size(), QSize(700, 350));
}

void HybridGroupContextTest::rejectsInvalidGeometry()
{
    QString error;
    QVERIFY(!mapHybridGroupFrameToArea(
                 QRect{}, QRectF(0, 0, 1, 1), QRectF(0, 0, 1, 1), &error)
                 .has_value());
    QVERIFY(!error.isEmpty());
}

void HybridGroupContextTest::retainsOnlyStillOwnedQueuedSources()
{
    const QMap<QString, QString> pending{
        {QStringLiteral("group-a"), QStringLiteral("a")},
        {QStringLiteral("group-b"), QStringLiteral("b")},
        {QStringLiteral("group-c"), QStringLiteral("c")},
    };
    const QHash<QString, QString> topology{
        {QStringLiteral("a"), QStringLiteral("group-a")},
        {QStringLiteral("b"), QStringLiteral("other")},
        {QStringLiteral("c"), QStringLiteral("group-c")},
    };
    const QHash<QString, QString> owners{
        {QStringLiteral("a"), QStringLiteral("group-a")},
        {QStringLiteral("b"), QStringLiteral("group-b")},
        {QStringLiteral("c"), QStringLiteral("released")},
    };

    const QMap<QString, QString> expected{
        {QStringLiteral("group-a"), QStringLiteral("a")},
    };
    QCOMPARE(retainValidGroupContextSources(pending, topology, owners), expected);
}

QTEST_GUILESS_MAIN(HybridGroupContextTest)

#include "tst_hybridgroupcontext.moc"
