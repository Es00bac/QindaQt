// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_service/display_service_model.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_brightness_test_support.h"
#include "support/display_service_test_support.h"

#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;
using namespace QindaQt::DisplayService::TestSupport::Brightness;
using Display::ErrorCode;
using Display::OperationStatus;
using DisplayTransaction::SafetyState;

// Composition rows: the brightness authority inside the real D2 model and D1
// machine, fed by complete inventory frames.
class DisplayServiceBrightnessModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void joinsInventoryAndClearsOnTransportLoss();
    void classATransactionsAndBrightnessExcludeEachOther();
};

void DisplayServiceBrightnessModelTest::joinsInventoryAndClearsOnTransportLoss()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("brightness-seed"); });
    (void)model.safetyChanged(SafetyState::Safe);
    QVERIFY(model.brightnessSnapshot() == nullptr);

    InventoryOutput external = output(QStringLiteral("DP-1"));
    external.runtimeCompositorUuid = QStringLiteral("uuid-dp");
    InventoryOutput internal = output(QStringLiteral("eDP-1"), QRect(1920, 0, 1920, 1080));
    internal.runtimeCompositorUuid = QStringLiteral("uuid-edp");
    internal.internal = true;
    QVERIFY(model.observeInventory(frame(1, {external, internal})).accepted());
    model.brightnessDevicesObserved(devices());
    const Display::Snapshot topologyNow = *model.snapshot();
    const Display::BrightnessSnapshot *published = model.brightnessSnapshot();
    QVERIFY(published != nullptr);
    QVERIFY(Display::validateBrightnessJoin(topologyNow, *published).accepted);
    const QString externalId = stableIdFor(topologyNow, QStringLiteral("DP-1"));
    for (const Display::OutputBrightness &row : published->outputs) {
        QCOMPARE(row.value, row.stableId == externalId ? quint32{6'000} : quint32{3'000});
    }

    QVERIFY(model.transportLost());
    QVERIFY(model.brightnessSnapshot() == nullptr);
    QVERIFY(model.takeBrightnessPublicationChanged());

    const QString owner = QStringLiteral(":1.77");
    QVERIFY(model.observeInventory(frame(1, {external, internal}, owner)).accepted());
    published = model.brightnessSnapshot();
    QVERIFY(published != nullptr);
    QCOMPARE(published->revision, quint64{1});
    QCOMPARE(published->serviceEpoch, model.snapshot()->serviceEpoch);

    const BrightnessRequestResult accepted = model.setOutputBrightness(
        requestFor(*published, stableIdFor(*model.snapshot(), QStringLiteral("DP-1")), 2'500));
    QVERIFY(accepted.available);
    QVERIFY(!accepted.final);
    InventoryOutput renamed = internal;
    renamed.model = QStringLiteral("Renamed Display");
    QVERIFY(model.observeInventory(frame(2, {external, renamed}, owner)).accepted());
    const QList<BrightnessFinish> finishes = model.takeBrightnessFinishes();
    QCOMPARE(finishes.size(), 1);
    QINDAQT_VERIFY_IMMEDIATE(finishes.at(0).operation, OperationStatus::Uncertain,
                             ErrorCode::TopologyChanged, "topology-changed");
    QCOMPARE(port.brightnessRequests.size(), 1);
}

void DisplayServiceBrightnessModelTest::classATransactionsAndBrightnessExcludeEachOther()
{
    FakeClock clock;
    FakeTransactionPort port;
    DisplayServiceModel model(clock, port, [] { return QStringLiteral("brightness-seed"); });
    (void)model.safetyChanged(SafetyState::Safe);
    InventoryOutput external = output(QStringLiteral("DP-1"));
    external.runtimeCompositorUuid = QStringLiteral("uuid-dp");
    QVERIFY(model.observeInventory(frame(1, {external})).accepted());
    DeviceBrightnessFrame single{
        .ownerGeneration = 5,
        .devices = {device(QStringLiteral("DP-1"), QStringLiteral("uuid-dp"), 6'000)}};
    model.brightnessDevicesObserved(single);
    const QString stableId = model.snapshot()->outputs.at(0).stableId;

    const BrightnessRequestResult pending =
        model.setOutputBrightness(requestFor(*model.brightnessSnapshot(), stableId, 2'500));
    QVERIFY(!pending.final);

    Display::Candidate candidate = DisplayTopology::candidateFromSnapshot(*model.snapshot());
    candidate.outputs[0].transform = Display::Transform::Rotate180;
    const ServiceOperationResult staged = model.stage(QStringLiteral("rotate"), candidate);
    QVERIFY2(staged.operation.status == OperationStatus::Accepted,
             qPrintable(staged.operation.diagnostic));
    const ServiceOperationResult refused = model.preview(QStringLiteral("rotate"));
    QCOMPARE(refused.operation.status, OperationStatus::Rejected);
    QCOMPARE(refused.operation.error, ErrorCode::TransactionActive);
    QVERIFY(port.applyRequests.isEmpty());

    model.brightnessCompleted(pending.requestId, BrightnessApplyOutcome::Applied);
    single.devices[0].value = 2'500;
    model.brightnessDevicesObserved(single);
    const QList<BrightnessFinish> finishes = model.takeBrightnessFinishes();
    QCOMPARE(finishes.size(), 1);
    QCOMPARE(finishes.at(0).operation.status, OperationStatus::Succeeded);

    const ServiceOperationResult previewed = model.preview(QStringLiteral("rotate"));
    QVERIFY2(previewed.operation.status == OperationStatus::Accepted,
             qPrintable(previewed.operation.diagnostic));
    QCOMPARE(port.applyRequests.size(), 1);

    const BrightnessRequestResult duringApply =
        model.setOutputBrightness(requestFor(*model.brightnessSnapshot(), stableId, 1'000));
    QVERIFY(duringApply.final);
    QINDAQT_VERIFY_IMMEDIATE(duringApply.operation, OperationStatus::Rejected,
                             ErrorCode::TransactionActive, "transaction-active");
    QCOMPARE(port.brightnessRequests.size(), 1);
}

QTEST_GUILESS_MAIN(DisplayServiceBrightnessModelTest)
#include "tst_display_service_brightness_model.moc"
