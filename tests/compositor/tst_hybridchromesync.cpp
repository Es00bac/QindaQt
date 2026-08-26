// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromesyncscheduler.h"
#include "hybridstackingorder.h"

#include <QRectF>
#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridChromeSyncTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ranksEachContainerByItsTopmostMember();
    void ignoresHighStackLeavesFromInactivePages();
    void workspaceEventsCoalesceAndOutputEventsResample();
    void windowSignalBurstsCoalesceIntoOneRefresh();
    void ignoresSignalsRaisedByItsOwnPublication();
};

void HybridChromeSyncTest::ranksEachContainerByItsTopmostMember()
{
    const QStringList containers{QStringLiteral("alpha"),
                                 QStringLiteral("beta"),
                                 QStringLiteral("missing")};
    QCOMPARE(topmostMemberContainerOrder(
                 {QStringLiteral("alpha"), QStringLiteral("beta"), {},
                  QStringLiteral("unknown"), QStringLiteral("alpha")},
                 containers),
             QStringList({QStringLiteral("missing"), QStringLiteral("beta"),
                          QStringLiteral("alpha")}));

    QCOMPARE(topmostMemberContainerOrder(
                 {QStringLiteral("beta"), QStringLiteral("alpha"),
                  QStringLiteral("beta")},
                 containers),
             QStringList({QStringLiteral("missing"), QStringLiteral("alpha"),
                          QStringLiteral("beta")}));
}

void HybridChromeSyncTest::windowSignalBurstsCoalesceIntoOneRefresh()
{
    int calls = 0;
    HybridChromeSyncReasons observed;
    HybridChromeSyncScheduler scheduler(
        [&](HybridChromeSyncReasons reasons) {
            ++calls;
            observed = reasons;
        });

    for (int index = 0; index < 100; ++index) {
        scheduler.windowsChanged();
    }
    QVERIFY(scheduler.pending());
    QTRY_COMPARE(calls, 1);
    QCOMPARE(observed,
             HybridChromeSyncReasons(HybridChromeSyncReason::Windows));
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
}

void HybridChromeSyncTest::ignoresHighStackLeavesFromInactivePages()
{
    const QStringList containers{QStringLiteral("alpha"),
                                 QStringLiteral("beta")};
    const QHash<QString, QString> activeOwners{
        {QStringLiteral("alpha-active"), QStringLiteral("alpha")},
        {QStringLiteral("beta-active"), QStringLiteral("beta")},
    };
    QCOMPARE(topmostActiveMemberContainerOrder(
                 {QStringLiteral("alpha-active"),
                  QStringLiteral("beta-active"),
                  QStringLiteral("alpha-inactive")},
                 activeOwners, containers),
             QStringList({QStringLiteral("alpha"),
                          QStringLiteral("beta")}));
}

void HybridChromeSyncTest::workspaceEventsCoalesceAndOutputEventsResample()
{
    int calls = 0;
    QVector<HybridChromeSyncReasons> observedReasons;
    QRectF liveGeometry(20, 30, 800, 500);
    qreal liveDpr = 1.0;
    QRectF sampledGeometry;
    qreal sampledDpr = 0.0;
    HybridChromeSyncScheduler scheduler(
        [&](HybridChromeSyncReasons reasons) {
            ++calls;
            observedReasons.append(reasons);
            sampledGeometry = liveGeometry;
            sampledDpr = liveDpr;
        });

    scheduler.stackingOrderChanged();
    scheduler.activeWindowChanged();
    QVERIFY(scheduler.pending());
    QTRY_COMPARE(calls, 1);
    QCOMPARE(observedReasons.constLast(),
             HybridChromeSyncReason::Stacking
                 | HybridChromeSyncReason::Activation);
    QCOMPARE(sampledGeometry, liveGeometry);
    QCOMPARE(sampledDpr, liveDpr);

    liveGeometry = QRectF(-1440, 0, 1440, 2560);
    liveDpr = 1.5;
    scheduler.outputsChanged();
    QTRY_COMPARE(calls, 2);
    QCOMPARE(observedReasons.constLast(),
             HybridChromeSyncReasons(HybridChromeSyncReason::Outputs));
    QCOMPARE(sampledGeometry, liveGeometry);
    QCOMPARE(sampledDpr, liveDpr);

    scheduler.activeWindowChanged();
    QTRY_COMPARE(calls, 3);
    QCOMPARE(observedReasons.constLast(),
             HybridChromeSyncReasons(HybridChromeSyncReason::Activation));
}

void HybridChromeSyncTest::ignoresSignalsRaisedByItsOwnPublication()
{
    int calls = 0;
    HybridChromeSyncScheduler *schedulerAddress = nullptr;
    HybridChromeSyncScheduler scheduler(
        [&](HybridChromeSyncReasons) {
            ++calls;
            // Models group compaction synchronously causing KWin to announce a
            // stacking change while chrome is being republished.
            schedulerAddress->stackingOrderChanged();
        });
    schedulerAddress = &scheduler;

    scheduler.stackingOrderChanged();
    QTRY_COMPARE(calls, 1);
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
    QVERIFY(!scheduler.pending());

    scheduler.outputsChanged();
    QTRY_COMPARE(calls, 2);
}

QTEST_GUILESS_MAIN(HybridChromeSyncTest)

#include "tst_hybridchromesync.moc"
