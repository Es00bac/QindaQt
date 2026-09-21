// SPDX-License-Identifier: GPL-3.0-or-later

// The upper-left pointer corner (ADR-0232). Pure: the KWin-linking reserver
// is a seam, so the reservation discipline and the announced strings qualify
// without a compositor.

#include "pointercorner.h"

#include <QtTest>

using QindaQt::Compositor::KWinIntegration::PointerCornerGesture;
using QindaQt::Compositor::KWinIntegration::PointerCornerReserver;

namespace {

class RecordingReserver final : public PointerCornerReserver {
public:
    void reserve(QObject *object, const char *callback) override
    {
        ++reserveCalls;
        lastObject = object;
        lastCallback = QString::fromLatin1(callback);
    }
    void unreserve(QObject *object) override
    {
        ++unreserveCalls;
        lastUnreserved = object;
    }

    int reserveCalls = 0;
    int unreserveCalls = 0;
    QObject *lastObject = nullptr;
    QObject *lastUnreserved = nullptr;
    QString lastCallback;
};

} // namespace

class PointerCornerTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reservesOnConstructionAndBalancesOnDestruction();
    void rearmingReservesAgainWithoutASecondUnreserve();
    void aTriggerAnnouncesTheSameActionATouchSwipeDoes();
};

// KWin requires reserve/unreserve to balance: an unbalanced call leaves the
// edge permanently active or permanently dead, and the corner is the one the
// whole feature hangs on.
void PointerCornerTests::reservesOnConstructionAndBalancesOnDestruction()
{
    RecordingReserver reserver;
    {
        PointerCornerGesture corner(reserver);
        QCOMPARE(reserver.reserveCalls, 1);
        QCOMPARE(reserver.unreserveCalls, 0);
        QVERIFY(corner.reserved());
        // The callback is invoked by NAME through QMetaObject::invokeMethod,
        // so the string has to match a real slot or the corner silently does
        // nothing. Assert both halves of that agreement.
        QCOMPARE(reserver.lastObject, &corner);
        QCOMPARE(reserver.lastCallback, QStringLiteral("cornerTriggered"));
        QVERIFY(corner.metaObject()->indexOfSlot("cornerTriggered()") >= 0);
    }
    QCOMPARE(reserver.unreserveCalls, 1);
}

// KWin rebuilds its edge objects when outputs change and carries over only
// the reservations the old edges held, so the plugin re-arms on every outputs
// change. KWin ignores a duplicate; a spurious unreserve would not be ignored.
void PointerCornerTests::rearmingReservesAgainWithoutASecondUnreserve()
{
    RecordingReserver reserver;
    PointerCornerGesture corner(reserver);
    QCOMPARE(reserver.reserveCalls, 1);
    corner.rearm();
    corner.rearm();
    QCOMPARE(reserver.reserveCalls, 3);
    QCOMPARE(reserver.unreserveCalls, 0);
    QVERIFY(corner.reserved());
}

void PointerCornerTests::aTriggerAnnouncesTheSameActionATouchSwipeDoes()
{
    RecordingReserver reserver;
    PointerCornerGesture corner(reserver);
    QSignalSpy spy(&corner, &PointerCornerGesture::triggered);

    // True: the corner is QindaQt's now, so the trigger is consumed rather
    // than falling through to whatever KWin would otherwise do.
    QVERIFY(corner.cornerTriggered());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("top-left"));
    // AGENT-CONTRACT: `overview` is the action the shell's
    // dispatchEdgeGesture already routes to the gather overview. Changing
    // this string without changing that dispatch turns the corner off with no
    // error anywhere, which is why it is asserted here rather than trusted.
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("overview"));
    QCOMPARE(PointerCornerGesture::action(), QStringLiteral("overview"));
    QCOMPARE(PointerCornerGesture::edgeName(), QStringLiteral("top-left"));
}

QTEST_MAIN(PointerCornerTests)
#include "tst_pointercorner.moc"
