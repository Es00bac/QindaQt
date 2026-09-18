// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/wiz_model/wiz_model.h>
#include <qindaqt/services/wiz_protocol/wiz_messages.h>

#include <QtTest/QtTest>

using namespace QindaQt::Wiz;

namespace
{

[[nodiscard]] DecodedMessage pilot(const QString &mac, const bool on,
                                   const quint8 dimming = 50)
{
    DecodedMessage message;
    message.method = Method::GetPilot;
    message.mac = mac;
    message.pilotKnown = true;
    message.pilot.on = on;
    message.pilot.dimmingKnown = true;
    message.pilot.dimmingPercent = dimming;
    return message;
}

[[nodiscard]] DecodedMessage systemConfig(const QString &mac, const QString &module)
{
    DecodedMessage message;
    message.method = Method::GetSystemConfig;
    message.mac = mac;
    message.systemConfigKnown = true;
    message.systemConfig.mac = mac;
    message.systemConfig.moduleName = module;
    message.systemConfig.firmwareVersion = QStringLiteral("1.38.0");
    return message;
}

[[nodiscard]] DecodedMessage modelConfig(const QString &mac)
{
    DecodedMessage message;
    message.method = Method::GetModelConfig;
    message.mac = mac;
    message.modelConfigKnown = true;
    message.modelConfig.temperatureRangeKnown = true;
    message.modelConfig.minimumKelvin = 2200;
    message.modelConfig.maximumKelvin = 6500;
    message.modelConfig.dimmingFloorKnown = true;
    message.modelConfig.minimumDimmingPercent = 1;
    return message;
}

const QString firstMac = QStringLiteral("d8a011769356");
const QString secondMac = QStringLiteral("d8a011696c62");

} // namespace

class WizModelTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresDatagramsWithoutIdentity();
    void admitsDeviceAndDerivesLabel();
    void advancesRevisionOnlyOnRealChange();
    void keepsCapabilitiesUnknownUntilModelConfigArrives();
    void attributesMacLessRepliesToTheKnownDeviceAtThatAddress();
    void refusesMacLessRepliesWhenTheAddressIsAmbiguous();
    void walksTheReachabilityLadder();
    void followsAddressChanges();
    void neverLearnsTheControlPortFromAPush();
    void ordersDevicesDeterministically();
    void storedLabelWinsOverDerivedName();
    void newEpochDropsInventoryButKeepsLabels();
    void refusesUnboundedInventory();
};

void WizModelTests::ignoresDatagramsWithoutIdentity()
{
    WizModel model;
    model.start();
    DecodedMessage anonymous;
    anonymous.method = Method::GetPilot;
    anonymous.pilotKnown = true;
    QVERIFY(!model.observe(QStringLiteral("10.0.0.5"), 38899, anonymous));
    QVERIFY(model.snapshot().devices.isEmpty());
}

void WizModelTests::admitsDeviceAndDerivesLabel()
{
    WizModel model;
    model.start();
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 38899, pilot(firstMac, true)));
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 38899,
                          systemConfig(firstMac, QStringLiteral("ESP25_SHRGB_01"))));

    const Snapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.devices.size(), 1);
    const Device &device = snapshot.devices.first();
    QCOMPARE(device.identity.address, QStringLiteral("10.0.0.234"));
    QCOMPARE(device.reachability, Reachability::Online);
    QVERIFY(device.pilotKnown);
    QCOMPARE(device.label, QStringLiteral("Colour light 769356"));
    QVERIFY(!device.labelStored);
}

void WizModelTests::advancesRevisionOnlyOnRealChange()
{
    WizModel model;
    model.start();
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 38899, pilot(firstMac, true)));
    const quint64 revision = model.revision();
    // AGENT-GUARD in WizModel: an identical poll answer must not advance the
    // revision, because the applet treats a new revision as fresh truth.
    QVERIFY(!model.observe(QStringLiteral("10.0.0.234"), 38899, pilot(firstMac, true)));
    QCOMPARE(model.revision(), revision);
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 38899, pilot(firstMac, false)));
    QVERIFY(model.revision() > revision);
}

void WizModelTests::keepsCapabilitiesUnknownUntilModelConfigArrives()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    QVERIFY(!model.device(firstMac)->capabilitiesKnown);

    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    systemConfig(firstMac,
                                                 QStringLiteral("ESP25_SHRGB_01"))));
    QVERIFY(!model.device(firstMac)->capabilitiesKnown);

    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    modelConfig(firstMac)));
    const auto device = model.device(firstMac);
    QVERIFY(device->capabilitiesKnown);
    QVERIFY(device->features.testFlag(Feature::Color));
    QCOMPARE(device->temperature.minimumKelvin, 2200);
}

void WizModelTests::attributesMacLessRepliesToTheKnownDeviceAtThatAddress()
{
    // Firmware 1.38.0 answers getModelConfig without a mac member. Dropping
    // such a reply would leave capabilities permanently unknown.
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    systemConfig(firstMac,
                                                 QStringLiteral("ESP25_SHRGB_01"))));
    DecodedMessage anonymousModel = modelConfig(firstMac);
    anonymousModel.mac.clear();
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 38899, anonymousModel));

    const auto device = model.device(firstMac);
    QVERIFY(device.has_value());
    QVERIFY(device->capabilitiesKnown);
    QCOMPARE(device->dimming.minimumPercent, 1);
}

