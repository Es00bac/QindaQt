// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/display_inventory.h>
#include <qindaqt/services/display_service/display_service_model.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_service_test_support.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

// Captured from a live KWin session after hot-plugging a 1920x1200 monitor
// into a 1920x1080 laptop: KWin restored a saved setup in which eDP-1 mirrors
// DP-1, reporting the source's geometry and a fitted 0.9 scale. Before the
// mirror members existed this frame withdrew Display1 entirely.
QJsonObject capturedExternal()
{
    return QJsonDocument::fromJson(R"({
        "enabled": true, "geometry": {"height": 1200, "width": 1920, "x": 0, "y": 0},
        "internal": false, "manufacturer": "Invalid Vendor Codename - RTK",
        "model": "Dopesplay", "name": "DP-1",
        "physicalSizeMm": {"height": 270, "width": 480}, "priority": 1,
        "refreshRateMilliHz": 59885, "scale": 1, "transform": "normal",
        "uuid": "fe84583a-2771-4024-9b57-1fd7ce927c49"})")
        .object();
}

QJsonObject capturedPanel()
{
    return QJsonDocument::fromJson(R"({
        "enabled": true, "geometry": {"height": 1200, "width": 1920, "x": 0, "y": 0},
        "internal": true, "manufacturer": "Lenovo Group Limited",
        "model": "eDP-1-0x9052", "name": "eDP-1",
        "physicalSizeMm": {"height": 194, "width": 344}, "priority": 2,
        "refreshRateMilliHz": 60000, "scale": 0.9, "transform": "normal",
        "uuid": "fdfdd4df-852d-4499-b3ae-168d0de9a0e5"})")
        .object();
}

QJsonArray modes(std::initializer_list<std::tuple<int, int, int, bool>> values)
{
    QJsonArray array;
    for (const auto &[width, height, refresh, preferred] : values) {
        array.append(QJsonObject{{QStringLiteral("width"), width},
                                 {QStringLiteral("height"), height},
                                 {QStringLiteral("refreshRateMilliHz"), refresh},
                                 {QStringLiteral("preferred"), preferred}});
    }
    return array;
}

QJsonObject withMirrorMembers(QJsonObject output, QSize modeSize, QJsonArray advertised,
                              QString replicationSource)
{
    output[QStringLiteral("modeSize")] = QJsonObject{
        {QStringLiteral("width"), modeSize.width()},
        {QStringLiteral("height"), modeSize.height()}};
    output[QStringLiteral("modes")] = advertised;
    output[QStringLiteral("replicationSource")] = replicationSource;
    return output;
}

QByteArray body(const QJsonArray &outputs, quint64 generation = 2)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                                     {QStringLiteral("schemaVersion"), 1},
                                     {QStringLiteral("outputGeneration"),
                                      QString::number(generation)},
                                     {QStringLiteral("outputs"), outputs}})
        .toJson(QJsonDocument::Compact);
}

QByteArray mirroredBody()
{
    return body({withMirrorMembers(capturedExternal(), QSize(1920, 1200),
                                   modes({{1920, 1200, 59885, true},
                                          {1920, 1080, 60000, false},
                                          {1280, 720, 60000, false}}),
                                   {}),
                 withMirrorMembers(capturedPanel(), QSize(1920, 1080),
                                   modes({{1920, 1080, 60000, true},
                                          {1680, 1050, 60000, false}}),
                                   QStringLiteral("DP-1"))});
}

Display::Output byConnector(const Display::Snapshot &snapshot, const QString &name)
{
    for (const Display::Output &output : snapshot.outputs) {
        if (output.connectorName == name) {
            return output;
        }
    }
    qFatal("missing output");
}

} // namespace

class DisplayInventoryMirroringTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void legacyMirroredFrameIsRejected();
    void projectsMirroredOutputWithRealMode();
    void mirroredInventoryKeepsServiceAvailableAndCanExtend();
    void projectsAdvertisedModesKeepingCurrent();
    void rejectsMalformedMirrorAndModeMembers();
};

