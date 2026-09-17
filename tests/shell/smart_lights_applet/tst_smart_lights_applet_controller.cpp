// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fakewiztransport.h"
#include "smart_lights_applet_controller.h"

#include <qindaqt/services/smart_lights_store/configuration_store.h>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt::Shell::SmartLightsApplet;
using QindaQt::SmartLights::ConfigurationStore;
using QindaQt::Wiz::Testing::FakeWizClock;
using QindaQt::Wiz::Testing::FakeWizTransport;

namespace
{

const QString deviceMac = QStringLiteral("d8a011769356");
const QString deviceAddress = QStringLiteral("10.0.0.234");

[[nodiscard]] QByteArray pilotReply(const bool on = true, const int dimming = 50)
{
    return QStringLiteral(
               R"({"method":"getPilot","result":{"mac":"%1","state":%2,"dimming":%3,"sceneId":0,"r":0,"g":0,"b":255,"c":0,"w":0,"rssi":-50}})")
        .arg(deviceMac, on ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(dimming)
        .toUtf8();
}

[[nodiscard]] QByteArray systemReply()
{
    return QStringLiteral(
               R"({"method":"getSystemConfig","result":{"mac":"%1","moduleName":"ESP25_SHRGB_01","fwVersion":"1.38.0"}})")
        .arg(deviceMac)
        .toUtf8();
}

[[nodiscard]] QByteArray modelReply()
{
    return QStringLiteral(
               R"({"method":"getModelConfig","result":{"mac":"%1","headTotal":1,"minDimLevel":1,"cctRange":[2200,2700,6500,6500]}})")
        .arg(deviceMac)
        .toUtf8();
}

constexpr auto setPilotAck = R"({"method":"setPilot","result":{"success":true}})";

// One live light, a temporary configuration file, and a controller over both.
class Harness
{
public:
    explicit Harness(const bool controlGranted = true)
        : store(QDir(directory.path()).filePath(QStringLiteral("smart-lights.json")))
        , client(&transport, &clock)
        , controller(&client, &store, true, controlGranted)
    {
        client.setAutomaticPolling(false);
        client.start();
    }

    void introduceDevice()
    {
        transport.deliver(deviceAddress, pilotReply());
        transport.deliver(deviceAddress, systemReply());
        transport.deliver(deviceAddress, modelReply());
        transport.unicasts.clear();
    }

    [[nodiscard]] QVariantMap firstRow() const
    {
        const QVariantList rows = controller.deviceRows();
        return rows.isEmpty() ? QVariantMap() : rows.first().toMap();
    }

    [[nodiscard]] QString firstRowId() const
    {
        return firstRow().value(QStringLiteral("deviceId")).toString();
    }

    QTemporaryDir directory;
    ConfigurationStore store;
    FakeWizTransport transport;
    FakeWizClock clock;
    QindaQt::Wiz::WizClient client;
    SmartLightsAppletController controller;
};

} // namespace

class SmartLightsControllerTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsDiscoveredLightWithOpaqueRowToken();
    void powerIntentReachesTheLightAndConfirms();
    void refusesControlWithoutTheGrant();
    void offersOnlySceneOptionsTheLightCanRun();
    void renamePersistsAndSurvivesReload();
    void savedPresetCapturesLiveStateAndReapplies();
    void forgettingALightRemovesItFromStoredConfiguration();
    void unknownRowTokenIsInert();
    void reportsUncertaintyWhenALightGoesSilent();
};

void SmartLightsControllerTests::projectsDiscoveredLightWithOpaqueRowToken()
{
    Harness harness;
    QSignalSpy changed(&harness.controller, &SmartLightsAppletController::stateChanged);
    harness.introduceDevice();

    QVERIFY(changed.count() > 0);
    QCOMPARE(harness.controller.deviceCount(), 1);
    const QVariantMap row = harness.firstRow();
    QCOMPARE(row.value(QStringLiteral("label")).toString(),
             QStringLiteral("Colour light 769356"));
    QVERIFY(row.value(QStringLiteral("controllable")).toBool());
    QVERIFY(row.value(QStringLiteral("supportsColor")).toBool());
    // AGENT-GUARD in the controller: no hardware address may reach QML.
    const QString token = row.value(QStringLiteral("deviceId")).toString();
    QVERIFY(!token.contains(deviceMac));
    QVERIFY(token.startsWith(QStringLiteral("light-")));
}

void SmartLightsControllerTests::powerIntentReachesTheLightAndConfirms()
{
    Harness harness;
    harness.introduceDevice();

    QVERIFY(harness.controller.requestPower(harness.firstRowId(), false));
    QCOMPARE(harness.transport.unicastCount("setPilot"), 1);
    QVERIFY(harness.transport.unicasts.last().datagram.contains("\"state\":false"));
    QVERIFY(harness.controller.operationPending());

    harness.transport.deliver(deviceAddress, QByteArray(setPilotAck));
    QVERIFY(!harness.controller.operationPending());
    QVERIFY(!harness.controller.feedbackPresent());

    harness.transport.deliver(deviceAddress, pilotReply(false));
    QVERIFY(!harness.firstRow().value(QStringLiteral("on")).toBool());
}

