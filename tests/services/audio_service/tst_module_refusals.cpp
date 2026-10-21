// SPDX-License-Identifier: GPL-3.0-or-later

// The worker's refused-module memory (audio-service wiki, 2026-10-21): a
// refusal PipeWire gave for exact arguments is not retried on every graph
// rebuild, only when the declaration changes or the core is replaced. Pure
// policy; the private-graph churn row proves the effect on the heap.

#include "module_refusals_p.h"

#include <QtTest>

using namespace QindaQt::Audio;

class ModuleRefusalsTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void aRefusalHoldsOnlyForTheSameArguments();
    void aChangedOrWithdrawnDeclarationEarnsOneNewAttempt();
    void clearForgetsEverything();
};

void ModuleRefusalsTests::aRefusalHoldsOnlyForTheSameArguments()
{
    ModuleRefusals refusals;
    QVERIFY(!refusals.refused("strip.hw.1", "{ a }"));
    refusals.remember("strip.hw.1", "{ a }");
    QVERIFY(refusals.refused("strip.hw.1", "{ a }"));
    // Another rack, or the same rack with other arguments, is not refused.
    QVERIFY(!refusals.refused("strip.hw.1", "{ b }"));
    QVERIFY(!refusals.refused("strip.hw.2", "{ a }"));
    // Declaring the same thing again changes nothing: still not retried.
    refusals.retainDeclared({{"strip.hw.1", "{ a }"}});
    QVERIFY(refusals.refused("strip.hw.1", "{ a }"));
    QCOMPARE(refusals.size(), std::size_t{1});
}

void ModuleRefusalsTests::aChangedOrWithdrawnDeclarationEarnsOneNewAttempt()
{
    ModuleRefusals refusals;
    refusals.remember("strip.hw.1", "{ a }");
    refusals.remember("strip.hw.2", "{ c }");
    // strip.hw.1 changed its rack and strip.hw.2 is no longer declared.
    refusals.retainDeclared({{"strip.hw.1", "{ b }"}});
    QCOMPARE(refusals.size(), std::size_t{0});
    QVERIFY(!refusals.refused("strip.hw.1", "{ a }"));
    QVERIFY(!refusals.refused("strip.hw.1", "{ b }"));
    QVERIFY(!refusals.refused("strip.hw.2", "{ c }"));
    // Refusing the new arguments holds them, and only them.
    refusals.remember("strip.hw.1", "{ b }");
    QVERIFY(refusals.refused("strip.hw.1", "{ b }"));
    QVERIFY(!refusals.refused("strip.hw.1", "{ a }"));
}

void ModuleRefusalsTests::clearForgetsEverything()
{
    ModuleRefusals refusals;
    refusals.remember("bus.a1", "{ a }");
    refusals.remember("bus.a2", "{ b }");
    refusals.clear();
    QCOMPARE(refusals.size(), std::size_t{0});
    QVERIFY(!refusals.refused("bus.a1", "{ a }"));
}

QTEST_APPLESS_MAIN(ModuleRefusalsTests)
#include "tst_module_refusals.moc"
