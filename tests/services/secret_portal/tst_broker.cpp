// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_bus.h"
#include <QDBusVirtualObject>
#include <QDBusVariant>
class Daemon final:public QDBusVirtualObject {
public:
    explicit Daemon(QDBusConnection connection):bus(std::move(connection)) {}
    QString introspect(const QString &) const override {return "<interface name=\"org.qindaqt.Keyring1\"/>";}
    QVariantMap policy() const {
        return {{"SettingsAvailable",true},{"ScreenLockAvailable",ready},{"IdleAvailable",true},{"ScreenLocked",!ready},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};
    }
    bool handleMessage(const QDBusMessage &m,const QDBusConnection &) override {
        if(m.member()=="RequestPolicyState") {
            ++policies;lastPolicy=m;if(holdPolicy) return true;auto map=policy();if(malformed && policies>1) map["LockAfterIdleMinutes"]=0U;
            auto signal=QDBusMessage::createTargetedSignal(m.service(),m.path(),"org.qindaqt.Keyring1","PolicyStateReceipt");signal.setArguments({m.arguments().value(0),map});bus.send(signal);bus.send(m.createReply());return true;
        }
        if(m.member()=="RequestPortalSecret") {
            ++retrieved;
            QTimer::singleShot(delay,this,[this,m] {
                auto signal=QDBusMessage::createTargetedSignal(m.service(),m.path(),"org.qindaqt.Keyring1","PortalSecretResult");signal.setArguments({m.arguments().value(1),prompt?QByteArray{}:QByteArray(32,char(0x33)),QVariant::fromValue(QDBusObjectPath(prompt?"/org/freedesktop/secrets/prompt/pfixture":"/"))});bus.send(signal);bus.send(m.createReply());
            });return true;
        }
        if(m.member()=="Prompt") {
            bus.send(m.createReply());
            QTimer::singleShot(delay,this,[this,m] {
                auto completed=QDBusMessage::createTargetedSignal(m.service(),m.path(),"org.freedesktop.Secret.Prompt","Completed");
                completed.setArguments({cancelled,QVariant::fromValue(QDBusVariant(cancelled?QByteArray{}:QByteArray(32,char(0x33))))});bus.send(completed);
            });return true;
        }
        if(m.member()=="Dismiss") {++dismissed;bus.send(m.createReply());return true;}
        return false;
    }
    void losePrivacy() {ready=false;auto m=QDBusMessage::createSignal("/org/freedesktop/secrets","org.qindaqt.Keyring1","PolicyStateChanged");m.setArguments({policy()});bus.send(m);}
    QDBusConnection bus;QDBusMessage lastPolicy;bool holdPolicy=false,ready=true,malformed=false,prompt=false,cancelled=false;int delay=0,policies=0,retrieved=0,dismissed=0;
};
class BrokerTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void forgedRpcAndOtherSenderReceiptCannotGrantPolicy() {
        Bus bus;auto service=bus.connection(),client=bus.connection(),attacker=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        Daemon daemon(service);daemon.holdPolicy=true;QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        // Reproduce the transport limitation using the actual incoming serial.
        // The private bus permits an unrelated actor's method reply.
        auto request=QDBusMessage::createMethodCall(service.baseService(),"/org/freedesktop/secrets","org.qindaqt.Keyring1","RequestPolicyState");request.setArguments({QString("probe")});
        QDBusPendingCallWatcher rpc(client.asyncCall(request));QTRY_COMPARE(daemon.policies,1);
        QVERIFY(attacker.send(daemon.lastPolicy.createReply(QVariantList{daemon.policy()})));QTRY_VERIFY(rpc.isFinished());QCOMPARE(rpc.reply().signature(),QString("a{sv}"));
        QtKeyringPortalBroker broker(client);int completed=0;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64,SecretPages pages,BrokerError) {++completed;if(pages) pages->clear();});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(daemon.policies,2);
        const auto nonce=daemon.lastPolicy.arguments().value(0);
        QVERIFY(attacker.send(daemon.lastPolicy.createReply(QVariantList{daemon.policy()})));
        auto signal=QDBusMessage::createTargetedSignal(client.baseService(),"/org/freedesktop/secrets","org.qindaqt.Keyring1","PolicyStateReceipt");signal.setArguments({nonce,daemon.policy()});
        QVERIFY(attacker.send(signal));QTest::qWait(100);QVERIFY(!broker.admitted());QCOMPARE(daemon.retrieved,0);QCOMPARE(completed,0);
        signal.setArguments({QString("stale"),daemon.policy()});QVERIFY(service.send(signal));QTest::qWait(50);QCOMPARE(daemon.retrieved,0);
        daemon.ready=false;signal.setArguments({nonce,daemon.policy()});QVERIFY(service.send(signal));QTRY_COMPARE(completed,1);QVERIFY(!broker.admitted());
    }
    void absentDaemonFailsClosed() {
        Bus bus;auto connection=bus.connection();QtKeyringPortalBroker broker(connection);bool completed=false;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64 token,SecretPages pages,BrokerError error) {QCOMPARE(token,1ULL);QVERIFY(!pages);QVERIFY(error!=BrokerError::None);completed=true;});
        broker.retrieve(1,"org.example.App");QTRY_VERIFY(completed);QVERIFY(!broker.admitted());
    }
    void exactNativeOwnerAndPolicyReadback_data() {
        QTest::addColumn<bool>("prompt");QTest::addColumn<bool>("malformed");
        QTest::newRow("direct")<<false<<false;QTest::newRow("prompt")<<true<<false;QTest::newRow("malformed-final")<<false<<true;
    }
    void exactNativeOwnerAndPolicyReadback() {
        QFETCH(bool,prompt);QFETCH(bool,malformed);Bus bus;auto service=bus.connection(),client=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        Daemon daemon(service);daemon.prompt=prompt;daemon.malformed=malformed;
        QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        QtKeyringPortalBroker broker(client);int completed=0;BrokerError result=BrokerError::Failed;SecretPages owned;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64 token,SecretPages pages,BrokerError error) {QCOMPARE(token,1ULL);++completed;result=error;owned=std::move(pages);});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(completed,1);
        if(malformed) {QVERIFY(!owned);QVERIFY(result!=BrokerError::None);}else {QVERIFY(result==BrokerError::None);QVERIFY(owned);QCOMPARE(owned->size(),std::size_t{32});owned->clear();}
        QCOMPARE(daemon.policies,2);QCOMPARE(daemon.retrieved,1);
    }
    void freshCompositionDeliversFirstRequest() {
        Bus bus;auto service=bus.connection(),backend=bus.connection(),frontend=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        QVERIFY(backend.registerService(BackendName));QVERIFY(frontend.registerService(FrontendName));Daemon daemon(service);
        QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        QtKeyringPortalBroker broker(backend);QObject host;new SecretPortalAdaptor(host,broker,backend);
        QVERIFY(backend.registerObject("/org/freedesktop/portal/desktop",&host,QDBusConnection::ExportAdaptors));Pair pair;
        QDBusPendingCallWatcher call(frontend.asyncCall(retrieve("org.example.App",pair.fd[1])));QTRY_VERIFY(call.isFinished());
        QCOMPARE(call.reply().arguments()[0].toUInt(),0U);QCOMPARE(pair.read().size(),32);QCOMPARE(daemon.policies,2);
    }
    void cancelledLatePromptIsDismissed() {
        Bus bus;auto service=bus.connection(),client=bus.connection();QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        Daemon daemon(service);daemon.delay=100;daemon.prompt=true;QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        QtKeyringPortalBroker broker(client);int completed=0;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64,SecretPages pages,BrokerError) {++completed;if(pages) pages->clear();});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(daemon.retrieved,1);broker.cancel(1);QTRY_COMPARE(daemon.dismissed,1);QCOMPARE(completed,0);
    }
    void nativeAndStandardOwnersMustMatch() {
        Bus bus;auto first=bus.connection(),other=bus.connection(),client=bus.connection();
        QVERIFY(first.registerService("org.qindaqt.Keyring1"));QVERIFY(other.registerService("org.freedesktop.secrets"));
        QtKeyringPortalBroker broker(client);int completed=0;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64,SecretPages pages,BrokerError error) {++completed;QVERIFY(!pages);QVERIFY(error!=BrokerError::None);});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(completed,1);QVERIFY(!broker.admitted());
    }
    void cancellationOwnerAndPrivacyLossFenceLateReplies_data() {
        QTest::addColumn<int>("reason");QTest::newRow("cancel")<<0;QTest::newRow("owner")<<1;QTest::newRow("privacy")<<2;QTest::newRow("duplicate-token")<<3;
    }
    void cancellationOwnerAndPrivacyLossFenceLateReplies() {
        QFETCH(int,reason);Bus bus;auto service=bus.connection(),client=bus.connection(),replacement=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        Daemon daemon(service);daemon.delay=150;QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        QtKeyringPortalBroker broker(client);int completed=0;bool delivered=false;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64,SecretPages pages,BrokerError error) {++completed;delivered=pages && error==BrokerError::None;if(pages) pages->clear();});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(daemon.retrieved,1);
        if(reason==0) broker.cancel(1);
        if(reason==1) {QVERIFY(service.unregisterService("org.qindaqt.Keyring1"));QVERIFY(replacement.registerService("org.qindaqt.Keyring1"));}
        if(reason==2) daemon.losePrivacy();
        if(reason==3) broker.retrieve(1,"org.example.App");
        QTest::qWait(300);QVERIFY(!delivered);QCOMPARE(completed,reason==0?0:1);
    }
    void promptCancelIsBounded() {
        Bus bus;auto service=bus.connection(),client=bus.connection();QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));
        Daemon daemon(service);daemon.prompt=true;daemon.cancelled=true;QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        QtKeyringPortalBroker broker(client);int completed=0;
        connect(&broker,&SecretBroker::completed,&broker,[&](quint64,SecretPages pages,BrokerError error) {++completed;QVERIFY(!pages);QVERIFY(error==BrokerError::Cancelled);});
        broker.retrieve(1,"org.example.App");QTRY_COMPARE(completed,1);
    }
};
QTEST_GUILESS_MAIN(BrokerTest)
#include "tst_broker.moc"
