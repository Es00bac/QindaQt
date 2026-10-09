// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_routing_gain_p.h"
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace QindaQt::Audio;

class ConsoleRoutingGainTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void independentStages_data();
    void independentStages();
    void balanceMuteAndInvalid();
    void cubicBoundaryAndHeadroom();
};

void ConsoleRoutingGainTests::independentStages_data()
{
    QTest::addColumn<double>("strip");
    QTest::addColumn<double>("send");
    QTest::addColumn<double>("bus");
    QTest::addColumn<double>("amplitude");
    QTest::newRow("unity") << 0.0 << 0.0 << 0.0 << 1.0;
    QTest::newRow("strip-minus6") << -6.0 << 0.0 << 0.0 << std::pow(10.0, -6.0 / 20.0);
    QTest::newRow("send-minus6") << 0.0 << -6.0 << 0.0 << std::pow(10.0, -6.0 / 20.0);
    QTest::newRow("bus-minus6") << 0.0 << 0.0 << -6.0 << std::pow(10.0, -6.0 / 20.0);
    QTest::newRow("strip-bottom") << -60.0 << 12.0 << 12.0 << 0.0;
    QTest::newRow("send-bottom") << 12.0 << -60.0 << 12.0 << 0.0;
    QTest::newRow("bus-bottom") << 12.0 << 12.0 << -60.0 << 0.0;
    QTest::newRow("below-one-floor") << -30.0 << -30.0 << 0.0 << 0.001;
    QTest::newRow("three-boosts") << 12.0 << 12.0 << 12.0 << std::pow(10.0, 36.0 / 20.0);
}

void ConsoleRoutingGainTests::independentStages()
{
    QFETCH(double, strip); QFETCH(double, send); QFETCH(double, bus); QFETCH(double, amplitude);
    const auto result = consoleSendAmplitude({strip, send, bus, 0.0, true});
    QVERIFY(result.valid);
    QVERIFY(std::fabs(result.left - amplitude) <= 1e-12 * std::max(1.0, amplitude));
    QCOMPARE(result.left, result.right);
}

void ConsoleRoutingGainTests::balanceMuteAndInvalid()
{
    QCOMPARE(consoleSendAmplitude({0, 0, 0, 0.5, true}).left, 0.5);
    QCOMPARE(consoleSendAmplitude({0, 0, 0, 0.5, true}).right, 1.0);
    QCOMPARE(consoleSendAmplitude({0, 0, 0, -1.0, true}).right, 0.0);
    QCOMPARE(consoleSendAmplitude({12, 12, 12, 0, false}).left, 0.0);
    for (const double bad : {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(), 13.0, -61.0}) {
        const auto invalid = consoleSendAmplitude({bad, 0, 0, 0, true});
        QVERIFY(!invalid.valid); QCOMPARE(invalid.left, 0.0); QCOMPARE(invalid.right, 0.0);
    }
    QVERIFY(!consoleSendAmplitude({0, 0, 0, 1.01, true}).valid);
}

void ConsoleRoutingGainTests::cubicBoundaryAndHeadroom()
{
    const double desired = std::pow(10.0, -6.0 / 20.0);
    const double scalar = consoleMixerScalar(desired);
    QVERIFY(std::fabs(scalar * scalar * scalar - desired) < 1e-12);
    QVERIFY(std::fabs(std::pow(consoleMixerScalar(0.5), 3.0) - 0.5) < 1e-12);
    const auto maximum = consoleSendAmplitude({12, 12, 12, 0, true});
    QVERIFY(static_cast<double>(consoleSendMaximumAmplitude()) >= maximum.left);
    QVERIFY(consoleSendMaximumAmplitude() < 63.096F);
    QCOMPARE(consoleMixerScalar(std::numeric_limits<double>::infinity()), 0.0);
    QCOMPARE(consoleMixerScalar(-1.0), 0.0);
}

QTEST_GUILESS_MAIN(ConsoleRoutingGainTests)
#include "tst_console_routing_gain.moc"
