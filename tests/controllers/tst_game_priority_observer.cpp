// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_priority_observer_p.h"
#include <QSemaphore>
#include <QScopeGuard>
#include <QTimer>
#include <QtTest>
#include <atomic>
#include <sched.h>
using namespace QindaQt::Controllers;

namespace {
GamePriorityState stateFor(bool steam, QSet<QString> paths, bool consumerPresent = false) {
    GamePriorityState state;
    state.steam = steam;
    state.busyPaths = std::move(paths);
    state.consumerPresent = consumerPresent;
    return state;
}
}

class GamePriorityObserverTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void slowObservationKeepsEventLoopResponsiveAndRejectsInvalidatedResult() {
        QSemaphore entered, release;
        std::atomic<int> calls = 0;
        std::atomic<bool> onWorker = false;
        std::atomic<int> policy = -1;
        auto *const guiThread = QThread::currentThread();
        GamePriorityObserver observer("unused", [&](const GamePriorityQuery &query, const QString &) {
            onWorker = QThread::currentThread() != guiThread;
            policy = sched_getscheduler(0);
            if (++calls == 1) {
                entered.release();
                release.acquire();
                return stateFor(true, query.paths);
            }
            return stateFor(false, query.paths);
        });
        const auto unblock = qScopeGuard([&] { release.release(); });
        QList<GamePriorityState> results;
        connect(&observer, &GamePriorityObserver::observed, this,
                [&](const GamePriorityState &state) { results.append(state); });
        observer.refresh({"/dev/input/event1"}, true);
        QVERIFY(entered.tryAcquire(1, 1000));
        bool heartbeat = false;
        QTimer::singleShot(0, this, [&] { heartbeat = true; });
        QTRY_VERIFY(heartbeat);
        QVERIFY(onWorker);
        QCOMPARE(policy.load(), SCHED_OTHER);
        for (int i = 0; i < 100; ++i) observer.refresh({"/dev/input/event1"});
        QCOMPARE(calls.load(), 1);
        observer.refresh({"/dev/input/event2"}, true);
        observer.refresh({"/dev/input/event3"}, true);
        QVERIFY(results.isEmpty());
        release.release();
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(calls.load(), 2);
        QVERIFY(!results.first().steam);
        QCOMPARE(results.first().busyPaths, QSet<QString>{"/dev/input/event3"});
    }
    void destructionInterruptsAndJoinsObservation() {
        QSemaphore entered;
        std::atomic<bool> finished = false;
        auto observer = std::make_unique<GamePriorityObserver>("unused",
            [&](const GamePriorityQuery &, const QString &) {
                entered.release();
                while (!QThread::currentThread()->isInterruptionRequested()) QThread::msleep(1);
                finished = true;
                return GamePriorityState{};
            });
        observer->refresh({});
        QVERIFY(entered.tryAcquire(1, 1000));
        observer.reset();
        QVERIFY(finished);
    }
    // ADR-0428: with no holder that could be a game, focus is irrelevant, so a
    // focus change must never release held desktop input (a held R2 Meta-drag
    // that activates another window keeps working) yet still scans afresh.
    void focusChangeWithoutAGameHolderNeverInvalidatesButRescans() {
        std::atomic<int> calls = 0;
        std::atomic<qint64> lastFocus = -1;
        GamePriorityObserver observer("unused", [&](const GamePriorityQuery &query, const QString &) {
            ++calls;
            lastFocus = query.focusedPid;
            return stateFor(false, query.paths);
        });
        int invalidations = 0;
        QList<GamePriorityState> results;
        connect(&observer, &GamePriorityObserver::invalidated, this, [&] { ++invalidations; });
        connect(&observer, &GamePriorityObserver::observed, this,
                [&](const GamePriorityState &state) { results.append(state); });
        observer.refresh({"/dev/input/event1"}, true);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(invalidations, 1);
        observer.setFocusedProcess(4242);
        QCOMPARE(invalidations, 1);
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(lastFocus.load(), qint64(4242));
        observer.setFocusedProcess(4242);
        QTest::qWait(30);
        QCOMPARE(calls.load(), 2);
        observer.setFocusedProcess(-1);
        QTRY_COMPARE(results.size(), 3);
        QCOMPARE(lastFocus.load(), qint64(0));
        QCOMPARE(invalidations, 1);
    }
    // A focus change while a holder that could be a game is known must release
    // input at once, before any scan result, and must discard the scan that
    // was computed for the previous focus.
    void focusChangeWithAGameHolderInvalidatesAtOnceAndFencesTheInFlightScan() {
        QSemaphore entered, release;
        std::atomic<int> calls = 0;
        std::atomic<qint64> lastFocus = -1;
        constexpr qint64 gameWindow = 77;
        GamePriorityObserver observer("unused", [&](const GamePriorityQuery &query, const QString &) {
            lastFocus = query.focusedPid;
            if (++calls == 2) {
                entered.release();
                release.acquire();
            }
            auto state = stateFor(false, query.paths, true);
            if (query.focusedPid == gameWindow) state.gamePaths = query.paths;
            return state;
        });
        const auto unblock = qScopeGuard([&] { release.release(); });
        int invalidations = 0;
        QList<GamePriorityState> results;
        connect(&observer, &GamePriorityObserver::invalidated, this, [&] { ++invalidations; });
        connect(&observer, &GamePriorityObserver::observed, this,
                [&](const GamePriorityState &state) { results.append(state); });
        observer.refresh({"/dev/input/event1"}, true);
        QTRY_COMPARE(results.size(), 1);
        QVERIFY(results.first().consumerPresent);
        QVERIFY(results.first().gamePaths.isEmpty());
        const int before = invalidations;
        observer.setFocusedProcess(gameWindow);
        QCOMPARE(invalidations, before + 1);
        QVERIFY(entered.tryAcquire(1, 1000));
        observer.setFocusedProcess(88);
        QCOMPARE(invalidations, before + 2);
        QCOMPARE(results.size(), 1);
        release.release();
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(calls.load(), 3);
        QCOMPARE(lastFocus.load(), qint64(88));
        QVERIFY(results.last().gamePaths.isEmpty());
        observer.setFocusedProcess(gameWindow);
        QTRY_COMPARE(results.size(), 3);
        QCOMPARE(results.last().gamePaths, QSet<QString>{"/dev/input/event1"});
    }
    // A scan that began under another focus is still valid when it names no
    // possible game (focus-independent); one that names a holder is rescanned
    // because its relation to the new focused application is unknown.
    void scanComputedForAnotherFocusIsRescannedOnlyWhenItNamesAHolder() {
        QSemaphore entered, release;
        std::atomic<int> calls = 0, blockOn = 2;
        std::atomic<bool> holder = false;
        std::atomic<qint64> lastFocus = -1;
        GamePriorityObserver observer("unused", [&](const GamePriorityQuery &query, const QString &) {
            const bool present = holder;
            lastFocus = query.focusedPid;
            if (++calls == blockOn.load()) {
                entered.release();
                release.acquire();
            }
            return stateFor(false, query.paths, present);
        });
        const auto unblock = qScopeGuard([&] { release.release(); });
        int invalidations = 0;
        QList<GamePriorityState> results;
        connect(&observer, &GamePriorityObserver::invalidated, this, [&] { ++invalidations; });
        connect(&observer, &GamePriorityObserver::observed, this,
                [&](const GamePriorityState &state) { results.append(state); });
        const QSet<QString> paths{"/dev/input/event1"};
        observer.refresh(paths, true);
        QTRY_COMPARE(results.size(), 1);
        observer.refresh(paths);
        QVERIFY(entered.tryAcquire(1, 1000));
        observer.setFocusedProcess(5);
        QCOMPARE(invalidations, 1);
        release.release();
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(calls.load(), 2);
        blockOn = 3;
        holder = true;
        observer.refresh(paths);
        QVERIFY(entered.tryAcquire(1, 1000));
        observer.setFocusedProcess(6);
        QCOMPARE(invalidations, 1);
        release.release();
        QTRY_COMPARE(results.size(), 3);
        QCOMPARE(calls.load(), 4);
        QCOMPARE(lastFocus.load(), qint64(6));
        QVERIFY(results.last().consumerPresent);
        QCOMPARE(invalidations, 1);
    }
    void focusBeforeAnyWatchedNodeOnlyRecordsThePid() {
        std::atomic<int> calls = 0;
        std::atomic<qint64> lastFocus = -1;
        GamePriorityObserver observer("unused", [&](const GamePriorityQuery &, const QString &) {
            ++calls;
            return GamePriorityState{};
        });
        observer.setFocusedProcess(99);
        QTest::qWait(30);
        QCOMPARE(calls.load(), 0);
        GamePriorityObserver recording("unused", [&](const GamePriorityQuery &query, const QString &) {
            lastFocus = query.focusedPid;
            return GamePriorityState{};
        });
        recording.setFocusedProcess(99);
        recording.refresh({"/dev/input/event1"}, true);
        QTRY_COMPARE(lastFocus.load(), qint64(99));
    }
};
QTEST_GUILESS_MAIN(GamePriorityObserverTest)
#include "tst_game_priority_observer.moc"
