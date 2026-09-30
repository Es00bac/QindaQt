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
#include <cstring>
#include <sys/stat.h>
using namespace QindaQt::Services::KeyringClient;
namespace p=qindaqt::keyring::protocol;
class QuietProcess : public QProcess {
public:
    ~QuietProcess() override {if(state()!=NotRunning) {kill();waitForFinished(3000);}}
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
            QTRY_COMPARE_WITH_TIMEOUT(actions.size(),1,5000);QVERIFY(actions.last()[1].toBool());
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
