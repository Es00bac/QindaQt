// SPDX-License-Identifier: GPL-3.0-or-later
#include "changeratelimiter.h"
#include "coalescedchangenotifier.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

constexpr qint64 Interval = 250;

// Drives a limiter through a deterministic event stream at 1 ms resolution
// and returns every delivery time, the way CoalescedChangeNotifier would
// deliver them (leading edges at the event, trailing ones at their deadline).
QVector<qint64> deliveriesFor(const QVector<qint64> &events, qint64 until)
{
    ChangeRateLimiter limiter(Interval);
    QVector<qint64> deliveries;
    qsizetype next = 0;
    for (qint64 now = 0; now <= until; ++now) {
        while (next < events.size() && events[next] == now) {
            if (limiter.admit(QStringLiteral("window"), now)) {
                deliveries.append(now);
            }
            ++next;
        }
        if (!limiter.takeDue(now).isEmpty()) {
            deliveries.append(now);
        }
    }
    return deliveries;
}

} // namespace

class ChangeRateLimiterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deliversAQuietKeyAtOnce();
    void foldsABurstIntoOneTrailingDelivery();
    void holdsATenHertzTitleToFourHertzWithAFinalFlush();
    void limitsKeysIndependently();
    void forgetDropsAPendingDelivery();
    void prunesQuietKeys();
    void survivesAClockThatStepsBack();
    void notifierNeverDeliversInsideNotify();
    void notifierDeliversTheTrailingChangeAtItsDeadline();
};

void ChangeRateLimiterTest::deliversAQuietKeyAtOnce()
{
    ChangeRateLimiter limiter(Interval);
    QVERIFY(limiter.admit(QStringLiteral("a"), 1000));
    QVERIFY(!limiter.isPending(QStringLiteral("a")));
    QVERIFY(!limiter.nextDeadline());
    // A full interval later the key is quiet again.
    QVERIFY(limiter.admit(QStringLiteral("a"), 1250));
}

void ChangeRateLimiterTest::foldsABurstIntoOneTrailingDelivery()
{
    ChangeRateLimiter limiter(Interval);
    QVERIFY(limiter.admit(QStringLiteral("a"), 0));
    QVERIFY(!limiter.admit(QStringLiteral("a"), 10));
    QVERIFY(!limiter.admit(QStringLiteral("a"), 200));
    QVERIFY(limiter.isPending(QStringLiteral("a")));
    QCOMPARE(limiter.nextDeadline(), std::optional<qint64>(250));
    QVERIFY(limiter.takeDue(249).isEmpty());
    QCOMPARE(limiter.takeDue(250), QStringList{QStringLiteral("a")});
    QVERIFY(!limiter.isPending(QStringLiteral("a")));
    QVERIFY(!limiter.nextDeadline());
}

