// SPDX-License-Identifier: GPL-3.0-or-later
#include "orchestration_test_fixtures.h"

#include "qindaqt/shell_orchestration/panel_runtime_plan_assembler.h"

#include <QtTest>

using namespace QindaQt;

namespace {

const ShellSurface::PanelSurfaceConfiguration &surface(
    const ShellSurface::PanelSurfacePlan &plan, const QString &outputId)
{
    for (const auto &candidate : plan.surfaces) {
        if (candidate.identity.outputId == outputId) {
            return candidate;
        }
    }
    Q_UNREACHABLE_RETURN(plan.surfaces.constFirst());
}

} // namespace

class PanelRuntimePlanAssemblerTests final : public QObject {
    Q_OBJECT

private slots:
    void safeFallbackMapsAndReservesEligibleSurfaces();
    void appliesAnExactVisibilityGeneration();
    void rejectsFailedAndMismatchedEvaluations();
};

void PanelRuntimePlanAssemblerTests::safeFallbackMapsAndReservesEligibleSurfaces()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto base = surfacePlan(profile());
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    const auto result = ShellOrchestration::PanelRuntimePlanAssembler::safeVisible(base);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.plan, base);
    for (const auto &candidate : result.plan.surfaces) {
        QCOMPARE(candidate.mapping, ShellSurface::PanelSurfaceMapping::Mapped);
        QVERIFY(candidate.reservationCarrier);
        QVERIFY(candidate.exclusiveZone > 0);
    }
}

void PanelRuntimePlanAssemblerTests::appliesAnExactVisibilityGeneration()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto base = surfacePlan(profile());
    QVERIFY2(base.ok(), qPrintable(base.error.message));
    ShellVisibility::PanelVisibilityEvaluation evaluation;
    evaluation.decisions = {
        {{QStringLiteral("main"), QStringLiteral("left")},
         ShellVisibility::PanelVisibility::Visible,
         ShellVisibility::PanelReservationIntent::Reserve,
         ShellVisibility::PanelVisibilityReason::NoConflict,
         {}},
        {{QStringLiteral("main"), QStringLiteral("main")},
         ShellVisibility::PanelVisibility::Hidden,
         ShellVisibility::PanelReservationIntent::Release,
         ShellVisibility::PanelVisibilityReason::AnyWindowOverlap,
         QStringLiteral("window-1")},
    };

    const auto result = ShellOrchestration::PanelRuntimePlanAssembler::fromEvaluation(
        base, evaluation);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(surface(result.plan, QStringLiteral("left")).mapping,
             ShellSurface::PanelSurfaceMapping::Mapped);
    QCOMPARE(surface(result.plan, QStringLiteral("main")).mapping,
             ShellSurface::PanelSurfaceMapping::Unmapped);
    QVERIFY(surface(result.plan, QStringLiteral("left")).reservationCarrier);
    QVERIFY(!surface(result.plan, QStringLiteral("main")).reservationCarrier);
}

void PanelRuntimePlanAssemblerTests::rejectsFailedAndMismatchedEvaluations()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto base = surfacePlan(profile());
    QVERIFY2(base.ok(), qPrintable(base.error.message));

    ShellVisibility::PanelVisibilityEvaluation failed;
    failed.error.code = ShellVisibility::PanelVisibilityErrorCode::InvalidScope;
    failed.error.message = QStringLiteral("invalid scope");
    const auto rejected = ShellOrchestration::PanelRuntimePlanAssembler::fromEvaluation(
        base, failed);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error.code,
             ShellOrchestration::PanelRuntimeAssemblyErrorCode::RejectedVisibilityEvaluation);
    QVERIFY(rejected.plan.surfaces.isEmpty());

    ShellVisibility::PanelVisibilityEvaluation incomplete;
    incomplete.decisions = {
        {{QStringLiteral("main"), QStringLiteral("left")},
         ShellVisibility::PanelVisibility::Visible,
         ShellVisibility::PanelReservationIntent::Reserve,
         ShellVisibility::PanelVisibilityReason::NoConflict,
         {}}};
    const auto mismatched = ShellOrchestration::PanelRuntimePlanAssembler::fromEvaluation(
        base, incomplete);
    QVERIFY(!mismatched.ok());
    QCOMPARE(mismatched.error.code,
             ShellOrchestration::PanelRuntimeAssemblyErrorCode::RuntimePlanRejected);
    QCOMPARE(mismatched.runtimeError.code,
             ShellSurface::PanelSurfaceRuntimePlanErrorCode::MissingDecision);
    QVERIFY(mismatched.plan.surfaces.isEmpty());
}

QTEST_GUILESS_MAIN(PanelRuntimePlanAssemblerTests)
#include "tst_panel_runtime_plan_assembler.moc"
