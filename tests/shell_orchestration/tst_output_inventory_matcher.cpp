// SPDX-License-Identifier: GPL-3.0-or-later
#include "orchestration_test_fixtures.h"

#include "qindaqt/shell_orchestration/output_inventory_matcher.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

using namespace QindaQt;

class OutputInventoryMatcherTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsOrderIndependentExactInventories();
    void acceptsQtIntegerBufferScaleForFractionalOutputs();
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
    // 'main' is a 1.5 output, so Qt's integer buffer scale of 2.0 is the
    // documented agreement and must NOT be drift. 3.0 satisfies neither the
    // same-ruler nor the integer-envelope relation.
    observed[1].scale = 3.0;
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::ScaleMismatch);

    observed = expected;
    observed[0].scale = 0.5;
    QCOMPARE(ShellOrchestration::OutputInventoryMatcher::match(expected, observed).code,
             Code::ScaleMismatch);
}

// Regression: the shell ran permanently in the safe-visible fallback on every
// fractionally scaled output because QScreen::devicePixelRatio() reports the
// compositor scale rounded up, and the matcher demanded exact equality. No
// panel could ever hide. Both rulers must describe the same output.
void OutputInventoryMatcherTests::acceptsQtIntegerBufferScaleForFractionalOutputs()
{
    const auto compositor = ShellOrchestration::TestFixtures::outputs();

    for (const qreal fractional : {1.25, 1.5, 1.75, 2.5}) {
        auto compositorInventory = compositor;
        compositorInventory[1].scale = fractional;
        auto qtInventory = compositor;
        qtInventory[1].scale = std::ceil(fractional);
        const auto result = ShellOrchestration::OutputInventoryMatcher::match(
            compositorInventory, qtInventory);
        QVERIFY2(result.ok(),
                 qPrintable(QStringLiteral("scale %1 rejected: %2")
                                .arg(fractional)
                                .arg(result.message)));
    }

    // Integer scales still agree on the same ruler.
    auto integerInventory = compositor;
    integerInventory[1].scale = 2.0;
    auto qtInventory = integerInventory;
    QVERIFY(ShellOrchestration::OutputInventoryMatcher::match(
                integerInventory, qtInventory)
                .ok());
}

QTEST_GUILESS_MAIN(OutputInventoryMatcherTests)
#include "tst_output_inventory_matcher.moc"
