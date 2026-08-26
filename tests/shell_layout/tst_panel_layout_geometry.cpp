// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include <QtTest>

#include <algorithm>
#include <limits>
#include <utility>

using namespace QindaQt;

namespace {

Profiles::PanelSpec panel(QString id, Profiles::Edge edge = Profiles::Edge::Top,
                          Profiles::Alignment alignment = Profiles::Alignment::Fill,
                          double length = 1.0, int thickness = 40)
{
    Profiles::PanelSpec result;
    result.id = std::move(id);
    result.edge = edge;
    result.alignment = alignment;
    result.length = length;
    result.thickness = thickness;
    return result;
}

ShellLayout::LogicalOutput output(QString id, QRect geometry = {0, 0, 1920, 1080},
                                  qreal scale = 1.0)
{
    return {std::move(id), geometry, scale};
}

const ShellLayout::OutputLayout &outputLayout(const ShellLayout::PanelLayoutResult &result,
                                              const QString &id)
{
    const auto found =
        std::find_if(result.outputs.cbegin(), result.outputs.cend(),
                     [&id](const auto &candidate) { return candidate.outputId == id; });
    Q_ASSERT(found != result.outputs.cend());
    return *found;
}

void verifyPairwiseNonIntersection(const ShellLayout::PanelLayoutResult &result)
{
    for (qsizetype first = 0; first < result.surfaces.size(); ++first) {
        for (qsizetype second = first + 1; second < result.surfaces.size(); ++second) {
            if (result.surfaces[first].outputId != result.surfaces[second].outputId) {
                continue;
            }
            const QRect &firstGeometry = result.surfaces[first].geometry;
            const QRect &secondGeometry = result.surfaces[second].geometry;
            const bool intersects =
                static_cast<qint64>(firstGeometry.left()) <= secondGeometry.right() &&
                static_cast<qint64>(secondGeometry.left()) <= firstGeometry.right() &&
                static_cast<qint64>(firstGeometry.top()) <= secondGeometry.bottom() &&
                static_cast<qint64>(secondGeometry.top()) <= firstGeometry.bottom();
            QVERIFY2(!intersects, qPrintable(QStringLiteral("panel '%1' intersects panel '%2'")
                                                 .arg(result.surfaces[first].panelId,
                                                      result.surfaces[second].panelId)));
        }
    }
}

} // namespace

class PanelLayoutGeometryTests final : public QObject {
    Q_OBJECT

private slots:
    void placesEveryEdge();
    void alignsFractionalLengths();
    void coversRepresentativeResolutions();
    void givesTopAndBottomStacksCornerOwnership();
    void keepsAllEdgesDisjointAtRepresentativeResolutions();
    void expandsWildcardAndNamedOutputsInLogicalCoordinates();
    void handlesNegativeAndIntegerBoundaryCoordinates();
    void stacksRowsAndComputesExclusiveWorkArea();
    void belowAndOverlayDoNotReserveWorkArea();
    void separatesPlacementLanesFromWorkAreaReservation();
};

void PanelLayoutGeometryTests::placesEveryEdge()
{
    struct Case {
        Profiles::Edge edge;
        QRect expected;
    };
    const QVector<Case> cases = {
        {Profiles::Edge::Top, {0, 0, 1920, 40}},
        {Profiles::Edge::Bottom, {0, 1040, 1920, 40}},
        {Profiles::Edge::Left, {0, 0, 40, 1080}},
        {Profiles::Edge::Right, {1880, 0, 40, 1080}},
    };

    for (const auto &testCase : cases) {
        const auto result = ShellLayout::PanelLayoutSolver::solve(
            {panel(QStringLiteral("edge"), testCase.edge)}, {output(QStringLiteral("main"))});
        QVERIFY2(result.ok(), qPrintable(result.error.message));
        QCOMPARE(result.surfaces.size(), 1);
        QCOMPARE(result.surfaces.constFirst().geometry, testCase.expected);
    }
}

