// SPDX-License-Identifier: GPL-3.0-or-later
#include "firewall_applet_controller.h"
#include "firewall_applet_test_support.h"

#include <QTest>

using namespace QindaQt::Shell::FirewallApplet;
using namespace QindaQt::Shell::FirewallApplet::TestSupport;
using QindaQt::Firewall::Client::FirewallClient;

namespace {

const QString kAlertId = QStringLiteral("password-guessing:203.0.113.9:1761940000");

} // namespace

class FirewallAppletControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void absentServiceIsUnavailableUntilItAppears();
    void serviceDisappearingRetiresEveryFact();
    void deniedGrantNeverTouchesTheClient();
    void alertRaisedUpdatesCountOnceTheRefreshLands();
    void openButtonCallsOpenWindowWithTopAlertIdOrEmpty();
    void dismissButtonCallsDismissOnlyWithAnAlert();
    void confirmMineButtonRespectsCanConfirm();
};

void FirewallAppletControllerTests::absentServiceIsUnavailableUntilItAppears()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(controller.state() == QStringLiteral("unavailable"));

    QVERIFY(client.start());
    QVERIFY(controller.state() == QStringLiteral("unavailable"));

    transport.setProperties(QStringLiteral("live"), headlineJson(QStringLiteral("ok"), {}, {}),
                            0, QByteArray());
    int changes = 0;
    connect(&controller, &FirewallAppletController::modelChanged, [&changes] { ++changes; });
    transport.announceAvailable(true);
    QCOMPARE(controller.state(), QStringLiteral("secure"));
    QVERIFY(changes >= 1);
}

void FirewallAppletControllerTests::serviceDisappearingRetiresEveryFact()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(client.start());
    transport.setProperties(
        QStringLiteral("live"), headlineJson(QStringLiteral("danger"), QStringLiteral("Alarm"), {}),
        1, alertJson(kAlertId, QStringLiteral("danger"), QStringLiteral("Alarm"),
                     QStringLiteral("s"), QStringLiteral("m"), QStringLiteral("a"), false));
    transport.announceAvailable(true);
    QVERIFY(controller.state() == QStringLiteral("alarm"));
    QVERIFY(controller.hasAlert());

    transport.announceAvailable(false);
    QVERIFY(controller.state() == QStringLiteral("unavailable"));
    QVERIFY(!controller.hasAlert());
    QCOMPARE(controller.alertCount(), quint32(0));
}

void FirewallAppletControllerTests::deniedGrantNeverTouchesTheClient()
{
    FirewallAppletController controller(nullptr, false);
    QVERIFY(controller.state() == QStringLiteral("unavailable"));
    // No crash, no-op: nothing to assert beyond "did not touch a null client".
    controller.openWindow();
    controller.dismissTopAlert();
    controller.confirmTopAlertMine();
}

void FirewallAppletControllerTests::alertRaisedUpdatesCountOnceTheRefreshLands()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(client.start());
    transport.setProperties(QStringLiteral("live"), headlineJson(QStringLiteral("ok"), {}, {}),
                            0, QByteArray());
    transport.announceAvailable(true);
    QCOMPARE(controller.alertCount(), quint32(0));

    // The fake answers requestSnapshot synchronously from whatever is set
    // here, exactly like the production transport would once the follow-up
    // GetAll lands; emitting alertRaised alone is what asks for that
    // refresh (FirewallClient::handleAlertRaised calls refresh()).
    transport.setProperties(
        QStringLiteral("live"),
        headlineJson(QStringLiteral("warning"), QStringLiteral("Someone is trying to get in"), {}),
        1, alertJson(kAlertId, QStringLiteral("warning"), QStringLiteral("Someone is trying to get in"),
                     QStringLiteral("summary"), QStringLiteral("meaning"), QStringLiteral("advice"),
                     false));
    transport.emitAlertRaised(
        alertJson(kAlertId, QStringLiteral("warning"), QStringLiteral("Someone is trying to get in"),
                 QStringLiteral("summary"), QStringLiteral("meaning"), QStringLiteral("advice"), false));

    QCOMPARE(controller.alertCount(), quint32(1));
    QVERIFY(controller.hasAlert());
    QCOMPARE(controller.alertTitle(), QStringLiteral("Someone is trying to get in"));
}

void FirewallAppletControllerTests::openButtonCallsOpenWindowWithTopAlertIdOrEmpty()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(client.start());

    transport.setProperties(QStringLiteral("live"), headlineJson(QStringLiteral("ok"), {}, {}),
                            0, QByteArray());
    transport.announceAvailable(true);
    controller.openWindow();
    QCOMPARE(transport.openedWindow, QStringList{QString()});

    transport.setProperties(
        QStringLiteral("live"), headlineJson(QStringLiteral("danger"), QStringLiteral("Alarm"), {}),
        1, alertJson(kAlertId, QStringLiteral("danger"), QStringLiteral("Alarm"), QStringLiteral("s"),
                     QStringLiteral("m"), QStringLiteral("a"), false));
    transport.emitChanged();
    controller.openWindow();
    QCOMPARE(transport.openedWindow, QStringList({QString(), kAlertId}));
}

void FirewallAppletControllerTests::dismissButtonCallsDismissOnlyWithAnAlert()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(client.start());
    transport.setProperties(QStringLiteral("live"), headlineJson(QStringLiteral("ok"), {}, {}),
                            0, QByteArray());
    transport.announceAvailable(true);

    controller.dismissTopAlert();
    QVERIFY(transport.dismissed.isEmpty());

    transport.setProperties(
        QStringLiteral("live"), headlineJson(QStringLiteral("warning"), QStringLiteral("t"), {}),
        1, alertJson(kAlertId, QStringLiteral("warning"), QStringLiteral("t"), QStringLiteral("s"),
                     QStringLiteral("m"), QStringLiteral("a"), false));
    transport.emitChanged();
    controller.dismissTopAlert();
    QCOMPARE(transport.dismissed, QStringList{kAlertId});
}

void FirewallAppletControllerTests::confirmMineButtonRespectsCanConfirm()
{
    FakeFirewallTransport transport;
    FirewallClient client(transport);
    FirewallAppletController controller(&client, true);
    QVERIFY(client.start());

    transport.setProperties(
        QStringLiteral("live"), headlineJson(QStringLiteral("warning"), QStringLiteral("t"), {}),
        1, alertJson(kAlertId, QStringLiteral("warning"), QStringLiteral("t"), QStringLiteral("s"),
                     QStringLiteral("m"), QStringLiteral("a"), /*canConfirm=*/false));
    transport.announceAvailable(true);
    QVERIFY(!controller.alertCanConfirm());
    controller.confirmTopAlertMine();
    QVERIFY(transport.confirmedMine.isEmpty());

    transport.setProperties(
        QStringLiteral("live"), headlineJson(QStringLiteral("warning"), QStringLiteral("t"), {}),
        1, alertJson(kAlertId, QStringLiteral("warning"), QStringLiteral("t"), QStringLiteral("s"),
                     QStringLiteral("m"), QStringLiteral("a"), /*canConfirm=*/true));
    transport.emitChanged();
    QVERIFY(controller.alertCanConfirm());
    controller.confirmTopAlertMine();
    QCOMPARE(transport.confirmedMine, QStringList{kAlertId});
}

QTEST_GUILESS_MAIN(FirewallAppletControllerTests)
#include "tst_firewall_applet_controller.moc"
