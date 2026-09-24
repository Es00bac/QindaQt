// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/system_status_controller.h"

#include "network_applet_controller.h"
#include "network_applet_test_support.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Shell::NetworkApplet::TestSupport;

namespace {

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

SystemStatusGrants networkGrants(const bool control)
{
  SystemStatusGrants grants;
  grants.networkRead = true;
  grants.networkControl = control;
  return grants;
}

} // namespace

// ADR-0258: System Status and the standalone Network applet must agree, so
// the lane is a projection of the very same borrowed facade.
class SystemStatusNetworkLaneTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void laneMirrorsTheNetworkFacade();
  void readDenialDropsTheLaneAndControlDenialKeepsItReadOnly();
};

void SystemStatusNetworkLaneTests::laneMirrorsTheNetworkFacade()
{
  qint64 now = 3'000;
  FakeNetworkTransport transport;
  Network::Client::NetworkClient client(transport, [&now] { return now; }, fastTiming());
  NetworkApplet::NetworkAppletController network(&client, true, true);
  SystemStatusController controller(nullptr, nullptr, nullptr, &network,
                                    networkGrants(true));
  QCOMPARE(controller.laneCount(), 1);
  QCOMPARE(lane(controller, QStringLiteral("network")).value(QStringLiteral("available")).toBool(),
           false);
  QCOMPARE(lane(controller, QStringLiteral("network")).value(QStringLiteral("iconName")).toString(),
           QStringLiteral("network-offline"));

  transport.setSnapshot(appletSnapshot());
  QVERIFY(client.start());
  transport.announceOwner(kOwner);
  QTRY_COMPARE(network.phase(), QStringLiteral("ready"));
  const QVariantMap ready = lane(controller, QStringLiteral("network"));
  QCOMPARE(ready.value(QStringLiteral("available")).toBool(), true);
  QCOMPARE(ready.value(QStringLiteral("label")).toString(), QStringLiteral("Network"));
  QCOMPARE(ready.value(QStringLiteral("iconName")).toString(), network.iconName());
  QCOMPARE(ready.value(QStringLiteral("summary")).toString(), QStringLiteral("Wi-Fi: Home"));
  QCOMPARE(ready.value(QStringLiteral("accessibleName")).toString(), network.accessibleName());
  QVERIFY(controller.accessibleName().contains(QStringLiteral("Wi-Fi: Home")));
  QCOMPARE(controller.network(), &network);
  QVERIFY(controller.networkControlGranted());

  // A refused change is attention on the lane, not a silent success.
  QVERIFY(!network.requestRadio(QStringLiteral("mobile"), true));
  QCOMPARE(lane(controller, QStringLiteral("network")).value(QStringLiteral("attention")).toBool(),
           true);

  transport.announceOwner(QString());
  QCOMPARE(lane(controller, QStringLiteral("network")).value(QStringLiteral("available")).toBool(),
           false);
  QCOMPARE(controller.availableLaneCount(), 0);
}

void SystemStatusNetworkLaneTests::readDenialDropsTheLaneAndControlDenialKeepsItReadOnly()
{
  FakeNetworkTransport transport;
  Network::Client::NetworkClient client(transport, [] { return qint64{1}; }, fastTiming());
  NetworkApplet::NetworkAppletController network(&client, true, true);

  SystemStatusGrants denied;
  denied.networkControl = true; // control without read is never effective
  SystemStatusController noRead(nullptr, nullptr, nullptr, &network, denied);
  QCOMPARE(noRead.laneCount(), 0);
  QCOMPARE(noRead.network(), nullptr);
  QVERIFY(!noRead.networkControlGranted());

  SystemStatusController readOnly(nullptr, nullptr, nullptr, &network, networkGrants(false));
  QCOMPARE(readOnly.laneCount(), 1);
  QVERIFY(!readOnly.networkControlGranted());

  // The three-facade constructor keeps working and has no network lane.
  SystemStatusController legacy(nullptr, nullptr, nullptr, networkGrants(true));
  QCOMPARE(legacy.laneCount(), 0);
  QCOMPARE(legacy.network(), nullptr);
}

QTEST_GUILESS_MAIN(SystemStatusNetworkLaneTests)
#include "tst_system_status_network_lane.moc"
