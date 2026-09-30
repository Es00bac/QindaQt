// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Services::SecretPortal;
class PolicyTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void identityAndOptionsAreBounded() {
        QVERIFY(validApplicationId("org.example.App"));QVERIFY(validApplicationId("org.example_app-1.App"));
        for(const auto &id:QStringList{"","app","../app","org..App","org.a/Other",QString(256,'a'),"org.example.App\n"}) QVERIFY(!validApplicationId(id));
        QVERIFY(applicationItemId("org.example.App")!=applicationItemId("org.example.app"));
        QVERIFY(applicationItemId("org.example.App").size()==64);QVERIFY(applicationItemId("").isEmpty());
        QVERIFY(validOptions({}));QVERIFY(validOptions({{"token","opaque"}}));
        QVERIFY(!validOptions({{"token",2}}));QVERIFY(!validOptions({{"token",QString(1025,'a')}}));QVERIFY(!validOptions({{"app_id","org.other.App"}}));
        QVERIFY(validRequestHandle("/org/freedesktop/portal/desktop/request/1_2/handle"));
        QVERIFY(!validRequestHandle("/org/freedesktop/portal/desktop/request/1_2/handle/extra"));
    }
    void persistentAppRecordsAreDistinctStableAndCollisionChecked() {
        using namespace qindaqt::keyring;
        QTemporaryDir root;QVERIFY(root.isValid());
        const auto password=std::span<const unsigned char>(reinterpret_cast<const unsigned char *>("synthetic-password"),18);
        CollectionStore store(root.path().toStdString(),"login");QVERIFY(store.create(password)==StoreError::None);
        auto first=newApplicationSecret("org.example.App"),other=newApplicationSecret("org.example.Other");
        QVERIFY(matchesApplicationSecret(first,"org.example.App"));QVERIFY(!matchesApplicationSecret(first,"org.example.Other"));
        QVERIFY(!std::equal(first.secret.bytes().begin(),first.secret.bytes().end(),other.secret.bytes().begin()));
        SecureBuffer expected(SecretSize);std::copy(first.secret.bytes().begin(),first.secret.bytes().end(),expected.bytes().begin());
        QVERIFY(store.put(std::move(first))==StoreError::None);QVERIFY(store.put(std::move(other))==StoreError::None);QVERIFY(store.save()==StoreError::None);store.lock();
        CollectionStore restarted(root.path().toStdString(),"login");QVERIFY(restarted.load()==StoreError::None);QVERIFY(restarted.unlock(password)==StoreError::None);
        const auto *item=restarted.item(applicationItemId("org.example.App").toStdString());QVERIFY(item);
        QVERIFY(matchesApplicationSecret(*item,"org.example.App"));QVERIFY(std::equal(expected.bytes().begin(),expected.bytes().end(),item->secret.bytes().begin()));
        auto forged=newApplicationSecret("org.example.App");forged.attributes["qindaqt.portal.application"]="org.other.App";QVERIFY(!matchesApplicationSecret(forged,"org.example.App"));
        forged.attributes["qindaqt.portal.application"]="org.example.App";forged.metadata.contentType="text/plain";QVERIFY(!matchesApplicationSecret(forged,"org.example.App"));
    }
};
QTEST_GUILESS_MAIN(PolicyTest)
#include "tst_policy.moc"
