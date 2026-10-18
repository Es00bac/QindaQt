// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_priority_observer_p.h"
#include <QSemaphore>
#include <QScopeGuard>
#include <QTimer>
#include <QtTest>
#include <atomic>
#include <sched.h>
using namespace QindaQt::Controllers;

class GamePriorityObserverTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void slowObservationKeepsEventLoopResponsiveAndRejectsInvalidatedResult() {
        QSemaphore entered, release;
        std::atomic<int> calls = 0;
        std::atomic<bool> onWorker = false;
        std::atomic<int> policy = -1;
        auto *const guiThread = QThread::currentThread();
        GamePriorityObserver observer("unused", [&](const QSet<QString> &paths, const QString &, qint64) {
            onWorker = QThread::currentThread() != guiThread;
            policy = sched_getscheduler(0);
            if (++calls == 1) {
                entered.release();
                release.acquire();
                return GamePriorityState{true, paths};
            }
            return GamePriorityState{false, paths};
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
            [&](const QSet<QString> &, const QString &, qint64) {
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
};
QTEST_GUILESS_MAIN(GamePriorityObserverTest)
#include "tst_game_priority_observer.moc"
