// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/display_writer_test_support.h"

#include <QtTest/QTest>

#include <limits>

using namespace QindaQt;
using namespace QindaQt::DisplayWriter;
using namespace QindaQt::DisplayWriter::TestSupport;

class DisplayWriterMapperTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsCompleteConnectorTopology();
    void mapsFullPreimageAndEveryTopologyField();
    void rejectsUnsupportedIdentityAndMode();
    void rejectsMalformedTopology();
    void mapsSurvivingProperties();
    void rejectsMalformedSurvivors();
    void preservesDisabledCurrentMode();
    void rejectsMutatedMappedConfigurations();
};

void DisplayWriterMapperTests::mapsCompleteConnectorTopology()
{
    const MapResult mapped = mapApplyRequest(completeRequest(11, true), 22);
    QVERIFY(mapped.accepted());
    QCOMPARE(mapped.configuration.requestId, 22);
    QCOMPARE(mapped.configuration.scope, ConfigurationScope::CompleteTopology);
    QCOMPARE(mapped.configuration.outputs.size(), 2);
    QCOMPARE(mapped.configuration.outputs[0].connectorName, QStringLiteral("DP-1"));
    QCOMPARE(mapped.configuration.outputs[0].mode.pixelSize, QSize(1920, 1080));
    QCOMPARE(mapped.configuration.outputs[0].mode.refreshMilliHertz, 60'000);
    QVERIFY(mapped.configuration.outputs[0].primary);
    QCOMPARE(mapped.configuration.outputs[1].priority, 2);
}

void DisplayWriterMapperTests::mapsFullPreimageAndEveryTopologyField()
{
    auto request = completeRequest(12, true);
    request.scope = DisplayTransaction::ApplyScope::FullPreimage;
    request.candidate.outputs[0].scale = 1.25;
    request.candidate.outputs[0].transform = Display::Transform::Rotate90;
    request.candidate.outputs[1].position = {};
    request.candidate.outputs[1].scale = 1.25;
    request.candidate.outputs[1].transform = Display::Transform::FlipX270;
    request.candidate.outputs[1].replicationSourceStableId =
        QStringLiteral("conn:DP-1");

    const MapResult mapped = mapApplyRequest(request, 23);
    QVERIFY(mapped.accepted());
    QCOMPARE(mapped.configuration.requestId, 23);
    QCOMPARE(mapped.configuration.scope, ConfigurationScope::CompleteTopology);
    QCOMPARE(mapped.configuration.outputs.size(), 2);

    const OutputChange &primary = mapped.configuration.outputs[0];
    QCOMPARE(primary.connectorName, QStringLiteral("DP-1"));
    QVERIFY(primary.enabled);
    QVERIFY(primary.primary);
    QCOMPARE(primary.mode.pixelSize, QSize(1920, 1080));
    QCOMPARE(primary.mode.refreshMilliHertz, 60'000);
    QCOMPARE(primary.position, QPoint{});
    QCOMPARE(primary.scale, 1.25);
    QCOMPARE(primary.transform, Display::Transform::Rotate90);
    QCOMPARE(primary.priority, 1);
    QVERIFY(primary.replicationSourceConnector.isEmpty());

    const OutputChange &replica = mapped.configuration.outputs[1];
    QCOMPARE(replica.connectorName, QStringLiteral("DP-2"));
    QVERIFY(replica.enabled);
    QVERIFY(!replica.primary);
    QCOMPARE(replica.mode.pixelSize, QSize(1920, 1080));
    QCOMPARE(replica.mode.refreshMilliHertz, 60'000);
    QCOMPARE(replica.position, QPoint{});
    QCOMPARE(replica.scale, 1.25);
    QCOMPARE(replica.transform, Display::Transform::FlipX270);
    QCOMPARE(replica.priority, 2);
    QCOMPARE(replica.replicationSourceConnector, QStringLiteral("DP-1"));
}

void DisplayWriterMapperTests::rejectsUnsupportedIdentityAndMode()
{
    auto request = completeRequest();
    request.candidate.outputs[0].stableId = QStringLiteral("edid:0123");
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::UnsupportedIdentity);

    request = completeRequest();
    request.candidate.outputs[0].modeId = QStringLiteral("opaque-mode-id");
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::UnsupportedMode);

    request = completeRequest();
    request.token = 0;
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::InvalidRequest);
}

