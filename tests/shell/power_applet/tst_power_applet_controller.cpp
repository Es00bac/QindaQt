// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_applet_controller.h"

#include "support/fake_power_transport.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::PowerApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

QVariantMap rowWithId(const QVariantList &rows, const QString &key,
                      const QString &value)
{
    for (const QVariant &candidate : rows) {
        const QVariantMap row = candidate.toMap();
        if (row.value(key).toString() == value) {
            return row;
        }
    }
    return {};
}

void publishReady(Power::PowerClient &client, FakePowerTransport &transport,
                  const Power::Snapshot &snapshot = powerClientSnapshot())
{
    client.start();
    transport.announceOwner(kOwner);
    QVERIFY(!transport.fetches.isEmpty());
    transport.reply(transport.fetches.constLast(), snapshot);
    QCOMPARE(client.state(), Power::PowerClientState::Ready);
}

} // namespace

class PowerAppletControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsBoundedProfileAndBrightnessRows();
    void capabilityDenialFailsClosedBeforeDispatch();
    void profileDispatchIsSerializedAndSuccessClearsPending();
    void brightnessDispatchConvertsAndRejectsFailure();
    void ownerReplacementEndsPendingWithoutReplay();
    void unavailableSnapshotHidesPreviouslyProjectedTruth();
};

