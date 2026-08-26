// SPDX-License-Identifier: GPL-3.0-or-later
#include "surface_test_fixtures.h"

#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"

#include <QtTest>

#include <utility>

using namespace QindaQt;

namespace {

const ShellSurface::PanelSurfaceConfiguration &surface(
    const ShellSurface::PanelSurfacePlan &plan, const char *panelId,
    const char *outputId = "main")
{
    for (const auto &candidate : plan.surfaces) {
        if (candidate.identity.panelId == QLatin1StringView(panelId) &&
            candidate.identity.outputId == QLatin1StringView(outputId)) {
            return candidate;
        }
    }
    Q_UNREACHABLE_RETURN(plan.surfaces.constFirst());
}

void verifyAtomicFailure(const ShellSurface::PanelSurfacePlan &plan,
                         ShellSurface::PanelSurfacePlanErrorCode code)
{
    QVERIFY(!plan.ok());
    QCOMPARE(plan.error.code, code);
    QVERIFY(plan.surfaces.isEmpty());
    QVERIFY(!plan.error.message.isEmpty());
}

} // namespace

class PanelSurfaceConfigurationTests final : public QObject {
    Q_OBJECT

private slots:
    void mapsAllEdgesAndLayersWithoutPhysicalScaling();
    void assignsOnlyTheDeepestReservationCarrier();
    void offsetsSideCarriersFromTheReservedHorizontalWorkArea();
    void expandsMultipleOutputsWithStableIdentity();
    void stretchesFullWidthSurfacesAndSizesPartialSurfaces();
    void rejectsInconsistentSolverResultsAtomically();
};

void PanelSurfaceConfigurationTests::mapsAllEdgesAndLayersWithoutPhysicalScaling()
{
    using namespace ShellSurface::TestFixtures;
    auto top = panel(QStringLiteral("top"), Profiles::Edge::Top, Profiles::Layer::Above, 30);
    auto bottom = panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
                        Profiles::Layer::Overlay, 44);
    bottom.alignment = Profiles::Alignment::Center;
    bottom.length = 0.5;
    auto left = panel(QStringLiteral("left"), Profiles::Edge::Left,
                      Profiles::Layer::Below, 36);
    left.alignment = Profiles::Alignment::End;
    left.length = 0.5;
    auto right = panel(QStringLiteral("right"), Profiles::Edge::Right,
                       Profiles::Layer::Normal, 40);

    const auto layout = solve({top, bottom, left, right},
                              {output(QStringLiteral("main"), {0, 0, 2048, 1152}, 1.25)});
    QVERIFY2(layout.ok(), qPrintable(layout.error.message));
    const auto plan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));

    const auto &topSurface = surface(plan, "top");
    QCOMPARE(topSurface.geometry, QRect(0, 0, 2048, 30));
    QCOMPARE(topSurface.desiredSize, QSize(0, 30));
    QVERIFY(topSurface.anchors.testFlag(ShellSurface::SurfaceAnchor::Top));
    QVERIFY(topSurface.anchors.testFlag(ShellSurface::SurfaceAnchor::Left));
    QVERIFY(topSurface.anchors.testFlag(ShellSurface::SurfaceAnchor::Right));
    QCOMPARE(topSurface.exclusiveZone, 30);
    QVERIFY(topSurface.reservationCarrier);

    const auto &bottomSurface = surface(plan, "bottom");
    QCOMPARE(bottomSurface.geometry, QRect(512, 1108, 1024, 44));
    QCOMPARE(bottomSurface.desiredSize, QSize(1024, 44));
    QCOMPARE(bottomSurface.margins.left(), 512);
    QCOMPARE(bottomSurface.exclusiveZone, -1);
    QVERIFY(!bottomSurface.reservationCarrier);
    QVERIFY(bottomSurface.layer == Profiles::Layer::Overlay);

    const auto &leftSurface = surface(plan, "left");
    QCOMPARE(leftSurface.geometry, QRect(0, 569, 36, 539));
    QCOMPARE(leftSurface.margins.top(), 569);
    QVERIFY(leftSurface.layer == Profiles::Layer::Below);
    QCOMPARE(leftSurface.exclusiveZone, -1);

    const auto &rightSurface = surface(plan, "right");
    QCOMPARE(rightSurface.geometry, QRect(2008, 30, 40, 1078));
    QCOMPARE(rightSurface.margins.top(), 0);
    QCOMPARE(rightSurface.margins.bottom(), 44);
    QCOMPARE(rightSurface.margins.right(), 0);
    QCOMPARE(rightSurface.desiredSize, QSize(40, 0));
    QVERIFY(rightSurface.anchors.testFlag(ShellSurface::SurfaceAnchor::Bottom));
    QCOMPARE(rightSurface.exclusiveZone, 40);
    QVERIFY(rightSurface.layer == Profiles::Layer::Normal);
}