void DisplayInventoryMirroringTest::legacyMirroredFrameIsRejected()
{
    // Pins the failure the mirror members exist to prevent: without them a
    // mirror is indistinguishable from an out-of-range extended output.
    const InventoryDecodeResult decoded = decodeCompositorInventory(
        body({capturedExternal(), capturedPanel()}), QStringLiteral(":1.42"));
    QVERIFY(!decoded.accepted());
}

void DisplayInventoryMirroringTest::projectsMirroredOutputWithRealMode()
{
    const InventoryDecodeResult decoded =
        decodeCompositorInventory(mirroredBody(), QStringLiteral(":1.42"));
    QVERIFY2(decoded.accepted(), qPrintable(decoded.reasonCode));
    const InventoryProjectionResult projected =
        projectInventory(decoded.frame, QStringLiteral("epoch-mirror"));
    QVERIFY2(projected.accepted(), qPrintable(projected.reasonCode));
    QVERIFY(Display::validateSnapshot(projected.snapshot).accepted);

    const Display::Output external = byConnector(projected.snapshot, QStringLiteral("DP-1"));
    QVERIFY(external.primary);
    QVERIFY(external.replicationSourceStableId.isEmpty());
    QCOMPARE(external.modeId, QStringLiteral("current:1920x1200@59885"));
    QCOMPARE(external.modes.size(), 3);
    QCOMPARE(external.logicalSize, QSize(1920, 1200));

    const Display::Output panel = byConnector(projected.snapshot, QStringLiteral("eDP-1"));
    QCOMPARE(panel.replicationSourceStableId, external.stableId);
    QCOMPARE(panel.modeId, QStringLiteral("current:1920x1080@60000"));
    QCOMPARE(panel.scale, Display::kMinimumScale);
    QCOMPARE(panel.position, external.position);
    QCOMPARE(panel.logicalSize, QSize(1920, 1080));
    QCOMPARE(panel.modes.size(), 2);
    QVERIFY(panel.modes.constFirst().preferred);
}

void DisplayInventoryMirroringTest::mirroredInventoryKeepsServiceAvailableAndCanExtend()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("epoch-a"); });
    const InventoryDecodeResult decoded =
        decodeCompositorInventory(mirroredBody(), QStringLiteral(":1.42"));
    QVERIFY(decoded.accepted());
    const InventoryObservationResult observed = model.observeInventory(decoded.frame);
    QVERIFY2(observed.status == InventoryObservationStatus::AcceptedNewLineage,
             qPrintable(observed.reasonCode));
    QVERIFY(model.available());

    // Settings' Extend: clear the mirror and place the panel beside the monitor.
    Display::Candidate extend = DisplayTopology::candidateFromSnapshot(*model.snapshot());
    for (Display::CandidateOutput &output : extend.outputs) {
        if (output.stableId == QStringLiteral("conn:eDP-1")) {
            output.replicationSourceStableId.clear();
            output.position = QPoint(1920, 0);
        }
    }
    const DisplayTopology::ValidationResult validation =
        DisplayTopology::validateAndNormalize(*model.snapshot(), extend);
    QVERIFY2(validation.accepted(), qPrintable(validation.reasonCode));
    QVERIFY(!validation.noOp);
    const ServiceOperationResult staged = model.stage(QStringLiteral("tx-extend"), extend);
    QVERIFY(staged.available);
    QVERIFY(staged.command.accepted);

    // A different advertised resolution is now a stageable choice as well.
    Display::Candidate resize = DisplayTopology::candidateFromSnapshot(*model.snapshot());
    for (Display::CandidateOutput &output : resize.outputs) {
        if (output.stableId == QStringLiteral("conn:DP-1")) {
            output.modeId = QStringLiteral("current:1920x1080@60000");
        }
    }
    QVERIFY(DisplayTopology::validateAndNormalize(*model.snapshot(), resize).accepted());
}