void PanelLayoutGeometryTests::alignsFractionalLengths()
{
    struct Case {
        Profiles::Alignment alignment;
        QRect expected;
    };
    const QVector<Case> cases = {
        {Profiles::Alignment::Start, {0, 0, 960, 40}},
        {Profiles::Alignment::Center, {480, 0, 960, 40}},
        {Profiles::Alignment::End, {960, 0, 960, 40}},
        // Fill deliberately owns the entire edge; fractional length applies
        // only to the three positioned alignments.
        {Profiles::Alignment::Fill, {0, 0, 1920, 40}},
    };

    for (const auto &testCase : cases) {
        const auto result = ShellLayout::PanelLayoutSolver::solve(
            {panel(QStringLiteral("aligned"), Profiles::Edge::Top, testCase.alignment, 0.5)},
            {output(QStringLiteral("main"))});
        QVERIFY2(result.ok(), qPrintable(result.error.message));
        QCOMPARE(result.surfaces.constFirst().geometry, testCase.expected);
    }

    const auto vertical = ShellLayout::PanelLayoutSolver::solve(
        {panel(QStringLiteral("vertical"), Profiles::Edge::Right, Profiles::Alignment::Center,
               0.5)},
        {output(QStringLiteral("main"))});
    QVERIFY2(vertical.ok(), qPrintable(vertical.error.message));
    QCOMPARE(vertical.surfaces.constFirst().geometry, QRect(1880, 270, 40, 540));
}

void PanelLayoutGeometryTests::coversRepresentativeResolutions()
{
    struct Case {
        QSize resolution;
        QRect expected;
    };
    const QVector<Case> cases = {
        {{1920, 1080}, {480, 1040, 960, 40}},
        {{1920, 1200}, {480, 1160, 960, 40}},
        {{2560, 1440}, {640, 1400, 1280, 40}},
    };

    for (const auto &testCase : cases) {
        const auto result = ShellLayout::PanelLayoutSolver::solve(
            {panel(QStringLiteral("dock"), Profiles::Edge::Bottom, Profiles::Alignment::Center,
                   0.5)},
            {output(QStringLiteral("main"), QRect(QPoint(0, 0), testCase.resolution))});
        QVERIFY2(result.ok(), qPrintable(result.error.message));
        QCOMPARE(result.surfaces.constFirst().geometry, testCase.expected);
    }
}

void PanelLayoutGeometryTests::givesTopAndBottomStacksCornerOwnership()
{
    auto left =
        panel(QStringLiteral("left"), Profiles::Edge::Left, Profiles::Alignment::Center, 0.5, 20);
    auto bottom = panel(QStringLiteral("bottom"), Profiles::Edge::Bottom);
    bottom.rows = 3;
    bottom.thickness = 20;
    auto right =
        panel(QStringLiteral("right"), Profiles::Edge::Right, Profiles::Alignment::Start, 0.5, 20);
    auto top = panel(QStringLiteral("top"), Profiles::Edge::Top);
    top.rows = 2;
    top.thickness = 20;

    // Deliberately put the side panels first. Their lane must use the complete
    // top/bottom stacks, not only the panels seen earlier in profile order.
    const auto result = ShellLayout::PanelLayoutSolver::solve(
        {left, bottom, right, top}, {output(QStringLiteral("main"), {0, 0, 200, 200})});

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.surfaces.size(), 4);
    QCOMPARE(result.surfaces[0].geometry, QRect(0, 65, 20, 50));
    QCOMPARE(result.surfaces[1].geometry, QRect(0, 140, 200, 60));
    QCOMPARE(result.surfaces[2].geometry, QRect(180, 40, 20, 50));
    QCOMPARE(result.surfaces[3].geometry, QRect(0, 0, 200, 40));
    QCOMPARE(result.outputs.constFirst().workArea, QRect(20, 40, 160, 100));
    verifyPairwiseNonIntersection(result);
}

