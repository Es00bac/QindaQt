// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/system_status_controller.h"

#include "audio_applet_controller.h"
#include "bluetooth_applet_controller.h"
#include "power_applet_controller.h"
#include "support/fake_audio_transport.h"
#include "support/fake_bluetooth_transport.h"
#include "support/fake_power_transport.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell;
using namespace QindaQt::Shell::DesktopControls;
using QindaQt::Tests::FakeAudioTransport;
using QindaQt::Tests::FakeBluetoothTransport;
using QindaQt::Tests::FakePowerTransport;

namespace {

SystemStatusGrants allGrants()
{
  return {true, true, true, true, true, true};
}

QVariantMap lane(const SystemStatusController &controller, const QString &id)
{
  for (const QVariant &value : controller.laneRows()) {
    const QVariantMap row = value.toMap();
    if (row.value(QStringLiteral("id")).toString() == id) {
      return row;
    }
  }
  return {};
}

} // namespace

class SystemStatusControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void aggregatesReadyLanesFromBorrowedFacades();
  void readDenialDropsTheLaneAndControlDenialKeepsItReadOnly();
  void absentFacadesYieldNoLanes();
};

void SystemStatusControllerTests::aggregatesReadyLanesFromBorrowedFacades()
{
  FakePowerTransport powerTransport;
  Power::PowerClient powerClient(&powerTransport);
  PowerApplet::PowerAppletController power(&powerClient, true, true);
  FakeAudioTransport audioTransport;
  Audio::AudioClient audioClient(&audioTransport);
  AudioApplet::AudioAppletController audio(&audioClient, true, true);
  FakeBluetoothTransport bluetoothTransport;
  Bluetooth::BluetoothClient bluetoothClient(&bluetoothTransport);
  BluetoothApplet::BluetoothAppletController bluetooth(&bluetoothClient, true, true);

  SystemStatusController controller(&audio, &bluetooth, &power, allGrants());
  QSignalSpy stateSpy(&controller, &SystemStatusController::stateChanged);
  QCOMPARE(controller.laneCount(), 3);
  QCOMPARE(controller.availableLaneCount(), 0);
  QVERIFY(!controller.anyLaneAvailable());

  powerClient.start();
  powerTransport.announceOwner(QStringLiteral(":1.42"));
  powerTransport.reply(powerTransport.fetches.constLast(),
                       QindaQt::Tests::powerClientSnapshot());
  QCOMPARE(power.phase(), QStringLiteral("ready"));
  QVERIFY(stateSpy.size() >= 1);
  const QVariantMap powerLane = lane(controller, QStringLiteral("power"));
  QCOMPARE(powerLane.value(QStringLiteral("available")).toBool(), true);
  QCOMPARE(powerLane.value(QStringLiteral("iconName")).toString(), QStringLiteral("battery-060"));
  QVERIFY(powerLane.value(QStringLiteral("summary")).toString().contains(QStringLiteral("%")));
  QCOMPARE(powerLane.value(QStringLiteral("attention")).toBool(), false);
  QCOMPARE(controller.availableLaneCount(), 1);

  audioClient.start();
  audioTransport.announceOwner(QStringLiteral(":1.2"));
  audioTransport.reply(audioTransport.fetches.constLast(), QindaQt::Tests::clientSnapshot());
  const QVariantMap audioLane = lane(controller, QStringLiteral("audio"));
  QCOMPARE(audioLane.value(QStringLiteral("available")).toBool(), true);
  QCOMPARE(audioLane.value(QStringLiteral("iconName")).toString(), QStringLiteral("audio-volume-medium"));
  QCOMPARE(audioLane.value(QStringLiteral("summary")).toString(), QStringLiteral("Output at 50%"));
  QCOMPARE(controller.availableLaneCount(), 2);

  const QVariantMap bluetoothLane = lane(controller, QStringLiteral("bluetooth"));
  QCOMPARE(bluetoothLane.value(QStringLiteral("available")).toBool(), false);
  QCOMPARE(bluetoothLane.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("network-bluetooth-inactive-symbolic"));

  QVERIFY(controller.accessibleName().startsWith(QStringLiteral("System status: ")));
  QVERIFY(controller.accessibleDescription().contains(QStringLiteral("Bluetooth")));
  QCOMPARE(controller.audio(), &audio);
  QCOMPARE(controller.bluetooth(), &bluetooth);
  QCOMPARE(controller.power(), &power);
  QVERIFY(controller.audioControlGranted());
  QVERIFY(controller.powerControlGranted());

  // Owner loss on one lane flows through as that lane's own truth.
  powerTransport.announceOwner(QString{});
  QCOMPARE(lane(controller, QStringLiteral("power")).value(QStringLiteral("available")).toBool(),
           false);
  QCOMPARE(controller.availableLaneCount(), 1);
}

void SystemStatusControllerTests::readDenialDropsTheLaneAndControlDenialKeepsItReadOnly()
{
  FakePowerTransport powerTransport;
  Power::PowerClient powerClient(&powerTransport);
  PowerApplet::PowerAppletController power(&powerClient, true, true);
  SystemStatusGrants grants;
  grants.powerRead = true;
  grants.powerControl = false;
  grants.audioRead = false;
  grants.audioControl = true; // control without read is never effective
  SystemStatusController controller(nullptr, nullptr, &power, grants);
  QCOMPARE(controller.laneCount(), 1);
  QCOMPARE(controller.laneRows().constFirst().toMap().value(QStringLiteral("id")).toString(),
           QStringLiteral("power"));
  QCOMPARE(controller.power(), &power);
  QCOMPARE(controller.audio(), nullptr);
  QVERIFY(!controller.powerControlGranted());
  QVERIFY(!controller.audioControlGranted());

  FakeAudioTransport audioTransport;
  Audio::AudioClient audioClient(&audioTransport);
  AudioApplet::AudioAppletController audio(&audioClient, true, true);
  SystemStatusController denied(&audio, nullptr, nullptr, grants);
  QCOMPARE(denied.laneCount(), 0); // audio facade exists but read is not granted
  QCOMPARE(denied.audio(), nullptr);
}

void SystemStatusControllerTests::absentFacadesYieldNoLanes()
{
  SystemStatusController controller(nullptr, nullptr, nullptr, allGrants());
  QCOMPARE(controller.laneCount(), 0);
  QVERIFY(!controller.anyLaneAvailable());
  QCOMPARE(controller.accessibleName(), QStringLiteral("System status"));
  QVERIFY(controller.laneRows().isEmpty());
}

QTEST_GUILESS_MAIN(SystemStatusControllerTests)
#include "tst_system_status_controller.moc"