void WizModelTests::refusesMacLessRepliesWhenTheAddressIsAmbiguous()
{
    WizModel model;
    model.start();
    // Nothing is known at this address yet, so the reply cannot be attributed
    // and must not conjure a device.
    DecodedMessage anonymousModel = modelConfig(firstMac);
    anonymousModel.mac.clear();
    QVERIFY(!model.observe(QStringLiteral("10.0.0.234"), 38899, anonymousModel));
    QVERIFY(model.snapshot().devices.isEmpty());

    // Two devices claiming one address is not a fact to guess from.
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(secondMac, true)));
    QVERIFY(!model.observe(QStringLiteral("10.0.0.234"), 38899, anonymousModel));
    QVERIFY(!model.device(firstMac)->capabilitiesKnown);
}

void WizModelTests::walksTheReachabilityLadder()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    QCOMPARE(model.device(firstMac)->reachability, Reachability::Online);

    static_cast<void>(model.noteMissedPoll(firstMac));
    QCOMPARE(model.device(firstMac)->reachability, Reachability::Online);
    static_cast<void>(model.noteMissedPoll(firstMac));
    QCOMPARE(model.device(firstMac)->reachability, Reachability::Stale);
    static_cast<void>(model.noteMissedPoll(firstMac));
    static_cast<void>(model.noteMissedPoll(firstMac));
    QCOMPARE(model.device(firstMac)->reachability, Reachability::Unreachable);
    // A silent light is no longer proof of its last pilot.
    QVERIFY(!model.device(firstMac)->pilotKnown);

    // The device is kept, so its stored settings survive the outage.
    QCOMPARE(model.snapshot().devices.size(), 1);

    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    QCOMPARE(model.device(firstMac)->reachability, Reachability::Online);
}

void WizModelTests::followsAddressChanges()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    QVERIFY(model.observe(QStringLiteral("10.0.0.99"), 38899, pilot(firstMac, true)));
    QCOMPARE(model.snapshot().devices.size(), 1);
    QCOMPARE(model.endpoint(firstMac)->address, QStringLiteral("10.0.0.99"));
}

void WizModelTests::neverLearnsTheControlPortFromAPush()
{
    // AGENT-GUARD in WizModel: firmware 1.38.0 pushes syncPilot from an
    // ephemeral source port. Adopting it sent every poll and control datagram
    // to a port the light never reads while the row kept looking live.
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.252"), 38899,
                                    pilot(secondMac, false)));
    QCOMPARE(model.endpoint(secondMac)->port, quint16{38899});

    DecodedMessage push = pilot(secondMac, true);
    push.method = Method::SyncPilot;
    push.unsolicited = true;
    QVERIFY(model.observe(QStringLiteral("10.0.0.252"), 51501, push));
    // The push is fresh truth about the light...
    QVERIFY(model.device(secondMac)->pilot.on);
    QCOMPARE(model.device(secondMac)->reachability, Reachability::Online);
    // ...but not about where to reach it.
    QCOMPARE(model.endpoint(secondMac)->port, quint16{38899});

    // A light first seen through a push has no endpoint port at all; the
    // client then falls back to the control port instead of guessing.
    DecodedMessage firstContact = pilot(firstMac, true);
    firstContact.method = Method::SyncPilot;
    firstContact.unsolicited = true;
    QVERIFY(model.observe(QStringLiteral("10.0.0.234"), 59321, firstContact));
    QCOMPARE(model.endpoint(firstMac)->port, quint16{0});
}

void WizModelTests::ordersDevicesDeterministically()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.252"), 38899,
                                    pilot(secondMac, true)));
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    static_cast<void>(model.applyStoredLabel(secondMac, QStringLiteral("Desk")));
    static_cast<void>(model.applyStoredLabel(firstMac, QStringLiteral("Ceiling")));

    const Snapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.devices.size(), 2);
    QCOMPARE(snapshot.devices.at(0).label, QStringLiteral("Ceiling"));
    QCOMPARE(snapshot.devices.at(1).label, QStringLiteral("Desk"));
}

void WizModelTests::storedLabelWinsOverDerivedName()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    systemConfig(firstMac,
                                                 QStringLiteral("ESP25_SHRGB_01"))));
    QVERIFY(model.applyStoredLabel(firstMac, QStringLiteral("Reading lamp")));
    QCOMPARE(model.device(firstMac)->label, QStringLiteral("Reading lamp"));
    QVERIFY(model.device(firstMac)->labelStored);

    // Clearing the label restores the device-derived name.
    QVERIFY(model.applyStoredLabel(firstMac, QString()));
    QCOMPARE(model.device(firstMac)->label, QStringLiteral("Colour light 769356"));
}

void WizModelTests::newEpochDropsInventoryButKeepsLabels()
{
    WizModel model;
    model.start();
    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    static_cast<void>(model.applyStoredLabel(firstMac, QStringLiteral("Reading lamp")));
    const quint64 epoch = model.epoch();

    model.start();
    QVERIFY(model.epoch() > epoch);
    QVERIFY(model.snapshot().devices.isEmpty());

    static_cast<void>(model.observe(QStringLiteral("10.0.0.234"), 38899,
                                    pilot(firstMac, true)));
    QCOMPARE(model.device(firstMac)->label, QStringLiteral("Reading lamp"));
}

void WizModelTests::refusesUnboundedInventory()
{
    WizModel model;
    model.start();
    for (int index = 0; index < 100; ++index) {
        const QString mac = QStringLiteral("d8a011%1").arg(index, 6, 16, QLatin1Char('0'));
        static_cast<void>(model.observe(QStringLiteral("10.0.0.5"), 38899,
                                        pilot(mac, true)));
    }
    QCOMPARE(model.snapshot().devices.size(), 64);
}

QTEST_MAIN(WizModelTests)
#include "tst_wiz_model.moc"
