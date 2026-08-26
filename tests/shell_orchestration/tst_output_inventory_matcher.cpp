// SPDX-License-Identifier: GPL-3.0-or-later
#include "orchestration_test_fixtures.h"

#include "qindaqt/shell_orchestration/output_inventory_matcher.h"

#include <QtTest>

#include <algorithm>

using namespace QindaQt;

class OutputInventoryMatcherTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsOrderIndependentExactInventories();
    void rejectsEveryFormOfDrift();
};

void OutputInventoryMatcherTests::acceptsOrderIndependentExactInventories()
{
    const auto expected = ShellOrchestration::TestFixtures::outputs();
    auto observed = expected;
    std::reverse(observed.begin(), observed.end());

    QVERIFY(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).ok());
}

void OutputInventoryMatcherTests::rejectsEveryFormOfDrift()
{
    using Code = ShellOrchestration::OutputInventoryMatchErrorCode;
    const auto expected = ShellOrchestration::TestFixtures::outputs();

    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match({}, {}).code,
             Code::EmptyInventory);

    auto observed = expected;
    observed.removeLast();
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::CountMismatch);

    observed = expected;
    observed[1].id = observed[0].id;
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::DuplicateOutput);

    observed = expected;
    observed[1].id = QStringLiteral("other");
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::MissingOutput);

    observed = expected;
    observed[1].geometry.translate(1, 0);
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::GeometryMismatch);

    observed = expected;
    observed[1].scale = 2.0;
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::ScaleMismatch);
}

QTEST_GUILESS_MAIN(OutputInventoryMatcherTests)
#include "tst_output_inventory_matcher.moc"
