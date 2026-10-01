// SPDX-License-Identifier: GPL-3.0-or-later
#include "dpms_wayland_fixture.h"
#include "kwayland_dpms_controller.h"

#include <QtTest>

using QindaQt::Session::IdlePolicy::KWaylandDpmsController;

class KWaylandDpmsControllerTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void admittedConnectionTracksCapabilityRemovalAndRestore();
    void revokedLineageAllowsOnlyRetainedFinalRestore();
    void finalRestoreWaitsForDelayedPeer_data();
    void finalRestoreWaitsForDelayedPeer();
    void unresponsivePeerBoundsFinalRestore();
    void absentOrDisconnectedPeerDoesNotDelayTeardown();
};

void KWaylandDpmsControllerTest::admittedConnectionTracksCapabilityRemovalAndRestore()
{
    DpmsWaylandFixture server;
    const quint32 firstOutput = server.addOutput();
    const quint32 secondOutput = server.addOutput();
    QVERIFY(firstOutput != 0);
    QVERIFY(secondOutput != 0);

    KWaylandDpmsController controller;
    QSignalSpy availabilityChanges(&controller,
                                  &KWaylandDpmsController::availabilityChanged);
    const auto start = [&] {
        QString error;
        const bool connected = controller.start([&server] { return server.openClientFd(); },
                                                [] { return true; }, &error);

        return connected;
    };
    QVERIFY(start());
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);

    controller.requestOff();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 2, 3'000);
    QCOMPARE(server.lastRequestedMode(),
             static_cast<quint32>(ORG_KDE_KWIN_DPMS_MODE_OFF));

    const qsizetype previousAvailabilitySignals = availabilityChanges.size();
    server.removeOutput(firstOutput);
    QTRY_VERIFY_WITH_TIMEOUT(availabilityChanges.size() > previousAvailabilitySignals,
                             3'000);
    QVERIFY(controller.available());
    controller.requestOn();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 3, 3'000);
    QCOMPARE(server.lastRequestedMode(),
             static_cast<quint32>(ORG_KDE_KWIN_DPMS_MODE_ON));

    server.removeOutput(secondOutput);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.available(), 3'000);
    controller.restoreAndStop();
    controller.stop();
    QVERIFY(!controller.available());

    // Reconnect to a fresh admitted FD, then prove the teardown On is flushed
    // before the worker disconnects. Repeated start/stop must not retain proxies.
    const quint32 thirdOutput = server.addOutput();
    QVERIFY(thirdOutput != 0);
    QVERIFY(start());
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);
    controller.requestOff();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 4, 3'000);
    controller.restoreAndStop();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 5, 3'000);
    QCOMPARE(server.lastRequestedMode(),
             static_cast<quint32>(ORG_KDE_KWIN_DPMS_MODE_ON));
    controller.stop();
}

void KWaylandDpmsControllerTest::revokedLineageAllowsOnlyRetainedFinalRestore()
{
    DpmsWaylandFixture server;
    QVERIFY(server.addOutput() != 0);
    bool admitted = true;
    int openedConnections = 0;
    KWaylandDpmsController controller;
    QString error;
    QVERIFY(controller.start([&] {
        ++openedConnections;
        return server.openClientFd();
    }, [&] { return admitted; }, &error));
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);
    controller.requestOff();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 1, 3'000);

    admitted = false;
    QVERIFY(!controller.available());
    controller.requestOn();
    controller.requestOff();
    controller.restoreAndStop();
    // Only the final restore reaches the retained peer after revocation. No
    // supplier call may reopen a same-name replacement compositor connection.
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 2, 3'000);
    QCOMPARE(server.lastRequestedMode(),
             static_cast<quint32>(ORG_KDE_KWIN_DPMS_MODE_ON));
    QCOMPARE(openedConnections, 1);
    QVERIFY(!controller.start([&] {
        ++openedConnections;
        return server.openClientFd();
    }, [&] { return admitted; }, &error));
    QCOMPARE(openedConnections, 1);
    QVERIFY(!error.isEmpty());
}

void KWaylandDpmsControllerTest::finalRestoreWaitsForDelayedPeer_data()
{
    QTest::addColumn<bool>("revoked");
    QTest::newRow("live") << false;
    QTest::newRow("revoked") << true;
}

void KWaylandDpmsControllerTest::finalRestoreWaitsForDelayedPeer()
{
    QFETCH(bool, revoked);
    DpmsWaylandFixture server;
    QVERIFY(server.addOutput() != 0);
    bool admitted = true;
    int openedConnections = 0;
    KWaylandDpmsController controller;
    QVERIFY(controller.start([&] {
        ++openedConnections;
        return server.openClientFd();
    }, [&] { return admitted; }));
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);
    controller.requestOff();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 1, 3'000);

    // AGENT-GUARD: a successful client flush is not server dispatch. Closing
    // while reads are held gives libwayland HANGUP alongside unread final On.
    server.pauseDispatch(80);
    admitted = !revoked;
    controller.restoreAndStop();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 2, 3'000);
    QCOMPARE(server.lastRequestedMode(),
             static_cast<quint32>(ORG_KDE_KWIN_DPMS_MODE_ON));
    QCOMPARE(openedConnections, 1);
    QVERIFY(!controller.available());
}

void KWaylandDpmsControllerTest::unresponsivePeerBoundsFinalRestore()
{
    DpmsWaylandFixture server;
    QVERIFY(server.addOutput() != 0);
    bool admitted = true;
    int openedConnections = 0;
    KWaylandDpmsController controller;
    QVERIFY(controller.start([&] {
        ++openedConnections;
        return server.openClientFd();
    }, [&] { return admitted; }));
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);
    controller.requestOff();
    QTRY_COMPARE_WITH_TIMEOUT(server.setRequestCount(), 1, 3'000);
    server.pauseDispatch(1'000);
    admitted = false;
    QElapsedTimer elapsed;
    elapsed.start();
    controller.restoreAndStop();
    // A dead/stalled peer cannot hold supervisor teardown indefinitely. The
    // lower bound proves we exercised the missing-ack path, not an early error.
    QVERIFY(elapsed.elapsed() >= 150);
    QVERIFY(elapsed.elapsed() < 750);
    QCOMPARE(openedConnections, 1);
    QVERIFY(!controller.available());
    controller.stop();
}

void KWaylandDpmsControllerTest::absentOrDisconnectedPeerDoesNotDelayTeardown()
{
    KWaylandDpmsController controller;
    QElapsedTimer elapsed;
    elapsed.start();
    controller.restoreAndStop();
    QVERIFY(elapsed.elapsed() < 750);

    DpmsWaylandFixture server;
    QVERIFY(server.addOutput() != 0);
    QVERIFY(controller.start([&] { return server.openClientFd(); }, [] { return true; }));
    QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 3'000);
    server.disconnectClients();
    // Teardown may race the connection-death notification; it must bound both
    // an already retired display and a retained display with a disconnected FD.
    elapsed.restart();
    controller.restoreAndStop();
    QVERIFY(elapsed.elapsed() < 750);
    QVERIFY(!controller.available());
}

QTEST_GUILESS_MAIN(KWaylandDpmsControllerTest)
#include "tst_kwayland_dpms_controller.moc"
