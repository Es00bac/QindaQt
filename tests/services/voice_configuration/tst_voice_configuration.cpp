// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <qindaqt/services/voice_configuration/voice_configuration_codec.h>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
using namespace QindaQt::Services::VoiceConfiguration;
using namespace Qt::StringLiterals;
namespace {
QVariantMap snapshot(quint64 revision = 1) {
    return {{u"schemaVersion"_qs,1},{u"revision"_qs,QVariant::fromValue(revision)},
        {u"configuredProvider"_qs,u"elevenlabs"_qs},{u"effectiveProvider"_qs,u"whisper_local"_qs},
        {u"credentialSource"_qs,u"unresolved"_qs},{u"statusCode"_qs,u"reload_required"_qs},
        {u"fallbackActive"_qs,true},{u"credentialCached"_qs,false},
        {u"environmentOverride"_qs,false},{u"canConfigure"_qs,true}};
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
        client.start(); Q_EMIT ownerChanged(u":1.2"_qs);
        Q_EMIT snapshotReply(owner, fetchToken, true, false, snapshot());
    }
};
}
class Tests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void canonicalBoundsAndAtomicRefusal() {
        Snapshot destination; QVERIFY(decodeSnapshot(snapshot(), destination));
        QCOMPARE(destination.effectiveProvider,u"whisper_local"_qs);
        for (const QVariant bad : {QVariant(true),QVariant(-1),QVariant(u"1"_qs),QVariant(1.5)}) {
            auto map=snapshot(); map[u"revision"_qs]=bad;
            QVERIFY(!decodeSnapshot(map,destination)); QCOMPARE(destination.revision,quint64(1));
        }
        auto map=snapshot();map[u"unknown"_qs]=true;QVERIFY(!decodeSnapshot(map,destination));
        map=snapshot();map[u"statusCode"_qs]=u"test-only-dummy-key"_qs;QVERIFY(!decodeSnapshot(map,destination));
        map=snapshot();map[u"fallbackActive"_qs]=false;QVERIFY(!decodeSnapshot(map,destination));
    }
    void revisionRegressionAndEquivocationRevokeReadiness() {
        for (const bool regression : {false, true}) {
            Fake fake; Client client(fake); fake.ready(client);
            client.refresh();
            Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
            client.refresh();
            auto changed = snapshot(regression ? 1 : 2);
            changed[u"statusCode"_qs] = u"ready"_qs;
            Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,changed);
            QVERIFY(!client.ready());
            QVERIFY(!client.reload());
        }
    }
    void keyBounds() {
        QVERIFY(validKey(u"test-only-dummy-key"_qs));
        QVERIFY(validKey(QString(512,u'x')));
        for (const QString &key : {QString{},QString(513,u'x'),u"bad key"_qs,u"bad\n"_qs,
                                   u"雪"_qs,QString(QChar(0xd800))}) QVERIFY(!validKey(key));
    }
    void deliberateReloadAndSingleFlight() {
        Fake fake;Client client(fake);fake.ready(client);
        QVERIFY(client.reload()); QVERIFY(client.busy()); QVERIFY(!client.reload());
        QCOMPARE(fake.submissions,1); QCOMPARE(fake.operation,Operation::ReloadCredentials);
        QVariantMap result{{u"schemaVersion"_qs,1},{u"requestId"_qs,QVariant::fromValue(fake.request)},
            {u"revision"_qs,QVariant::fromValue(quint64(2))},{u"operation"_qs,1},
            {u"status"_qs,0},{u"reasonCode"_qs,u"ok"_qs}};
        Q_EMIT fake.operationReply(fake.owner,fake.token,true,result);
        QVERIFY(!client.busy()); QCOMPARE(fake.fetches,2);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
        QVERIFY(client.ready()); QCOMPARE(fake.submissions,1);
    }
    void ownerChangeAndLateReplyNeverReplay() {
        Fake fake;Client client(fake);fake.ready(client);QVERIFY(client.save(u"test-only-dummy-key"_qs));
        const quint64 oldToken=fake.token;Q_EMIT fake.ownerChanged(u":1.3"_qs);
        QVERIFY(!client.ready());QCOMPARE(client.status(),u"uncertain"_qs);
        Q_EMIT fake.operationReply(u":1.2"_qs,oldToken,true,{});
        QCOMPARE(fake.submissions,1);QCOMPARE(client.status(),u"uncertain"_qs);
    }
    void timeoutRetainsUncertaintyWithoutReplay() {
        Fake fake;Client client(fake,nullptr,1);fake.ready(client);QVERIFY(client.reload());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(),1000);
        QCOMPARE(client.status(),u"uncertain"_qs);QCOMPARE(fake.submissions,1);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,true,false,snapshot(2));
        QCOMPARE(client.status(),u"uncertain"_qs);QCOMPARE(fake.submissions,1);
    }
    void unsupportedExtensionAndEntryClearing() {
        Fake fake;Client client(fake);QSignalSpy cleared(&client,&Client::entryClearRequested);
        client.start();Q_EMIT fake.ownerChanged(u":1.2"_qs);
        Q_EMIT fake.snapshotReply(fake.owner,fake.fetchToken,false,true,{});
        QCOMPARE(client.status(),u"unsupported"_qs);QVERIFY(!client.save(u"test-only-dummy-key"_qs));
        QCOMPARE(fake.submissions,0);QVERIFY(cleared.count()>=2);
    }
};
QTEST_GUILESS_MAIN(Tests)
#include "tst_voice_configuration.moc"