void PanelLayoutGeometryTests::keepsAllEdgesDisjointAtRepresentativeResolutions()
{
    const QVector<QSize> resolutions = {
        {1920, 1080},
        {1920, 1200},
        {2560, 1440},
    };

    for (const auto &resolution : resolutions) {
        auto top =
            panel(QStringLiteral("top"), Profiles::Edge::Top, Profiles::Alignment::Fill, 1.0, 20);
        top.rows = 2;
        auto bottom = panel(QStringLiteral("bottom"), Profiles::Edge::Bottom,
                            Profiles::Alignment::Fill, 1.0, 20);
        bottom.rows = 3;
        const auto left =
            panel(QStringLiteral("left"), Profiles::Edge::Left, Profiles::Alignment::Fill, 1.0, 24);
        const auto right = panel(QStringLiteral("right"), Profiles::Edge::Right,
                                 Profiles::Alignment::Fill, 1.0, 28);

        const auto result = ShellLayout::PanelLayoutSolver::solve(
            {right, top, left, bottom},
            {output(QStringLiteral("main"), QRect(QPoint(0, 0), resolution))});

        QVERIFY2(result.ok(), qPrintable(result.error.message));
        const int sideHeight = resolution.height() - 100;
        QCOMPARE(result.surfaces[0].geometry, QRect(resolution.width() - 28, 40, 28, sideHeight));
        QCOMPARE(result.surfaces[1].geometry, QRect(0, 0, resolution.width(), 40));
        QCOMPARE(result.surfaces[2].geometry, QRect(0, 40, 24, sideHeight));
        QCOMPARE(result.surfaces[3].geometry,
                 QRect(0, resolution.height() - 60, resolution.width(), 60));
        QCOMPARE(result.outputs.constFirst().workArea,
                 QRect(24, 40, resolution.width() - 52, sideHeight));
        verifyPairwiseNonIntersection(result);
    }
}

void PanelLayoutGeometryTests::expandsWildcardAndNamedOutputsInLogicalCoordinates()
{
    auto wildcard = panel(QStringLiteral("global"), Profiles::Edge::Top);
    wildcard.thickness = 32;
    auto named = panel(QStringLiteral("primary-dock"), Profiles::Edge::Bottom,
                       Profiles::Alignment::Center, 0.5, 48);
    named.output = QStringLiteral("primary");
    named.layer = Profiles::Layer::Overlay;

    const QVector<ShellLayout::LogicalOutput> outputs = {
        output(QStringLiteral("secondary"), {-1920, 0, 1920, 1080}, 1.0),
        // The platform has already converted 2560x1440 at 1.25 scale into its
        // exact 2048x1152 logical rectangle.
        output(QStringLiteral("primary"), {0, 0, 2048, 1152}, 1.25),
    };
    const auto result = ShellLayout::PanelLayoutSolver::solve({wildcard, named}, outputs);

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.surfaces.size(), 3);
    QCOMPARE(result.surfaces[0].outputId, QStringLiteral("secondary"));
    QCOMPARE(result.surfaces[0].geometry, QRect(-1920, 0, 1920, 32));
    QCOMPARE(result.surfaces[1].outputId, QStringLiteral("primary"));
    QCOMPARE(result.surfaces[1].geometry, QRect(0, 0, 2048, 32));
    QCOMPARE(result.surfaces[2].outputId, QStringLiteral("primary"));
    QCOMPARE(result.surfaces[2].geometry, QRect(512, 1104, 1024, 48));
    QCOMPARE(outputLayout(result, QStringLiteral("secondary")).workArea,
             QRect(-1920, 32, 1920, 1048));
    QCOMPARE(outputLayout(result, QStringLiteral("primary")).workArea, QRect(0, 32, 2048, 1120));
    QCOMPARE(outputLayout(result, QStringLiteral("secondary")).scale, 1.0);
    QCOMPARE(outputLayout(result, QStringLiteral("primary")).scale, 1.25);
    verifyPairwiseNonIntersection(result);
}

