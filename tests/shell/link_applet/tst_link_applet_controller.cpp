// SPDX-License-Identifier: GPL-3.0-or-later
#include "link_applet_controller.h"
#include "link_applet_test_support.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Shell::LinkApplet;
using namespace QindaQt::Shell::LinkApplet::TestSupport;
using QindaQt::Link::LinkClient;

namespace {

const QString kId = QStringLiteral("aa");
const QStringList kRing{QStringLiteral("ring")};

QByteArray connectedPhone(const QStringList &caps = {})
{
    return devicesJson({deviceObject(kId, QStringLiteral("Pixel"), true, true, true, 80, false,
                                     QStringLiteral("usb"), caps)});
}

QByteArray strangerConnecting()
{
    return devicesJson({deviceObject(QStringLiteral("bb"), QStringLiteral("New"), false, true, false)});
}

} // namespace

class LinkAppletControllerTests final : public QObject {
    Q_OBJECT
private slots:
    void ownerAppearsAfterTheShellAndVanishes();
    void deniedReadNeverTouchesTheClient();
    void pairingRequestAcceptAndReject();
    void pairingRequestEndsWhenTheServiceVanishesOrThePhoneLeaves();
    void controlDeniedKeepsTheReadViewButNoActions();
    void ringIsGatedOnTheAdvertisedCapability();
    void unpairCallsTheTypedMethod();
    void openAppUsesTheInjectedLauncherOnlyWhenAvailable();
    void malformedPairingCodeIsIgnored();
};

void LinkAppletControllerTests::ownerAppearsAfterTheShellAndVanishes()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QCOMPARE(controller.state(), QStringLiteral("unavailable"));
    QVERIFY(client.start());
    QCOMPARE(controller.state(), QStringLiteral("unavailable"));

    transport.devicesJson = connectedPhone();
    QSignalSpy changed(&controller, &LinkAppletController::modelChanged);
    transport.announceAvailable(true);
    QCOMPARE(controller.state(), QStringLiteral("connected"));
    QCOMPARE(controller.batteryText(), QStringLiteral("80%"));
    QVERIFY(changed.count() >= 1);

    // The phone pairs later, with the service already up.
    transport.setDevices(devicesJson({}));
    QCOMPARE(controller.state(), QStringLiteral("no-phone"));
    transport.setDevices(connectedPhone());
    QCOMPARE(controller.state(), QStringLiteral("connected"));

    transport.announceAvailable(false);
    QCOMPARE(controller.state(), QStringLiteral("unavailable"));
    QVERIFY(controller.batteryText().isEmpty());
    QVERIFY(!controller.hasPhone());
}

void LinkAppletControllerTests::deniedReadNeverTouchesTheClient()
{
    LinkAppletController controller(nullptr, false, true);
    QCOMPARE(controller.state(), QStringLiteral("unavailable"));
    QVERIFY(!controller.canControl());
    controller.acceptPairing();
    controller.rejectPairing();
    controller.ringPhone();
    controller.unpairPhone();
    controller.refreshAppAvailability();
    QVERIFY(!controller.canOpenApp());
}

void LinkAppletControllerTests::pairingRequestAcceptAndReject()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QVERIFY(client.start());
    transport.devicesJson = strangerConnecting();
    transport.announceAvailable(true);
    QCOMPARE(controller.state(), QStringLiteral("no-phone"));

    transport.raisePairing(QStringLiteral("bb"), QStringLiteral("New"), QStringLiteral("SM-N"),
                           QStringLiteral("012345"));
    QCOMPARE(controller.state(), QStringLiteral("pairing-request"));
    QCOMPARE(controller.pairingCode(), QStringLiteral("012 345"));
    QCOMPARE(controller.pairingName(), QStringLiteral("New"));
    controller.acceptPairing();
    QCOMPARE(transport.confirmations, QStringList{QStringLiteral("bb:accept")});
    QVERIFY(!controller.hasPairing());

    transport.raisePairing(QStringLiteral("bb"), QStringLiteral("New"), QStringLiteral("SM-N"),
                           QStringLiteral("012345"));
    controller.rejectPairing();
    QCOMPARE(transport.confirmations,
             (QStringList{QStringLiteral("bb:accept"), QStringLiteral("bb:reject")}));
    controller.acceptPairing(); // nothing pending any more
    QCOMPARE(transport.confirmations.size(), 2);
}

