// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshutdownrecovery.h"

#include <QSet>
#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridShutdownRecoveryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void retriesTransientReleaseFailure();
    void reconcilesWindowClosedDuringTeardown();
    void fallsBackAfterBoundedReleaseFailures();
    void reportsIncompleteFallbackHonestly();
};

void HybridShutdownRecoveryTest::retriesTransientReleaseFailure()
{
    int releaseCalls = 0;
    bool owned = true;
    const auto result = HybridShutdownRecovery::recover({
        .snapshot = [&] {
            return HybridShutdownSnapshot{{QStringLiteral("a")},
                                          owned ? QStringList{QStringLiteral("a")}
                                                : QStringList{},
                                          owned ? 1 : 0};
        },
        .windowExists = [](const QString &) { return true; },
        .forgetClosedWindow = [](const QString &, QString *) { return true; },
        .releaseAll = [&](QString *error) {
            ++releaseCalls;
            if (releaseCalls == 1) {
                *error = QStringLiteral("injected transient failure");
                return false;
            }
            owned = false;
            return true;
        },
        .fallbackCleanup = [](QString *) { return false; },
    });

    QVERIFY(result.complete);
    QVERIFY(!result.fallbackUsed);
    QCOMPARE(result.releaseAttempts, 2);
    QCOMPARE(result.diagnostics.size(), 1);
}

void HybridShutdownRecoveryTest::reconcilesWindowClosedDuringTeardown()
{
    QSet<QString> topology{QStringLiteral("a"), QStringLiteral("closed")};
    QSet<QString> live{QStringLiteral("a")};
    const auto result = HybridShutdownRecovery::recover({
        .snapshot = [&] {
            auto topologyIds = topology.values();
            auto liveIds = live.values();
            topologyIds.sort();
            liveIds.sort();
            return HybridShutdownSnapshot{topologyIds, liveIds,
                                          topology.isEmpty() ? 0 : 1};
        },
        .windowExists = [&](const QString &id) { return live.contains(id); },
        .forgetClosedWindow = [&](const QString &id, QString *) {
            return topology.remove(id);
        },
        .releaseAll = [&](QString *) {
            topology.clear();
            live.clear();
            return true;
        },
        .fallbackCleanup = [](QString *) { return false; },
    });

    QVERIFY(result.complete);
    QCOMPARE(result.reconciledClosedWindows, 1);
    QCOMPARE(result.releaseAttempts, 1);
}

void HybridShutdownRecoveryTest::fallsBackAfterBoundedReleaseFailures()
{
    bool owned = true;
    int releaseCalls = 0;
    const auto result = HybridShutdownRecovery::recover({
        .snapshot = [&] {
            return HybridShutdownSnapshot{{QStringLiteral("a")},
                                          owned ? QStringList{QStringLiteral("a")}
                                                : QStringList{},
                                          1};
        },
        .windowExists = [](const QString &) { return true; },
        .forgetClosedWindow = [](const QString &, QString *) { return true; },
        .releaseAll = [&](QString *error) {
            ++releaseCalls;
            *error = QStringLiteral("persistent transaction failure");
            return false;
        },
        .fallbackCleanup = [&](QString *) {
            owned = false;
            return true;
        },
    }, 2);

    QVERIFY(result.complete);
    QVERIFY(result.fallbackUsed);
    QCOMPARE(releaseCalls, 2);
    QCOMPARE(result.releaseAttempts, 2);
}

void HybridShutdownRecoveryTest::reportsIncompleteFallbackHonestly()
{
    const auto result = HybridShutdownRecovery::recover({
        .snapshot = [] {
            return HybridShutdownSnapshot{{QStringLiteral("a")},
                                          {QStringLiteral("a")}, 1};
        },
        .windowExists = [](const QString &) { return true; },
        .forgetClosedWindow = [](const QString &, QString *) { return true; },
        .releaseAll = [](QString *error) {
            *error = QStringLiteral("release failed");
            return false;
        },
        .fallbackCleanup = [](QString *error) {
            *error = QStringLiteral("fallback failed");
            return false;
        },
    }, 1);

    QVERIFY(!result.complete);
    QVERIFY(result.fallbackUsed);
    QCOMPARE(result.diagnostics.size(), 2);
}

QTEST_GUILESS_MAIN(HybridShutdownRecoveryTest)
#include "tst_hybridshutdownrecovery.moc"