void PanelLayoutGeometryTests::handlesNegativeAndIntegerBoundaryCoordinates()
{
    const auto allEdges = [] {
        return QVector<Profiles::PanelSpec>{
            panel(QStringLiteral("top"), Profiles::Edge::Top, Profiles::Alignment::Fill, 1.0, 20),
            panel(QStringLiteral("bottom"), Profiles::Edge::Bottom, Profiles::Alignment::Fill, 1.0,
                  20),
            panel(QStringLiteral("left"), Profiles::Edge::Left, Profiles::Alignment::Fill, 1.0, 20),
            panel(QStringLiteral("right"), Profiles::Edge::Right, Profiles::Alignment::Fill, 1.0,
                  20),
        };
    };

    const auto negative = ShellLayout::PanelLayoutSolver::solve(
        allEdges(), {output(QStringLiteral("negative"), {-2560, -1440, 2560, 1440})});
    QVERIFY2(negative.ok(), qPrintable(negative.error.message));
    QCOMPARE(negative.surfaces[0].geometry, QRect(-2560, -1440, 2560, 20));
    QCOMPARE(negative.surfaces[1].geometry, QRect(-2560, -20, 2560, 20));
    QCOMPARE(negative.surfaces[2].geometry, QRect(-2560, -1420, 20, 1400));
    QCOMPARE(negative.surfaces[3].geometry, QRect(-20, -1420, 20, 1400));
    QCOMPARE(negative.outputs.constFirst().workArea, QRect(-2540, -1420, 2520, 1400));
    verifyPairwiseNonIntersection(negative);

    constexpr int extent = 100;
    const auto lower = ShellLayout::PanelLayoutSolver::solve(
        allEdges(),
        {output(QStringLiteral("lower"), {std::numeric_limits<int>::min(),
                                          std::numeric_limits<int>::min(), extent, extent})});
    QVERIFY2(lower.ok(), qPrintable(lower.error.message));
    QCOMPARE(lower.surfaces[0].geometry,
             QRect(std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), extent, 20));
    QCOMPARE(lower.surfaces[3].geometry,
             QRect(std::numeric_limits<int>::min() + extent - 20,
                   std::numeric_limits<int>::min() + 20, 20, extent - 40));
    verifyPairwiseNonIntersection(lower);

    constexpr int upperOrigin = std::numeric_limits<int>::max() - extent + 1;
    const auto upper = ShellLayout::PanelLayoutSolver::solve(
        allEdges(),
        {output(QStringLiteral("upper"),
                QRect(QPoint(upperOrigin, upperOrigin),
                      QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max())))});
    QVERIFY2(upper.ok(), qPrintable(upper.error.message));
    QCOMPARE(upper.surfaces[1].geometry,
             QRect(QPoint(upperOrigin, std::numeric_limits<int>::max() - 19),
                   QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max())));
    QCOMPARE(upper.surfaces[3].geometry,
             QRect(QPoint(std::numeric_limits<int>::max() - 19, upperOrigin + 20),
                   QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max() - 20)));
    QCOMPARE(upper.outputs.constFirst().workArea,
             QRect(upperOrigin + 20, upperOrigin + 20, extent - 40, extent - 40));
    verifyPairwiseNonIntersection(upper);
}