// The acceptance shape of ADR-0383: a spinner title changing every 100 ms for
// ten seconds reaches consumers about 4 times a second, and the last title is
// always delivered after the burst ends.
void ChangeRateLimiterTest::holdsATenHertzTitleToFourHertzWithAFinalFlush()
{
    QVector<qint64> events;
    for (qint64 time = 0; time < 10'000; time += 100) {
        events.append(time);
    }
    const auto deliveries = deliveriesFor(events, 11'000);
    QCOMPARE(events.size(), 100);
    QVERIFY2(deliveries.size() <= 41, qPrintable(QString::number(deliveries.size())));
    QVERIFY(deliveries.size() >= 39);
    for (qsizetype index = 1; index < deliveries.size(); ++index) {
        QVERIFY(deliveries[index] - deliveries[index - 1] >= Interval);
    }
    // The final change (t = 9900) is flushed, at most one interval later.
    QVERIFY(deliveries.constLast() >= events.constLast());
    QVERIFY(deliveries.constLast() <= events.constLast() + Interval);
    qInfo("10 Hz for 10 s: %lld changes -> %lld deliveries",
          qlonglong(events.size()), qlonglong(deliveries.size()));
}

void ChangeRateLimiterTest::limitsKeysIndependently()
{
    ChangeRateLimiter limiter(Interval);
    QVERIFY(limiter.admit(QStringLiteral("a"), 0));
    QVERIFY(limiter.admit(QStringLiteral("b"), 10));
    QVERIFY(!limiter.admit(QStringLiteral("a"), 20));
    QVERIFY(!limiter.admit(QStringLiteral("b"), 30));
    QCOMPARE(limiter.nextDeadline(), std::optional<qint64>(250));
    QCOMPARE(limiter.takeDue(250), QStringList{QStringLiteral("a")});
    QCOMPARE(limiter.takeDue(260), QStringList{QStringLiteral("b")});
}

void ChangeRateLimiterTest::forgetDropsAPendingDelivery()
{
    ChangeRateLimiter limiter(Interval);
    QVERIFY(limiter.admit(QStringLiteral("a"), 0));
    QVERIFY(!limiter.admit(QStringLiteral("a"), 100));
    limiter.forget(QStringLiteral("a"));
    QVERIFY(!limiter.nextDeadline());
    QVERIFY(limiter.takeDue(1000).isEmpty());
    QCOMPARE(limiter.trackedKeyCount(), 0);
}

void ChangeRateLimiterTest::prunesQuietKeys()
{
    ChangeRateLimiter limiter(Interval);
    for (int index = 0; index < 50; ++index) {
        QVERIFY(limiter.admit(QStringLiteral("window-%1").arg(index), index));
    }
    QCOMPARE(limiter.trackedKeyCount(), 50);
    QVERIFY(limiter.takeDue(1000).isEmpty());
    QCOMPARE(limiter.trackedKeyCount(), 0);
}

void ChangeRateLimiterTest::survivesAClockThatStepsBack()
{
    ChangeRateLimiter limiter(Interval);
    QVERIFY(limiter.admit(QStringLiteral("a"), 5000));
    // A key must never wait for a deadline that a stepped clock put far in
    // its future.
    QVERIFY(limiter.admit(QStringLiteral("a"), 100));
}

void ChangeRateLimiterTest::notifierNeverDeliversInsideNotify()
{
    qint64 now = 0;
    QStringList delivered;
    CoalescedChangeNotifier notifier(
        Interval, [&](const QStringList &keys) { delivered += keys; },
        [&] { return now; });
    notifier.notify(QStringLiteral("a"));
    notifier.notify(QStringLiteral("b"));
    QVERIFY(delivered.isEmpty());
    QVERIFY(notifier.hasPendingDelivery());
    // The end of the event-loop turn: both leading edges in one delivery.
    notifier.deliverDue();
    QCOMPARE(delivered, (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(notifier.deliveryCount(), quint64(1));
    QVERIFY(!notifier.hasPendingDelivery());
}

void ChangeRateLimiterTest::notifierDeliversTheTrailingChangeAtItsDeadline()
{
    qint64 now = 0;
    int deliveries = 0;
    CoalescedChangeNotifier notifier(
        Interval, [&](const QStringList &) { ++deliveries; }, [&] { return now; });
    notifier.notify();
    notifier.deliverDue();
    QCOMPARE(deliveries, 1);
    now = 100;
    notifier.notify();
    now = 200;
    notifier.notify();
    notifier.deliverDue();
    QCOMPARE(deliveries, 1);
    QCOMPARE(notifier.nextDeadline(), std::optional<qint64>(250));
    now = 250;
    notifier.deliverDue();
    QCOMPARE(deliveries, 2);
    QVERIFY(!notifier.hasPendingDelivery());
    notifier.forget(QString());
    QVERIFY(!notifier.hasPendingDelivery());
}

QTEST_GUILESS_MAIN(ChangeRateLimiterTest)
#include "tst_changeratelimiter.moc"
