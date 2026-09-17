// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/smart_lights_applet/smart_lights_applet_presentation.h>

#include <QtTest/QtTest>

using namespace QindaQt::Shell::SmartLightsApplet;
using QindaQt::SmartLights::PresetMember;
using QindaQt::SmartLights::StoredPreset;

namespace
{

const QString firstMac = QStringLiteral("d8a011769356");
const QString secondMac = QStringLiteral("d8a011696c62");

[[nodiscard]] QindaQt::Wiz::Device colourLight(const QString &mac, const QString &label)
{
    QindaQt::Wiz::Device device;
    device.identity.mac = mac;
    device.identity.moduleName = QStringLiteral("ESP25_SHRGB_01");
    device.label = label;
    device.features |= QindaQt::Wiz::Feature::Power;
    device.features |= QindaQt::Wiz::Feature::Dimming;
    device.features |= QindaQt::Wiz::Feature::Color;
    device.features |= QindaQt::Wiz::Feature::ColorTemperature;
    device.features |= QindaQt::Wiz::Feature::Scenes;
    device.features |= QindaQt::Wiz::Feature::SceneSpeed;
    device.dimming = {1, 100};
    device.temperature = {2200, 6500};
    device.capabilitiesKnown = true;
    device.reachability = QindaQt::Wiz::Reachability::Online;
    device.pilotKnown = true;
    device.pilot.on = true;
    device.pilot.dimmingKnown = true;
    device.pilot.dimmingPercent = 60;
    return device;
}

[[nodiscard]] QindaQt::Wiz::Snapshot readySnapshot(QList<QindaQt::Wiz::Device> devices)
{
    QindaQt::Wiz::Snapshot snapshot;
    snapshot.availability = QindaQt::Wiz::Availability::Ready;
    snapshot.epoch = 3;
    snapshot.revision = 12;
    snapshot.devices = std::move(devices);
    return snapshot;
}

[[nodiscard]] QHash<QString, QString> rowIds()
{
    return {{firstMac, QStringLiteral("light-1")}, {secondMac, QStringLiteral("light-2")}};
}

} // namespace

class SmartLightsPresentationTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutReadGrantNothingIsProjected();
    void projectsCapabilitiesAsControlPermissions();
    void unreachableDeviceIsNotControllable();
    void offersOnlyPowerUntilCapabilitiesAreProven();
    void doesNotProjectDevicesWithoutARowToken();
    void describesSceneAndWhiteModes();
    void countsOnlyReachableLightsInTheSummary();
    void presetApplicabilityFollowsPresentDevices();
};

void SmartLightsPresentationTests::withoutReadGrantNothingIsProjected()
{
    const auto model = projectSmartLightsApplet(
        readySnapshot({colourLight(firstMac, QStringLiteral("Lamp"))}), {}, rowIds(),
        false, false);
    QCOMPARE(model.phase, ServicePhase::Unavailable);
    QVERIFY(model.devices.isEmpty());
    QVERIFY(!model.diagnostic.isEmpty());
}

void SmartLightsPresentationTests::projectsCapabilitiesAsControlPermissions()
{
    const auto model = projectSmartLightsApplet(
        readySnapshot({colourLight(firstMac, QStringLiteral("Lamp"))}), {}, rowIds(),
        true, true);
    QCOMPARE(model.phase, ServicePhase::Ready);
    QCOMPARE(model.devices.size(), 1);

    const DeviceRow &row = model.devices.first();
    QCOMPARE(row.id, QStringLiteral("light-1"));
    QVERIFY(row.controllable);
    QVERIFY(row.supportsDimming);
    QVERIFY(row.supportsColor);
    QVERIFY(row.supportsTemperature);
    QCOMPARE(row.minimumBrightnessPercent, 1);
    QCOMPARE(row.minimumKelvin, 2200);
    QCOMPARE(row.brightnessPercent, 60);
    // No colour reported, so no swatch is claimed as current.
    QVERIFY(row.colorHex.isEmpty());
}

void SmartLightsPresentationTests::unreachableDeviceIsNotControllable()
{
    QindaQt::Wiz::Device device = colourLight(firstMac, QStringLiteral("Lamp"));
    device.reachability = QindaQt::Wiz::Reachability::Unreachable;
    device.pilotKnown = false;

    const auto model =
        projectSmartLightsApplet(readySnapshot({device}), {}, rowIds(), true, true);
    const DeviceRow &row = model.devices.first();
    QVERIFY(!row.reachable);
    QVERIFY(!row.controllable);
    QCOMPARE(model.onCount, 0);
    QVERIFY(!row.statusLabel.isEmpty());
}