void PanelLayoutGeometryTests::stacksRowsAndComputesExclusiveWorkArea()
{
    auto above = panel(QStringLiteral("above"), Profiles::Edge::Top);
    above.rows = 2;
    above.thickness = 20;

    auto overlay = panel(QStringLiteral("overlay"), Profiles::Edge::Top);
    overlay.thickness = 30;
    overlay.layer = Profiles::Layer::Overlay;

    auto normal = panel(QStringLiteral("normal"), Profiles::Edge::Top);
    normal.thickness = 25;
    normal.layer = Profiles::Layer::Normal;

    auto below = panel(QStringLiteral("below"), Profiles::Edge::Top);
    below.thickness = 20;
    below.layer = Profiles::Layer::Below;

    const auto result = ShellLayout::PanelLayoutSolver::solve({above, overlay, normal, below},
                                                              {output(QStringLiteral("main"))});

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.surfaces[0].geometry, QRect(0, 0, 1920, 40));
    QCOMPARE(result.surfaces[0].stackIndex, 0);
    QCOMPARE(result.surfaces[1].geometry, QRect(0, 40, 1920, 30));
    QCOMPARE(result.surfaces[1].stackIndex, 1);
    QCOMPARE(result.surfaces[2].geometry, QRect(0, 70, 1920, 25));
    QCOMPARE(result.surfaces[3].geometry, QRect(0, 95, 1920, 20));
    QVERIFY(result.surfaces[0].reservesWorkArea);
    QVERIFY(!result.surfaces[1].reservesWorkArea);
    QVERIFY(result.surfaces[2].reservesWorkArea);
    QVERIFY(!result.surfaces[3].reservesWorkArea);
    // Normal is the deepest reserving surface. The trailing Below panel does
    // not grow the exclusive area, while the preceding Overlay lane cannot be
    // subtracted without overlapping the Normal panel itself.
    QCOMPARE(result.outputs.constFirst().workArea, QRect(0, 95, 1920, 985));
}

void PanelLayoutGeometryTests::belowAndOverlayDoNotReserveWorkArea()
{
    for (const auto layer : {Profiles::Layer::Below, Profiles::Layer::Overlay}) {
        auto nonReserving = panel(QStringLiteral("non-reserving"), Profiles::Edge::Bottom);
        nonReserving.layer = layer;
        const auto result =
            ShellLayout::PanelLayoutSolver::solve({nonReserving}, {output(QStringLiteral("main"))});

        QVERIFY2(result.ok(), qPrintable(result.error.message));
        QVERIFY(!result.surfaces.constFirst().reservesWorkArea);
        QCOMPARE(result.outputs.constFirst().workArea, QRect(0, 0, 1920, 1080));
    }
}

void PanelLayoutGeometryTests::separatesPlacementLanesFromWorkAreaReservation()
{
    auto topOverlay = panel(QStringLiteral("top-overlay"), Profiles::Edge::Top);
    topOverlay.thickness = 20;
    topOverlay.layer = Profiles::Layer::Overlay;

    auto bottomBelow = panel(QStringLiteral("bottom-below"), Profiles::Edge::Bottom);
    bottomBelow.thickness = 20;
    bottomBelow.layer = Profiles::Layer::Below;

    auto leftAbove = panel(QStringLiteral("left-above"), Profiles::Edge::Left);
    leftAbove.thickness = 20;

    auto rightOverlay = panel(QStringLiteral("right-overlay"), Profiles::Edge::Right);
    rightOverlay.thickness = 20;
    rightOverlay.layer = Profiles::Layer::Overlay;

    const auto result =
        ShellLayout::PanelLayoutSolver::solve({leftAbove, topOverlay, rightOverlay, bottomBelow},
                                              {output(QStringLiteral("main"), {0, 0, 200, 120})});

    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.surfaces[0].geometry, QRect(0, 20, 20, 80));
    QCOMPARE(result.surfaces[1].geometry, QRect(0, 0, 200, 20));
    QCOMPARE(result.surfaces[2].geometry, QRect(180, 20, 20, 80));
    QCOMPARE(result.surfaces[3].geometry, QRect(0, 100, 200, 20));
    QVERIFY(result.surfaces[0].reservesWorkArea);
    QVERIFY(!result.surfaces[1].reservesWorkArea);
    QVERIFY(!result.surfaces[2].reservesWorkArea);
    QVERIFY(!result.surfaces[3].reservesWorkArea);
    // Non-reserving horizontal stacks still constrain the side placement lane,
    // but only the reserving left panel changes the rectangular work area.
    QCOMPARE(result.outputs.constFirst().workArea, QRect(20, 0, 180, 120));
    verifyPairwiseNonIntersection(result);
}

QTEST_GUILESS_MAIN(PanelLayoutGeometryTests)
#include "tst_panel_layout_geometry.moc"
