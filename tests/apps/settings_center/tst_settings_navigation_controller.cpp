// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::SettingsCenter;

class SettingsNavigationControllerTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testInitialRouteBinding();
  void testRouteSelectionAndSignals();
  void testRejectsUnknownRoutes();
  void testSequentialNavigation();
  void testIndexNavigation();
  void testPreviousRouteTracking();
  void testUnavailableRouteReporting();
  void testRoutesListExposure();
};

void SettingsNavigationControllerTest::testInitialRouteBinding() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();

  // Default route
  SettingsNavigationController controller1(registry,
                                           QStringLiteral("notifications"));
  QCOMPARE(controller1.activeRouteId(), QStringLiteral("notifications"));
  QCOMPARE(controller1.activeRouteComponent(), QStringLiteral("notifications"));
  QCOMPARE(controller1.activeIndex(), 0);
  QVERIFY(!controller1.activeRouteTitle().isEmpty());
  QVERIFY(controller1.activeRouteAvailable());

  // Appearance route
  SettingsNavigationController controller2(registry,
                                           QStringLiteral("appearance"));
  QCOMPARE(controller2.activeRouteId(), QStringLiteral("appearance"));
  QCOMPARE(controller2.activeRouteComponent(), QStringLiteral("appearance"));
  QCOMPARE(controller2.activeIndex(), 1);
  QVERIFY(!controller2.activeRouteTitle().isEmpty());
  QVERIFY(controller2.activeRouteAvailable());

  // Fallback on non-existent initial route
  SettingsNavigationController controller3(registry,
                                           QStringLiteral("non-existent"));
  QCOMPARE(controller3.activeRouteId(), QStringLiteral("notifications"));
}

void SettingsNavigationControllerTest::testRouteSelectionAndSignals() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry,
                                          QStringLiteral("notifications"));

  QSignalSpy idSpy(&controller,
                   &SettingsNavigationController::activeRouteIdChanged);
  QSignalSpy routeSpy(&controller,
                      &SettingsNavigationController::activeRouteChanged);
  QSignalSpy prevSpy(&controller,
                     &SettingsNavigationController::previousRouteIdChanged);

  QVERIFY(controller.selectRoute(QStringLiteral("appearance")));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("appearance"));
  QCOMPARE(controller.activeIndex(), 1);
  QCOMPARE(controller.previousRouteId(), QStringLiteral("notifications"));

  QCOMPARE(idSpy.count(), 1);
  QCOMPARE(routeSpy.count(), 1);
  QCOMPARE(prevSpy.count(), 1);
  QCOMPARE(idSpy.at(0).at(0).toString(), QStringLiteral("appearance"));
  QCOMPARE(prevSpy.at(0).at(0).toString(), QStringLiteral("notifications"));

  // Selecting same route is a no-op
  QVERIFY(controller.selectRoute(QStringLiteral("appearance")));
  QCOMPARE(idSpy.count(), 1);
  QCOMPARE(routeSpy.count(), 1);
}

void SettingsNavigationControllerTest::testRejectsUnknownRoutes() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry,
                                          QStringLiteral("notifications"));

  QSignalSpy rejectSpy(&controller,
                       &SettingsNavigationController::routeSelectionRejected);
  QSignalSpy idSpy(&controller,
                   &SettingsNavigationController::activeRouteIdChanged);

  QVERIFY(!controller.selectRoute(QStringLiteral("unknown-route")));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("notifications"));
  QCOMPARE(idSpy.count(), 0);
  QCOMPARE(rejectSpy.count(), 1);
  QCOMPARE(rejectSpy.at(0).at(0).toString(), QStringLiteral("unknown-route"));
}

void SettingsNavigationControllerTest::testSequentialNavigation() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry,
                                          QStringLiteral("notifications"));

  // selectNext from 0 ("notifications") -> 1 ("appearance")
  QVERIFY(controller.selectNext());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("appearance"));

  // selectNext from 1 ("appearance") -> 2 ("display")
  QVERIFY(controller.selectNext());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("display"));

  // selectNext from 2 ("display") wraps to 0 ("notifications")
  QVERIFY(controller.selectNext());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("notifications"));

  // selectPrevious from 0 wraps to 2 ("display")
  QVERIFY(controller.selectPrevious());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("display"));

  // selectPrevious from 2 -> 1 ("appearance")
  QVERIFY(controller.selectPrevious());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("appearance"));

  // selectPrevious from 1 -> 0 ("notifications")
  QVERIFY(controller.selectPrevious());
  QCOMPARE(controller.activeRouteId(), QStringLiteral("notifications"));
}