void PanelSurfaceConfigurationTests::assignsOnlyTheDeepestReservationCarrier()
{
    using namespace ShellSurface::TestFixtures;
    auto first = panel(QStringLiteral("first"), Profiles::Edge::Top,
                       Profiles::Layer::Above, 20);
    auto overlay = panel(QStringLiteral("overlay"), Profiles::Edge::Top,
                         Profiles::Layer::Overlay, 30);
    auto deepest = panel(QStringLiteral("deepest"), Profiles::Edge::Top,
                         Profiles::Layer::Normal, 25);
    auto below = panel(QStringLiteral("below"), Profiles::Edge::Top,
                       Profiles::Layer::Below, 20);

    const auto layout = solve({first, overlay, deepest, below});
    QVERIFY2(layout.ok(), qPrintable(layout.error.message));
    QCOMPARE(layout.outputs.constFirst().workArea.top(), 75);
    const auto plan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));

    QCOMPARE(surface(plan, "first").exclusiveZone, -1);
    QCOMPARE(surface(plan, "overlay").exclusiveZone, -1);
    const auto &carrier = surface(plan, "deepest");
    QVERIFY(carrier.reservationCarrier);
    QCOMPARE(carrier.exclusiveZone, 25);
    QCOMPARE(carrier.margins.top(), 50);
    QCOMPARE(carrier.placementOrder, 0);
    QCOMPARE(surface(plan, "below").exclusiveZone, -1);
}

void PanelSurfaceConfigurationTests::offsetsSideCarriersFromTheReservedHorizontalWorkArea()
{
    using namespace ShellSurface::TestFixtures;
    auto topOverlay = panel(QStringLiteral("top-overlay"), Profiles::Edge::Top,
                            Profiles::Layer::Overlay, 30);
    auto topReserve = panel(QStringLiteral("top-reserve"), Profiles::Edge::Top,
                            Profiles::Layer::Above, 20);
    auto topBelow = panel(QStringLiteral("top-below"), Profiles::Edge::Top,
                          Profiles::Layer::Below, 20);
    auto left = panel(QStringLiteral("left"), Profiles::Edge::Left,
                      Profiles::Layer::Above, 32);

    const auto layout = solve({topOverlay, topReserve, topBelow, left});
    QVERIFY2(layout.ok(), qPrintable(layout.error.message));
    QCOMPARE(layout.outputs.constFirst().workArea.top(), 50);
    QCOMPARE(layout.surfaces.constLast().geometry.top(), 70);
    const auto plan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));

    const auto &leftSurface = surface(plan, "left");
    QVERIFY(leftSurface.reservationCarrier);
    QCOMPARE(leftSurface.margins.top(), 20);
    QCOMPARE(leftSurface.desiredSize.height(), 0);
    QCOMPARE(leftSurface.exclusiveZone, 32);
    QCOMPARE(leftSurface.placementOrder, 2);
}

