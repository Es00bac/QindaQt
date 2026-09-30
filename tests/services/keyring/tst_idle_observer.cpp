// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_observer.h"
#include "ext-idle-notify-server.h"
#include <QSocketNotifier>
#include <QTest>
#include <wayland-server.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
using qindaqt::keyring::service::WaylandIdleObservation;
#include "idle_fixture_server.h"
class IdleTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void actualProtocolEventsDriveIdleAndRearm(){
        Server server;WaylandIdleObservation observer([&]{return server.connection();},[]{return true;});
        observer.setTimeout(500);QTRY_VERIFY(observer.available());QTRY_COMPARE(server.timeout,500U);
        QVERIFY(!observer.idle());QTest::qWait(550);QVERIFY(!observer.idle());
        server.idled();QTRY_VERIFY(observer.idle());server.resumed();QTRY_VERIFY(!observer.idle());
        observer.setTimeout(1000);QTRY_COMPARE(server.timeout,1000U);
        observer.revoke();QVERIFY(!observer.available());QVERIFY(!observer.idle());
        observer.refresh();QTRY_VERIFY(observer.available());observer.setTimeout(0);QVERIFY(!observer.available());
    }
    void currentLineageIsRecheckedBeforeReportingAvailability(){
        Server server;bool live=true;WaylandIdleObservation observer([&]{return server.connection();},[&]{return live;});
        observer.setTimeout(100);QTRY_VERIFY(observer.available());live=false;QVERIFY(!observer.available());QVERIFY(!observer.idle());
        observer.refresh();QVERIFY(!observer.available());
    }
    void missingAndAmbiguousAuthoritiesStayUnavailable(){
        for(const auto count:{0,2}) {
            Server server(count);WaylandIdleObservation observer([&]{return server.connection();},[]{return true;});
            observer.setTimeout(100);QTest::qWait(150);QVERIFY(!observer.available());QVERIFY(!observer.idle());
        }
        Server server(1,false);WaylandIdleObservation observer([&]{return server.connection();},[]{return true;});
        observer.setTimeout(100);QTest::qWait(150);QVERIFY(!observer.available());
        WaylandIdleObservation unavailable([]{return -1;},[]{return true;});unavailable.setTimeout(100);QVERIFY(!unavailable.available());
    }
    void peerLossFailsClosedAndInvalidBoundsDoNotRequest(){
        Server server;WaylandIdleObservation observer([&]{return server.connection();},[]{return true;});
        observer.setTimeout(10);QTRY_VERIFY(observer.available());
        wl_display_destroy_clients(server.display);QTRY_VERIFY(!observer.available());QVERIFY(!observer.idle());
        observer.setTimeout(1440*60000+1);QVERIFY(!observer.available());
    }
};
QTEST_GUILESS_MAIN(IdleTest)
#include "tst_idle_observer.moc"