void DisplayInventoryMirroringTest::projectsAdvertisedModesKeepingCurrent()
{
    InventoryOutput value = output(QStringLiteral("DP-2"));
    value.modePixelSize = QSize(1920, 1080);
    value.modes = {{QSize(2560, 1440), 60'000, true}, {QSize(1280, 720), 60'000, false}};
    const InventoryProjectionResult projected =
        projectInventory(frame(3, {value}), QStringLiteral("epoch-modes"));
    QVERIFY2(projected.accepted(), qPrintable(projected.reasonCode));
    const Display::Output &result = projected.snapshot.outputs.constFirst();
    QCOMPARE(result.modes.size(), 3);
    QCOMPARE(result.modes.at(0).id, QStringLiteral("current:2560x1440@60000"));
    QVERIFY(result.modes.at(0).preferred);
    QCOMPARE(result.modes.at(2).id, result.modeId);
    QVERIFY(!result.modes.at(2).preferred);

    // A full list loses its last entry, never the current mode.
    value.modes.clear();
    for (int index = 0; index < Display::kMaxModesPerOutput; ++index) {
        value.modes.push_back({QSize(640 + index, 480), 60'000, false});
    }
    const InventoryProjectionResult full =
        projectInventory(frame(4, {value}), QStringLiteral("epoch-modes"));
    QVERIFY2(full.accepted(), qPrintable(full.reasonCode));
    QCOMPARE(full.snapshot.outputs.constFirst().modes.size(), Display::kMaxModesPerOutput);
    QCOMPARE(full.snapshot.outputs.constFirst().modes.constLast().id,
             full.snapshot.outputs.constFirst().modeId);

    // A modeSize that does not reproduce an extended output's geometry is
    // ignored in favour of the geometry-derived mode rather than rejected.
    value.modes.clear();
    value.modePixelSize = QSize(1921, 1080);
    const InventoryProjectionResult mismatched =
        projectInventory(frame(5, {value}), QStringLiteral("epoch-modes"));
    QVERIFY2(mismatched.accepted(), qPrintable(mismatched.reasonCode));
    QCOMPARE(mismatched.snapshot.outputs.constFirst().modeId,
             QStringLiteral("current:1920x1080@60000"));
}

void DisplayInventoryMirroringTest::rejectsMalformedMirrorAndModeMembers()
{
    const auto decode = [](const QJsonArray &outputs) {
        return decodeCompositorInventory(body(outputs), QStringLiteral(":1.42"));
    };
    const QJsonObject external = capturedExternal();
    QJsonObject panel = capturedPanel();

    panel[QStringLiteral("replicationSource")] = QStringLiteral("HDMI-A-9");
    QCOMPARE(decode({external, panel}).reasonCode,
             QStringLiteral("unknown-replication-source"));
    panel[QStringLiteral("replicationSource")] = QStringLiteral("eDP-1");
    QVERIFY(!decode({external, panel}).accepted());

    panel = capturedPanel();
    panel[QStringLiteral("scale")] = 1;
    panel[QStringLiteral("modes")] = modes({{1920, 1080, 60000, true}, {1920, 1080, 60000, false}});
    QVERIFY(!decode({external, panel}).accepted());
    panel[QStringLiteral("modes")] = modes({{0, 1080, 60000, true}});
    QVERIFY(!decode({external, panel}).accepted());
    panel[QStringLiteral("modes")] = QJsonArray{};
    panel[QStringLiteral("modeSize")] = QJsonObject{{QStringLiteral("width"), 1920},
                                                    {QStringLiteral("height"), 0}};
    QVERIFY(!decode({external, panel}).accepted());

    // A mirror whose source is itself mirroring or disabled cannot be projected.
    QJsonObject chained = capturedExternal();
    chained[QStringLiteral("replicationSource")] = QStringLiteral("eDP-1");
    panel = capturedPanel();
    panel[QStringLiteral("replicationSource")] = QStringLiteral("DP-1");
    const InventoryDecodeResult cycle = decode({chained, panel});
    QVERIFY(cycle.accepted());
    QVERIFY(!projectInventory(cycle.frame, QStringLiteral("epoch-x")).accepted());
}

QTEST_MAIN(DisplayInventoryMirroringTest)

#include "tst_display_inventory_mirroring.moc"
