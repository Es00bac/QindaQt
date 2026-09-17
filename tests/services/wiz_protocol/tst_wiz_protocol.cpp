// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/wiz_protocol/wiz_capabilities.h>
#include <qindaqt/services/wiz_protocol/wiz_messages.h>
#include <qindaqt/services/wiz_protocol/wiz_scenes.h>
#include <qindaqt/services/wiz_protocol/wiz_types.h>
#include <qindaqt/services/wiz_protocol/wiz_validation.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtTest/QtTest>

using namespace QindaQt::Wiz;

namespace
{

// Captured verbatim from an ESP25_SHRGB_01 luminaire on firmware 1.38.0.
constexpr auto pilotReply = R"({"method":"getPilot","env":"pro","result":{"mac":"d8a011769356","rssi":-59,"state":true,"sceneId":0,"r":0,"g":35,"b":255,"c":0,"w":48,"dimming":100}})";
constexpr auto modelReply = R"({"method":"getModelConfig","env":"pro","result":{"devTotal":1,"headTotal":1,"ps":3,"minDimLevel":1,"lightType":1,"cctRange":[2200,2700,6500,6500]}})";
constexpr auto systemReply = R"({"method":"getSystemConfig","env":"pro","result":{"mac":"d8a011769356","homeId":16201343,"roomId":27370616,"moduleName":"ESP25_SHRGB_01","fwVersion":"1.38.0"}})";
constexpr auto errorReply = R"({"method":"setPilot","env":"pro","error":{"code":-32602,"message":"Invalid params"}})";

[[nodiscard]] Device colourLight()
{
    Device device;
    device.identity.mac = QStringLiteral("d8a011769356");
    device.identity.moduleName = QStringLiteral("ESP25_SHRGB_01");
    const auto payload = decodeMessage(QByteArray(modelReply));
    const auto profile = inferCapabilities(device.identity.moduleName,
                                           payload->modelConfig);
    device.features = profile.features;
    device.dimming = profile.dimming;
    device.temperature = profile.temperature;
    device.capabilitiesKnown = true;
    device.reachability = Reachability::Online;
    device.pilotKnown = true;
    return device;
}

} // namespace

class WizProtocolTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void decodesPilotReply();
    void decodesErrorWithoutPayload();
    void refusesOversizedAndMalformedDatagrams();
    void refusesOutOfRangeNumbers();
    void keepsMissingFieldsUnknown();
    void infersCapabilitiesFromModuleNameAndModelConfig();
    void withholdsCapabilitiesUntilTheDeviceNamesItself();
    void encodesOnlyRequestedFields();
    void collapsesColourLanes();
    void sendsPowerOffAlone();
    void clampsToDeviceRanges();
    void refusesSpeedWithoutADynamicScene();
    void normalizesMacFormats();
    void scenesFollowCapabilities();
};

void WizProtocolTests::decodesPilotReply()
{
    const auto message = decodeMessage(QByteArray(pilotReply));
    QVERIFY(message.has_value());
    QCOMPARE(message->method, Method::GetPilot);
    QCOMPARE(message->mac, QStringLiteral("d8a011769356"));
    QVERIFY(message->pilotKnown);
    QVERIFY(message->pilot.on);
    QCOMPARE(message->pilot.dimmingPercent, quint8{100});
    QCOMPARE(message->pilot.blue, quint8{255});
    QCOMPARE(message->pilot.signalDbm, qint16{-59});
    QCOMPARE(message->pilot.mode(), LightMode::Color);
}

void WizProtocolTests::decodesErrorWithoutPayload()
{
    const auto message = decodeMessage(QByteArray(errorReply));
    QVERIFY(message.has_value());
    QVERIFY(message->hasError);
    QCOMPARE(message->errorCode, -32602);
    QVERIFY(!message->pilotKnown);
    QVERIFY(!message->acknowledged);
}

void WizProtocolTests::refusesOversizedAndMalformedDatagrams()
{
    QVERIFY(!decodeMessage(QByteArray()).has_value());
    QVERIFY(!decodeMessage(QByteArray("not json")).has_value());
    QVERIFY(!decodeMessage(QByteArray(R"({"result":{"mac":"aabbccddeeff"}})")).has_value());
    QVERIFY(!decodeMessage(QByteArray(8192, 'x')).has_value());
}

