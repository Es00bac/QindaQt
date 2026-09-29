// SPDX-License-Identifier: GPL-3.0-or-later
#include "minimizedgatherpager.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class MinimizedGatherPagerRouterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pointerPagesOnlyWhenReleaseMatchesPressedButton();
    void touchTapPagesAndCancelDropsThePendingAction();
};

void MinimizedGatherPagerRouterTests::pointerPagesOnlyWhenReleaseMatchesPressedButton()
{
    MinimizedGatherPagerRouter router([](const QPointF &position)
        -> std::optional<MinimizedPagerHit> {
        if (position.x() < 50) {
            return MinimizedPagerHit{QStringLiteral("output-a"),
                                     MinimizedPagerButton::Previous};
        }
        if (position.x() < 100) {
            return MinimizedPagerHit{QStringLiteral("output-a"),
                                     MinimizedPagerButton::Next};
        }
        return std::nullopt;
    });
    QVERIFY(!router.pointerPress(QPointF(10, 10), false));
    QVERIFY(router.pointerPress(QPointF(10, 10), true));
    QVERIFY(router.active());
    QVERIFY(!router.pointerRelease(QPointF(70, 10)).has_value());
    QVERIFY(!router.active());
    QVERIFY(router.pointerPress(QPointF(70, 10), true));
    QCOMPARE(router.pointerRelease(QPointF(70, 10)),
             std::optional(MinimizedPagerHit{QStringLiteral("output-a"),
                                              MinimizedPagerButton::Next}));
}

void MinimizedGatherPagerRouterTests::touchTapPagesAndCancelDropsThePendingAction()
{
    MinimizedGatherPagerRouter router([](const QPointF &position)
        -> std::optional<MinimizedPagerHit> {
        return position.x() < 50
            ? std::optional(MinimizedPagerHit{QStringLiteral("output-b"),
                                               MinimizedPagerButton::Previous})
            : std::nullopt;
    });
    QVERIFY(router.touchDown(7, QPointF(20, 20)));
    QVERIFY(router.touchMotion(7, QPointF(25, 22)));
    QVERIFY(!router.touchUp(8, QPointF(20, 20)).has_value());
    QCOMPARE(router.touchUp(7, QPointF(20, 20)),
             std::optional(MinimizedPagerHit{QStringLiteral("output-b"),
                                              MinimizedPagerButton::Previous}));
    QVERIFY(router.touchDown(9, QPointF(20, 20)));
    router.cancel();
    QVERIFY(!router.touchUp(9, QPointF(20, 20)).has_value());
}

QTEST_APPLESS_MAIN(MinimizedGatherPagerRouterTests)
#include "tst_minimizedgatherpagerrouter.moc"
