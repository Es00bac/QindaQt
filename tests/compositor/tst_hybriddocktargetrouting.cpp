// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybriddocktargetrouting.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

class HybridDockTargetRoutingTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsOnlyCompleteChromeTabHits();
    void routesSourcesByExposureAndNativeOwnership();
    void routesDropsByExposureAndNativeOwnership();
    void resolvesContainerEdgeBandZones();
};

void HybridDockTargetRoutingTest::mapsOnlyCompleteChromeTabHits()
{
    HybridInput::HitTarget tab{HybridInput::HitKind::Tab,
                               QStringLiteral("container"),
                               QStringLiteral("representative"), {}};
    tab.pageId = QStringLiteral("page");
    QCOMPARE(tabDockTargetFromChromeHit(tab),
             HybridInput::DockTarget({QStringLiteral("container"),
                                      QStringLiteral("representative"),
                                      HybridInput::DockZone::Tab}));

    QCOMPARE(tabDockTargetFromChromeHit(
                 {HybridInput::HitKind::MemberTitle,
                  QStringLiteral("container"), QStringLiteral("member"), {}}),
             HybridInput::DockTarget{});
    QCOMPARE(tabDockTargetFromChromeHit(
                 {HybridInput::HitKind::Tab, {}, QStringLiteral("member"), {}}),
             HybridInput::DockTarget{});
}

void HybridDockTargetRoutingTest::routesSourcesByExposureAndNativeOwnership()
{
    const HybridInput::HitTarget nativeTitle{
        HybridInput::HitKind::MemberTitle, {}, QStringLiteral("independent"), {}};
    const HybridInput::HitTarget underlyingDivider{
        HybridInput::HitKind::Divider, QStringLiteral("container"), {},
        QStringLiteral("split")};

    QCOMPARE(sourceHitRespectingChromeExposure(
                 false, false, nativeTitle, underlyingDivider), nativeTitle);
    QCOMPARE(sourceHitRespectingChromeExposure(
                 false, false, {}, underlyingDivider), HybridInput::HitTarget{});
    QCOMPARE(sourceHitRespectingChromeExposure(
                 true, false, nativeTitle, underlyingDivider), underlyingDivider);
    QCOMPARE(sourceHitRespectingChromeExposure(
                 true, true, nativeTitle, underlyingDivider), nativeTitle);
    QCOMPARE(sourceHitRespectingChromeExposure(
                 true, true, {}, underlyingDivider), HybridInput::HitTarget{});
    QCOMPARE(sourceHitRespectingChromeExposure(
                 false, false, nativeTitle, {}), nativeTitle);
}

void HybridDockTargetRoutingTest::routesDropsByExposureAndNativeOwnership()
{
    const HybridInput::DockTarget chrome{
        QStringLiteral("container"), QStringLiteral("tab-member"),
        HybridInput::DockZone::Tab};
    const HybridInput::DockTarget native{
        {}, QStringLiteral("independent"), HybridInput::DockZone::Left};

    QCOMPARE(dockTargetRespectingChromeExposure(
                 true, false, chrome, native), chrome);
    QCOMPARE(dockTargetRespectingChromeExposure(
                 false, false, chrome, native), native);
    QCOMPARE(dockTargetRespectingChromeExposure(
                 true, true, chrome, native), native);
    QCOMPARE(dockTargetRespectingChromeExposure(
                 false, false, chrome, {}), HybridInput::DockTarget{});
}

void HybridDockTargetRoutingTest::resolvesContainerEdgeBandZones()
{
    using HybridInput::DockZone;
    const QRectF frame(100.0, 200.0, 1000.0, 600.0);
    QCOMPARE(containerEdgeBand(frame), 64.0);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(110.0, 500.0)), DockZone::Left);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(1090.0, 500.0)), DockZone::Right);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(600.0, 210.0)), DockZone::Top);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(600.0, 790.0)), DockZone::Bottom);
    // Corners resolve to the nearer edge.
    QCOMPARE(containerEdgeDockZone(frame, QPointF(110.0, 220.0)), DockZone::Left);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(130.0, 210.0)), DockZone::Top);
    // The band boundary is inclusive; deeper inside, member-tile zones apply.
    QCOMPARE(containerEdgeDockZone(frame, QPointF(164.0, 500.0)), DockZone::Left);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(165.0, 500.0)), DockZone::None);
    QCOMPARE(containerEdgeDockZone(frame, QPointF(600.0, 500.0)), DockZone::None);
    // Outside the frame there is no container target at all.
    QCOMPARE(containerEdgeDockZone(frame, QPointF(50.0, 500.0)), DockZone::None);
    QCOMPARE(containerEdgeDockZone({}, QPointF(0.0, 0.0)), DockZone::None);

    // Small frames scale the band down so the interior stays reachable.
    const QRectF small(0.0, 0.0, 300.0, 120.0);
    QCOMPARE(containerEdgeBand(small), 18.0);
    const QRectF tiny(0.0, 0.0, 300.0, 40.0);
    QCOMPARE(containerEdgeBand(tiny), 10.0);
    QCOMPARE(containerEdgeDockZone(tiny, QPointF(150.0, 20.0)), DockZone::None);
    QCOMPARE(containerEdgeDockZone(tiny, QPointF(150.0, 5.0)), DockZone::Top);
}

QTEST_APPLESS_MAIN(HybridDockTargetRoutingTest)

#include "tst_hybriddocktargetrouting.moc"