void WizProtocolTests::refusesOutOfRangeNumbers()
{
    // A hostile peer cannot push an impossible brightness or colour channel
    // into the model by answering a poll.
    const auto message = decodeMessage(QByteArray(
        R"({"method":"getPilot","result":{"mac":"d8a011769356","state":true,"dimming":4000,"r":9000,"g":0,"b":0}})"));
    QVERIFY(message.has_value());
    QVERIFY(!message->pilot.dimmingKnown);
    QVERIFY(!message->pilot.colorKnown);
}

void WizProtocolTests::keepsMissingFieldsUnknown()
{
    const auto message = decodeMessage(QByteArray(
        R"({"method":"getPilot","result":{"mac":"d8a011769356","state":false}})"));
    QVERIFY(message.has_value());
    QVERIFY(message->pilotKnown);
    QVERIFY(!message->pilot.on);
    QVERIFY(!message->pilot.dimmingKnown);
    QCOMPARE(message->pilot.dimmingPercent, quint8{0});
    QVERIFY(!message->pilot.temperatureKnown);
}

void WizProtocolTests::infersCapabilitiesFromModuleNameAndModelConfig()
{
    const auto model = decodeMessage(QByteArray(modelReply));
    const auto system = decodeMessage(QByteArray(systemReply));
    QVERIFY(model.has_value());
    QVERIFY(system.has_value());
    QCOMPARE(system->systemConfig.moduleName, QStringLiteral("ESP25_SHRGB_01"));

    const CapabilityProfile profile =
        inferCapabilities(system->systemConfig.moduleName, model->modelConfig);
    QVERIFY(profile.complete);
    QVERIFY(profile.features.testFlag(Feature::Color));
    QVERIFY(profile.features.testFlag(Feature::ColorTemperature));
    QVERIFY(profile.features.testFlag(Feature::Dimming));
    QVERIFY(profile.features.testFlag(Feature::Scenes));
    QVERIFY(profile.features.testFlag(Feature::SceneSpeed));
    QVERIFY(!profile.features.testFlag(Feature::DualHeadRatio));
    QCOMPARE(profile.temperature.minimumKelvin, 2200);
    QCOMPARE(profile.temperature.maximumKelvin, 6500);
    // The device reports a one-percent floor and is trusted about it.
    QCOMPARE(profile.dimming.minimumPercent, 1);

    const CapabilityProfile tunable =
        inferCapabilities(QStringLiteral("ESP56_SHTW3_01"), std::nullopt);
    QVERIFY(tunable.features.testFlag(Feature::ColorTemperature));
    QVERIFY(!tunable.features.testFlag(Feature::Color));
    // Without a model configuration the conservative vendor floor applies.
    QCOMPARE(tunable.dimming.minimumPercent, 10);
}

void WizProtocolTests::withholdsCapabilitiesUntilTheDeviceNamesItself()
{
    const CapabilityProfile unknown = inferCapabilities(QString(), std::nullopt);
    QVERIFY(unknown.features.testFlag(Feature::Power));
    QVERIFY(!unknown.features.testFlag(Feature::Dimming));
    QVERIFY(!unknown.features.testFlag(Feature::Color));
    QVERIFY(!unknown.complete);
}

void WizProtocolTests::encodesOnlyRequestedFields()
{
    StateRequest request;
    request.setDimming = true;
    request.dimmingPercent = 40;
    const QJsonObject params =
        QJsonDocument::fromJson(encodeSetPilot(request)).object()
            .value(QStringLiteral("params")).toObject();
    QCOMPARE(params.size(), 1);
    QCOMPARE(params.value(QStringLiteral("dimming")).toInt(), 40);
    QVERIFY(encodeSetPilot(StateRequest()).isEmpty());
}

void WizProtocolTests::collapsesColourLanes()
{
    const Device device = colourLight();
    StateRequest request;
    request.setScene = true;
    request.sceneId = 4;
    request.setColor = true;
    request.red = 255;
    request.setTemperature = true;
    request.temperatureKelvin = 3000;

    const ValidatedRequest validated = validateStateRequest(device, request);
    QVERIFY(validated.accepted());
    QVERIFY(validated.request.setScene);
    QVERIFY(!validated.request.setColor);
    QVERIFY(!validated.request.setTemperature);
}

