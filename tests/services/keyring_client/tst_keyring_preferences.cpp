// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/keyring_preferences.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/settings/settings_schema.h>
#include <QTest>
using namespace QindaQt::Apps::SettingsKeyring;
namespace sc=QindaQt::Services::SettingsClient;
namespace sp=QindaQt::Services::SettingsProtocol;
class Transport : public sc::SettingsTransport {
public:
    quint64 snapshotToken=0,commitToken=0;QString owner;int commits=0;
    bool start(QString *) override{return true;}
    void stop() override{}
    void requestSnapshot(quint64 t,const QString &o,const QStringList &) override{snapshotToken=t;owner=o;}
    void commit(quint64 t,const QString &o,const QString &,quint64,const QVariantList &) override{commitToken=t;owner=o;++commits;}
    void requestActivation() override{}
};
QVariantMap snapshotWire(bool lock,int idle,quint64 revision) {
    using sp::WireContract;
    return {{QLatin1StringView(WireContract::FieldStatus),quint32(0)},
        {QLatin1StringView(WireContract::FieldWireSchemaVersion),WireContract::WireSchemaVersion},
        {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),quint32(2)},
        {QLatin1StringView(WireContract::FieldEpoch),"epoch"},
        {QLatin1StringView(WireContract::FieldRevision),revision},
        {QLatin1StringView(WireContract::FieldValues),QVariantMap{{"keyring.lockOnScreenLock",lock},{"keyring.lockAfterIdleMinutes",idle}}},
        {QLatin1StringView(WireContract::FieldSourceLayers),QVariantMap{{"keyring.lockOnScreenLock","user-overrides"},{"keyring.lockAfterIdleMinutes","user-overrides"}}},
        {QLatin1StringView(WireContract::FieldMessage),QString()}};
}
class PreferencesTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void schemaDefaultsAndHostileTypesAreBounded(){
        QString error;
        const auto schema=QindaQt::Settings::SettingsSchema::fromFile(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json",nullptr,&error);
        QVERIFY2(schema.has_value(),qPrintable(error));
        QCOMPARE(schema->version(),2);QCOMPARE(schema->systemDefaults().value("keyring.lockOnScreenLock").toBool(),false);
        QCOMPARE(schema->systemDefaults().value("keyring.lockAfterIdleMinutes").toInt(),0);
        QVERIFY(schema->validateValue("keyring.lockAfterIdleMinutes",1440).isValid());
        for(const auto &value:QVariantList{QVariant(-1),QVariant(1441),QVariant(0.5),QVariant("5"),QVariant(true)})
            QVERIFY(!schema->validateValue("keyring.lockAfterIdleMinutes",value).isValid());
        QVERIFY(!schema->validateValue("keyring.lockOnScreenLock","true").isValid());
    }
    void pendingAndUncertainSavesNeverClaimPersistence(){
        Transport transport;sc::SettingsClient client(transport,KeyringPreferences::scopedKeys(),{.requestTimeoutMilliseconds=100,.debounceMilliseconds=0,.retryMilliseconds={10}});
        KeyringPreferences preferences(client);QVERIFY(client.start());
        emit transport.ownerChanged(":1.1");QTRY_VERIFY(transport.snapshotToken!=0);
        emit transport.snapshotReceived(transport.snapshotToken,transport.owner,snapshotWire(false,0,0));
        QTRY_VERIFY(preferences.available());
        preferences.setLockOnScreenLock(true);QCOMPARE(transport.commits,1);
        QVERIFY(!preferences.lockOnScreenLock());QCOMPARE(preferences.status(),"Saving preferences…");
        emit transport.ownerChanged(QString());
        QVERIFY(!preferences.available());QVERIFY(!preferences.lockOnScreenLock());
        QVERIFY(preferences.status().startsWith("Save not confirmed"));
        const auto count=transport.commits;preferences.setLockAfterIdleMinutes(1441);QCOMPARE(transport.commits,count);
        emit transport.ownerChanged(":1.2");const auto stale=transport.snapshotToken;
        QTRY_VERIFY(transport.snapshotToken!=stale);
        emit transport.snapshotReceived(transport.snapshotToken,transport.owner,snapshotWire(true,5,1));
        QTRY_VERIFY(preferences.available());QVERIFY(preferences.lockOnScreenLock());QCOMPARE(preferences.lockAfterIdleMinutes(),5);
        emit transport.ownerChanged(QString());QVERIFY(preferences.lockOnScreenLock());QCOMPARE(preferences.lockAfterIdleMinutes(),5);
    }
};
QTEST_GUILESS_MAIN(PreferencesTest)
#include "tst_keyring_preferences.moc"