void LinkAppletControllerTests::pairingRequestEndsWhenTheServiceVanishesOrThePhoneLeaves()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QVERIFY(client.start());
    transport.devicesJson = strangerConnecting();
    transport.announceAvailable(true);
    transport.raisePairing(QStringLiteral("bb"), QStringLiteral("New"), QStringLiteral("M"),
                           QStringLiteral("111111"));
    QVERIFY(controller.hasPairing());
    transport.setDevices(devicesJson({})); // phone dropped (timeout/disconnect)
    QVERIFY(!controller.hasPairing());

    transport.setDevices(strangerConnecting());
    transport.raisePairing(QStringLiteral("bb"), QStringLiteral("New"), QStringLiteral("M"),
                           QStringLiteral("222222"));
    QVERIFY(controller.hasPairing());
    transport.announceAvailable(false);
    QVERIFY(!controller.hasPairing());
    transport.devicesJson = strangerConnecting();
    transport.announceAvailable(true);
    QVERIFY(!controller.hasPairing()); // not resurrected
}

void LinkAppletControllerTests::controlDeniedKeepsTheReadViewButNoActions()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, false);
    QVERIFY(client.start());
    transport.devicesJson = connectedPhone(kRing);
    transport.announceAvailable(true);
    QCOMPARE(controller.state(), QStringLiteral("connected"));
    QVERIFY(!controller.canControl());
    QVERIFY(!controller.canRing());
    controller.ringPhone();
    controller.unpairPhone();
    transport.raisePairing(kId, QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("123456"));
    controller.acceptPairing();
    QVERIFY(transport.rang.isEmpty() && transport.unpaired.isEmpty() && transport.confirmations.isEmpty());
}

void LinkAppletControllerTests::ringIsGatedOnTheAdvertisedCapability()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QVERIFY(client.start());
    transport.devicesJson = connectedPhone();
    transport.announceAvailable(true);
    QVERIFY(!controller.canRing()); // the phone does not advertise `ring` yet
    controller.ringPhone();
    QVERIFY(transport.rang.isEmpty());

    transport.setDevices(connectedPhone(kRing));
    QVERIFY(controller.canRing());
    controller.ringPhone();
    QCOMPARE(transport.rang, QStringList{kId});

    transport.setDevices(devicesJson({deviceObject(kId, QStringLiteral("Pixel"), true, false, false,
                                                   -1, false, {}, kRing)}));
    QVERIFY(!controller.canRing());
}

void LinkAppletControllerTests::unpairCallsTheTypedMethod()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QVERIFY(client.start());
    controller.unpairPhone(); // no phone yet
    QVERIFY(transport.unpaired.isEmpty());
    transport.devicesJson = connectedPhone();
    transport.announceAvailable(true);
    controller.unpairPhone();
    QCOMPARE(transport.unpaired, QStringList{kId});
}

void LinkAppletControllerTests::openAppUsesTheInjectedLauncherOnlyWhenAvailable()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    int launched = 0;
    bool present = false;
    controller.openApp();
    QCOMPARE(launched, 0);
    controller.setAppLaunch([&] { return present; }, [&] { ++launched; return true; });
    QVERIFY(!controller.canOpenApp());
    controller.openApp();
    QCOMPARE(launched, 0);
    present = true;
    QSignalSpy spy(&controller, &LinkAppletController::canOpenAppChanged);
    controller.refreshAppAvailability();
    QVERIFY(controller.canOpenApp());
    QCOMPARE(spy.count(), 1);
    controller.openApp();
    QCOMPARE(launched, 1);
}

void LinkAppletControllerTests::malformedPairingCodeIsIgnored()
{
    FakeLinkTransport transport;
    LinkClient client(transport);
    LinkAppletController controller(&client, true, true);
    QVERIFY(client.start());
    transport.devicesJson = strangerConnecting();
    transport.announceAvailable(true);
    transport.raisePairing(QStringLiteral("bb"), QStringLiteral("N"), QStringLiteral("M"),
                           QStringLiteral("12"));
    QVERIFY(!controller.hasPairing());
}

QTEST_GUILESS_MAIN(LinkAppletControllerTests)
#include "tst_link_applet_controller.moc"