void PanelSurfaceConfigurationTests::expandsMultipleOutputsWithStableIdentity()
{
    using namespace ShellSurface::TestFixtures;
    auto wildcard = panel(QStringLiteral("global"), Profiles::Edge::Top,
                          Profiles::Layer::Above, 32);
    auto dock = panel(QStringLiteral("dock"), Profiles::Edge::Bottom,
                      Profiles::Layer::Overlay, 48);
    dock.output = QStringLiteral("primary");
    dock.alignment = Profiles::Alignment::Center;
    dock.length = 0.5;

    const auto layout = solve(
        {wildcard, dock},
        {output(QStringLiteral("secondary"), {-1920, 0, 1920, 1080}),
         output(QStringLiteral("primary"), {0, 0, 2048, 1152}, 1.25)});
    QVERIFY2(layout.ok(), qPrintable(layout.error.message));
    const auto plan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));
    QCOMPARE(plan.surfaces.size(), 3);
    QCOMPARE(surface(plan, "global", "secondary").geometry,
             QRect(-1920, 0, 1920, 32));
    QCOMPARE(surface(plan, "global", "primary").geometry,
             QRect(0, 0, 2048, 32));
    QCOMPARE(surface(plan, "dock", "primary").margins.left(),
             512);
}

void PanelSurfaceConfigurationTests::stretchesFullWidthSurfacesAndSizesPartialSurfaces()
{
    using namespace ShellSurface::TestFixtures;
    auto fill = panel(QStringLiteral("fill"));
    auto partial = panel(QStringLiteral("partial"), Profiles::Edge::Bottom,
                         Profiles::Layer::Overlay, 40);
    partial.alignment = Profiles::Alignment::End;
    partial.length = 0.25;
    const auto plan = ShellSurface::PanelSurfaceConfigurationPlanner::plan(
        solve({fill, partial}, {output(QStringLiteral("main"), {0, 0, 1920, 1200})}));
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));

    QCOMPARE(surface(plan, "fill").desiredSize.width(), 0);
    const auto &partialSurface = surface(plan, "partial");
    QCOMPARE(partialSurface.geometry, QRect(1440, 1160, 480, 40));
    QCOMPARE(partialSurface.desiredSize, QSize(480, 40));
    QVERIFY(!partialSurface.anchors.testFlag(ShellSurface::SurfaceAnchor::Right));
    QCOMPARE(partialSurface.margins.left(), 1440);
}

void PanelSurfaceConfigurationTests::rejectsInconsistentSolverResultsAtomically()
{
    using namespace ShellSurface::TestFixtures;
    auto valid = solve({panel(QStringLiteral("top"))});
    QVERIFY(valid.ok());

    auto rejected = valid;
    rejected.error.code = ShellLayout::PanelLayoutErrorCode::InvalidPanel;
    rejected.error.message = QStringLiteral("fixture rejection");
    verifyAtomicFailure(ShellSurface::PanelSurfaceConfigurationPlanner::plan(rejected),
                        ShellSurface::PanelSurfacePlanErrorCode::RejectedLayout);

    auto missingOutput = valid;
    missingOutput.surfaces[0].outputId = QStringLiteral("missing");
    verifyAtomicFailure(ShellSurface::PanelSurfaceConfigurationPlanner::plan(missingOutput),
                        ShellSurface::PanelSurfacePlanErrorCode::MissingOutput);

    auto duplicate = valid;
    duplicate.surfaces.push_back(duplicate.surfaces.constFirst());
    verifyAtomicFailure(ShellSurface::PanelSurfaceConfigurationPlanner::plan(duplicate),
                        ShellSurface::PanelSurfacePlanErrorCode::DuplicateSurface);

    auto reservationMismatch = valid;
    reservationMismatch.outputs[0].workArea.adjust(0, 1, 0, 0);
    verifyAtomicFailure(ShellSurface::PanelSurfaceConfigurationPlanner::plan(reservationMismatch),
                        ShellSurface::PanelSurfacePlanErrorCode::ReservationMismatch);

    auto invalidSurface = valid;
    invalidSurface.surfaces[0].geometry.translate(0, -1);
    verifyAtomicFailure(ShellSurface::PanelSurfaceConfigurationPlanner::plan(invalidSurface),
                        ShellSurface::PanelSurfacePlanErrorCode::InvalidSurface);
}

QTEST_GUILESS_MAIN(PanelSurfaceConfigurationTests)
#include "tst_panel_surface_configuration.moc"
