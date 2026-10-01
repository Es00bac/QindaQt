// SPDX-License-Identifier: GPL-3.0-or-later
#include "lock_policy.h"
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_service/settings_repository.h>
#include <qindaqt/settings/settings_document.h>
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
using namespace qindaqt::keyring::service;
namespace sc=QindaQt::Services::SettingsClient;
namespace sp=QindaQt::Services::SettingsProtocol;
namespace sl=QindaQt::Services::SessionLockState;
class PolicyTransport : public sc::SettingsTransport {
public:
    quint64 token=0;QString owner;
    bool start(QString *) override{return true;}
    void stop() override{}
    void requestSnapshot(quint64 t,const QString &o,const QStringList &) override{token=t;owner=o;}
    void commit(quint64,const QString &,const QString &,quint64,const QVariantList &) override{}
    void requestActivation() override{}
    void deliver(bool lock,const QVariant &idle,quint64 revision=0){
        using sp::WireContract;
        const QVariantMap wire{{QLatin1StringView(WireContract::FieldStatus),quint32(0)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch),"epoch"},{QLatin1StringView(WireContract::FieldRevision),revision},
            {QLatin1StringView(WireContract::FieldValues),QVariantMap{{"keyring.lockOnScreenLock",lock},{"keyring.lockAfterIdleMinutes",idle}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),QVariantMap{{"keyring.lockOnScreenLock","user-overrides"},{"keyring.lockAfterIdleMinutes","user-overrides"}}},
            {QLatin1StringView(WireContract::FieldMessage),QString()}};
        emit snapshotReceived(token,owner,wire);
    }
};
class Screen : public LockObservation {
public:
    sl::LockState value=sl::LockState::Unlocked;
    sl::LockState state() const override{return value;}
    void set(sl::LockState next){value=next;emit changed();}
};
class Idle : public IdleObservation {
public:
    bool ready=true,active=false;int timeout=0;
    void setTimeout(int value) override{timeout=value;}
    bool available() const override{return ready;}
    bool idle() const override{return active;}
    // AGENT-CONTRACT: the public idle port's new generation never retains
    // prior idle; revocation clears availability as well as the idle sample.
    void refresh() override{active=false;publish();}
    void revoke() override{ready=false;active=false;publish();}
    void publish(){emit changed();}
};
class PolicyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void screenLossAndSettingsLossRetainConfirmedFailClosedPolicy(){
        PolicyTransport transport;sc::SettingsClient settings(transport,{"keyring.lockOnScreenLock","keyring.lockAfterIdleMinutes"},{.requestTimeoutMilliseconds=100,.debounceMilliseconds=0,.retryMilliseconds={10}});
        Screen screen;Idle idle;int locks=0;
        KeyringLockPolicy policy(settings,screen,idle,[&]{++locks;policy.enforce();});
        QVERIFY(settings.start());emit transport.ownerChanged(":1.1");QTRY_VERIFY(transport.token!=0);
        transport.deliver(true,0);QTRY_COMPARE(settings.state(),sc::ClientState::Ready);
        QCOMPARE(locks,0);screen.set(sl::LockState::Locking);QCOMPARE(locks,1);
        screen.set(sl::LockState::Unlocked);QCOMPARE(locks,1);
        screen.set(sl::LockState::Unknown);QCOMPARE(locks,2);QVERIFY(!policy.status().value("ScreenLockAvailable").toBool());
        emit transport.ownerChanged(QString());QVERIFY(!policy.status().value("SettingsAvailable").toBool());
        screen.set(sl::LockState::Locked);QCOMPARE(locks,3);QVERIFY(policy.status().value("LockOnScreenLock").toBool());
    }
    void trueIdleOnlyAndUnavailableObservationIsExplicit(){
        PolicyTransport transport;sc::SettingsClient settings(transport,{"keyring.lockOnScreenLock","keyring.lockAfterIdleMinutes"},{.requestTimeoutMilliseconds=100,.debounceMilliseconds=0,.retryMilliseconds={10}});
        Screen screen;Idle idle;int locks=0;KeyringLockPolicy policy(settings,screen,idle,[&]{++locks;});
        QVERIFY(settings.start());emit transport.ownerChanged(":1.1");QTRY_VERIFY(transport.token!=0);
        transport.deliver(false,5);QTRY_COMPARE(settings.state(),sc::ClientState::Ready);
        QCOMPARE(idle.timeout,300000);QTest::qWait(20);QCOMPARE(locks,0);
        idle.active=true;idle.publish();QCOMPARE(locks,1);idle.active=false;idle.publish();QCOMPARE(locks,1);
        idle.ready=false;idle.publish();QCOMPARE(locks,2);QVERIFY(!idle.idle());QVERIFY(!policy.status().value("IdleAvailable").toBool());
        settings.refresh();const auto old=transport.token;QTRY_VERIFY(transport.token!=old);
        transport.deliver(false,0,1);QTRY_COMPARE(idle.timeout,0);
        idle.active=true;idle.publish();QCOMPARE(locks,2);
    }
    void typedPolicyRoundtripAndFailedSaveAreAtomic(){
        using namespace QindaQt::Settings;
        using QindaQt::Services::SettingsService::SettingsRepository;
        QString error;const auto schema=SettingsSchema::fromFile(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json",nullptr,&error);
        QVERIFY2(schema.has_value(),qPrintable(error));
        QCOMPARE(toString(SettingDomain::Keyring),"keyring");QCOMPARE(domainKeyPrefix(SettingDomain::Keyring),"keyring");
        QTemporaryDir directory;const auto path=directory.filePath("policy.json");
        SettingsRepository repository(LayeredSettings(*schema),path,"epoch");
        const auto result=repository.commitUserOverrides(0,{{.key="keyring.lockOnScreenLock",.remove=false,.value=true},{.key="keyring.lockAfterIdleMinutes",.remove=false,.value=5}});
        QVERIFY(result.ok());const auto disk=SettingsFileStore::load(path,*schema);QVERIFY(disk.ok);
        QCOMPARE(disk.document.values.value("keyring.lockOnScreenLock").toBool(),true);QCOMPARE(disk.document.values.value("keyring.lockAfterIdleMinutes").toInt(),5);
        LayeredSettings restored(*schema);QVERIFY(restored.replaceLayer(SettingLayer::UserOverrides,disk.document.values).ok());
        QCOMPARE(restored.value("keyring.lockAfterIdleMinutes").toInt(),5);
        const auto bad=repository.commitUserOverrides(1,{{.key="keyring.lockAfterIdleMinutes",.remove=false,.value=1441}});
        QVERIFY(!bad.ok());QCOMPARE(repository.revision(),quint64(1));
        const auto blocked=directory.filePath("blocked");QVERIFY(QDir().mkdir(blocked));
        SettingsRepository failing(LayeredSettings(*schema),blocked,"epoch");
        const auto failure=failing.commitUserOverrides(0,{{.key="keyring.lockOnScreenLock",.remove=false,.value=true}});
        QVERIFY(!failure.ok());QCOMPARE(failing.revision(),quint64(0));
        QVERIFY(!failing.snapshot({"keyring.lockOnScreenLock"}).values.value("keyring.lockOnScreenLock").toBool());
    }
};
QTEST_GUILESS_MAIN(PolicyTest)
#include "tst_lock_policy.moc"
