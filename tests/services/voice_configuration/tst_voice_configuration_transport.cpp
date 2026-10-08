// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/voice_configuration/src/native_voice_wire_p.h"
#include "../../../src/services/voice_configuration/src/native_voice_codec_p.h"
#include <qindaqt/services/voice_configuration/qt_voice_configuration_transport.h>
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusMessage>
#include <QtCore/QProcessEnvironment>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
using namespace QindaQt::Services::VoiceConfiguration;
using namespace Qt::StringLiterals;
namespace {
QVariantMap snapshot() {
    return {{u"schemaVersion"_s,1},{u"revision"_s,QVariant::fromValue(quint64(1))},
        {u"configuredProvider"_s,u"elevenlabs"_s},{u"effectiveProvider"_s,u"whisper_local"_s},
        {u"credentialSource"_s,u"unresolved"_s},{u"statusCode"_s,u"reload_required"_s},
        {u"fallbackActive"_s,true},{u"credentialCached"_s,false},
        {u"environmentOverride"_s,false},{u"canConfigure"_s,true}};
}
class Provider final : public QDBusVirtualObject {
public:
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &) override {
        pending=message; ++received; return true;
    }
    int received=0;
    QDBusMessage pending;
};
}
class Tests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void genuineAndForeignNativeReply_data() {
        QTest::addColumn<bool>("foreign");
        QTest::newRow("genuine") << false;
        QTest::newRow("foreign-same-serial-payload") << true;
    }
    void genuineAndForeignNativeReply() {
        QFETCH(bool,foreign);
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        Provider provider;
        QVERIFY(bus.connection.registerVirtualObject(QString::fromLatin1(ObjectPath),&provider));
        const QString name=QUuid::createUuid().toString();
        const auto attacker=QDBusConnection::connectToBus(bus.address,name);
        NativeVoiceWire wire; QVERIFY(wire.open(bus.address));
        auto pending=wire.send(nativeMethod(bus.connection.baseService(),ObjectPath,Interface,"GetSnapshot"),1500);
        QVERIFY(pending); QTRY_COMPARE(provider.received,1);
        const auto responder=foreign ? attacker : bus.connection;
        QVERIFY(responder.send(provider.pending.createReply(QVariantList{snapshot()})));
        QTRY_VERIFY(pending->completed()); const auto reply=pending->take(); QVERIFY(reply);
        QCOMPARE(dbus_message_get_type(reply.get()),DBUS_MESSAGE_TYPE_METHOD_RETURN);
        QCOMPARE(QString::fromUtf8(dbus_message_get_sender(reply.get())),responder.baseService());
        QVERIFY(dbus_message_get_reply_serial(reply.get())!=0);
        QVariantMap decoded; QVERIFY(nativeMap(reply.get(),&decoded));
        QCOMPARE(decoded,snapshot());
        QCOMPARE(pending->fromExpectedPeer(reply.get()),!foreign);
        QDBusConnection::disconnectFromBus(name);
    }
    void transportAcceptsGenuineAndRejectsForeign_data() { genuineAndForeignNativeReply_data(); }
    void transportAcceptsGenuineAndRejectsForeign() {
        QFETCH(bool,foreign);
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); Provider provider;
        QVERIFY(bus.connection.registerVirtualObject(QString::fromLatin1(ObjectPath),&provider));
        QVERIFY(bus.connection.registerService(u"org.qindaqt.Voice1"_s));
        const QString name=QUuid::createUuid().toString();
        const auto attacker=QDBusConnection::connectToBus(bus.address,name);
        QtTransport transport(bus.address); Client client(transport);
        QSignalSpy replies(&transport,&Transport::snapshotReply);
        client.start(); QTRY_COMPARE(provider.received,1);
        QVERIFY((foreign ? attacker : bus.connection).send(provider.pending.createReply(QVariantList{snapshot()})));
        QTRY_COMPARE(replies.size(),1); QCOMPARE(client.ready(),!foreign);
        QVERIFY(bus.connection.unregisterService(u"org.qindaqt.Voice1"_s));
        QTRY_VERIFY(client.owner().isEmpty()); QVERIFY(!client.ready());
        QDBusConnection::disconnectFromBus(name);
    }
    void destructionOrStopInsidePublicEmission_data() {
        QTest::addColumn<bool>("onOwner"); QTest::addColumn<bool>("destroy");
        QTest::newRow("delete-on-owner") << true << true;
        QTest::newRow("stop-on-owner") << true << false;
        QTest::newRow("delete-on-reply") << false << true;
        QTest::newRow("stop-on-reply") << false << false;
    }
    void destructionOrStopInsidePublicEmission() {
        QFETCH(bool,onOwner); QFETCH(bool,destroy);
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); Provider provider;
        QVERIFY(bus.connection.registerVirtualObject(QString::fromLatin1(ObjectPath),&provider));
        QVERIFY(bus.connection.registerService(u"org.qindaqt.Voice1"_s));
        auto transport=std::make_unique<QtTransport>(bus.address); bool acted=false;
        const auto retire=[&] {
            if (acted) return;
            acted=true;
            if (destroy) transport.reset(); else transport->stop();
        };
        connect(transport.get(),&Transport::ownerChanged,this,[&](const QString &owner) {
            if (owner.isEmpty()) return;
            if (onOwner) retire(); else transport->fetch(owner,1);
        });
        connect(transport.get(),&Transport::snapshotReply,this,[&] { retire(); });
        transport->start();
        if (!onOwner) {
            QTRY_COMPARE(provider.received,1);
            QVERIFY(bus.connection.send(provider.pending.createReply(QVariantList{snapshot()})));
        }
        QTRY_VERIFY(acted);
        QTest::qWait(30);
        QCOMPARE(bool(transport),!destroy);
    }
    void noOwnerNeverActivates() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        QtTransport transport(bus.address); Client client(transport);
        client.start(); QTest::qWait(100);
        QVERIFY(client.owner().isEmpty()); QVERIFY(!client.ready()); QVERIFY(!client.reload());
    }
    void actualPythonProviderRoundtrip() {
        const QString source=qEnvironmentVariable("QINDAQT_GABBEE_SOURCE");
        if (source.isEmpty()) QSKIP("Set QINDAQT_GABBEE_SOURCE to the reviewed Gabbee checkout for interop");
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); QProcess provider;
        auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert(u"DBUS_SESSION_BUS_ADDRESS"_s,bus.address);
        environment.insert(u"DBUS_SYSTEM_BUS_ADDRESS"_s,u"unix:path=/nonexistent"_s);
        environment.insert(u"QINDAQT_PRIVATE_VOICE_TEST"_s,u"1"_s);
        environment.insert(u"PYTHONPATH"_s,source+u"/src"_s);
        environment.insert(u"OPENBLAS_NUM_THREADS"_s,u"1"_s);
        environment.insert(u"OMP_NUM_THREADS"_s,u"1"_s);
        provider.setProcessEnvironment(environment);
        provider.start(u"python3"_s,{source+u"/tests/voice_configuration_private_bus.py"_s,u"--serve"_s});
        QVERIFY(provider.waitForStarted()); QVERIFY(provider.waitForReadyRead(5000));
        QCOMPARE(provider.readLine().trimmed(),QByteArray("READY"));
        QtTransport transport(bus.address); Client client(transport); client.start();
        QTRY_VERIFY(client.ready()); QCOMPARE(client.snapshot().effectiveProvider,u"whisper_local"_s);
        QVERIFY(client.reload()); QTRY_VERIFY(!client.busy() && client.ready());
        QCOMPARE(client.snapshot().effectiveProvider,u"elevenlabs"_s);
        QVERIFY(!client.snapshot().fallbackActive);
        QVERIFY(client.save(u"test-only-dummy-key"_s));
        QTRY_VERIFY(!client.busy() && client.ready());
        QCOMPARE(client.status(),u"ok"_s);
        provider.terminate(); QVERIFY(provider.waitForFinished(3000));
        QTRY_VERIFY(client.owner().isEmpty());
    }
};
QTEST_GUILESS_MAIN(Tests)
#include "tst_voice_configuration_transport.moc"