void SmartLightsPresentationTests::offersOnlyPowerUntilCapabilitiesAreProven()
{
    // A device that answered a pilot but has not yet reported its model config
    // still has inferred features. Offering their controls would render an
    // enabled slider the client refuses as "capabilities-unknown".
    QindaQt::Wiz::Device device = colourLight(firstMac, QStringLiteral("Lamp"));
    device.capabilitiesKnown = false;

    const auto model =
        projectSmartLightsApplet(readySnapshot({device}), {}, rowIds(), true, true);
    const DeviceRow &row = model.devices.first();
    QVERIFY(row.controllable);
    QVERIFY(!row.capabilitiesKnown);
    QVERIFY(!row.supportsDimming);
    QVERIFY(!row.supportsTemperature);
    QVERIFY(!row.supportsColor);
    QVERIFY(!row.supportsScenes);
    QVERIFY(!row.supportsSpeed);
}

void SmartLightsPresentationTests::doesNotProjectDevicesWithoutARowToken()
{
    // A device the controller has not tokenized cannot be addressed, so
    // showing its controls would offer a dead row.
    const auto model = projectSmartLightsApplet(
        readySnapshot({colourLight(firstMac, QStringLiteral("Lamp"))}), {}, {}, true,
        true);
    QVERIFY(model.devices.isEmpty());
}

void SmartLightsPresentationTests::describesSceneAndWhiteModes()
{
    QindaQt::Wiz::Device scene = colourLight(firstMac, QStringLiteral("Lamp"));
    scene.pilot.sceneKnown = true;
    scene.pilot.sceneId = 1; // Ocean, an animated programme.
    scene.pilot.speedKnown = true;
    scene.pilot.speedPercent = 120;

    auto model = projectSmartLightsApplet(readySnapshot({scene}), {}, rowIds(), true, true);
    QCOMPARE(model.devices.first().sceneName, QStringLiteral("Ocean"));
    QVERIFY(model.devices.first().speedApplies);
    QVERIFY(model.devices.first().statusLabel.contains(QStringLiteral("Ocean")));

    QindaQt::Wiz::Device white = colourLight(secondMac, QStringLiteral("Desk"));
    white.pilot.temperatureKnown = true;
    white.pilot.temperatureKelvin = 2700;
    model = projectSmartLightsApplet(readySnapshot({white}), {}, rowIds(), true, true);
    QVERIFY(model.devices.first().statusLabel.contains(QStringLiteral("2700")));
    QVERIFY(!model.devices.first().speedApplies);
}

void SmartLightsPresentationTests::countsOnlyReachableLightsInTheSummary()
{
    QindaQt::Wiz::Device on = colourLight(firstMac, QStringLiteral("Lamp"));
    QindaQt::Wiz::Device off = colourLight(secondMac, QStringLiteral("Desk"));
    off.pilot.on = false;

    const auto model =
        projectSmartLightsApplet(readySnapshot({on, off}), {}, rowIds(), true, true);
    QCOMPARE(model.devices.size(), 2);
    QCOMPARE(model.reachableCount, 2);
    QCOMPARE(model.onCount, 1);
    QVERIFY(model.summaryLabel.contains(QStringLiteral("1")));
}

void SmartLightsPresentationTests::presetApplicabilityFollowsPresentDevices()
{
    PresetMember present;
    present.mac = firstMac;
    present.state.setPower = true;
    present.state.on = true;
    PresetMember absent;
    absent.mac = QStringLiteral("aabbccddeeff");
    absent.state = present.state;

    StoredPreset preset;
    preset.id = QStringLiteral("evening");
    preset.name = QStringLiteral("Evening");
    preset.members = {present, absent};

    auto model = projectSmartLightsApplet(
        readySnapshot({colourLight(firstMac, QStringLiteral("Lamp"))}), {preset},
        rowIds(), true, true);
    QCOMPARE(model.presets.size(), 1);
    QVERIFY(model.presets.first().applicable);
    QVERIFY(model.presets.first().summary.contains(QStringLiteral("1 of 2")));

    // Without the control grant nothing is applicable, even when present.
    model = projectSmartLightsApplet(
        readySnapshot({colourLight(firstMac, QStringLiteral("Lamp"))}), {preset},
        rowIds(), true, false);
    QVERIFY(!model.presets.first().applicable);
    QVERIFY(!model.devices.first().controllable);
}

QTEST_MAIN(SmartLightsPresentationTests)
#include "tst_smart_lights_applet_presentation.moc"
