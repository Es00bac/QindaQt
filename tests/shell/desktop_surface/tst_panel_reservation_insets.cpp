// SPDX-License-Identifier: GPL-3.0-or-later

// The runtime half of the desktop work-area contract (ADR-0261): which panel
// surfaces reserve room on an output, and how deep, read from the same
// accepted plan the shell sends to the compositor. Every row builds its plan
// through the real layout solver and surface planners, so the adapter is
// checked against how those planners actually write carriers, exclusive zones
// and anchored-edge margins rather than against hand-made configurations.

#include "panelreservationinsets.h"

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"
#include "qindaqt/shell_surface/panel_surface_runtime_planner.h"

#include <QMargins>
#include <QtTest>

#include <utility>

using namespace QindaQt;
using QindaQt::Shell::PanelReservationInsets;

namespace {

const QRect kMain(0, 0, 1920, 1080);
const QRect kSide(1920, 0, 1280, 1024);

Profiles::PanelSpec panel(QString id, Profiles::Edge edge, int thickness,
                          Profiles::Layer layer = Profiles::Layer::Above)
{
    Profiles::PanelSpec result;
    result.id = std::move(id);
    result.edge = edge;
    result.layer = layer;
    result.thickness = thickness;
    return result;
}

Profiles::PanelSpec on(Profiles::PanelSpec spec, QString output)
{
    spec.output = std::move(output);
    return spec;
}

// Solves and plans `panels`, then applies the runtime visibility the shell
// would: every panel mapped and reserving when its layer may, except the ids
// in `hidden`, which are unmapped and reserve nothing (an auto-hidden panel).
ShellSurface::PanelSurfacePlan plan(
    const QVector<Profiles::PanelSpec> &panels,
    const QVector<ShellLayout::LogicalOutput> &outputs = {{QStringLiteral("MAIN"), kMain, 1.0}},
    const QStringList &hidden = {})
{
    const auto layout = ShellLayout::PanelLayoutSolver::solve(panels, outputs);
    if (!layout.ok()) {
        qWarning().noquote() << "fixture layout rejected:" << layout.error.message;
        return {};
    }
    const auto base = ShellSurface::PanelSurfaceConfigurationPlanner::plan(layout);
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> decisions;
    for (const auto &surface : base.surfaces) {
        const bool isHidden = hidden.contains(surface.identity.panelId);
        decisions.append({surface.identity,
                          isHidden ? ShellSurface::PanelSurfaceMapping::Unmapped
                                   : ShellSurface::PanelSurfaceMapping::Mapped,
                          !isHidden && surface.reservesWorkArea});
    }
    const auto runtime = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);
    if (!runtime.ok()) {
        qWarning().noquote() << "fixture runtime plan rejected:" << runtime.error.message;
        return {};
    }
    return runtime.plan;
}

} // namespace

class PanelReservationInsetsTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aTopBarReservesItsThickness();
    void aTopBarAndABottomDockReserveBothEdges();
    void sidePanelsReserveTheirOwnEdges();
    void stackedPanelsReserveTheWholeStackOnce();
    void eachOutputKeepsItsOwnPanels();
    void anAutoHiddenPanelReservesNothing();
    void anOverlayDockReservesNothing();
    void aRejectedPlanReservesNothing();
};

void PanelReservationInsetsTests::aTopBarReservesItsThickness()
{
    const auto insets = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 26)}));
    QCOMPARE(insets.size(), 1);
    QCOMPARE(insets.value(QStringLiteral("MAIN")), QMargins(0, 26, 0, 0));
}

void PanelReservationInsetsTests::aTopBarAndABottomDockReserveBothEdges()
{
    const auto insets = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 24),
              panel(QStringLiteral("dock"), Profiles::Edge::Bottom, 72)}));
    QCOMPARE(insets.value(QStringLiteral("MAIN")), QMargins(0, 24, 0, 72));
}

