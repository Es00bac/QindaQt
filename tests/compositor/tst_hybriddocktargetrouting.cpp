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

QTEST_APPLESS_MAIN(HybridDockTargetRoutingTest)

#include "tst_hybriddocktargetrouting.moc"