void SettingsNavigationControllerTest::testIndexNavigation() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry,
                                          QStringLiteral("notifications"));

  QVERIFY(controller.selectIndex(1));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("appearance"));

  QVERIFY(controller.selectIndex(2));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("display"));

  QVERIFY(controller.selectIndex(0));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("notifications"));

  // Out of bounds
  QVERIFY(!controller.selectIndex(-1));
  QVERIFY(!controller.selectIndex(3));
  QCOMPARE(controller.activeRouteId(), QStringLiteral("notifications"));
}

void SettingsNavigationControllerTest::testPreviousRouteTracking() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry,
                                          QStringLiteral("notifications"));

  QCOMPARE(controller.previousRouteId(), QString());

  controller.selectRoute(QStringLiteral("appearance"));
  QCOMPARE(controller.previousRouteId(), QStringLiteral("notifications"));

  controller.selectRoute(QStringLiteral("notifications"));
  QCOMPARE(controller.previousRouteId(), QStringLiteral("appearance"));
}

void SettingsNavigationControllerTest::testUnavailableRouteReporting() {
  SettingsRouteRegistry registry;
  SettingsRoute availableRoute{
      .id = QStringLiteral("r1"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Route 1"),
      .description = QString(),
      .iconName = QString(),
      .category = QString(),
      .available = true,
      .unavailableReason = QString(),
  };
  SettingsRoute unavailableRoute{
      .id = QStringLiteral("r2"),
      .component = SettingsRouteComponent::Appearance,
      .title = QStringLiteral("Route 2"),
      .description = QString(),
      .iconName = QString(),
      .category = QString(),
      .available = false,
      .unavailableReason = QStringLiteral("Hardware driver missing"),
  };
  QVERIFY(registry.registerRoute(availableRoute));
  QVERIFY(registry.registerRoute(unavailableRoute));

  SettingsNavigationController controller(registry, QStringLiteral("r1"));
  QVERIFY(controller.activeRouteAvailable());
  QCOMPARE(controller.activeRouteUnavailableReason(), QString());

  controller.selectRoute(QStringLiteral("r2"));
  QVERIFY(!controller.activeRouteAvailable());
  QCOMPARE(controller.activeRouteUnavailableReason(),
           QStringLiteral("Hardware driver missing"));
}

void SettingsNavigationControllerTest::testRoutesListExposure() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController controller(registry);

  const QVariantList list = controller.routesList();
  QCOMPARE(list.size(), 3);

  const QVariantMap notifMap = list.at(0).toMap();
  QCOMPARE(notifMap.value(QStringLiteral("id")).toString(),
           QStringLiteral("notifications"));

  const QVariantMap appMap = list.at(1).toMap();
  QCOMPARE(appMap.value(QStringLiteral("id")).toString(),
           QStringLiteral("appearance"));

  const QVariantMap dispMap = list.at(2).toMap();
  QCOMPARE(dispMap.value(QStringLiteral("id")).toString(),
           QStringLiteral("display"));

  const QVariantMap itemAt0 = controller.routeAt(0);
  QCOMPARE(itemAt0.value(QStringLiteral("id")).toString(),
           QStringLiteral("notifications"));

  const QVariantMap itemAt1 = controller.routeAt(1);
  QCOMPARE(itemAt1.value(QStringLiteral("id")).toString(),
           QStringLiteral("appearance"));

  const QVariantMap itemAt2 = controller.routeAt(2);
  QCOMPARE(itemAt2.value(QStringLiteral("id")).toString(),
           QStringLiteral("display"));

  const QVariantMap itemOutOfBounds = controller.routeAt(5);
  QVERIFY(itemOutOfBounds.isEmpty());
}

QTEST_MAIN(SettingsNavigationControllerTest)
#include "tst_settings_navigation_controller.moc"