void DisplayWriterMapperTests::rejectsMalformedTopology()
{
    auto request = completeRequest(11, true);
    request.candidate.outputs[1].primary = true;
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::InvalidTopology);

    request = completeRequest(11, true);
    request.candidate.outputs[1].priority = 1;
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::InvalidTopology);

    request = completeRequest(11, true);
    request.candidate.outputs[0].replicationSourceStableId = QStringLiteral("conn:DP-2");
    request.candidate.outputs[1].replicationSourceStableId = QStringLiteral("conn:DP-1");
    QCOMPARE(mapApplyRequest(request, 1).error, MapError::InvalidTopology);
}

void DisplayWriterMapperTests::mapsSurvivingProperties()
{
    DisplayTransaction::ApplyRequest request{
        .token = 77,
        .scope = DisplayTransaction::ApplyScope::SurvivingOutputProperties,
        .candidate = {},
        .survivingProperties = {
            {.stableId = QStringLiteral("conn:DP-1"),
             .modeId = QStringLiteral("current:1920x1080@60000"),
             .scale = 1.25,
             .transform = Display::Transform::Rotate90},
        }};
    const MapResult mapped = mapApplyRequest(request, 9);
    QVERIFY(mapped.accepted());
    QCOMPARE(mapped.configuration.scope,
             ConfigurationScope::SurvivingProperties);
    QCOMPARE(mapped.configuration.outputs.size(), 1);
    QCOMPARE(mapped.configuration.outputs[0].scale, 1.25);
    QCOMPARE(mapped.configuration.outputs[0].transform,
             Display::Transform::Rotate90);
}

void DisplayWriterMapperTests::rejectsMalformedSurvivors()
{
    DisplayTransaction::ApplyRequest request{
        .token = 77,
        .scope = DisplayTransaction::ApplyScope::SurvivingOutputProperties,
        .candidate = {},
        .survivingProperties = {}};
    QCOMPARE(mapApplyRequest(request, 9).error, MapError::InvalidRequest);

    request.survivingProperties.push_back(
        {.stableId = QStringLiteral("conn:DP-1"),
         .modeId = QStringLiteral("current:99999x1080@60000"),
         .scale = 1.0,
         .transform = Display::Transform::Normal});
    QCOMPARE(mapApplyRequest(request, 9).error, MapError::UnsupportedMode);
}

void DisplayWriterMapperTests::preservesDisabledCurrentMode()
{
    auto request = completeRequest(88, true);
    auto &disabled = request.candidate.outputs[1];
    disabled.enabled = false;
    disabled.primary = false;
    disabled.position = {};
    disabled.priority = 0;
    disabled.replicationSourceStableId.clear();

    const MapResult mapped = mapApplyRequest(request, 12);
    QVERIFY(mapped.accepted());
    QVERIFY(!mapped.configuration.outputs[1].enabled);
    QCOMPARE(mapped.configuration.outputs[1].mode.pixelSize, QSize(1920, 1080));
    QCOMPARE(mapped.configuration.outputs[1].mode.refreshMilliHertz, 60'000);
}

void DisplayWriterMapperTests::rejectsMutatedMappedConfigurations()
{
    const Configuration baseline =
        mapApplyRequest(completeRequest(90, true), 13).configuration;
    QVERIFY(validateConfiguration(baseline));

    auto mutation = baseline;
    mutation.outputs[0].connectorName.clear();
    QVERIFY(!validateConfiguration(mutation));

    mutation = baseline;
    mutation.outputs[1].connectorName = mutation.outputs[0].connectorName;
    QVERIFY(!validateConfiguration(mutation));

    mutation = baseline;
    mutation.outputs[0].scale = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!validateConfiguration(mutation));

    mutation = baseline;
    mutation.outputs[0].transform = static_cast<Display::Transform>(255);
    QVERIFY(!validateConfiguration(mutation));

    mutation = baseline;
    mutation.outputs[1].priority = 3;
    QVERIFY(!validateConfiguration(mutation));

    mutation = baseline;
    mutation.outputs[0].replicationSourceConnector =
        mutation.outputs[0].connectorName;
    QVERIFY(!validateConfiguration(mutation));
}

QTEST_GUILESS_MAIN(DisplayWriterMapperTests)
#include "tst_display_writer_mapper.moc"
