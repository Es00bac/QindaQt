// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_client/qt_keyring_gateway.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include "keyring_reply_validation.h"
#include <QTest>
#include <QSignalSpy>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDBusVirtualObject>
#include <cstring>
#include <QUuid>
#include <sys/stat.h>
using namespace QindaQt::Services::KeyringClient;
namespace p=qindaqt::keyring::protocol;
class QuietProcess : public QProcess {
public:
    ~QuietProcess() override {if(state()!=NotRunning) {kill();waitForFinished(3000);}}
};
class PolicyDaemon final:public QDBusVirtualObject {
public:
    explicit PolicyDaemon(QDBusConnection connection):bus(std::move(connection)) {}
    QString introspect(const QString &) const override {return "<interface name=\"org.qindaqt.Keyring1\"/>";}
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &) override {
        if(message.member()=="RequestMetadata") {
            metadata=message;
            if(!holdMetadata) {auto receipt=QDBusMessage::createTargetedSignal(message.service(),message.path(),"org.qindaqt.Keyring1","MetadataReceipt");
                const auto kind=message.arguments().value(1).toString();const auto rows=kind=="collections"?QVariant(QVariantMap{}):QVariant::fromValue(p::MetadataRows{});
                receipt.setArguments({message.arguments()[0],kind,QVariant::fromValue(QDBusVariant(rows))});bus.send(receipt);bus.send(message.createReply());}
            return true;
        }
        if(message.member()=="ListCollections") {metadata=message;if(!holdMetadata) bus.send(message.createReply(QVariantList{QVariantMap{}}));return true;}
        if(message.member()=="RequestPolicyState") {
            last=message;++requests;
            if(automaticPolicy) {auto receipt=QDBusMessage::createTargetedSignal(message.service(),message.path(),"org.qindaqt.Keyring1","PolicyStateReceipt");receipt.setArguments({message.arguments()[0],unlocked()});bus.send(receipt);bus.send(message.createReply());}
            return true;
        }
        if(message.member()=="OpenSession") {bus.send(message.createReply(QVariantList{QVariant::fromValue(QDBusVariant(QString())),QVariant::fromValue(QDBusObjectPath("/org/freedesktop/secrets/session/sfixture"))}));return true;}
        if(message.member()=="ReadSecretWithPrompt") {read=message;return true;}
        if(message.member()=="Prompt") {started=message;bus.send(message.createReply());return true;}
        if(message.member()=="GetSecret" || message.member()=="GetSecrets") {++secretRpc;return true;}
        if(message.member()=="Close" || message.member()=="Dismiss") {bus.send(message.createReply());return true;}
        return false;
    }
    static QVariantMap unlocked() {return {{"SettingsAvailable",true},{"ScreenLockAvailable",true},{"IdleAvailable",false},{"ScreenLocked",false},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};}
    QDBusConnection bus;QDBusMessage last,metadata,read,started;int requests=0,secretRpc=0;bool automaticPolicy=false,holdMetadata=false;
};
class ProvenanceBus final {
public:
    ProvenanceBus() {
        QFile config(directory.path()+"/bus.conf");if(!config.open(QIODevice::WriteOnly)) throw std::runtime_error("private fixture config");
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen><policy context='default'><allow send_destination='*' eavesdrop='true'/><allow eavesdrop='true'/><allow own='*'/></policy></busconfig>");config.close();
        process.start("dbus-daemon",{"--config-file="+config.fileName(),"--nofork","--print-address=1"});
        if(!process.waitForStarted() || !process.waitForReadyRead()) throw std::runtime_error("private fixture bus");
        address=QString::fromUtf8(process.readLine()).trimmed();
    }
    ~ProvenanceBus() {for(const auto &name:names) QDBusConnection::disconnectFromBus(name);}
    QDBusConnection connection() {const auto name=QUuid::createUuid().toString();names.append(name);return QDBusConnection::connectToBus(address,name);}
    QTemporaryDir directory;QuietProcess process;QString address;QStringList names;
};
class KeyringClientTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void hostileMetadataRejectsWithoutDisclosure() {
        QVariantMap row{{"Path",QVariant::fromValue(QDBusObjectPath("/org/freedesktop/secrets/collection/c1/i1"))},{"Locked",true},{"IndexAuthenticated",false}};
        QVERIFY(!validateMetadata(row,false).contains("label"));
        for(const auto &bad:QVariantList{QVariant(QString("false")),QVariant(1),QVariant()}) {
            auto copy=row;copy["Locked"]=bad;QVERIFY_EXCEPTION_THROWN(validateMetadata(copy,false),std::exception);
        }
        row["Label"]="forged";QVERIFY_EXCEPTION_THROWN(validateMetadata(row,false),std::exception);
        p::MetadataRows rows(1025);QVERIFY_EXCEPTION_THROWN(validateItems(rows),std::exception);
        QVERIFY(!validObjectPath("/org/freedesktop/secrets/collection/../bad","collection"));
    }
    void hostilePolicyCannotAdmitDisclosure() {
        const QVariantMap admitted{{"SettingsAvailable",true},{"ScreenLockAvailable",true},{"IdleAvailable",false},{"ScreenLocked",false},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};
        QCOMPARE(validatePolicy(admitted),admitted);
        for(const auto &key:{"SettingsAvailable","ScreenLockAvailable","IdleAvailable","ScreenLocked","LockOnScreenLock"}) {
            auto wire=admitted;wire[key]=QString("true");QVERIFY_EXCEPTION_THROWN(validatePolicy(wire),std::exception);
        }
        for(const auto &invalid:QVariantList{QVariant(-1),QVariant(1441),QVariant(0U),QVariant(QString("0"))}) {
            auto wire=admitted;wire["LockAfterIdleMinutes"]=invalid;QVERIFY_EXCEPTION_THROWN(validatePolicy(wire),std::exception);
        }
        auto extended=admitted;extended["Forged"]=false;QVERIFY_EXCEPTION_THROWN(validatePolicy(extended),std::exception);
    }
    void nativePolicyRequiresFreshActualOwnerReceipt() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());QFile config(temporary.path()+"/bus.conf");QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen><policy context='default'><allow send_destination='*' eavesdrop='true'/><allow eavesdrop='true'/><allow own='*'/></policy></busconfig>");config.close();
        QuietProcess daemonProcess;daemonProcess.start("dbus-daemon",{"--config-file="+config.fileName(),"--nofork","--print-address=1"});QVERIFY(daemonProcess.waitForStarted());QVERIFY(daemonProcess.waitForReadyRead());const auto address=QString::fromUtf8(daemonProcess.readLine()).trimmed();
        auto service=QDBusConnection::connectToBus(address,"receipt-service"),client=QDBusConnection::connectToBus(address,"receipt-client"),attacker=QDBusConnection::connectToBus(address,"receipt-attacker");
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));PolicyDaemon daemon(service);
        QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));
        {
            QtKeyringGateway gateway(client);QSignalSpy rows(&gateway,&KeyringGateway::rowsReady),policies(&gateway,&KeyringGateway::policyChanged);QTRY_VERIFY(gateway.available());
            gateway.request(1,Request::Collections);QTRY_COMPARE(daemon.requests,1);
            const QVariantMap unlocked{{"SettingsAvailable",true},{"ScreenLockAvailable",true},{"IdleAvailable",false},{"ScreenLocked",false},{"LockOnScreenLock",false},{"LockAfterIdleMinutes",0}};
            QVERIFY(attacker.send(daemon.last.createReply(QVariantList{unlocked})));
            auto signal=QDBusMessage::createTargetedSignal(client.baseService(),"/org/freedesktop/secrets","org.qindaqt.Keyring1","PolicyStateReceipt");signal.setArguments({daemon.last.arguments()[0],unlocked});
            QVERIFY(attacker.send(signal));QTest::qWait(100);QCOMPARE(policies.size(),0);QCOMPARE(rows.size(),0);
            signal.setArguments({QString("stale"),unlocked});QVERIFY(service.send(signal));QTest::qWait(50);QCOMPARE(policies.size(),0);
            auto locked=unlocked;locked["ScreenLocked"]=true;signal.setArguments({daemon.last.arguments()[0],locked});QVERIFY(service.send(signal));QTRY_COMPARE(rows.size(),1);QCOMPARE(policies.size(),1);QVERIFY(policies.last()[0].toMap().value("ScreenLocked").toBool());
        }
        for(const auto &name:{"receipt-attacker","receipt-client","receipt-service"}) QDBusConnection::disconnectFromBus(name);
    }
    void secretBytesRequireActualOwnedPromptEvenAfterForgedRpc() {
        ProvenanceBus bus;auto service=bus.connection(),client=bus.connection(),attacker=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));PolicyDaemon daemon(service);daemon.automaticPolicy=true;
        QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));QtKeyringGateway gateway(client);
        QSignalSpy rows(&gateway,&KeyringGateway::rowsReady),secrets(&gateway,&KeyringGateway::secretReady);QTRY_VERIFY(gateway.available());
        gateway.request(1,Request::Collections);QTRY_COMPARE(rows.size(),1);
        gateway.request(2,Request::Reveal,"/org/freedesktop/secrets/collection/login/item");QTRY_COMPARE(daemon.read.member(),QString("ReadSecretWithPrompt"));
        const auto prompt=QString("/org/freedesktop/secrets/prompt/pfixture");
        // Actual bus reproduces senderless Qt RPC acceptance for the path. That
        // path never grants bytes: the owned actual-sender signal is mandatory.
        QVERIFY(attacker.send(daemon.read.createReply(QVariantList{QVariant::fromValue(QDBusObjectPath(prompt))})));QTRY_COMPARE(daemon.started.member(),QString("Prompt"));
        p::WireSecret wire{QDBusObjectPath("/org/freedesktop/secrets/session/sfixture"),{},QByteArray(8,char(0x66)),"application/octet-stream"};
        auto completed=QDBusMessage::createTargetedSignal(client.baseService(),prompt,"org.freedesktop.Secret.Prompt","Completed");completed.setArguments({false,QVariant::fromValue(QDBusVariant(QVariant::fromValue(wire)))});
        QVERIFY(attacker.send(completed));QTest::qWait(50);QCOMPARE(secrets.size(),0);QCOMPARE(daemon.secretRpc,0);QCOMPARE(daemon.requests,1);
        wire.value=QByteArray(8,char(0x55));completed.setArguments({false,QVariant::fromValue(QDBusVariant(QVariant::fromValue(wire)))});QVERIFY(service.send(completed));QTRY_COMPARE(secrets.size(),1);
        auto pages=qvariant_cast<std::shared_ptr<qindaqt::keyring::SecureBuffer>>(secrets.last()[1]);QVERIFY(pages);QCOMPARE(pages->size(),std::size_t{8});QVERIFY(std::all_of(pages->bytes().begin(),pages->bytes().end(),[](unsigned char value){return value==0x55;}));pages->clear();p::wipe(wire.value);QCOMPARE(daemon.requests,2);QCOMPARE(daemon.secretRpc,0);
    }
    void metadataRequiresFreshActualOwnerReceipt_data() {QTest::addColumn<bool>("collections");QTest::newRow("collections")<<true;QTest::newRow("items")<<false;}
    void metadataRequiresFreshActualOwnerReceipt() {
        QFETCH(bool,collections);ProvenanceBus bus;auto service=bus.connection(),client=bus.connection(),attacker=bus.connection();
        QVERIFY(service.registerService("org.qindaqt.Keyring1"));QVERIFY(service.registerService("org.freedesktop.secrets"));PolicyDaemon daemon(service);daemon.automaticPolicy=true;daemon.holdMetadata=true;
        QVERIFY(service.registerVirtualObject("/org/freedesktop/secrets",&daemon,QDBusConnection::SubPath));QtKeyringGateway gateway(client);QSignalSpy rows(&gateway,&KeyringGateway::rowsReady);QTRY_VERIFY(gateway.available());
        gateway.request(1,collections?Request::Collections:Request::Items,"/org/freedesktop/secrets/collection/login");QTRY_COMPARE(daemon.metadata.member(),QString("RequestMetadata"));
        const auto kind=collections?QString("collections"):QString("items"),nonce=daemon.metadata.arguments()[0].toString();
        QVariantMap row{{"Path",QVariant::fromValue(QDBusObjectPath(collections?"/org/freedesktop/secrets/collection/login":"/org/freedesktop/secrets/collection/login/item"))},{"Locked",false},{"IndexAuthenticated",true},{"Label",QString("actual-label")},{"Created",QVariant::fromValue(quint64{1})},{"Modified",QVariant::fromValue(quint64{1})}};
        const auto wire=collections?QVariant(QVariantMap{{"login",row}}):QVariant::fromValue(p::MetadataRows{row});
        QVERIFY(attacker.send(daemon.metadata.createReply(QVariantList{wire})));QTest::qWait(30);QCOMPARE(rows.size(),0);
        auto receipt=QDBusMessage::createTargetedSignal(client.baseService(),"/org/freedesktop/secrets","org.qindaqt.Keyring1","MetadataReceipt");receipt.setArguments({nonce,kind,QVariant::fromValue(QDBusVariant(wire))});
        QVERIFY(attacker.send(receipt));QTest::qWait(30);QCOMPARE(rows.size(),0);
        receipt.setArguments({QString("stale"),kind,QVariant::fromValue(QDBusVariant(wire))});QVERIFY(service.send(receipt));QTest::qWait(30);QCOMPARE(rows.size(),0);
        receipt.setArguments({nonce,QString("other"),QVariant::fromValue(QDBusVariant(wire))});QVERIFY(service.send(receipt));QTest::qWait(30);QCOMPARE(rows.size(),0);
        receipt.setArguments({nonce,kind,QVariant::fromValue(QDBusVariant(wire))});QVERIFY(service.send(receipt));QTRY_COMPARE(rows.size(),1);QCOMPARE(rows.last()[1].toList().first().toMap().value("label").toString(),QString("actual-label"));
        QVERIFY(service.send(receipt));QTest::qWait(30);QCOMPARE(rows.size(),1);
        gateway.request(2,collections?Request::Collections:Request::Items,"/org/freedesktop/secrets/collection/login");QTRY_VERIFY(daemon.metadata.arguments()[0].toString()!=nonce);
        receipt.setArguments({daemon.metadata.arguments()[0],kind,QVariant::fromValue(QDBusVariant(wire))});gateway.cancel();QVERIFY(service.send(receipt));QTest::qWait(30);QCOMPARE(rows.size(),1);
        gateway.request(3,collections?Request::Collections:Request::Items,"/org/freedesktop/secrets/collection/login");QTRY_VERIFY(daemon.metadata.arguments()[0].toString()!=receipt.arguments()[0].toString());
        receipt.setArguments({daemon.metadata.arguments()[0],kind,QVariant::fromValue(QDBusVariant(wire))});QVERIFY(service.unregisterService("org.qindaqt.Keyring1"));QVERIFY(service.send(receipt));QTRY_VERIFY(!gateway.available());QTest::qWait(30);QCOMPARE(rows.size(),1);
    }
    void privateRealDaemonUsesOwnedPromptAndSession() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        const auto runtime=temporary.path()+"/runtime",storage=temporary.path()+"/storage";
        QVERIFY(QDir().mkpath(runtime));QVERIFY(QDir().mkpath(storage));chmod(runtime.toUtf8().constData(),0700);chmod(storage.toUtf8().constData(),0700);
        QFile configuration(temporary.path()+"/bus.conf");QVERIFY(configuration.open(QIODevice::WriteOnly));
        configuration.write("<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen><policy context='default'><allow send_destination='*'/><allow eavesdrop='true'/><allow own='*'/></policy></busconfig>");
        configuration.close();
        QuietProcess broker;broker.start("dbus-daemon",{"--config-file="+configuration.fileName(),"--nofork","--print-address=1"});
        QVERIFY(broker.waitForStarted());QVERIFY(broker.waitForReadyRead());const auto address=QString::fromUtf8(broker.readLine()).trimmed();
        QuietProcess daemon;auto env=QProcessEnvironment::systemEnvironment();
        env.insert("DBUS_SESSION_BUS_ADDRESS",address);env.insert("XDG_RUNTIME_DIR",runtime);
        daemon.setProcessEnvironment(env);
        daemon.start(QINDAQT_KEYRING_BINARY,{"--private-bus",address,"--storage-root",storage,"--runtime-root",runtime,"--prompt-program",QINDAQT_PROMPT_SCRIPT,"--policy-fixture"});
        QVERIFY(daemon.waitForStarted());
        const auto name=QStringLiteral("keyring-client-private");
        auto bus=QDBusConnection::connectToBus(address,name);
        QVERIFY(bus.isConnected());
        QuietProcess compositor;compositor.setProcessEnvironment(env);
        compositor.start(QINDAQT_POLICY_COMPOSITOR,{"qindaqt-7"});
        QVERIFY(compositor.waitForStarted());QVERIFY(compositor.waitForReadyRead());
        QCOMPARE(compositor.readLine(),QByteArray("ready\n"));
        QDBusInterface native("org.qindaqt.Keyring1","/org/freedesktop/secrets","org.qindaqt.Keyring1",bus);
        QTRY_VERIFY_WITH_TIMEOUT(native.isValid(),5000);
        const auto attachment=native.call("AttachSessionWithDisplay","qindaqt-7");
        QCOMPARE(attachment.type(),QDBusMessage::ReplyMessage);QVERIFY(attachment.arguments().first().toBool());
        auto screenReady=[&]{
            const auto reply=native.call("GetPolicyState");
            if(reply.type()!=QDBusMessage::ReplyMessage) return false;
            const auto state=p::argument<QVariantMap>(reply.arguments().first());
            return state.value("ScreenLockAvailable").toBool() && !state.value("ScreenLocked").toBool();
        };
        QTRY_VERIFY_WITH_TIMEOUT(screenReady(),5000);
        {
            QtKeyringGateway gateway(bus);
            QSignalSpy invalidated(&gateway,&KeyringGateway::secretsInvalidated);
            QSignalSpy actions(&gateway,&KeyringGateway::actionFinished),rows(&gateway,&KeyringGateway::rowsReady),secrets(&gateway,&KeyringGateway::secretReady);
            QTRY_VERIFY_WITH_TIMEOUT(gateway.available(),5000);
            gateway.request(1,Request::Create,{},"Synthetic login");
            QTRY_COMPARE_WITH_TIMEOUT(actions.size(),1,5000);QVERIFY2(actions.last()[1].toBool(),qPrintable(actions.last()[2].toString()));
            gateway.request(2,Request::Collections);
            QTRY_COMPARE_WITH_TIMEOUT(rows.size(),1,5000);
            const auto values=rows.last()[1].toList();QCOMPARE(values.size(),2);
            QString collection;
            for(const auto &row:values) if(row.toMap().value("label").toString()=="Synthetic login") collection=row.toMap().value("path").toString();
            QVERIFY(!collection.isEmpty());
            QDBusInterface service("org.freedesktop.secrets","/org/freedesktop/secrets","org.freedesktop.Secret.Service",bus);
            auto sessionReply=service.call("OpenSession","plain",QVariant::fromValue(QDBusVariant(QString())));
            QCOMPARE(sessionReply.type(),QDBusMessage::ReplyMessage);
            const auto session=p::argument<QDBusObjectPath>(sessionReply.arguments()[1]);
            p::WireSecret wire{session,{},QByteArray("synthetic-value"),"text/plain"};
            QDBusInterface col("org.freedesktop.secrets",collection,"org.freedesktop.Secret.Collection",bus);
            const QVariantMap props{{"org.freedesktop.Secret.Item.Label","Fixture"},{"org.freedesktop.Secret.Item.Attributes",QVariant::fromValue(p::StringMap{})}};
            const auto create=col.call("CreateItem",props,QVariant::fromValue(wire),false);
            QCOMPARE(create.type(),QDBusMessage::ReplyMessage);
            const auto item=p::argument<QDBusObjectPath>(create.arguments()[0]).path();p::wipe(wire.value);
            gateway.request(3,Request::Items,collection);QTRY_COMPARE_WITH_TIMEOUT(rows.size(),2,5000);
            QCOMPARE(rows.last()[1].toList().first().toMap().value("label").toString(),"Fixture");
            gateway.request(4,Request::Reveal,item);QTRY_COMPARE_WITH_TIMEOUT(secrets.size(),1,5000);
            const auto secret=qvariant_cast<std::shared_ptr<qindaqt::keyring::SecureBuffer>>(secrets.last()[1]);
            QVERIFY(secret);QCOMPARE(secret->size(),15U);QVERIFY(std::memcmp(secret->bytes().data(),"synthetic-value",15)==0);secret->clear();secrets.clear();
            // A real prompt holds its reply until after the native screen locks.
            const QVariantMap delayedProps{{"org.freedesktop.Secret.Item.Label","delayed-reveal-fixture"},{"org.freedesktop.Secret.Item.Attributes",QVariant::fromValue(p::StringMap{})}};
            p::WireSecret delayedWire{session,{},QByteArray("late-value"),"text/plain"};
            const auto delayedCreate=col.call("CreateItem",delayedProps,QVariant::fromValue(delayedWire),false);
            QCOMPARE(delayedCreate.type(),QDBusMessage::ReplyMessage);p::wipe(delayedWire.value);
            const auto delayedItem=p::argument<QDBusObjectPath>(delayedCreate.arguments()[0]).path();
            gateway.request(40,Request::Reveal,delayedItem);QTest::qWait(150);
            compositor.write("lock\n");QVERIFY(compositor.waitForBytesWritten());
            QTRY_VERIFY_WITH_TIMEOUT(actions.size()==2,5000);
            QVERIFY(!actions.last()[1].toBool());QCOMPARE(secrets.size(),0);
            // Default collection policy remains false: the UI fence is stricter.
            QCOMPARE(col.property("Locked").toBool(),false);
            compositor.write("unlock\n");QVERIFY(compositor.waitForBytesWritten());
            QTRY_VERIFY_WITH_TIMEOUT(screenReady(),5000);
            gateway.request(5,Request::Lock,collection);QTRY_COMPARE_WITH_TIMEOUT(actions.size(),3,5000);QVERIFY(actions.last()[1].toBool());
            QVERIFY(invalidated.size()>=1);
            gateway.request(6,Request::Unlock,collection);gateway.cancel();
            QTest::qWait(1200);QCOMPARE(actions.size(),3);
            gateway.request(7,Request::Collections);QTRY_COMPARE_WITH_TIMEOUT(rows.size(),3,5000);
            daemon.terminate();QVERIFY(daemon.waitForFinished(5000));QTRY_VERIFY(!gateway.available());
        }
        QDBusConnection::disconnectFromBus(name);broker.terminate();QVERIFY(broker.waitForFinished(5000));
    }
};
QTEST_GUILESS_MAIN(KeyringClientTest)
#include "tst_keyring_client.moc"
