// SPDX-License-Identifier: GPL-3.0-or-later
// Devices view model over a scripted service: the paired phone, pairing requests (listed and live),
// accept/reject/unpair, and the USB setup report.
#include "devices_view_model.h"
#include "fake_link_api.h"

#include <QtTest/QtTest>

using namespace QindaQt::LinkApp;

namespace {

QString pendingDevice(const QString &id)
{
    return QStringLiteral("[{\"id\":\"%1\",\"name\":\"New phone\",\"model\":\"SM-X\",\"paired\":false,"
                          "\"connected\":true,\"ready\":false,\"transport\":\"usb\"}]").arg(id);
}

} // namespace

class DevicesTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void describesThePairedPhone()
    {
        FakeLinkApi api;
        DevicesViewModel devices(api);
        QSignalSpy idChanged(&devices, &DevicesViewModel::deviceIdChanged);
        devices.refresh();
        QTRY_VERIFY(devices.hasDevice());
        QVERIFY(devices.phoneReady());
        QCOMPARE(devices.device().value(QStringLiteral("name")).toString(), QStringLiteral("Demo phone"));
        QCOMPARE(devices.device().value(QStringLiteral("transport")).toString(), QStringLiteral("USB"));
        QVERIFY(devices.device().value(QStringLiteral("battery")).toString().startsWith(QStringLiteral("87%")));
        QVERIFY(devices.device().value(QStringLiteral("network")).toString().contains(QStringLiteral("Wi-Fi")));
        QVERIFY(devices.capabilities().contains(QStringLiteral("sms")));
        QCOMPARE(idChanged.size(), 1);
    }

    void aMissingServiceIsAStateNotACrash()
    {
        FakeLinkApi api(FakeLinkApi::Scenario::Unavailable);
        DevicesViewModel devices(api);
        devices.refresh();
        QTRY_VERIFY(!devices.error().isEmpty());
        QVERIFY(!devices.serviceAvailable());
        QVERIFY(!devices.hasDevice());
    }

    void showsLivePairingRequestWithSpacedCodeAndAcceptSendsDecision()
    {
        FakeLinkApi api(FakeLinkApi::Scenario::Empty);
        api.devicesJson = pendingDevice(QStringLiteral("aa"));
        DevicesViewModel devices(api);
        devices.refresh();
        QTRY_VERIFY(devices.hasDevice());
        Q_EMIT api.pairingRequested(QStringLiteral("aa"), QStringLiteral("New phone"), QStringLiteral("SM-X"), QStringLiteral("007123"));
        QCOMPARE(devices.pairings()->count(), 1);
        QCOMPARE(devices.pairings()->get(0).value(QStringLiteral("code")).toString(), QStringLiteral("007 123"));
        QVERIFY(!api.confirmations.contains(QStringLiteral("aa")));  // showing a code never decides anything

        devices.accept(QStringLiteral("aa"));
        QCOMPARE(api.confirmations.value(QStringLiteral("aa")), true);
        QCOMPARE(devices.pairings()->count(), 0);
    }

    void rejectSendsADeclineAndTheRequestDisappears()
    {
        FakeLinkApi api(FakeLinkApi::Scenario::Empty);
        api.devicesJson = pendingDevice(QStringLiteral("bb"));
        DevicesViewModel devices(api);
        devices.refresh();
        QTRY_VERIFY(devices.hasDevice());
        Q_EMIT api.pairingRequested(QStringLiteral("bb"), QStringLiteral("New phone"), QStringLiteral("SM-X"), QStringLiteral("123456"));
        devices.reject(QStringLiteral("bb"));
        QCOMPARE(api.confirmations.value(QStringLiteral("bb"), true), false);
        QCOMPARE(devices.pairings()->count(), 0);
    }

    void listsPendingPairingsFromTheServiceAndToleratesItsAbsence()
    {
        FakeLinkApi api(FakeLinkApi::Scenario::Empty);
        api.devicesJson = pendingDevice(QStringLiteral("cc"));
        api.pending = QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("cc")}, {QStringLiteral("name"), QStringLiteral("New phone")},
                                             {QStringLiteral("model"), QStringLiteral("SM-X")}, {QStringLiteral("code"), QStringLiteral("654321")}}};
        {
            DevicesViewModel devices(api);
            devices.refresh();
            QTRY_COMPARE(devices.pairings()->count(), 1);
            QCOMPARE(devices.pairings()->get(0).value(QStringLiteral("code")).toString(), QStringLiteral("654 321"));
        }
        api.pendingSupported = false;  // a service that predates PendingPairings
        DevicesViewModel old(api);
        old.refresh();
        QTRY_VERIFY(old.hasDevice());
        QTest::qWait(30);
        QCOMPARE(old.pairings()->count(), 0);
        QVERIFY(old.error().isEmpty());
    }

    void aRequestForADeviceThatLeftIsDropped()
    {
        FakeLinkApi api(FakeLinkApi::Scenario::Empty);
        api.devicesJson = pendingDevice(QStringLiteral("dd"));
        DevicesViewModel devices(api);
        devices.refresh();
        QTRY_VERIFY(devices.hasDevice());
        Q_EMIT api.pairingRequested(QStringLiteral("dd"), QStringLiteral("New phone"), QStringLiteral("SM-X"), QStringLiteral("111222"));
        QCOMPARE(devices.pairings()->count(), 1);
        Q_EMIT api.devicesChanged(QStringLiteral("[]"));
        QCOMPARE(devices.pairings()->count(), 0);
    }

    void unpairGoesToTheService()
    {
        FakeLinkApi api;
        DevicesViewModel devices(api);
        devices.refresh();
        QTRY_VERIFY(devices.hasDevice());
        devices.unpair(devices.deviceId());
        QCOMPARE(api.unpaired, QStringList{devices.deviceId()});
    }

    void setupReportsEveryStepAndNamesFailures()
    {
        FakeLinkApi api;
        api.setupReportJson = QStringLiteral("{\"serial\":\"DEMO0001\",\"ok\":false,\"steps\":["
                                             "{\"id\":\"grant:READ_SMS\",\"ok\":true,\"code\":0},"
                                             "{\"id\":\"notification-listener\",\"ok\":false,\"code\":1}]}");
        DevicesViewModel devices(api);
        QSignalSpy changed(&devices, &DevicesViewModel::setupChanged);
        devices.setupPhone();
        QVERIFY(devices.setupBusy());
        QTRY_VERIFY(!devices.setupBusy());
        QCOMPARE(devices.adbSerial(), QStringLiteral("DEMO0001"));
        QCOMPARE(devices.setupSteps()->count(), 2);
        QCOMPARE(devices.setupSteps()->get(1).value(QStringLiteral("ok")).toBool(), false);
        QVERIFY(devices.setupMessage().contains(QStringLiteral("1")));
        QCOMPARE(api.calls.count(QStringLiteral("setupPhone")), 1);
    }

    void setupWithoutAPhoneOverUsbSaysSoAndRunsNothing()
    {
        FakeLinkApi api;
        api.adb = QJsonArray();
        DevicesViewModel devices(api);
        devices.setupPhone();
        QTRY_VERIFY(!devices.setupBusy() && !devices.setupMessage().isEmpty());
        QVERIFY(devices.setupMessage().contains(QStringLiteral("USB")));
        QVERIFY(!api.calls.contains(QStringLiteral("setupPhone")));
    }

    void checkingStatusNeverRunsSetup()
    {
        FakeLinkApi api;
        api.statusReportJson = QStringLiteral("{\"serial\":\"DEMO0001\",\"ok\":true,\"steps\":[{\"id\":\"battery-whitelist\",\"ok\":true,\"code\":0}]}");
        DevicesViewModel devices(api);
        devices.checkSetup();
        QTRY_COMPARE(devices.setupSteps()->count(), 1);
        QVERIFY(api.calls.contains(QStringLiteral("setupStatus")));
        QVERIFY(!api.calls.contains(QStringLiteral("setupPhone")));
    }
};

QTEST_GUILESS_MAIN(DevicesTest)
#include "tst_devices.moc"
