// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/legacy_snapshot.h>
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <QtTest>
#include <set>
#include <algorithm>
using namespace qindaqt::keyring;
namespace {
LegacyItemSnapshot item(QString id,QString folder={},int type=0,std::size_t size=4) {
    LegacyItemSnapshot value;value.sourceId=id;value.folder=folder;value.entryType=type;value.metadata.label=id.toStdString();
    value.secret=SecureBuffer(size);for(std::size_t n=0;n<size;++n) value.secret.bytes()[n]=static_cast<unsigned char>(n);
    return value;
}
std::vector<LegacySnapshot> sources() {
    std::vector<LegacySnapshot> all;
    LegacySnapshot ss;ss.kind="secret-service";LegacyCollectionSnapshot login;
    login.sourceId="/org/freedesktop/secrets/collection/login";login.label="Original login";login.created=1;login.modified=2;
    login.items.push_back(item("/original/item"));login.items.back().attributes={{"app","gogcli"}};
    ss.aliases["default"]=login.sourceId;ss.collections.push_back(std::move(login));
    LegacyCollectionSnapshot volatileSource;volatileSource.sourceId="/org/freedesktop/secrets/collection/session";volatileSource.label="Legacy session";
    ss.collections.push_back(std::move(volatileSource));all.push_back(std::move(ss));
    LegacySnapshot kw;kw.kind="kwallet";LegacyCollectionSnapshot wallet;wallet.sourceId=wallet.label="kdewallet";
    wallet.items.push_back(item("same","Passwords",1));wallet.items.push_back(item("same","Maps",3));wallet.items.push_back(item("same","Streams",2));
    wallet.items.push_back(item("org.example.Legacy","xdg-desktop-portal",2,64));kw.collections.push_back(std::move(wallet));all.push_back(std::move(kw));return all;
}
}
class LegacyPlanTest final:public QObject {
    Q_OBJECT
private slots:
    void preservesEveryEntryAndDerivesPortal() {
        auto plan=planLegacyCollections(sources());QCOMPARE(plan.error,CollectionImportError::None);QCOMPARE(plan.batch.collections.size(),std::size_t(3));
        const CollectionImportRecord *login=nullptr,*wallet=nullptr,*session=nullptr;
        for(const auto &c:plan.batch.collections) {if(c.id=="login") login=&c;else if(c.id=="kdewallet") wallet=&c;else session=&c;}
        QVERIFY(login && wallet && session);QVERIFY(session->id!="session");QVERIFY(session->sourceId.endsWith("/session"));
        QCOMPARE(login->label,QString("Original login"));QCOMPARE(login->created,std::uint64_t(1));QCOMPARE(login->modified,std::uint64_t(2));
        QCOMPARE(plan.batch.aliases.value("default"),QString("login"));QCOMPARE(wallet->items.size(),std::size_t(4));
        std::set<std::string> ids;for(const auto &entry:wallet->items) QVERIFY(ids.insert(entry.id).second);
        QCOMPARE(wallet->items[0].attributes.at("qindaqt.import.kwallet.type"),std::string("1"));
        QCOMPARE(wallet->items[1].attributes.at("qindaqt.import.kwallet.type"),std::string("3"));
        QCOMPARE(wallet->items[2].attributes.at("qindaqt.import.kwallet.type"),std::string("2"));
        const auto portalId=QindaQt::Services::SecretPortal::applicationItemId("org.example.Legacy").toStdString();
        const Item *portal=nullptr;for(const auto &entry:login->items) if(entry.id==portalId) portal=&entry;
        QVERIFY(portal);QVERIFY(portal->secret.size()==64);bool exact=true;
        for(std::size_t n=0;n<64;++n) exact=exact && portal->secret.bytes()[n]==n;
        QVERIFY(exact);
        QCOMPARE(portal->metadata.created,std::uint64_t(0));QCOMPARE(portal->metadata.modified,std::uint64_t(0));
        QCOMPARE(portal->attributes.at("qindaqt.portal.source.wallet"),std::string("kdewallet"));
    }
    void deterministicMappingsAndFixedLogin() {
        auto input=sources();input[1].collections[0].sourceId=input[1].collections[0].label="login";
        auto first=planLegacyCollections(std::move(input));QCOMPARE(first.error,CollectionImportError::None);
        input=sources();input[1].collections[0].sourceId=input[1].collections[0].label="login";std::reverse(input.begin(),input.end());
        auto second=planLegacyCollections(std::move(input));QCOMPARE(second.error,CollectionImportError::None);
        QCOMPARE(first.batch.collections.size(),second.batch.collections.size());
        for(std::size_t n=0;n<first.batch.collections.size();++n) {
            QCOMPARE(first.batch.collections[n].id,second.batch.collections[n].id);
            QVERIFY(first.batch.collections[n].id.size()<=40);QVERIFY(first.batch.collections[n].id!="session");
        }
    }
    void nativeAliasGrammarIsIndependentOfFileIds() {
        auto input=sources();const auto target=input[0].collections[0].sourceId;
        input[0].aliases[QString(60,QChar('a'))]=target;input[0].aliases["session"]=target;
        auto plan=planLegacyCollections(std::move(input));QCOMPARE(plan.error,CollectionImportError::None);
        QCOMPARE(plan.batch.aliases.value(QString(60,QChar('a'))),QString("login"));
        QCOMPARE(plan.batch.aliases.value("session"),QString("login"));
    }
    void wholePlanRejection_data() {
        QTest::addColumn<int>("bad");QTest::newRow("duplicate-entry")<<0;QTest::newRow("oversize")<<1;
        QTest::newRow("duplicate-app")<<2;QTest::newRow("wrong-portal-size")<<3;QTest::newRow("missing-alias-target")<<4;
    }
    void wholePlanRejection() {
        QFETCH(int,bad);auto input=sources();auto &wallet=input[1].collections[0];
        if(bad==0) wallet.items.push_back(item("same","Passwords",1));
        if(bad==1) wallet.items[0].secret=SecureBuffer(1024*1024+1);
        if(bad==2) {LegacyCollectionSnapshot another;another.sourceId=another.label="other";
            another.items.push_back(item("org.example.Legacy","xdg-desktop-portal",2,64));input[1].collections.push_back(std::move(another));}
        if(bad==3) wallet.items.back().secret=SecureBuffer(63);
        if(bad==4) input[0].aliases["bad"]="/missing";
        auto result=planLegacyCollections(std::move(input));QVERIFY(result.error!=CollectionImportError::None);QVERIFY(result.batch.collections.empty());QVERIFY(result.batch.aliases.empty());
    }
};
QTEST_GUILESS_MAIN(LegacyPlanTest)
#include "tst_legacy_plan.moc"
