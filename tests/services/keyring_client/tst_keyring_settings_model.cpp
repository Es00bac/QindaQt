// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/keyring_settings_model.h>
#include <QTest>
#include <cstring>
using namespace QindaQt::Services::KeyringClient;
using QindaQt::Apps::SettingsKeyring::KeyringSettingsModel;
class FakeGateway : public KeyringGateway {
public:
    bool ready=true;quint64 token=0;Request kind=Request::Collections;int calls=0,cancels=0;
    bool available() const override{return ready;}
    void request(quint64 t,Request k,const QString & = {},const QString & = {}) override{token=t;kind=k;++calls;}
    void cancel() override{++cancels;}
};
class ModelTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void lockedUnauthenticatedMetadataCannotRevealOrDelete(){
        FakeGateway gateway;KeyringSettingsModel model(gateway);model.reload();
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","collection"},{"label","Public collection"},{"locked",true}}}});
        QCOMPARE(gateway.kind,Request::Items);
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","item"},{"locked",true},{"indexAuthenticated",false}}}});
        const auto before=gateway.calls;model.revealItem("item");model.deleteItem("item");QCOMPARE(gateway.calls,before);
    }
    void departureAndOwnerLossFenceLateSecretAndClearVisibleBytes(){
        FakeGateway gateway;KeyringSettingsModel model(gateway);model.reload();
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","collection"},{"locked",false}}}});
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","item"},{"locked",false},{"indexAuthenticated",true}}}});
        model.revealItem("item");auto secret=std::make_shared<qindaqt::keyring::SecureBuffer>(5);
        std::memcpy(secret->bytes().data(),"value",5);emit gateway.secretReady(gateway.token,secret,"text/plain");
        QVERIFY(model.secretVisible());QCOMPARE(model.secretText(),"value");
        emit gateway.authorityChanged();QVERIFY(!model.secretVisible());QCOMPARE(secret->size(),0U);
        model.deactivate();auto late=std::make_shared<qindaqt::keyring::SecureBuffer>(5);
        emit gateway.secretReady(gateway.token,late,"text/plain");QCOMPARE(late->size(),0U);QVERIFY(!model.secretVisible());
    }
    void screenLockRetiresPendingRevealAndWipesLateReply(){
        FakeGateway gateway;KeyringSettingsModel model(gateway);model.reload();
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","collection"},{"locked",false}}}});
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","item"},{"locked",false},{"indexAuthenticated",true}}}});
        model.revealItem("item");const auto pending=gateway.token;
        emit gateway.secretsInvalidated();QVERIFY(!model.busy());QVERIFY(gateway.cancels>0);
        auto late=std::make_shared<qindaqt::keyring::SecureBuffer>(5);
        std::memcpy(late->bytes().data(),"value",5);
        emit gateway.secretReady(pending,late,"text/plain");
        QCOMPARE(late->size(),0U);QVERIFY(!model.secretVisible());
    }
    void copyingRequiresAnActualSinkAcknowledgement(){
        FakeGateway gateway;KeyringSettingsModel model(gateway);model.reload();
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","collection"},{"locked",false}}}});
        emit gateway.rowsReady(gateway.token,{{QVariantMap{{"path","item"},{"locked",false},{"indexAuthenticated",true}}}});
        model.copyItem("item");auto secret=std::make_shared<qindaqt::keyring::SecureBuffer>(5);
        emit gateway.secretReady(gateway.token,secret,"text/plain");QCOMPARE(model.status(),"Copy unavailable");
        model.acknowledgeCopy(true);QCOMPARE(model.status(),"Copied for 30 seconds");
    }
    void uncertainMutationHasNoSuccessOrAutomaticReplay(){
        FakeGateway gateway;KeyringSettingsModel model(gateway);model.createCollection("Fixture");
        const auto before=gateway.calls;emit gateway.actionFinished(gateway.token,false,"Persistence uncertain");
        QVERIFY(!model.busy());QCOMPARE(model.status(),"Persistence uncertain");QCOMPARE(gateway.calls,before);
    }
};
QTEST_GUILESS_MAIN(ModelTest)
#include "tst_keyring_settings_model.moc"
