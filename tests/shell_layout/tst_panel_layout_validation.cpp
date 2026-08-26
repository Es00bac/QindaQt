// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include <QtTest>

#include <limits>
#include <utility>

using namespace QindaQt;

namespace {

Profiles::PanelSpec validPanel(QString id = QStringLiteral("panel"))
{
    Profiles::PanelSpec panel;
    panel.id = std::move(id);
    panel.thickness = 20;
    return panel;
}

ShellLayout::LogicalOutput validOutput(QString id = QStringLiteral("main"))
{
    return {std::move(id), {0, 0, 1920, 1080}, 1.0};
}

void verifyAtomicFailure(const ShellLayout::PanelLayoutResult &result,
                         ShellLayout::PanelLayoutErrorCode expected)
{
    QVERIFY(!result.ok());
    QCOMPARE(result.error.code, expected);
    QVERIFY(result.surfaces.isEmpty());
    QVERIFY(result.outputs.isEmpty());
    QVERIFY(!result.error.message.isEmpty());
}

} // namespace

class PanelLayoutValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMalformedOutputInventoryAtomically();
    void rejectsInvalidPanelValuesAtomically();
    void rejectsMissingAndDuplicateExpandedTargets();
    void rejectsOverConstrainedOutputs();
    void rejectsExtremeArithmeticInputsAtomically();
    void acceptsRepresentableIntegerBoundaryGeometry();
    void permitsAProfileWithoutPanels();
};

void PanelLayoutValidationTests::rejectsMalformedOutputInventoryAtomically()
{
    const auto panel = validPanel();
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {}),
                        ShellLayout::PanelLayoutErrorCode::EmptyOutputInventory);

    auto emptyId = validOutput(QString());
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {emptyId}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputId);

    verifyAtomicFailure(
        ShellLayout::PanelLayoutSolver::solve(
            {panel}, {validOutput(QStringLiteral("same")), validOutput(QStringLiteral("same"))}),
        ShellLayout::PanelLayoutErrorCode::DuplicateOutputId);

    auto badGeometry = validOutput();
    badGeometry.geometry = QRect(0, 0, 0, 1080);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {badGeometry}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputGeometry);

    auto tooWide = validOutput();
    tooWide.geometry.setCoords(std::numeric_limits<int>::min(), 0, std::numeric_limits<int>::max(),
                               99);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {tooWide}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputGeometry);

    auto tooTall = validOutput();
    tooTall.geometry.setCoords(0, std::numeric_limits<int>::min(), 99,
                               std::numeric_limits<int>::max());
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {tooTall}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputGeometry);

    auto zeroScale = validOutput();
    zeroScale.scale = 0.0;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {zeroScale}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputScale);

    auto nanScale = validOutput();
    nanScale.scale = std::numeric_limits<qreal>::quiet_NaN();
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {nanScale}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputScale);

    // A valid first output must not escape when a later inventory item fails.
    nanScale.id = QStringLiteral("later-bad-scale");
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput(), nanScale}),
                        ShellLayout::PanelLayoutErrorCode::InvalidOutputScale);
}

void PanelLayoutValidationTests::rejectsInvalidPanelValuesAtomically()
{
    auto panel = validPanel();
    panel.rows = 0;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.thickness = 0;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.length = std::numeric_limits<double>::quiet_NaN();
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.edge = static_cast<Profiles::Edge>(99);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.id = QStringLiteral("   ");
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.output = QStringLiteral("   ");
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.rows = 5;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.thickness = 193;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.length = 0.09;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.layer = static_cast<Profiles::Layer>(99);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.hideMode = static_cast<Profiles::HideMode>(99);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    panel = validPanel();
    panel.alignment = static_cast<Profiles::Alignment>(99);
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({panel}, {validOutput()}),
                        ShellLayout::PanelLayoutErrorCode::InvalidPanel);
}

void PanelLayoutValidationTests::rejectsMissingAndDuplicateExpandedTargets()
{
    auto missing = validPanel();
    missing.output = QStringLiteral("absent");
    const auto missingResult = ShellLayout::PanelLayoutSolver::solve({missing}, {validOutput()});
    verifyAtomicFailure(missingResult, ShellLayout::PanelLayoutErrorCode::MissingOutput);
    QCOMPARE(missingResult.error.outputId, QStringLiteral("absent"));

    auto first = validPanel(QStringLiteral("duplicate"));
    auto second = validPanel(QStringLiteral("duplicate"));
    second.edge = Profiles::Edge::Bottom;
    const auto duplicateResult =
        ShellLayout::PanelLayoutSolver::solve({first, second}, {validOutput()});
    verifyAtomicFailure(duplicateResult, ShellLayout::PanelLayoutErrorCode::DuplicatePanelInstance);

    // A wildcard colliding with a named instance on just one output still
    // invalidates every previously staged expansion.
    second.output = QStringLiteral("external");
    const auto expansionResult = ShellLayout::PanelLayoutSolver::solve(
        {first, second}, {validOutput(), validOutput(QStringLiteral("external"))});
    verifyAtomicFailure(expansionResult, ShellLayout::PanelLayoutErrorCode::DuplicatePanelInstance);
    QCOMPARE(expansionResult.error.outputId, QStringLiteral("external"));
}

