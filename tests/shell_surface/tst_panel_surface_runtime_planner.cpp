// SPDX-License-Identifier: GPL-3.0-or-later
#include "surface_test_fixtures.h"

#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"
#include "qindaqt/shell_surface/panel_surface_runtime_planner.h"

#include <QtTest>

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

ShellSurface::PanelSurfacePlan basePlan(
    const QVector<Profiles::PanelSpec> &panels,
    const QVector<ShellLayout::LogicalOutput> &outputs = {
        ShellSurface::TestFixtures::output()})
{
    return ShellSurface::PanelSurfaceConfigurationPlanner::plan(
        ShellSurface::TestFixtures::solve(panels, outputs));
}

QVector<ShellSurface::PanelSurfaceRuntimeDecision> mappedDecisions(
    const ShellSurface::PanelSurfacePlan &plan)
{
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> result;
    result.reserve(plan.surfaces.size());
    for (const auto &candidate : plan.surfaces) {
        result.append({candidate.identity, ShellSurface::PanelSurfaceMapping::Mapped,
                       candidate.reservesWorkArea});
    }
    return result;
}

ShellSurface::PanelSurfaceRuntimeDecision &decision(
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> &decisions,
    const char *panelId, const char *outputId = "main")
{
    for (auto &candidate : decisions) {
        if (candidate.identity.panelId == QLatin1StringView(panelId) &&
            candidate.identity.outputId == QLatin1StringView(outputId)) {
            return candidate;
        }
    }
    Q_UNREACHABLE_RETURN(decisions.first());
}

void verifyFailure(const ShellSurface::PanelSurfaceRuntimePlanResult &result,
                   ShellSurface::PanelSurfaceRuntimePlanErrorCode code)
{
    QVERIFY(!result.ok());
    QCOMPARE(result.error.code, code);
    QVERIFY(result.plan.surfaces.isEmpty());
    QVERIFY(!result.error.message.isEmpty());
}

} // namespace

class PanelSurfaceRuntimePlannerTests final : public QObject {
    Q_OBJECT

private slots:
    void preservesTheStaticPlanWhenEverySurfaceUsesItsDefaultPolicy();
    void promotesTheNextVisibleReservationCarrier();
    void releasesAnEdgeWhenEveryEligibleSurfaceIsHidden();
    void rejectsInvalidAndIneligibleDecisionsAtomically();
    void requiresAnExactDecisionBijection();
    void rejectsForgedBasePlansBeforeRuntimeMutation();
    void isolatesOutputsAndEdges();
    void preservesStretchedSideGeometryWhenItsCarrierIsDemoted();
    void preservesPartialSideGeometryWhenItsCarrierIsDemoted();
    void recomputesMarginsWhenAFormerNoncarrierIsPromoted();
};

void PanelSurfaceRuntimePlannerTests::
    preservesTheStaticPlanWhenEverySurfaceUsesItsDefaultPolicy()
{
    using namespace ShellSurface::TestFixtures;
    auto overlay = panel(QStringLiteral("overlay"), Profiles::Edge::Top,
                         Profiles::Layer::Overlay, 22);
    auto top = panel(QStringLiteral("top"), Profiles::Edge::Top,
                     Profiles::Layer::Above, 30);
    auto left = panel(QStringLiteral("left"), Profiles::Edge::Left,
                      Profiles::Layer::Normal, 36);
    auto below = panel(QStringLiteral("below"), Profiles::Edge::Right,
                       Profiles::Layer::Below, 28);
    const auto base = basePlan({overlay, top, left, below});
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(
        base, mappedDecisions(base));

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.plan, base);
}

void PanelSurfaceRuntimePlannerTests::promotesTheNextVisibleReservationCarrier()
{
    using namespace ShellSurface::TestFixtures;
    auto shallow = panel(QStringLiteral("shallow"), Profiles::Edge::Top,
                         Profiles::Layer::Above, 20);
    auto deep = panel(QStringLiteral("deep"), Profiles::Edge::Top,
                      Profiles::Layer::Normal, 30);
    const auto base = basePlan({shallow, deep});
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    auto decisions = mappedDecisions(base);
    decision(decisions, "deep") = {
        {QStringLiteral("deep"), QStringLiteral("main")},
        ShellSurface::PanelSurfaceMapping::Unmapped, false};

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    const auto &promoted = surface(result.plan, "shallow");
    QVERIFY(promoted.reservationCarrier);
    QCOMPARE(promoted.exclusiveZone, 20);
    QCOMPARE(promoted.placementOrder, qsizetype(0));
    const auto &hidden = surface(result.plan, "deep");
    QCOMPARE(hidden.mapping, ShellSurface::PanelSurfaceMapping::Unmapped);
    QVERIFY(!hidden.reservationCarrier);
    QCOMPARE(hidden.exclusiveZone, -1);
}

