// SPDX-License-Identifier: GPL-3.0-or-later
#include "orchestration_test_fixtures.h"

#include "qindaqt/shell_orchestration/panel_visibility_inventory_assembler.h"

#include <QtTest>

using namespace QindaQt;

namespace {

void verifyFailure(const ShellOrchestration::PanelVisibilityAssemblyResult &result,
                   ShellOrchestration::PanelVisibilityAssemblyErrorCode code)
{
    QVERIFY(!result.ok());
    QVERIFY(!result.inventory.has_value());
    QVERIFY(result.evaluation.decisions.isEmpty());
    QCOMPARE(result.error.code, code);
    QVERIFY(!result.error.message.isEmpty());
}

} // namespace

class PanelVisibilityInventoryAssemblerTests final : public QObject {
    Q_OBJECT

private slots:
    void expandsWildcardPanelsAcrossExactOutputs();
    void appliesWindowPolicyAndInteractionOverrides();
    void rejectsOutputDriftAtomically();
    void rejectsMissingDuplicateAndForgedSurfaces();
    void rejectsInvalidRuntimeInventory();
};

void PanelVisibilityInventoryAssemblerTests::expandsWildcardPanelsAcrossExactOutputs()
{
    using namespace ShellOrchestration::TestFixtures;
    auto layoutProfile = profile();
    const auto solved = layout(layoutProfile);
    const auto snapshot = compositor();

    const auto result = ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
        layoutProfile, solved, snapshot, {});

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.inventory->outputs, snapshot.outputs);
    QCOMPARE(result.inventory->panels.size(), 2);
    QCOMPARE(result.evaluation.decisions.size(), 2);
    for (const auto &decision : result.evaluation.decisions) {
        QCOMPARE(decision.visibility, ShellVisibility::PanelVisibility::Visible);
        QCOMPARE(decision.reservation,
                 ShellVisibility::PanelReservationIntent::Reserve);
        QCOMPARE(decision.reason, ShellVisibility::PanelVisibilityReason::NeverMode);
    }
}

void PanelVisibilityInventoryAssemblerTests::appliesWindowPolicyAndInteractionOverrides()
{
    using namespace ShellOrchestration::TestFixtures;
    auto panelSpec = panel();
    panelSpec.hideMode = Profiles::HideMode::DodgeAll;
    auto layoutProfile = profile(panelSpec);
    const auto solved = layout(layoutProfile);
    auto snapshot = compositor();
    ShellVisibility::LogicalWindowSnapshot window;
    window.id = QStringLiteral("window-1");
    window.outputId = QStringLiteral("main");
    window.frameGeometry = {0, 0, 800, 500};
    window.workspaceIds = {snapshot.scope.workspaceId};
    window.activityIds = {snapshot.scope.activityId};
    snapshot.windows = {window};

    const auto hidden = ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
        layoutProfile, solved, snapshot, {});
    QVERIFY2(hidden.ok(), qPrintable(hidden.error.message));
    QCOMPARE(hidden.evaluation.decisions.at(1).visibility,
             ShellVisibility::PanelVisibility::Hidden);
    QCOMPARE(hidden.evaluation.decisions.at(1).reservation,
             ShellVisibility::PanelReservationIntent::Release);

    const auto held = ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
        layoutProfile, solved, snapshot,
        {{{QStringLiteral("main"), QStringLiteral("main")}, false, true}});
    QVERIFY2(held.ok(), qPrintable(held.error.message));
    QCOMPARE(held.evaluation.decisions.at(1).visibility,
             ShellVisibility::PanelVisibility::Visible);
    QCOMPARE(held.evaluation.decisions.at(1).reason,
             ShellVisibility::PanelVisibilityReason::VisibilityHeld);
}

void PanelVisibilityInventoryAssemblerTests::rejectsOutputDriftAtomically()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto layoutProfile = profile();
    const auto solved = layout(layoutProfile);

    auto geometryDrift = compositor();
    geometryDrift.outputs[1].geometry.translate(1, 0);
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, solved, geometryDrift, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::OutputMismatch);

    auto scaleDrift = compositor();
    scaleDrift.outputs[1].scale = 2.0;
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, solved, scaleDrift, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::OutputMismatch);

    auto missing = compositor();
    missing.outputs.removeLast();
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, solved, missing, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::OutputMismatch);
}

void PanelVisibilityInventoryAssemblerTests::rejectsMissingDuplicateAndForgedSurfaces()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto layoutProfile = profile();
    const auto snapshot = compositor();

    auto missing = layout(layoutProfile);
    missing.surfaces.removeLast();
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, missing, snapshot, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::MissingSurface);

    auto duplicate = layout(layoutProfile);
    duplicate.surfaces.append(duplicate.surfaces.constFirst());
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, duplicate, snapshot, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::DuplicateSurface);

    auto forged = layout(layoutProfile);
    forged.surfaces[0].edge = Profiles::Edge::Bottom;
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, forged, snapshot, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::SurfaceContractMismatch);
}

void PanelVisibilityInventoryAssemblerTests::rejectsInvalidRuntimeInventory()
{
    using namespace ShellOrchestration::TestFixtures;
    const auto layoutProfile = profile();
    const auto solved = layout(layoutProfile);
    const auto snapshot = compositor();

    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, solved, snapshot,
                      {{{QStringLiteral("unknown"), QStringLiteral("main")}, true, false}}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::InvalidInventory);

    auto invalidLineage = snapshot;
    invalidLineage.revision = 0;
    verifyFailure(ShellOrchestration::PanelVisibilityInventoryAssembler::assemble(
                      layoutProfile, solved, invalidLineage, {}),
                  ShellOrchestration::PanelVisibilityAssemblyErrorCode::InvalidInventory);
}

QTEST_GUILESS_MAIN(PanelVisibilityInventoryAssemblerTests)
#include "tst_panel_visibility_inventory_assembler.moc"
