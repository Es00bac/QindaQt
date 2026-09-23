// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinpointercornerreserver.h"

#include <QSignalSpy>
#include <QtTest>

using QindaQt::Compositor::KWinIntegration::KWinPointerCornerReserver;
using QindaQt::Compositor::KWinIntegration::PointerCornerGesture;

// Reproduce KWin's Q_ARG(ElectricBorder, ...) spelling from screenedge.cpp.
// QMetaObject invocation silently fails if the adapter's slot has the wrong
// signature, which is exactly the live hot-corner regression this guards.
namespace KWin {
bool invokeEdge(KWinPointerCornerReserver &reserver, ElectricBorder border,
                bool *consumed)
{
    return QMetaObject::invokeMethod(&reserver, "edgeTriggered",
                                     Q_RETURN_ARG(bool, *consumed),
                                     Q_ARG(ElectricBorder, border));
}
} // namespace KWin

class KWinPointerCornerReserverTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void matchesKWinCallbackAndForwardsOnlyTheUpperLeft();
};

void KWinPointerCornerReserverTests::matchesKWinCallbackAndForwardsOnlyTheUpperLeft()
{
    KWinPointerCornerReserver reserver;
    PointerCornerGesture gesture(reserver);
    QSignalSpy triggered(&gesture, &PointerCornerGesture::triggered);
    QVERIFY(triggered.isValid());

    bool consumed = true;
    QVERIFY(KWin::invokeEdge(reserver, KWin::ElectricTop, &consumed));
    QVERIFY(!consumed);
    QCOMPARE(triggered.count(), 0);

    QVERIFY(KWin::invokeEdge(reserver, KWin::ElectricTopLeft, &consumed));
    QVERIFY(consumed);
    QCOMPARE(triggered.count(), 1);
    QCOMPARE(triggered.constFirst().at(0).toString(), QStringLiteral("top-left"));
    QCOMPARE(triggered.constFirst().at(1).toString(), QStringLiteral("overview"));
}

QTEST_GUILESS_MAIN(KWinPointerCornerReserverTests)
#include "tst_kwinpointercornerreserver.moc"
