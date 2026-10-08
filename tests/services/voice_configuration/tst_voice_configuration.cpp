// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <qindaqt/services/voice_configuration/voice_configuration_codec.h>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
using namespace QindaQt::Services::VoiceConfiguration;
using namespace Qt::StringLiterals;
namespace {
QVariantMap snapshot(quint64 revision = 1) {
    return {{u"schemaVersion"_s,1},{u"revision"_s,QVariant::fromValue(revision)},
        {u"configuredProvider"_s,u"elevenlabs"_s},{u"effectiveProvider"_s,u"whisper_local"_s},
        {u"credentialSource"_s,u"unresolved"_s},{u"statusCode"_s,u"reload_required"_s},
        {u"fallbackActive"_s,true},{u"credentialCached"_s,false},
        {u"environmentOverride"_s,false},{u"canConfigure"_s,true}};
}
class Fake final : public Transport {
public:
    int submissions = 0, starts = 0, fetches = 0;
    quint64 token = 0, request = 0, fetchToken = 0;
    QString owner;
    Operation operation = Operation::ReloadCredentials;
    void start() override { ++starts; }
    void stop() override {}
    void fetch(const QString &value, quint64 valueToken) override { owner=value; fetchToken=valueToken; ++fetches; }
    void submit(const QString &value, quint64 valueToken, quint64 id, quint64,
                Operation kind, const QString &) override {
        owner=value; token=valueToken; request=id; operation=kind; ++submissions;
    }
    void ready(Client &client) {
        client.start(); Q_EMIT ownerChanged(u":1.2"_s);
        Q_EMIT snapshotReply(owner, fetchToken, true, false, snapshot());
    }
};
}
class Tests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void canonicalBoundsAndAtomicRefusal() {
        Snapshot destination; QVERIFY(decodeSnapshot(snapshot(), destination));
        QCOMPARE(destination.effectiveProvider,u"whisper_local"_s);
        for (const QVariant &bad : {QVariant(true),QVariant(-1),QVariant(u"1"_s),QVariant(1.5)}) {
            auto map=snapshot(); map[u"revision"_s]=bad;
            QVERIFY(!decodeSnapshot(map,destination)); QCOMPARE(destination.revision,quint64(1));
        }
        auto map=snapshot();map[u"unknown"_s]=true;QVERIFY(!decodeSnapshot(map,destination));
        map=snapshot();map[u"statusCode"_s]=u"test-only-dummy-key"_s;QVERIFY(!decodeSnapshot(map,destination));
        map=snapshot();map[u"fallbackActive"_s]=false;QVERIFY(!decodeSnapshot(map,destination));
    }
    void revisionRegressionAndEquivocationRevokeReadiness() {
        for (const bool regression : {false, true}) {
            Fake fake; Client client(fake); fake.ready(client);
            client.refresh();
            Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
            client.refresh();
            auto changed = snapshot(regression ? 1 : 2);
            changed[u"statusCode"_s] = u"ready"_s;
            Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,changed);
            QVERIFY(!client.ready());
            QVERIFY(!client.reload());
        }
    }
    void keyBounds() {
        QVERIFY(validKey(u"test-only-dummy-key"_s));
        QVERIFY(validKey(QString(512,u'x')));
        for (const QString &key : {QString{},QString(513,u'x'),u"bad key"_s,u"bad\n"_s,
                                   u"雪"_s,QString(QChar(0xd800))}) QVERIFY(!validKey(key));
    }
    void deliberateReloadAndSingleFlight() {
        Fake fake;Client client(fake);fake.ready(client);
        QVERIFY(client.reload()); QVERIFY(client.busy()); QVERIFY(!client.reload());
        QCOMPARE(fake.submissions,1); QCOMPARE(fake.operation,Operation::ReloadCredentials);
        QVariantMap result{{u"schemaVersion"_s,1},{u"requestId"_s,QVariant::fromValue(fake.request)},
            {u"revision"_s,QVariant::fromValue(quint64(2))},{u"operation"_s,1},
            {u"status"_s,0},{u"reasonCode"_s,u"ok"_s}};
        Q_EMIT fake.operationReply(fake.owner,fake.token,true,result);
        QVERIFY(!client.busy()); QCOMPARE(fake.fetches,2);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
        QVERIFY(client.ready()); QCOMPARE(fake.submissions,1);
    }
    void ownerChangeAndLateReplyNeverReplay() {
        Fake fake;Client client(fake);fake.ready(client);QVERIFY(client.save(u"test-only-dummy-key"_s));
        const quint64 oldToken=fake.token;Q_EMIT fake.ownerChanged(u":1.3"_s);
        QVERIFY(!client.ready());QCOMPARE(client.status(),u"uncertain"_s);
        Q_EMIT fake.operationReply(u":1.2"_s,oldToken,true,{});
        QCOMPARE(fake.submissions,1);QCOMPARE(client.status(),u"uncertain"_s);
    }
    void timeoutRetainsUncertaintyWithoutReplay() {
        Fake fake;Client client(fake,nullptr,1);fake.ready(client);QVERIFY(client.reload());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(),1000);
        QCOMPARE(client.status(),u"uncertain"_s);QCOMPARE(fake.submissions,1);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
        QCOMPARE(client.status(),u"uncertain"_s);QCOMPARE(fake.submissions,1);
    }
    void unsupportedExtensionAndEntryClearing() {
        Fake fake;Client client(fake);QSignalSpy cleared(&client,&Client::entryClearRequested);
        client.start();Q_EMIT fake.ownerChanged(u":1.2"_s);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,false,true,{});
        QCOMPARE(client.status(),u"unsupported"_s);QVERIFY(!client.save(u"test-only-dummy-key"_s));
        QCOMPARE(fake.submissions,0);QVERIFY(cleared.count()>=2);
    }
};
QTEST_GUILESS_MAIN(Tests)
#include "tst_voice_configuration.moc"