void SmartLightsControllerTests::refusesControlWithoutTheGrant()
{
    Harness harness(false);
    harness.introduceDevice();

    const QVariantMap row = harness.firstRow();
    QVERIFY(!row.value(QStringLiteral("controllable")).toBool());
    QVERIFY(!harness.controller.controlAvailable());
    QVERIFY(!harness.controller.requestPower(
        row.value(QStringLiteral("deviceId")).toString(), true));
    QCOMPARE(harness.transport.unicastCount("setPilot"), 0);
    QVERIFY(harness.controller.feedbackPresent());
}

void SmartLightsControllerTests::offersOnlySceneOptionsTheLightCanRun()
{
    Harness harness;
    harness.introduceDevice();

    const QVariantList options = harness.controller.sceneOptions(harness.firstRowId());
    QVERIFY(options.size() > 20);
    QCOMPARE(options.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Ocean"));
    QVERIFY(harness.controller.sceneOptions(QStringLiteral("light-404")).isEmpty());
}

void SmartLightsControllerTests::renamePersistsAndSurvivesReload()
{
    const QString path = [] {
        static QTemporaryDir shared;
        return QDir(shared.path()).filePath(QStringLiteral("smart-lights.json"));
    }();

    {
        ConfigurationStore store(path);
        FakeWizTransport transport;
        FakeWizClock clock;
        QindaQt::Wiz::WizClient client(&transport, &clock);
        client.setAutomaticPolling(false);
        SmartLightsAppletController controller(&client, &store, true, true);
        client.start();
        transport.deliver(deviceAddress, pilotReply());
        transport.deliver(deviceAddress, systemReply());
        transport.deliver(deviceAddress, modelReply());

        const QString token =
            controller.deviceRows().first().toMap().value(QStringLiteral("deviceId")).toString();
        QVERIFY(controller.requestRename(token, QStringLiteral("Reading lamp")));
        QCOMPARE(controller.deviceRows().first().toMap()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("Reading lamp"));
    }

    // A new session adopts the stored name and the stored endpoint.
    ConfigurationStore reopened(path);
    FakeWizTransport transport;
    FakeWizClock clock;
    QindaQt::Wiz::WizClient client(&transport, &clock);
    client.setAutomaticPolling(false);
    client.start();
    SmartLightsAppletController controller(&client, &reopened, true, true);
    transport.deliver(deviceAddress, pilotReply());

    QCOMPARE(controller.deviceRows().first().toMap()
                 .value(QStringLiteral("label")).toString(),
             QStringLiteral("Reading lamp"));
}

void SmartLightsControllerTests::savedPresetCapturesLiveStateAndReapplies()
{
    Harness harness;
    harness.introduceDevice();
    harness.transport.deliver(deviceAddress, pilotReply(true, 35));

    QVERIFY(harness.controller.savePreset(QStringLiteral("Evening")));
    QCOMPARE(harness.controller.presetRows().size(), 1);
    const QString presetId = harness.controller.presetRows().first().toMap()
                                 .value(QStringLiteral("presetId")).toString();
    QCOMPARE(presetId, QStringLiteral("evening"));

    harness.transport.unicasts.clear();
    QVERIFY(harness.controller.applyPreset(presetId));
    QCOMPARE(harness.transport.unicastCount("setPilot"), 1);
    const QByteArray sent = harness.transport.unicasts.last().datagram;
    QVERIFY(sent.contains("\"dimming\":35"));
    QVERIFY(sent.contains("\"state\":true"));

    QVERIFY(harness.controller.deletePreset(presetId));
    QVERIFY(harness.controller.presetRows().isEmpty());
}

void SmartLightsControllerTests::forgettingALightRemovesItFromStoredConfiguration()
{
    Harness harness;
    harness.introduceDevice();
    const QString token = harness.firstRowId();
    QVERIFY(harness.controller.requestRename(token, QStringLiteral("Reading lamp")));
    QVERIFY(harness.controller.savePreset(QStringLiteral("Evening")));

    QVERIFY(harness.controller.requestForget(token));
    QCOMPARE(harness.controller.deviceCount(), 0);
    // A preset whose only member is gone goes with it.
    QVERIFY(harness.controller.presetRows().isEmpty());

    const auto stored = harness.store.load();
    QVERIFY(stored.devices.isEmpty());
    QVERIFY(stored.presets.isEmpty());
}

void SmartLightsControllerTests::unknownRowTokenIsInert()
{
    Harness harness;
    harness.introduceDevice();
    QVERIFY(!harness.controller.requestPower(QStringLiteral("light-404"), true));
    QVERIFY(!harness.controller.requestBrightness(QStringLiteral(""), 50));
    QVERIFY(!harness.controller.requestRename(QStringLiteral("light-404"),
                                              QStringLiteral("Nope")));
    QCOMPARE(harness.transport.unicastCount("setPilot"), 0);
}

void SmartLightsControllerTests::reportsUncertaintyWhenALightGoesSilent()
{
    Harness harness;
    harness.introduceDevice();
    harness.client.setRequestTimeoutMilliseconds(1000);

    QVERIFY(harness.controller.requestBrightness(harness.firstRowId(), 20));
    for (int round = 0; round < 3; ++round) {
        harness.clock.now += 1500;
        harness.client.tick();
    }

    QVERIFY(harness.controller.feedbackPresent());
    QVERIFY(!harness.controller.operationPending());
    harness.controller.clearFeedback();
    QVERIFY(!harness.controller.feedbackPresent());
}

QTEST_MAIN(SmartLightsControllerTests)
#include "tst_smart_lights_applet_controller.moc"
