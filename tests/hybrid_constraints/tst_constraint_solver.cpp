// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_constraints/constraint_solver.h"

#include "layoutnode.h"

#include <QTest>

#include <cmath>

using namespace QindaQt;
using namespace QindaQt::HybridConstraints;

class ConstraintSolverTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void laysOutNormalSplitInsideChrome();
    void honorsFixedAndMaximumSizes();
    void solvesNestedHorizontalAndVerticalSplits();
    void roundsFractionalRatiosDeterministically();
    void reportsOverflowWhenMinimumsCannotFit();
    void rejectsInvalidConstraintSets();
};

void ConstraintSolverTest::laysOutNormalSplitInsideChrome()
{
    const auto root = Core::LayoutNode::makeSplit(
        QStringLiteral("split"),
        Core::SplitOrientation::Horizontal,
        0.5,
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-a"), QStringLiteral("a")),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-b"), QStringLiteral("b")));
    const QHash<QString, MemberSizeConstraints> constraints{
        {QStringLiteral("a"),
         {.minimumSize = QSize(200, 100),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
        {QStringLiteral("b"),
         {.minimumSize = QSize(200, 100),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
    };
    const LayoutMetrics metrics{.contentInsets = QMargins(10, 30, 10, 10),
                                .dividerThickness = 4};

    QString error;
    const auto solution = ConstraintSolver::solve(
        root, QRect(100, 50, 1000, 600), constraints, metrics, &error);

    QVERIFY2(solution.has_value(), qPrintable(error));
    QCOMPARE(solution->contentFrame, QRect(110, 80, 980, 560));
    QCOMPARE(solution->requiredContentSize, QSize(404, 100));
    QVERIFY(!solution->hasOverflow());
    QCOMPARE(solution->members.value(QStringLiteral("a")).tileFrame,
             QRect(110, 80, 488, 560));
    QCOMPARE(solution->members.value(QStringLiteral("b")).tileFrame,
             QRect(602, 80, 488, 560));
    QCOMPARE(solution->members.value(QStringLiteral("a")).windowFrame,
             QRect(110, 80, 488, 560));
    QCOMPARE(solution->splits.value(QStringLiteral("split")).dividerFrame,
             QRect(598, 80, 4, 560));
    QCOMPARE(solution->splits.value(QStringLiteral("split")).effectiveRatio, 0.5);
}

void ConstraintSolverTest::honorsFixedAndMaximumSizes()
{
    const auto split = Core::LayoutNode::makeSplit(
        QStringLiteral("split"),
        Core::SplitOrientation::Horizontal,
        0.5,
        Core::LayoutNode::makeLeaf(QStringLiteral("fixed-leaf"), QStringLiteral("fixed")),
        Core::LayoutNode::makeLeaf(QStringLiteral("flex-leaf"), QStringLiteral("flex")));
    const QHash<QString, MemberSizeConstraints> splitConstraints{
        {QStringLiteral("fixed"),
         {.minimumSize = QSize(100, 80),
          .maximumSize = QSize(300, 200),
          .fixedSize = QSize(200, 120)}},
        {QStringLiteral("flex"),
         {.minimumSize = QSize(300, 100),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
    };

    QString error;
    const auto splitSolution = ConstraintSolver::solve(
        split,
        QRect(0, 0, 1000, 400),
        splitConstraints,
        {.contentInsets = {}, .dividerThickness = 4},
        &error);
    QVERIFY2(splitSolution.has_value(), qPrintable(error));
    QCOMPARE(splitSolution->members.value(QStringLiteral("fixed")).tileFrame,
             QRect(0, 0, 200, 400));
    QCOMPARE(splitSolution->members.value(QStringLiteral("fixed")).windowFrame,
             QRect(0, 140, 200, 120));
    QCOMPARE(splitSolution->members.value(QStringLiteral("flex")).tileFrame,
             QRect(204, 0, 796, 400));
    QVERIFY(std::abs(splitSolution->splits.value(QStringLiteral("split")).effectiveRatio
                     - (200.0 / 996.0))
            < 1.0e-12);

    const auto leaf = Core::LayoutNode::makeLeaf(QStringLiteral("leaf"),
                                                 QStringLiteral("bounded"));
    const QHash<QString, MemberSizeConstraints> maximumConstraint{
        {QStringLiteral("bounded"),
         {.minimumSize = QSize(100, 80),
          .maximumSize = QSize(300, 200),
          .fixedSize = std::nullopt}},
    };
    const auto leafSolution = ConstraintSolver::solve(
        leaf,
        QRect(10, 20, 800, 600),
        maximumConstraint,
        {},
        &error);
    QVERIFY2(leafSolution.has_value(), qPrintable(error));
    QCOMPARE(leafSolution->members.value(QStringLiteral("bounded")).tileFrame,
             QRect(10, 20, 800, 600));
    QCOMPARE(leafSolution->members.value(QStringLiteral("bounded")).windowFrame,
             QRect(260, 220, 300, 200));
}

void ConstraintSolverTest::solvesNestedHorizontalAndVerticalSplits()
{
    auto upper = Core::LayoutNode::makeSplit(
        QStringLiteral("upper-split"),
        Core::SplitOrientation::Horizontal,
        0.25,
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-a"), QStringLiteral("a")),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-b"), QStringLiteral("b")));
    const auto root = Core::LayoutNode::makeSplit(
        QStringLiteral("root-split"),
        Core::SplitOrientation::Vertical,
        0.4,
        std::move(upper),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-c"), QStringLiteral("c")));
    const QHash<QString, MemberSizeConstraints> constraints{
        {QStringLiteral("a"),
         {.minimumSize = QSize(100, 100),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
        {QStringLiteral("b"),
         {.minimumSize = QSize(200, 100),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
        {QStringLiteral("c"),
         {.minimumSize = QSize(300, 200),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
    };

    QString error;
    const auto solution = ConstraintSolver::solve(
        root,
        QRect(10, 20, 805, 605),
        constraints,
        {.contentInsets = {}, .dividerThickness = 5},
        &error);

    QVERIFY2(solution.has_value(), qPrintable(error));
    QCOMPARE(solution->splits.size(), 2);
    QCOMPARE(solution->members.size(), 3);
    QCOMPARE(solution->requiredContentSize, QSize(305, 305));
    QVERIFY(!solution->hasOverflow());
    QCOMPARE(solution->members.value(QStringLiteral("a")).tileFrame,
             QRect(10, 20, 200, 240));
    QCOMPARE(solution->members.value(QStringLiteral("b")).tileFrame,
             QRect(215, 20, 600, 240));
    QCOMPARE(solution->members.value(QStringLiteral("c")).tileFrame,
             QRect(10, 265, 805, 360));
    QCOMPARE(solution->splits.value(QStringLiteral("root-split")).dividerFrame,
             QRect(10, 260, 805, 5));
    QCOMPARE(solution->splits.value(QStringLiteral("upper-split")).dividerFrame,
             QRect(210, 20, 5, 240));
}

void ConstraintSolverTest::roundsFractionalRatiosDeterministically()
{
    const auto thirds = Core::LayoutNode::makeSplit(
        QStringLiteral("thirds"),
        Core::SplitOrientation::Horizontal,
        1.0 / 3.0,
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-a"), QStringLiteral("a")),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-b"), QStringLiteral("b")));
    const auto first = ConstraintSolver::solve(
        thirds,
        QRect(0, 0, 1005, 100),
        {},
        {.contentInsets = {}, .dividerThickness = 5});
    const auto second = ConstraintSolver::solve(
        thirds,
        QRect(0, 0, 1005, 100),
        {},
        {.contentInsets = {}, .dividerThickness = 5});

    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QCOMPARE(first->members.value(QStringLiteral("a")).tileFrame.width(), 333);
    QCOMPARE(first->members.value(QStringLiteral("b")).tileFrame.width(), 667);
    QCOMPARE(first->members, second->members);
    QCOMPARE(first->splits, second->splits);

    const auto half = Core::LayoutNode::makeSplit(
        QStringLiteral("half"),
        Core::SplitOrientation::Horizontal,
        0.5,
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-c"), QStringLiteral("c")),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-d"), QStringLiteral("d")));
    const auto tie = ConstraintSolver::solve(
        half,
        QRect(0, 0, 1004, 100),
        {},
        {.contentInsets = {}, .dividerThickness = 5});
    QVERIFY(tie.has_value());
    QCOMPARE(tie->members.value(QStringLiteral("c")).tileFrame.width(), 500);
    QCOMPARE(tie->members.value(QStringLiteral("d")).tileFrame.width(), 499);
    QVERIFY(std::abs(tie->splits.value(QStringLiteral("half")).effectiveRatio
                     - (500.0 / 999.0))
            < 1.0e-12);
}

void ConstraintSolverTest::reportsOverflowWhenMinimumsCannotFit()
{
    const auto root = Core::LayoutNode::makeSplit(
        QStringLiteral("split"),
        Core::SplitOrientation::Horizontal,
        0.8,
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-a"), QStringLiteral("a")),
        Core::LayoutNode::makeLeaf(QStringLiteral("leaf-b"), QStringLiteral("b")));
    const QHash<QString, MemberSizeConstraints> constraints{
        {QStringLiteral("a"),
         {.minimumSize = QSize(200, 50),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
        {QStringLiteral("b"),
         {.minimumSize = QSize(200, 50),
          .maximumSize = std::nullopt,
          .fixedSize = std::nullopt}},
    };

    QString error;
    const auto solution = ConstraintSolver::solve(
        root,
        QRect(0, 0, 300, 100),
        constraints,
        {.contentInsets = QMargins(10, 0, 10, 0), .dividerThickness = 10},
        &error);

    QVERIFY2(solution.has_value(), qPrintable(error));
    QVERIFY(solution->hasOverflow());
    QCOMPARE(solution->requiredContentSize, QSize(410, 50));
    QCOMPARE(solution->overflow.availableOuterSize, QSize(300, 100));
    QCOMPARE(solution->overflow.requiredOuterSize, QSize(430, 50));
    QCOMPARE(solution->overflow.missingSize, QSize(130, 0));
    QCOMPARE(solution->members.value(QStringLiteral("a")).tileFrame,
             QRect(10, 0, 135, 100));
    QCOMPARE(solution->splits.value(QStringLiteral("split")).dividerFrame,
             QRect(145, 0, 10, 100));
    QCOMPARE(solution->members.value(QStringLiteral("b")).tileFrame,
             QRect(155, 0, 135, 100));
    QVERIFY(!solution->members.value(QStringLiteral("a")).minimumSizeSatisfied());
    QVERIFY(!solution->members.value(QStringLiteral("b")).minimumSizeSatisfied());
    QVERIFY(!solution->splits.value(QStringLiteral("split")).primaryMinimumsSatisfied);
}

void ConstraintSolverTest::rejectsInvalidConstraintSets()
{
    const auto leaf = Core::LayoutNode::makeLeaf(QStringLiteral("leaf"),
                                                 QStringLiteral("window"));
    const QHash<QString, MemberSizeConstraints> constraints{
        {QStringLiteral("window"),
         {.minimumSize = QSize(400, 200),
          .maximumSize = QSize(300, 300),
          .fixedSize = std::nullopt}},
    };

    QString error;
    const auto solution = ConstraintSolver::solve(leaf, QRect(0, 0, 800, 600),
                                                  constraints, {}, &error);
    QVERIFY(!solution.has_value());
    QVERIFY(error.contains(QStringLiteral("maximum size")));
}

QTEST_GUILESS_MAIN(ConstraintSolverTest)
#include "tst_constraint_solver.moc"
