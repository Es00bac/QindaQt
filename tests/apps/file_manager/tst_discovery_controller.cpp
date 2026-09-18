// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/discovery_controller.h"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

// A discovery backend the test drives directly: no bus, no daemon, no network.
class FakeDiscovery final : public ServiceDiscovery {
public:
  void start() override { ++m_starts; }
  void stop() override { ++m_stops; }

  void find(const QString &key, const QString &name) {
    DiscoveredService service;
    service.key = key;
    service.name = name;
    service.host = key.section(QStringLiteral("//"), 1);
    service.scheme = key.section(QLatin1Char(':'), 0, 0);
    service.address = QStringLiteral("10.0.0.1");
    Q_EMIT serviceFound(service);
  }
  void lose(const QString &key) { Q_EMIT serviceLost(key); }
  void refuse(const QString &diagnostic) { Q_EMIT unavailable(diagnostic); }

  int m_starts = 0;
  int m_stops = 0;
};

struct Fixture final {
  FakeDiscovery *discovery = nullptr;
  std::unique_ptr<DiscoveryController> controller;

  Fixture() {
    auto owned = std::make_unique<FakeDiscovery>();
    discovery = owned.get();
    controller = std::make_unique<DiscoveryController>(std::move(owned));
  }
};

} // namespace

class TestDiscoveryController final : public QObject {
  Q_OBJECT

private slots:
  void publishesNothingUntilEnabled();
  void keepsAStableOrderAndIgnoresDuplicates();
  void losingAServiceRemovesItsRow();
  void disablingStopsAndEmptiesTheList();
  void reportsAndClearsAnUnavailableReason();
  void withoutABackendEverythingStaysQuiet();
};

void TestDiscoveryController::publishesNothingUntilEnabled() {
  Fixture fixture;
  QVERIFY(fixture.controller->supported());
  QVERIFY(!fixture.controller->scanning());
  QCOMPARE(fixture.discovery->m_starts, 0);

  // A find that arrives while disabled is dropped: nothing is watching, so
  // nothing may be shown.
  fixture.discovery->find(QStringLiteral("sftp://qinda.local"), QStringLiteral("qinda"));
  QVERIFY(fixture.controller->services().isEmpty());

  fixture.controller->setEnabled(true);
  QCOMPARE(fixture.discovery->m_starts, 1);
  QVERIFY(fixture.controller->scanning());
  // Enabling twice starts once.
  fixture.controller->setEnabled(true);
  QCOMPARE(fixture.discovery->m_starts, 1);
}

void TestDiscoveryController::keepsAStableOrderAndIgnoresDuplicates() {
  Fixture fixture;
  QSignalSpy changed(fixture.controller.get(), &DiscoveryController::servicesChanged);
  fixture.controller->setEnabled(true);

  fixture.discovery->find(QStringLiteral("sftp://zeta.local"), QStringLiteral("zeta"));
  fixture.discovery->find(QStringLiteral("sftp://alpha.local"), QStringLiteral("alpha"));
  fixture.discovery->find(QStringLiteral("smb://middle.local"), QStringLiteral("middle"));
  QCOMPARE(fixture.controller->serviceValues().size(), 3);
  QCOMPARE(fixture.controller->serviceValues().at(0).name, QStringLiteral("alpha"));
  QCOMPARE(fixture.controller->serviceValues().at(1).name, QStringLiteral("middle"));
  QCOMPARE(fixture.controller->serviceValues().at(2).name, QStringLiteral("zeta"));

  // The same key again is the same machine, not a second row.
  const qsizetype before = changed.count();
  fixture.discovery->find(QStringLiteral("sftp://alpha.local"), QStringLiteral("alpha"));
  QCOMPARE(fixture.controller->serviceValues().size(), 3);
  QCOMPARE(changed.count(), before);

  const QVariantMap first = fixture.controller->services().constFirst().toMap();
  QCOMPARE(first.value(QStringLiteral("index")).toInt(), 0);
  QCOMPARE(fixture.controller->addressAt(0), QStringLiteral("sftp://alpha.local"));
  QVERIFY(fixture.controller->addressAt(9).isEmpty());
  QVERIFY(fixture.controller->addressAt(-1).isEmpty());
  // The subtitle carries the address so two machines with one name are still
  // distinguishable; it is never what gets opened.
  QVERIFY(first.value(QStringLiteral("subtitle")).toString().contains(
      QStringLiteral("10.0.0.1")));
}

void TestDiscoveryController::losingAServiceRemovesItsRow() {
  Fixture fixture;
  fixture.controller->setEnabled(true);
  fixture.discovery->find(QStringLiteral("sftp://one.local"), QStringLiteral("one"));
  fixture.discovery->find(QStringLiteral("sftp://two.local"), QStringLiteral("two"));

  fixture.discovery->lose(QStringLiteral("sftp://one.local"));
  QCOMPARE(fixture.controller->serviceValues().size(), 1);
  QCOMPARE(fixture.controller->serviceValues().constFirst().name, QStringLiteral("two"));
  // Losing something that was never there changes nothing.
  fixture.discovery->lose(QStringLiteral("sftp://nowhere.local"));
  QCOMPARE(fixture.controller->serviceValues().size(), 1);
}

void TestDiscoveryController::disablingStopsAndEmptiesTheList() {
  Fixture fixture;
  fixture.controller->setEnabled(true);
  fixture.discovery->find(QStringLiteral("sftp://one.local"), QStringLiteral("one"));
  fixture.discovery->refuse(QStringLiteral("browser failed"));

  fixture.controller->setEnabled(false);
  QCOMPARE(fixture.discovery->m_stops, 1);
  QVERIFY(!fixture.controller->scanning());
  // AGENT-GUARD: stale rows must not outlive the browse that found them.
  QVERIFY(fixture.controller->services().isEmpty());
  QVERIFY(fixture.controller->unavailableReason().isEmpty());
  fixture.controller->setEnabled(false);
  QCOMPARE(fixture.discovery->m_stops, 1);
}

void TestDiscoveryController::reportsAndClearsAnUnavailableReason() {
  Fixture fixture;
  QSignalSpy reasons(fixture.controller.get(),
                     &DiscoveryController::unavailableReasonChanged);
  fixture.controller->setEnabled(true);

  fixture.discovery->refuse(QStringLiteral("Avahi is not running"));
  QCOMPARE(fixture.controller->unavailableReason(), QStringLiteral("Avahi is not running"));
  QCOMPARE(reasons.count(), 1);
  // An empty diagnostic means the problem is over.
  fixture.discovery->refuse(QString());
  QVERIFY(fixture.controller->unavailableReason().isEmpty());

  fixture.discovery->refuse(QStringLiteral("again"));
  fixture.controller->clearUnavailableReason();
  QVERIFY(fixture.controller->unavailableReason().isEmpty());
}

void TestDiscoveryController::withoutABackendEverythingStaysQuiet() {
  // A platform with no Avahi is a supported composition, not a failure.
  DiscoveryController controller(nullptr);
  QVERIFY(!controller.supported());
  controller.setEnabled(true);
  QVERIFY(!controller.scanning());
  QVERIFY(controller.services().isEmpty());
  QVERIFY(controller.unavailableReason().isEmpty());
  QVERIFY(controller.addressAt(0).isEmpty());
}

QTEST_MAIN(TestDiscoveryController)
#include "tst_discovery_controller.moc"
