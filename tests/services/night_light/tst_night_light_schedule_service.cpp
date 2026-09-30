// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_client.h>
#include <qindaqt/services/night_light/night_light_schedule_service.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/display_client/client.h>
#include "../display_client/support/fake_display_transport.h"
#include "../display_client/support/display_client_test_support.h"
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::NightLight;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
namespace {
const QString ServiceName = QStringLiteral("org.qindaqt.NightLight");

class PrivateBus final {
public:
    PrivateBus()
    {
        process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
            {QStringLiteral("--session"), QStringLiteral("--nofork"),
             QStringLiteral("--print-address=1"), QStringLiteral("--print-pid=1")});
        if (!process.waitForStarted() || !process.waitForReadyRead()) return;
        address = QString::fromUtf8(process.readLine()).trimmed();
        process.readLine();
        valid = !address.isEmpty();
    }
    ~PrivateBus() { process.terminate(); process.waitForFinished(); }
    QProcess process;
    QString address;
    bool valid = false;
};

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    struct Request { quint64 token; QString owner; QStringList keys; };
    using SettingsTransport::SettingsTransport;
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &keys) override
    { snapshots.append({token, owner, keys}); }
    void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override {}
    void requestActivation() override { ++activations; }
    void acceptSnapshot()
    {
        const auto request = snapshots.takeFirst();
        QVariantMap values{
            {QStringLiteral("display.nightLight.active"), true},
            {QStringLiteral("display.nightLight.mode"), QStringLiteral("DarkLight")},
            {QStringLiteral("display.nightLight.dayTemperatureKelvin"), qint64(6500)},
            {QStringLiteral("display.nightLight.nightTemperatureKelvin"), qint64(4500)},
            {QStringLiteral("display.nightLight.scheduleSource"), QStringLiteral("Times")},
            {QStringLiteral("display.nightLight.automaticLocation"), false},
            {QStringLiteral("display.nightLight.latitudeDegrees"), 0.0},
            {QStringLiteral("display.nightLight.longitudeDegrees"), 0.0},
            {QStringLiteral("display.nightLight.sunriseStart"), QStringLiteral("06:00:00")},
            {QStringLiteral("display.nightLight.sunsetStart"), QStringLiteral("18:00:00")},
            {QStringLiteral("display.nightLight.transitionSeconds"), qint64(1800)},
            {QStringLiteral("display.nightLight.disabledOutputs"), QStringList{QStringLiteral("edid:test")}}};
        QVariantMap layers;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            layers.insert(it.key(), QStringLiteral("user-overrides"));
        QVariantMap wire{
            {QString::fromLatin1(WireContract::FieldStatus),
             quint32(QindaQt::Services::SettingsProtocol::SettingsWireStatus::Applied)},
            {QString::fromLatin1(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QString::fromLatin1(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QString::fromLatin1(WireContract::FieldEpoch), QStringLiteral("night-light-test-epoch")},
            {QString::fromLatin1(WireContract::FieldRevision), qulonglong(1)},
            {QString::fromLatin1(WireContract::FieldValues), values},
            {QString::fromLatin1(WireContract::FieldSourceLayers), layers},
            {QString::fromLatin1(WireContract::FieldMessage), QString{}}};
        Q_EMIT snapshotReceived(request.token, request.owner, wire);
    }
    QList<Request> snapshots;
    int activations = 0;
};
}

class NightLightScheduleServiceTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void residentServicePublishesSettingsBackedTargetedScheduleReceipt();
};

void NightLightScheduleServiceTests::residentServicePublishesSettingsBackedTargetedScheduleReceipt()
{
    PrivateBus daemon;
    QVERIFY(daemon.valid);
    const auto connectBus = [&daemon](const QString &name) {
        return QDBusConnection::connectToBus(daemon.address, name);
    };
    auto serviceBus = connectBus(QStringLiteral("night-light-service"));
    auto clientBus = connectBus(QStringLiteral("night-light-client"));
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());

    FakeTransport transport;
    SettingsClient settings(transport, nightLightSettingKeys());
    QVERIFY(settings.start());
    QCOMPARE(transport.activations, 1);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.20"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    transport.acceptSnapshot();
    QTRY_VERIFY(settings.state() == ClientState::Ready);

    QindaQt::DisplayClient::TestSupport::FakeDisplayTransport displayTransport;
    QindaQt::DisplayClient::Client displayClient(&displayTransport);
    displayClient.start();
    displayTransport.publishOwner(QStringLiteral(":1.21"));
    auto outputSnapshot = QindaQt::DisplayClient::TestSupport::testSnapshot();
    outputSnapshot.outputs[0].runtimeCompositorUuid = QStringLiteral("compositor-uuid");
    displayTransport.replySnapshot(displayTransport.fetches.constLast(), outputSnapshot);
    QTRY_COMPARE(displayClient.state(), QindaQt::DisplayClient::ClientState::Ready);

    NightLightScheduleService service(serviceBus, settings, displayClient);
    QString error;
    QCOMPARE(service.start(&error), ScheduleServiceStart::Started);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(serviceBus.baseService().startsWith(QLatin1Char(':')));

    QtNightLightScheduleClient client(clientBus);
    QSignalSpy states(&client, &QtNightLightScheduleClient::stateChanged);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.state().has_value(), 5000);
    QCOMPARE(states.size(), 1);
    QCOMPARE(client.state()->settings.output.disabledOutputs,
             QStringList{QStringLiteral("edid:test")});
    QCOMPARE(client.state()->disabledOutputUuids,
             QStringList{QStringLiteral("compositor-uuid")});
    QVERIFY(client.state()->settings.output.active);
    QVERIFY(client.state()->schedule.available);
    QVERIFY(client.state()->outputIdentityAvailable);
    QVERIFY(client.state()->revision >= quint64(1));


    client.stop();
    service.stop();
    settings.stop();
    displayClient.stop();
    displayClient.stop();
    serviceBus.unregisterObject(QStringLiteral("/org/qindaqt/NightLight"));
    serviceBus.unregisterService(ServiceName);
    QDBusConnection::disconnectFromBus(QStringLiteral("night-light-service"));
    QDBusConnection::disconnectFromBus(QStringLiteral("night-light-client"));
}

QTEST_GUILESS_MAIN(NightLightScheduleServiceTests)
#include "tst_night_light_schedule_service.moc"