void WizProtocolTests::sendsPowerOffAlone()
{
    const Device device = colourLight();
    StateRequest request;
    request.setPower = true;
    request.on = false;
    request.setDimming = true;
    request.dimmingPercent = 100;
    request.setColor = true;
    request.red = 255;

    const ValidatedRequest validated = validateStateRequest(device, request);
    QVERIFY(validated.accepted());
    QVERIFY(validated.request.setPower);
    QVERIFY(!validated.request.on);
    QVERIFY(!validated.request.setDimming);
    QVERIFY(!validated.request.setColor);
}

void WizProtocolTests::clampsToDeviceRanges()
{
    const Device device = colourLight();
    StateRequest request;
    request.setTemperature = true;
    request.temperatureKelvin = 9000;
    const ValidatedRequest validated = validateStateRequest(device, request);
    QVERIFY(validated.accepted());
    QCOMPARE(validated.request.temperatureKelvin, quint16{6500});

    StateRequest dim;
    dim.setDimming = true;
    dim.dimmingPercent = 0;
    const ValidatedRequest dimmed = validateStateRequest(device, dim);
    QVERIFY(dimmed.accepted());
    QCOMPARE(dimmed.request.dimmingPercent, quint8{1});
}

void WizProtocolTests::refusesSpeedWithoutADynamicScene()
{
    Device device = colourLight();
    device.pilot.sceneKnown = true;
    device.pilot.sceneId = 12; // Daylight: a fixed white point, not animated.

    StateRequest request;
    request.setSpeed = true;
    request.speedPercent = 150;
    const ValidatedRequest refused = validateStateRequest(device, request);
    QVERIFY(!refused.accepted());
    QCOMPARE(refused.outcome, ValidationOutcome::Unsupported);
    QCOMPARE(refused.reasonCode, QStringLiteral("scene-not-dynamic"));

    device.pilot.sceneId = 4; // Party animates.
    const ValidatedRequest accepted = validateStateRequest(device, request);
    QVERIFY(accepted.accepted());
    QCOMPARE(accepted.request.speedPercent, quint8{150});

    // Below the vendor floor the firmware answers "Invalid params", so the
    // floor is applied before anything is sent.
    request.speedPercent = 5;
    const ValidatedRequest floored = validateStateRequest(device, request);
    QVERIFY(floored.accepted());
    QCOMPARE(floored.request.speedPercent, quint8{10});
}

void WizProtocolTests::normalizesMacFormats()
{
    QCOMPARE(normalizeMac(QStringLiteral("D8:A0:11:76:93:56")),
             QStringLiteral("d8a011769356"));
    QCOMPARE(normalizeMac(QStringLiteral("d8a011769356")),
             QStringLiteral("d8a011769356"));
    QVERIFY(normalizeMac(QStringLiteral("nothex")).isEmpty());
    QVERIFY(normalizeMac(QStringLiteral("d8a01176935")).isEmpty());
    QVERIFY(normalizeMac(QStringLiteral("d8a0117693567")).isEmpty());
}

void WizProtocolTests::scenesFollowCapabilities()
{
    const Device device = colourLight();
    const auto colourScenes = scenesFor(device.features);
    QVERIFY(colourScenes.size() > 20);
    QVERIFY(sceneSupported(1, device.features));

    Features dimmableOnly;
    dimmableOnly |= Feature::Power;
    dimmableOnly |= Feature::Dimming;
    dimmableOnly |= Feature::Scenes;
    QVERIFY(!sceneSupported(1, dimmableOnly));
    QVERIFY(sceneSupported(14, dimmableOnly));
    QVERIFY(scenesFor(dimmableOnly).size() < colourScenes.size());

    const auto ocean = sceneById(1);
    QVERIFY(ocean.has_value());
    QVERIFY(ocean->dynamic);
    QVERIFY(!sceneById(999).has_value());
}

QTEST_MAIN(WizProtocolTests)
#include "tst_wiz_protocol.moc"
