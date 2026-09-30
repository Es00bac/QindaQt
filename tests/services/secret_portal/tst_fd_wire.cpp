// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_bus.h"
#include <QTemporaryFile>
class WireTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void realFdWireAndAuthentication() {
        Bus bus;auto backend=bus.connection(),frontend=bus.connection(),stranger=bus.connection();
        QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;QObject host;new SecretPortalAdaptor(host,broker,backend);
        QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));
        Pair pair;QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",pair.fd[1])));QTRY_VERIFY(call.isFinished());
        QCOMPARE(call.reply().signature(),QString("ua{sv}"));QCOMPARE(call.reply().arguments()[0].toUInt(),0U);
        const auto bytes=pair.read();QCOMPARE(bytes.size(),32);QVERIFY(std::all_of(bytes.cbegin(),bytes.cend(),[](char c) {return c==0x5a;}));
        QVERIFY(broker.last);QCOMPARE(broker.last->size(),std::size_t{0});
        Pair denied;QDBusPendingCallWatcher unauthenticated(stranger.asyncCall(retrieve("org.example.App",denied.fd[1],2)));QTRY_VERIFY(unauthenticated.isFinished());
        QCOMPARE(unauthenticated.reply().type(),QDBusMessage::ErrorMessage);QCOMPARE(broker.acquired,1);QVERIFY(denied.read().isEmpty());
    }
    void legacy64WritesExactlyAndWipesOnClosedSink_data() {QTest::addColumn<bool>("closed");QTest::newRow("delivered")<<false;QTest::newRow("closed")<<true;}
    void legacy64WritesExactlyAndWipesOnClosedSink() {
        QFETCH(bool,closed);Bus bus;auto backend=bus.connection(),frontend=bus.connection();QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;broker.size=64;QObject host;new SecretPortalAdaptor(host,broker,backend);QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));Pair pair;if(closed) pair.closeRead();
        QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.Legacy",pair.fd[1])));QTRY_VERIFY(call.isFinished());QCOMPARE(call.reply().arguments()[0].toUInt(),closed?2U:0U);
        if(!closed) {QCOMPARE(pair.read().size(),64);}QVERIFY(broker.last);QCOMPARE(broker.last->size(),std::size_t{0});
    }
    void malformedDuplicateAndCloseAreExplicit() {
        Bus bus;auto backend=bus.connection(),frontend=bus.connection();QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;QObject host;broker.hold=true;new SecretPortalAdaptor(host,broker,backend);
        QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));Pair pair;
        for(const auto &app:QStringList{"","unknown",QString(256,'a')}) {
            QDBusPendingCallWatcher bad(frontend.asyncCall(retrieve(app,pair.fd[1])));QTRY_VERIFY(bad.isFinished());QCOMPARE(bad.reply().arguments()[0].toUInt(),2U);
        }
        QDBusPendingCallWatcher malformed(frontend.asyncCall(retrieve("org.example.App",pair.fd[1],1,{{"token",2}})));QTRY_VERIFY(malformed.isFinished());QCOMPARE(malformed.reply().arguments()[0].toUInt(),2U);
        QDBusPendingCallWatcher first(frontend.asyncCall(retrieve("org.example.App",pair.fd[1])));QTRY_COMPARE(broker.acquired,1);
        QDBusPendingCallWatcher duplicate(frontend.asyncCall(retrieve("org.example.App",pair.fd[1])));QTRY_VERIFY(duplicate.isFinished());QCOMPARE(duplicate.reply().arguments()[0].toUInt(),2U);
        auto close=QDBusMessage::createMethodCall(BackendName,handle(1),RequestInterface,"Close");QDBusPendingCallWatcher closed(frontend.asyncCall(close));
        QTRY_VERIFY(closed.isFinished());QTRY_VERIFY(first.isFinished());QCOMPARE(first.reply().arguments()[0].toUInt(),1U);QCOMPARE(broker.cancelled,1);QVERIFY(pair.read().isEmpty());
    }
    void ownerAndLockLossRetirePendingRequests_data() {QTest::addColumn<bool>("lock");QTest::newRow("owner")<<false;QTest::newRow("lock")<<true;}
    void ownerAndLockLossRetirePendingRequests() {
        QFETCH(bool,lock);Bus bus;auto backend=bus.connection(),frontend=bus.connection(),replacement=bus.connection();
        QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));MockBroker broker;QObject host;broker.hold=true;
        new SecretPortalAdaptor(host,broker,backend);QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));Pair pair;
        QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",pair.fd[1])));QTRY_COMPARE(broker.acquired,1);
        if(lock) broker.lose();else {QVERIFY(frontend.unregisterService(FrontendName));QVERIFY(replacement.registerService(FrontendName));}
        QTRY_VERIFY(call.isFinished());QCOMPARE(call.reply().arguments()[0].toUInt(),2U);QVERIFY(pair.read().isEmpty());QCOMPARE(broker.cancelled,1);
    }
    void closedBackpressuredAndRegularSinksFail_data() {
        QTest::addColumn<int>("kind");QTest::newRow("closed")<<0;QTest::newRow("backpressure")<<1;QTest::newRow("regular")<<2;
    }
    void closedBackpressuredAndRegularSinksFail() {
        QFETCH(int,kind);Bus bus;auto backend=bus.connection(),frontend=bus.connection();QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;QObject host;new SecretPortalAdaptor(host,broker,backend,100);QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));
        Pair pair;QTemporaryFile file;QVERIFY(file.open());
        if(kind==0) pair.closeRead();
        if(kind==1) {char fill[4096]{};while(send(pair.fd[1],fill,sizeof(fill),MSG_DONTWAIT|MSG_NOSIGNAL)>0) {}}
        QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",kind==2?file.handle():pair.fd[1])));QTRY_VERIFY(call.isFinished());
        QCOMPARE(call.reply().arguments()[0].toUInt(),kind==1?1U:2U);
        if(broker.last) QCOMPARE(broker.last->size(),std::size_t{0});
        if(kind==2) QCOMPARE(broker.acquired,0);
    }
    void writablePipeDeliversAndClosedPipeDoesNotRaiseSigpipe() {
        Bus bus;auto backend=bus.connection(),frontend=bus.connection();QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;QObject host;new SecretPortalAdaptor(host,broker,backend);QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));
        for(int index=0;index<2;++index) {
            int descriptors[2];QVERIFY(::pipe(descriptors)==0);if(index) {::close(descriptors[0]);descriptors[0]=-1;}
            QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",descriptors[1],index)));QTRY_VERIFY(call.isFinished());
            QCOMPARE(call.reply().arguments()[0].toUInt(),index?2U:0U);QVERIFY(broker.last);QCOMPARE(broker.last->size(),std::size_t{0});
            if(!index) {char bytes[32];QCOMPARE(::read(descriptors[0],bytes,sizeof(bytes)),ssize_t{32});::close(descriptors[0]);}
            ::close(descriptors[1]);
        }
    }
    void perAppRateLimitIsBounded() {
        Bus bus;auto backend=bus.connection(),frontend=bus.connection();QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));
        MockBroker broker;QObject host;new SecretPortalAdaptor(host,broker,backend);QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));
        for(int index=0;index<17;++index) {
            Pair pair;QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",pair.fd[1],index)));QTRY_VERIFY(call.isFinished());
            QCOMPARE(call.reply().arguments()[0].toUInt(),index<16?0U:2U);
        }
        QCOMPARE(broker.acquired,16);
    }
};
QTEST_GUILESS_MAIN(WireTest)
#include "tst_fd_wire.moc"