void PanelLayoutValidationTests::rejectsOverConstrainedOutputs()
{
    auto top = validPanel(QStringLiteral("top"));
    top.thickness = 60;
    auto bottom = validPanel(QStringLiteral("bottom"));
    bottom.edge = Profiles::Edge::Bottom;
    bottom.thickness = 40;

    const ShellLayout::LogicalOutput tiny = {QStringLiteral("tiny"), {0, 0, 100, 100}, 1.0};
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({top, bottom}, {tiny}),
                        ShellLayout::PanelLayoutErrorCode::OverConstrainedOutput);

    auto tooDeep = validPanel(QStringLiteral("too-deep"));
    tooDeep.rows = 2;
    tooDeep.thickness = 60;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({tooDeep}, {tiny}),
                        ShellLayout::PanelLayoutErrorCode::OverConstrainedOutput);

    auto left = validPanel(QStringLiteral("left"));
    left.edge = Profiles::Edge::Left;
    left.thickness = 50;
    auto right = validPanel(QStringLiteral("right"));
    right.edge = Profiles::Edge::Right;
    right.thickness = 50;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({left, right}, {tiny}),
                        ShellLayout::PanelLayoutErrorCode::OverConstrainedOutput);

    auto almostTop = validPanel(QStringLiteral("almost-top"));
    almostTop.thickness = 80;
    auto lastBottom = validPanel(QStringLiteral("last-bottom"));
    lastBottom.edge = Profiles::Edge::Bottom;
    lastBottom.thickness = 20;
    verifyAtomicFailure(ShellLayout::PanelLayoutSolver::solve({almostTop, lastBottom}, {tiny}),
                        ShellLayout::PanelLayoutErrorCode::OverConstrainedOutput);
}

void PanelLayoutValidationTests::rejectsExtremeArithmeticInputsAtomically()
{
    auto hugeFirst = validPanel(QStringLiteral("huge-first"));
    hugeFirst.rows = std::numeric_limits<int>::max();
    hugeFirst.thickness = std::numeric_limits<int>::max();
    auto hugeSecond = hugeFirst;
    hugeSecond.id = QStringLiteral("huge-second");
    auto hugeThird = hugeFirst;
    hugeThird.id = QStringLiteral("huge-third");

    // Extreme typed values must stop at the Profiles-owned validation boundary
    // before multiplication or stack accumulation can overflow.
    const auto overflowAttempt =
        ShellLayout::PanelLayoutSolver::solve({hugeFirst, hugeSecond, hugeThird}, {validOutput()});
    verifyAtomicFailure(overflowAttempt, ShellLayout::PanelLayoutErrorCode::InvalidPanel);

    auto maximumValidated = validPanel(QStringLiteral("maximum-validated"));
    maximumValidated.rows = 4;
    maximumValidated.thickness = 192;
    const auto maximumResult = ShellLayout::PanelLayoutSolver::solve(
        {maximumValidated}, {{QStringLiteral("large"), {0, 0, 1000, 1000}, 1.0}});
    QVERIFY2(maximumResult.ok(), qPrintable(maximumResult.error.message));
    QCOMPARE(maximumResult.surfaces.constFirst().geometry, QRect(0, 0, 1000, 768));
}

void PanelLayoutValidationTests::acceptsRepresentableIntegerBoundaryGeometry()
{
    auto boundaryPanel = validPanel();
    boundaryPanel.thickness = 20;

    constexpr int extent = 100;
    const ShellLayout::LogicalOutput lower = {
        QStringLiteral("lower"),
        {std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), extent, extent},
        1.0};
    const auto lowerResult = ShellLayout::PanelLayoutSolver::solve({boundaryPanel}, {lower});
    QVERIFY2(lowerResult.ok(), qPrintable(lowerResult.error.message));

    constexpr int upperOrigin = std::numeric_limits<int>::max() - extent + 1;
    const ShellLayout::LogicalOutput upper = {
        QStringLiteral("upper"),
        QRect(QPoint(upperOrigin, upperOrigin),
              QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max())),
        1.0};
    boundaryPanel.edge = Profiles::Edge::Bottom;
    const auto upperResult = ShellLayout::PanelLayoutSolver::solve({boundaryPanel}, {upper});
    QVERIFY2(upperResult.ok(), qPrintable(upperResult.error.message));
    QCOMPARE(upperResult.surfaces.constFirst().geometry.bottom(), std::numeric_limits<int>::max());
}

void PanelLayoutValidationTests::permitsAProfileWithoutPanels()
{
    const auto result = ShellLayout::PanelLayoutSolver::solve({}, {validOutput()});
    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QVERIFY(result.surfaces.isEmpty());
    QCOMPARE(result.outputs.size(), 1);
    QCOMPARE(result.outputs.constFirst().workArea, QRect(0, 0, 1920, 1080));
}

QTEST_GUILESS_MAIN(PanelLayoutValidationTests)
#include "tst_panel_layout_validation.moc"