// The stock default layout's shape: a top bar plus a full-height left shelf,
// and a right-edge rail for the mirror image.
void PanelReservationInsetsTests::sidePanelsReserveTheirOwnEdges()
{
    const auto left = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 26),
              panel(QStringLiteral("shelf"), Profiles::Edge::Left, 52)}));
    QCOMPARE(left.value(QStringLiteral("MAIN")), QMargins(52, 26, 0, 0));

    const auto right = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("rail"), Profiles::Edge::Right, 64)}));
    QCOMPARE(right.value(QStringLiteral("MAIN")), QMargins(0, 0, 64, 0));
}

// Only the deepest panel carries the zone, and the protocol counts its
// anchored-edge margin inside it, so the depth is the whole stack - once.
void PanelReservationInsetsTests::stackedPanelsReserveTheWholeStackOnce()
{
    const auto runtime = plan({panel(QStringLiteral("outer"), Profiles::Edge::Top, 26),
                               panel(QStringLiteral("inner"), Profiles::Edge::Top, 30)});
    int carriers = 0;
    for (const auto &surface : runtime.surfaces) {
        carriers += surface.reservationCarrier ? 1 : 0;
    }
    QCOMPARE(carriers, 1);
    QCOMPARE(PanelReservationInsets::fromPlan(runtime).value(QStringLiteral("MAIN")),
             QMargins(0, 56, 0, 0));
}

void PanelReservationInsetsTests::eachOutputKeepsItsOwnPanels()
{
    const QVector<ShellLayout::LogicalOutput> outputs{
        {QStringLiteral("MAIN"), kMain, 1.0}, {QStringLiteral("SIDE"), kSide, 1.0}};
    const auto insets = PanelReservationInsets::fromPlan(plan(
        {on(panel(QStringLiteral("bar"), Profiles::Edge::Top, 26), QStringLiteral("MAIN")),
         on(panel(QStringLiteral("shelf"), Profiles::Edge::Left, 52), QStringLiteral("SIDE"))},
        outputs));
    QCOMPARE(insets.size(), 2);
    QCOMPARE(insets.value(QStringLiteral("MAIN")), QMargins(0, 26, 0, 0));
    QCOMPARE(insets.value(QStringLiteral("SIDE")), QMargins(52, 0, 0, 0));
}

// An auto-hiding panel that is hidden publishes no exclusive zone to the
// compositor, so it must not push desktop icons around either.
void PanelReservationInsetsTests::anAutoHiddenPanelReservesNothing()
{
    const auto insets = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 26),
              panel(QStringLiteral("dock"), Profiles::Edge::Bottom, 72)},
             {{QStringLiteral("MAIN"), kMain, 1.0}}, {QStringLiteral("dock")}));
    QCOMPARE(insets.value(QStringLiteral("MAIN")), QMargins(0, 26, 0, 0));

    const auto allHidden = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("dock"), Profiles::Edge::Bottom, 72)},
             {{QStringLiteral("MAIN"), kMain, 1.0}}, {QStringLiteral("dock")}));
    QVERIFY(allHidden.isEmpty());
}

// The macOS-style layout floats its dock over windows on the overlay layer;
// it never reserves, even while shown.
void PanelReservationInsetsTests::anOverlayDockReservesNothing()
{
    const auto insets = PanelReservationInsets::fromPlan(
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 24),
              panel(QStringLiteral("dock"), Profiles::Edge::Bottom, 72,
                    Profiles::Layer::Overlay)}));
    QCOMPARE(insets.value(QStringLiteral("MAIN")), QMargins(0, 24, 0, 0));
}

void PanelReservationInsetsTests::aRejectedPlanReservesNothing()
{
    ShellSurface::PanelSurfacePlan rejected =
        plan({panel(QStringLiteral("bar"), Profiles::Edge::Top, 26)});
    QVERIFY(!rejected.surfaces.isEmpty());
    rejected.error.code = ShellSurface::PanelSurfacePlanErrorCode::RejectedLayout;
    QVERIFY(PanelReservationInsets::fromPlan(rejected).isEmpty());
}

QTEST_GUILESS_MAIN(PanelReservationInsetsTests)
#include "tst_panel_reservation_insets.moc"