void PanelSurfaceRuntimePlannerTests::releasesAnEdgeWhenEveryEligibleSurfaceIsHidden()
{
    using namespace ShellSurface::TestFixtures;
    const auto base = basePlan({
        panel(QStringLiteral("one"), Profiles::Edge::Bottom,
              Profiles::Layer::Above, 20),
        panel(QStringLiteral("two"), Profiles::Edge::Bottom,
              Profiles::Layer::Normal, 24),
    });
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    auto decisions = mappedDecisions(base);
    for (auto &item : decisions) {
        item.mapping = ShellSurface::PanelSurfaceMapping::Unmapped;
        item.reserve = false;
    }

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    for (const auto &item : result.plan.surfaces) {
        QCOMPARE(item.mapping, ShellSurface::PanelSurfaceMapping::Unmapped);
        QVERIFY(!item.reservationCarrier);
        QCOMPARE(item.exclusiveZone, -1);
    }
}

void PanelSurfaceRuntimePlannerTests::rejectsInvalidAndIneligibleDecisionsAtomically()
{
    using namespace ShellSurface::TestFixtures;
    const auto base = basePlan({
        panel(QStringLiteral("reserve"), Profiles::Edge::Top,
              Profiles::Layer::Above),
        panel(QStringLiteral("overlay"), Profiles::Edge::Bottom,
              Profiles::Layer::Overlay),
    });
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    auto invalidMapping = mappedDecisions(base);
    decision(invalidMapping, "reserve").mapping =
        static_cast<ShellSurface::PanelSurfaceMapping>(99);
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, invalidMapping),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::InvalidDecisionState);

    auto hiddenReservation = mappedDecisions(base);
    decision(hiddenReservation, "reserve").mapping =
        ShellSurface::PanelSurfaceMapping::Unmapped;
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, hiddenReservation),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::HiddenSurfaceReservation);

    auto ineligible = mappedDecisions(base);
    decision(ineligible, "overlay").reserve = true;
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, ineligible),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::IneligibleSurfaceReservation);
}

void PanelSurfaceRuntimePlannerTests::requiresAnExactDecisionBijection()
{
    using namespace ShellSurface::TestFixtures;
    const auto base = basePlan({panel(QStringLiteral("top"))});
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, {}),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::MissingDecision);

    auto duplicate = mappedDecisions(base);
    duplicate.append(duplicate.constFirst());
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, duplicate),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::DuplicateDecision);

    auto unknown = mappedDecisions(base);
    unknown.append({{QStringLiteral("unknown"), QStringLiteral("main")},
                    ShellSurface::PanelSurfaceMapping::Mapped, false});
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, unknown),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::UnknownDecision);

    auto emptyIdentity = mappedDecisions(base);
    emptyIdentity[0].identity.panelId.clear();
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(base, emptyIdentity),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::InvalidDecisionIdentity);
}

void PanelSurfaceRuntimePlannerTests::rejectsForgedBasePlansBeforeRuntimeMutation()
{
    using namespace ShellSurface::TestFixtures;
    const auto base = basePlan({panel(QStringLiteral("top"))});
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    auto duplicate = base;
    duplicate.surfaces.append(duplicate.surfaces.constFirst());
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(
                      duplicate, mappedDecisions(duplicate)),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::DuplicateBaseSurface);

    auto alreadyHidden = base;
    alreadyHidden.surfaces[0].mapping = ShellSurface::PanelSurfaceMapping::Unmapped;
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(
                      alreadyHidden, mappedDecisions(alreadyHidden)),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::InvalidSurface);

    auto forgedEligibility = base;
    forgedEligibility.surfaces[0].reservesWorkArea = false;
    verifyFailure(ShellSurface::PanelSurfaceRuntimePlanner::apply(
                      forgedEligibility, mappedDecisions(forgedEligibility)),
                  ShellSurface::PanelSurfaceRuntimePlanErrorCode::InvalidSurface);
}