void PowerAppletControllerTests::projectsBoundedProfileAndBrightnessRows()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);

    QCOMPARE(controller.phase(), QStringLiteral("ready"));
    QCOMPARE(controller.batteryLabel(), QStringLiteral("56%"));
    QCOMPARE(controller.profileRows().size(), 2);
    const QVariantMap balanced = rowWithId(
        controller.profileRows(), QStringLiteral("profileId"),
        QStringLiteral("balanced"));
    QVERIFY(!balanced.isEmpty());
    QVERIFY(balanced.value(QStringLiteral("active")).toBool());
    QVERIFY(!balanced.value(QStringLiteral("adjustable")).toBool());
    const QVariantMap saver = rowWithId(
        controller.profileRows(), QStringLiteral("profileId"),
        QStringLiteral("power-saver"));
    QVERIFY(saver.value(QStringLiteral("adjustable")).toBool());
    QVERIFY(!saver.value(QStringLiteral("accessibleName")).toString().isEmpty());

    QCOMPARE(controller.keyboardRows().size(), 1);
    const QVariantMap keyboard = controller.keyboardRows().constFirst().toMap();
    QCOMPARE(keyboard.value(QStringLiteral("controlId")).toString(),
             QStringLiteral("keyboard-kbd0"));
    QCOMPARE(keyboard.value(QStringLiteral("normalizedCurrent")).toUInt(),
             quint32(5'020));
    QVERIFY(keyboard.value(QStringLiteral("adjustable")).toBool());
    QVERIFY(!keyboard.value(QStringLiteral("accessibleDescription"))
                 .toString()
                 .isEmpty());
}

void PowerAppletControllerTests::capabilityDenialFailsClosedBeforeDispatch()
{
    FakePowerTransport deniedTransport;
    Power::PowerClient deniedClient(&deniedTransport);
    PowerAppletController denied(&deniedClient, false, true);
    publishReady(deniedClient, deniedTransport);

    QCOMPARE(denied.phase(), QStringLiteral("unavailable"));
    QVERIFY(denied.profileRows().isEmpty());
    QVERIFY(denied.keyboardRows().isEmpty());
    QVERIFY(denied.diagnostic().contains(QStringLiteral("not granted")));
    QVERIFY(!denied.requestProfile(QStringLiteral("power-saver")));
    QVERIFY(!denied.requestKeyboardBrightness(QStringLiteral("keyboard-kbd0"),
                                              5'000));
    QVERIFY(deniedTransport.operations.isEmpty());

    FakePowerTransport readOnlyTransport;
    Power::PowerClient readOnlyClient(&readOnlyTransport);
    PowerAppletController readOnly(&readOnlyClient, true, false);
    publishReady(readOnlyClient, readOnlyTransport);
    QVERIFY(!rowWithId(readOnly.profileRows(), QStringLiteral("profileId"),
                       QStringLiteral("power-saver"))
                 .value(QStringLiteral("adjustable"))
                 .toBool());
    QVERIFY(!readOnly.keyboardRows().constFirst().toMap()
                 .value(QStringLiteral("adjustable"))
                 .toBool());
    QVERIFY(!readOnly.requestKeyboardBrightness(
        QStringLiteral("keyboard-kbd0"), 5'000));
    QCOMPARE(readOnlyTransport.operations.size(), 0);
}

void PowerAppletControllerTests::profileDispatchIsSerializedAndSuccessClearsPending()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);

    QVERIFY(controller.requestProfile(QStringLiteral("power-saver")));
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.kind,
             Power::OperationKind::SetProfile);
    QCOMPARE(transport.operations.constFirst().request.profileId,
             QStringLiteral("power-saver"));
    QVERIFY(controller.operationPending());
    QVERIFY(rowWithId(controller.profileRows(), QStringLiteral("profileId"),
                      QStringLiteral("power-saver"))
                .value(QStringLiteral("pending"))
                .toBool());

    QVERIFY(!controller.requestKeyboardBrightness(
        QStringLiteral("keyboard-kbd0"), 4'000));
    QCOMPARE(transport.operations.size(), 1);

    transport.finish(
        transport.operations.constFirst(),
        powerClientResult(transport.operations.constFirst(),
                          Power::OperationStatus::Succeeded,
                          QStringLiteral("applied")));
    QTRY_VERIFY(!controller.operationPending());
    QVERIFY(!controller.feedbackPresent());
    QCOMPARE(transport.operations.size(), 1);
}

void PowerAppletControllerTests::brightnessDispatchConvertsAndRejectsFailure()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);

    QVERIFY(!controller.requestKeyboardBrightness(
        QStringLiteral("keyboard-kbd0"), 10'001));
    QCOMPARE(transport.operations.size(), 0);
    controller.clearFeedback();

    QVERIFY(controller.requestKeyboardBrightness(
        QStringLiteral("keyboard-kbd0"), 5'000));
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.kind,
             Power::OperationKind::SetKeyboardBrightness);
    QCOMPARE(transport.operations.constFirst().request.handle.opaqueId,
             QStringLiteral("keyboard-kbd0"));
    QCOMPARE(transport.operations.constFirst().request.value, quint32(128));

    transport.finish(
        transport.operations.constFirst(),
        powerClientResult(transport.operations.constFirst(),
                          Power::OperationStatus::Rejected,
                          QStringLiteral("policy-rejected")));
    QTRY_VERIFY(!controller.operationPending());
    QVERIFY(controller.feedbackPresent());
    QVERIFY(controller.feedback().contains(QStringLiteral("rejected")));
}

void PowerAppletControllerTests::ownerReplacementEndsPendingWithoutReplay()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);

    QVERIFY(controller.requestProfile(QStringLiteral("power-saver")));
    const FakePowerTransport::Operation oldOperation =
        transport.operations.constFirst();
    transport.announceOwner(QStringLiteral(":1.99"));
    QVERIFY(!controller.operationPending());
    QVERIFY(controller.feedback().contains(QStringLiteral("replaced")));
    QCOMPARE(controller.phase(), QStringLiteral("loading"));

    transport.reply(transport.fetches.constLast(), powerClientSnapshot(47, 1));
    QCOMPARE(controller.phase(), QStringLiteral("ready"));
    transport.finish(oldOperation,
                     powerClientResult(oldOperation,
                                       Power::OperationStatus::Succeeded,
                                       QStringLiteral("late")));
    QCoreApplication::processEvents();
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(!controller.operationPending());
    QVERIFY(controller.feedback().contains(QStringLiteral("replaced")));
}

void PowerAppletControllerTests::unavailableSnapshotHidesPreviouslyProjectedTruth()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);
    QVERIFY(!controller.keyboardRows().isEmpty());

    Power::Snapshot unavailable = powerClientSnapshot(11, 3);
    unavailable.availability = Power::Availability::Unavailable;
    unavailable.reasonCode = QStringLiteral("upstream-not-integrated");
    unavailable.capabilities = {};
    unavailable.composite = {};
    unavailable.supplies.clear();
    unavailable.profiles = {};
    unavailable.inhibitors.clear();
    unavailable.keyboardBacklights.clear();
    unavailable.source = {};
    transport.invalidate(kOwner, unavailable.epoch, unavailable.revision);
    transport.reply(transport.fetches.constLast(), unavailable);

    QCOMPARE(controller.phase(), QStringLiteral("unavailable"));
    QVERIFY(controller.profileRows().isEmpty());
    QVERIFY(controller.keyboardRows().isEmpty());
    QVERIFY(!controller.hasBattery());
}

QTEST_GUILESS_MAIN(PowerAppletControllerTests)
#include "tst_power_applet_controller.moc"
