// SPDX-License-Identifier: GPL-3.0-or-later
#include "orchestration_test_fixtures.h"

#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_orchestration/panel_profile_output_filter.h"
#include "qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h"

#include <QtTest>

using namespace QindaQt;

class PanelProfileOutputOccupancyTests final : public QObject {
    Q_OBJECT

private slots:
    void keepsWildcardPanelsAndPinsThatResolve();
    void dropsOnlyThePanelWhosePinnedOutputIsAbsent();
    void anAbsentPinLeavesEveryOtherPanelSolvableAndEvaluable();
};

void PanelProfileOutputOccupancyTests::keepsWildcardPanelsAndPinsThatResolve()
{
    using namespace ShellOrchestration;
    auto pinned = TestFixtures::panel(QStringLiteral("pinned"));
    pinned.output = QStringLiteral("main");
    auto profile = TestFixtures::profile();
    profile.panels.append(pinned);

    const auto filtered = PanelProfileOutputOccupancy::presentOutputsOnly(
        profile, TestFixtures::outputs());

    QCOMPARE(filtered.panels.size(), 2);
    QCOMPARE(filtered.id, profile.id);
}

void PanelProfileOutputOccupancyTests::dropsOnlyThePanelWhosePinnedOutputIsAbsent()
{
    using namespace ShellOrchestration;
    auto pinned = TestFixtures::panel(QStringLiteral("pinned"));
    pinned.output = QStringLiteral("unplugged");
    auto profile = TestFixtures::profile();
    profile.panels.append(pinned);

    const auto filtered = PanelProfileOutputOccupancy::presentOutputsOnly(
        profile, TestFixtures::outputs());

    QCOMPARE(filtered.panels.size(), 1);
    QCOMPARE(filtered.panels.constFirst().id, QStringLiteral("main"));
}

// Regression: a dock pinned to the built-in display used to take every panel
// on the remaining displays with it the moment that display left the
// generation - the solve failed, so the shell kept a stale surface set. The
// solver and the assembler stay strict; the runtime hands both the same
// filtered profile.
void PanelProfileOutputOccupancyTests::anAbsentPinLeavesEveryOtherPanelSolvableAndEvaluable()
{
    using namespace ShellOrchestration;
    auto pinned = TestFixtures::panel(QStringLiteral("dock"));
    pinned.output = QStringLiteral("unplugged");
    auto profile = TestFixtures::profile();
    profile.panels.append(pinned);
    const auto outputs = TestFixtures::outputs();

    // Unfiltered is still rejected, which is what refuses a user who picks a
    // display that is not there.
    QVERIFY(!ShellLayout::PanelLayoutSolver::solve(profile.panels, outputs).ok());

    const auto filtered =
        PanelProfileOutputOccupancy::presentOutputsOnly(profile, outputs);
    const auto layout = ShellLayout::PanelLayoutSolver::solve(filtered.panels, outputs);
    QVERIFY2(layout.ok(), qPrintable(layout.error.message));

    const auto evaluation = PanelVisibilityInventoryAssembler::assemble(
        filtered, layout, TestFixtures::compositor(outputs), {});
    QVERIFY2(evaluation.ok(), qPrintable(evaluation.error.message));
    QCOMPARE(evaluation.evaluation.decisions.size(), outputs.size());
}

QTEST_GUILESS_MAIN(PanelProfileOutputOccupancyTests)
#include "tst_panel_profile_output_filter.moc"