void PanelSurfaceRuntimePlannerTests::isolatesOutputsAndEdges()
{
    using namespace ShellSurface::TestFixtures;
    auto wildcardTop = panel(QStringLiteral("top"), Profiles::Edge::Top,
                             Profiles::Layer::Above, 20);
    auto primaryBottom = panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
                               Profiles::Layer::Above, 30);
    primaryBottom.output = QStringLiteral("primary");
    const auto base = basePlan(
        {wildcardTop, primaryBottom},
        {output(QStringLiteral("secondary"), {-1600, 0, 1600, 900}, 1.0),
         output(QStringLiteral("primary"), {0, 0, 2560, 1440}, 1.5)});
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    auto decisions = mappedDecisions(base);
    decision(decisions, "top", "primary") = {
        {QStringLiteral("top"), QStringLiteral("primary")},
        ShellSurface::PanelSurfaceMapping::Unmapped, false};

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QVERIFY(surface(result.plan, "top", "secondary").reservationCarrier);
    QVERIFY(!surface(result.plan, "top", "primary").reservationCarrier);
    QVERIFY(surface(result.plan, "bottom", "primary").reservationCarrier);
}

void PanelSurfaceRuntimePlannerTests::
    preservesStretchedSideGeometryWhenItsCarrierIsDemoted()
{
    using namespace ShellSurface::TestFixtures;
    const auto base = basePlan({
        panel(QStringLiteral("top"), Profiles::Edge::Top,
              Profiles::Layer::Above, 30),
        panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
              Profiles::Layer::Above, 40),
        panel(QStringLiteral("side"), Profiles::Edge::Left,
              Profiles::Layer::Above, 32),
    });
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    QVERIFY(surface(base, "side").reservationCarrier);
    auto decisions = mappedDecisions(base);
    decision(decisions, "side").reserve = false;

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    const auto &demoted = surface(result.plan, "side");
    QCOMPARE(demoted.geometry, surface(base, "side").geometry);
    QVERIFY(!demoted.reservationCarrier);
    QCOMPARE(demoted.exclusiveZone, -1);
    QCOMPARE(demoted.margins.top(), 30);
    QCOMPARE(demoted.margins.bottom(), 40);
}

void PanelSurfaceRuntimePlannerTests::
    preservesPartialSideGeometryWhenItsCarrierIsDemoted()
{
    using namespace ShellSurface::TestFixtures;
    auto side = panel(QStringLiteral("side"), Profiles::Edge::Right,
                      Profiles::Layer::Above, 32);
    side.alignment = Profiles::Alignment::Center;
    side.length = 0.5;
    const auto base = basePlan({
        panel(QStringLiteral("top"), Profiles::Edge::Top,
              Profiles::Layer::Above, 30),
        panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
              Profiles::Layer::Above, 40),
        side,
    });
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    auto decisions = mappedDecisions(base);
    decision(decisions, "side").reserve = false;

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    const auto &demoted = surface(result.plan, "side");
    QCOMPARE(demoted.geometry, surface(base, "side").geometry);
    QCOMPARE(demoted.margins.top(), demoted.geometry.top());
    QVERIFY(!demoted.anchors.testFlag(ShellSurface::SurfaceAnchor::Bottom));
}

void PanelSurfaceRuntimePlannerTests::
    recomputesMarginsWhenAFormerNoncarrierIsPromoted()
{
    using namespace ShellSurface::TestFixtures;
    auto shallow = panel(QStringLiteral("shallow"), Profiles::Edge::Left,
                         Profiles::Layer::Above, 20);
    auto deep = panel(QStringLiteral("deep"), Profiles::Edge::Left,
                      Profiles::Layer::Normal, 24);
    const auto base = basePlan({
        panel(QStringLiteral("top"), Profiles::Edge::Top,
              Profiles::Layer::Above, 30),
        panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
              Profiles::Layer::Above, 40),
        shallow, deep,
    });
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    QVERIFY(!surface(base, "shallow").reservationCarrier);
    auto decisions = mappedDecisions(base);
    decision(decisions, "deep") = {
        {QStringLiteral("deep"), QStringLiteral("main")},
        ShellSurface::PanelSurfaceMapping::Unmapped, false};

    const auto result = ShellSurface::PanelSurfaceRuntimePlanner::apply(base, decisions);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    const auto &promoted = surface(result.plan, "shallow");
    QVERIFY(promoted.reservationCarrier);
    QCOMPARE(promoted.geometry, surface(base, "shallow").geometry);
    QCOMPARE(promoted.margins.top(), 0);
    QCOMPARE(promoted.margins.bottom(), 0);
}

QTEST_GUILESS_MAIN(PanelSurfaceRuntimePlannerTests)
#include "tst_panel_surface_runtime_planner.moc"
