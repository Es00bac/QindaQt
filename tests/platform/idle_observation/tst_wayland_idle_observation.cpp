// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_fixture_server.h"
#include "qindaqt/platform/idle_observation/idle_observation.h"
#include <QtTest>
using QindaQt::Platform::Idle::WaylandIdleObservation;
class IdleTest final : public QObject {
  Q_OBJECT
private slots:
  void actualProtocolEventsDriveIdleAndRearm();
  void currentLineageIsRecheckedBeforeDisclosure();
  void missingAndAmbiguousGlobalsStayUnavailable_data();
  void missingAndAmbiguousGlobalsStayUnavailable();
  void dynamicallyAddedAmbiguityRevokes_data();
  void dynamicallyAddedAmbiguityRevokes();
  void boundGlobalRemovalAndPeerLossRevoke();
  void queuedPriorAndReentrantRearmEventsCannotGrantIdle();
  void reentrantRefreshRecoversAndRetiredContextsStayBounded();
};
void IdleTest::actualProtocolEventsDriveIdleAndRearm() {
  Server server;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(50);
  QTRY_VERIFY(observer.available());
  QTRY_COMPARE(server.timeout, 50U);
  QVERIFY(!observer.idle());
  QTest::qWait(70);
  QVERIFY(!observer.idle());
  server.idled();
  QTRY_VERIFY(observer.idle());
  server.resumed();
  QTRY_VERIFY(!observer.idle());
  observer.setTimeout(1000);
  QTRY_COMPARE(server.timeout, 1000U);
  observer.revoke();
  QVERIFY(!observer.available());
  QVERIFY(!observer.idle());
  observer.refresh();
  QTRY_VERIFY(observer.available());
  observer.setTimeout(0);
  QVERIFY(!observer.available());
  observer.setTimeout(1440 * 60000 + 1);
  QVERIFY(!observer.available());
}
void IdleTest::currentLineageIsRecheckedBeforeDisclosure() {
  Server server;
  bool live = true;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [&] { return live; });
  observer.setTimeout(100);
  QTRY_VERIFY(observer.available());
  server.idled();
  QTRY_VERIFY(observer.idle());
  live = false;
  QVERIFY(!observer.available());
  QVERIFY(!observer.idle());
  observer.refresh();
  QVERIFY(!observer.available());
  WaylandIdleObservation absent([] { return -1; }, [] { return true; });
  absent.setTimeout(100);
  QVERIFY(!absent.available());
}
void IdleTest::missingAndAmbiguousGlobalsStayUnavailable_data() {
  QTest::addColumn<int>("seats");
  QTest::addColumn<int>("notifiers");
  QTest::newRow("no-seat") << 0 << 1;
  QTest::newRow("two-seats") << 2 << 1;
  QTest::newRow("no-notifier") << 1 << 0;
  QTest::newRow("two-notifiers") << 1 << 2;
}
void IdleTest::missingAndAmbiguousGlobalsStayUnavailable() {
  QFETCH(int, seats);
  QFETCH(int, notifiers);
  Server server(seats, notifiers);
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(100);
  QTest::qWait(100);
  QVERIFY(!observer.available());
  QVERIFY(!observer.idle());
}
void IdleTest::dynamicallyAddedAmbiguityRevokes_data() {
  QTest::addColumn<bool>("seat");
  QTest::newRow("second-seat") << true;
  QTest::newRow("second-notifier") << false;
}
void IdleTest::dynamicallyAddedAmbiguityRevokes() {
  QFETCH(bool, seat);
  Server server;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(100);
  QTRY_VERIFY(observer.available());
  server.idled();
  QTRY_VERIFY(observer.idle());
  if (seat)
    server.addSeat();
  else
    server.addNotifier();
  QTRY_VERIFY(!observer.available());
  QVERIFY(!observer.idle());
}
void IdleTest::boundGlobalRemovalAndPeerLossRevoke() {
  Server server;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(100);
  QTRY_VERIFY(observer.available());
  wl_global_destroy(server.manager);
  server.manager = nullptr;
  wl_display_flush_clients(server.display);
  QTRY_VERIFY(!observer.available());
  QVERIFY(!observer.idle());
  server.addNotifier();
  observer.refresh();
  QTRY_VERIFY(observer.available());
  wl_display_destroy_clients(server.display);
  QTRY_VERIFY(!observer.available());
  QVERIFY(!observer.idle());
}
void IdleTest::queuedPriorAndReentrantRearmEventsCannotGrantIdle() {
  Server server;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(100);
  QTRY_VERIFY(observer.available());
  QTRY_VERIFY(server.notification);
  server.idled();
  observer.setTimeout(200);
  QTRY_COMPARE(server.timeout, 200U);
  QVERIFY(!observer.idle());
  bool reset = false;
  connect(&observer, &WaylandIdleObservation::changed, &observer, [&] {
    if (observer.idle() && !reset) {
      reset = true;
      observer.setTimeout(300);
    }
  });
  server.idled();
  server.idled();
  QTRY_VERIFY(reset);
  QTRY_COMPARE(server.timeout, 300U);
  QVERIFY(!observer.idle());
}
void IdleTest::reentrantRefreshRecoversAndRetiredContextsStayBounded() {
  Server server;
  WaylandIdleObservation observer([&] { return server.connection(); },
                                  [] { return true; });
  observer.setTimeout(100);
  QTRY_VERIFY(observer.available());
  QTRY_VERIFY(server.notification);
  bool refreshed = false;
  connect(&observer, &WaylandIdleObservation::changed, &observer, [&] {
    if (observer.idle() && !refreshed) {
      refreshed = true;
      observer.refresh();
    }
  });
  server.idled();
  QTRY_VERIFY(refreshed);
  QTRY_VERIFY(observer.available());
  QVERIFY(!observer.idle());
  for (int timeout = 101; timeout < 171; ++timeout) {
    observer.setTimeout(timeout);
    QTRY_VERIFY(observer.available());
    QTRY_COMPARE(server.timeout, uint32_t(timeout));
  }
  QVERIFY(server.connections >= 3);
  server.idled();
  QTRY_VERIFY(observer.idle());
}
QTEST_GUILESS_MAIN(IdleTest)
#include "tst_wayland_idle_observation.moc"
