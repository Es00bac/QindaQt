// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/collection_import.h>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
using namespace qindaqt::keyring;
namespace {
int failCatalog=0;bool pendingDirectory=false;bool admitted=true;
SecureBuffer password() {SecureBuffer p(8);std::fill(p.bytes().begin(),p.bytes().end(),0x42);return p;}
class Passwords final:public CollectionImportPasswords {
public:
    CollectionImportError error=CollectionImportError::None;
    bool wrong=false;int calls=0;
    ImportPassword take(const QString &,const QString &,bool) override {
        ++calls;if(error!=CollectionImportError::None) return {error,SecureBuffer{}};
        auto p=password();if(wrong) p.bytes()[0]=0x43;return {CollectionImportError::None,std::move(p)};
    }
};
CollectionImportBatch batch(int offset=0) {
    CollectionImportBatch result;
    for(int n=0;n<2;++n) {
        CollectionImportRecord c;c.id=QString("legacy%1").arg(n+offset);c.label=QString("Collection %1").arg(n+offset);
        c.sourceKind="secret-service";c.sourceId="/legacy/"+c.id;c.created=10;c.modified=20;
        Item item;item.id="item";item.metadata.label="Synthetic";item.metadata.created=11;item.metadata.modified=12;
        item.attributes={{"source","synthetic"}};item.secret=SecureBuffer(64);
        for(std::size_t i=0;i<64;++i) item.secret.bytes()[i]=static_cast<unsigned char>(i+static_cast<std::size_t>(n+offset));
        c.items.push_back(std::move(item));result.collections.push_back(std::move(c));
    }
    result.aliases["default"]=result.collections.front().id;return result;
}
QByteArray disk(const QString &path) {QFile file(path);if(!file.open(QIODevice::ReadOnly)) return {};return file.readAll();}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    struct stat info{};char first=0;bool catalog=false;
    if(failCatalog && fstat(fd,&info)==0 && S_ISREG(info.st_mode)) {
        char path[64];std::snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);
        const int readOnly=open(path,O_RDONLY|O_CLOEXEC);
        catalog=readOnly>=0 && pread(readOnly,&first,1,0)==1 && first=='{';
        if(readOnly>=0) close(readOnly);
    } else fstat(fd,&info);
    if(catalog) {
        if(failCatalog==1) {failCatalog=0;errno=EIO;return -1;}
        if(failCatalog==2) pendingDirectory=true;
        if(failCatalog==3) {admitted=false;failCatalog=0;}
    }
    if(pendingDirectory && S_ISDIR(info.st_mode)) {pendingDirectory=false;failCatalog=0;errno=EIO;return -1;}
    return __real_fsync(fd);
}
class CollectionImportTest final:public QObject {
    Q_OBJECT
private slots:
    void init() {admitted=true;failCatalog=0;pendingDirectory=false;}
    void roundTripIdempotence() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());Passwords passwords;
        auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        auto receipt=catalog->commit(batch(),passwords,[]{return admitted;});
        QCOMPARE(receipt.error,CollectionImportError::None);QCOMPARE(receipt.collectionsAdded,std::size_t(2));
        QCOMPARE(receipt.itemsAdded,std::size_t(2));const auto bytes=disk(temporary.path()+"/catalog.json");
        const auto sealed=disk(temporary.path()+"/legacy0.qkr");QVERIFY(!sealed.isEmpty());
        catalog.reset();catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        receipt=catalog->commit(batch(),passwords,[]{return admitted;});
        QCOMPARE(receipt.error,CollectionImportError::None);QCOMPARE(receipt.collectionsAdded,std::size_t(0));
        QCOMPARE(receipt.collectionsUnchanged,std::size_t(2));QCOMPARE(disk(temporary.path()+"/catalog.json"),bytes);
        QCOMPARE(disk(temporary.path()+"/legacy0.qkr"),sealed);
        CollectionStore stored(temporary.path().toStdString(),"legacy0");QCOMPARE(stored.load(),StoreError::None);
        QVERIFY(stored.locked());auto p=password();QCOMPARE(stored.unlock(p.bytes()),StoreError::None);
        const auto item=stored.item("item");QVERIFY(item);QVERIFY(item->secret.size()==64);
        bool exact=true;for(std::size_t i=0;i<64;++i) exact=exact && item->secret.bytes()[i]==i;QVERIFY(exact);
    }
    void wholeBatchConflict() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::None);
        const auto before=disk(temporary.path()+"/catalog.json");auto changed=batch();
        changed.collections[1].items[0].secret.bytes()[0]=0x99;
        auto extra=batch(2);changed.collections.push_back(std::move(extra.collections[0]));
        QCOMPARE(catalog->commit(std::move(changed),passwords,[]{return admitted;}).error,CollectionImportError::Conflict);
        QCOMPARE(disk(temporary.path()+"/catalog.json"),before);QVERIFY(!QFile::exists(temporary.path()+"/legacy2.qkr"));
        passwords.wrong=true;
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::AuthenticationFailed);
        passwords.wrong=false;
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::None);
    }
    void cancellationAndBounds() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        const auto before=disk(temporary.path()+"/catalog.json");passwords.error=CollectionImportError::Cancelled;
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::Cancelled);
        passwords.error=CollectionImportError::None;auto invalid=batch();invalid.collections[1].id=invalid.collections[0].id;
        const auto calls=passwords.calls;
        QCOMPARE(catalog->commit(std::move(invalid),passwords,[]{return admitted;}).error,CollectionImportError::InvalidInput);
        QCOMPARE(passwords.calls,calls);invalid=batch();invalid.collections[0].items[0].secret=SecureBuffer(1024*1024+1);
        QCOMPARE(catalog->commit(std::move(invalid),passwords,[]{return admitted;}).error,CollectionImportError::InvalidInput);
        admitted=false;QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::OwnerLost);
        QCOMPARE(disk(temporary.path()+"/catalog.json"),before);QCOMPARE(QDir(temporary.path()).entryList({"*.qkr"}).size(),0);
    }
    void queuedRetirementBeforePublication() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        const auto before=disk(temporary.path()+"/catalog.json");bool staged=false,nested=false;
        const auto receipt=catalog->commit(batch(),passwords,[]{return admitted;},[&]{
            if(!nested) {
                nested=true;
                const auto denied=catalog->commit(batch(),passwords,[]{return admitted;});
                QCOMPARE(denied.error,CollectionImportError::Unavailable);
            }
            if(!staged && !QDir(temporary.path()).entryList({"*.qkr"}).empty()) {
                staged=true;QTimer::singleShot(0,[]{admitted=false;});
            }
            QCoreApplication::processEvents();
        });
        QVERIFY(staged && nested);QCOMPARE(receipt.error,CollectionImportError::OwnerLost);
        QCOMPARE(disk(temporary.path()+"/catalog.json"),before);QCOMPARE(QDir(temporary.path()).entryList({"*.qkr"}).size(),0);
        admitted=true;
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;},[]{QCoreApplication::processEvents();}).error,CollectionImportError::None);
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;},[]{QCoreApplication::processEvents();}).collectionsUnchanged,std::size_t(2));
    }
    void checkpointExceptionRollsBackStaging() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        const auto before=disk(temporary.path()+"/catalog.json");bool staged=false;
        const auto receipt=catalog->commit(batch(),passwords,[]{return admitted;},[&]{
            if(!QDir(temporary.path()).entryList({"*.qkr"}).empty()) {staged=true;throw 7;}
        });
        QVERIFY(staged);QCOMPARE(receipt.error,CollectionImportError::Unavailable);
        QCOMPARE(disk(temporary.path()+"/catalog.json"),before);QCOMPARE(QDir(temporary.path()).entryList({"*.qkr"}).size(),0);
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::None);
    }
    void catalogFailure_data() {QTest::addColumn<int>("failure");QTest::newRow("file-fsync")<<1;QTest::newRow("owner-before-rename")<<3;}
    void catalogFailure() {
        QFETCH(int,failure);QTemporaryDir temporary;Passwords passwords;
        auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});const auto before=disk(temporary.path()+"/catalog.json");
        failCatalog=failure;auto receipt=catalog->commit(batch(),passwords,[]{return admitted;});
        QCOMPARE(receipt.error,failure==3?CollectionImportError::OwnerLost:CollectionImportError::Unavailable);
        QCOMPARE(receipt.collectionsAdded,std::size_t(0));QCOMPARE(disk(temporary.path()+"/catalog.json"),before);
        QCOMPARE(QDir(temporary.path()).entryList({"*.qkr"}).size(),0);
        admitted=true;catalog.reset();catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::None);
    }
    void durabilityUnknown() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        failCatalog=2;const auto receipt=catalog->commit(batch(),passwords,[]{return admitted;});
        QCOMPARE(receipt.error,CollectionImportError::DurabilityUnknown);QCOMPARE(receipt.persistence,StoreError::DurabilityUnknown);
        QCOMPARE(receipt.collectionsAdded,std::size_t(0));
        // Public storage observes only sealed locked files after uncertain catalog publication.
        CollectionStore sealed(temporary.path().toStdString(),"legacy0");QCOMPARE(sealed.load(),StoreError::None);QVERIFY(sealed.locked());
        catalog.reset();catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        const auto retry=catalog->commit(batch(),passwords,[]{return admitted;});
        QCOMPARE(retry.error,CollectionImportError::None);QCOMPARE(retry.collectionsUnchanged,std::size_t(2));
    }
    void capacityAndEmptyCollections() {
        QTemporaryDir temporary;Passwords passwords;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        CollectionImportBatch full;
        for(int n=0;n<64;++n) {
            CollectionImportRecord c;c.id=QString("empty%1").arg(n);c.label=c.id;c.sourceKind="kwallet";c.sourceId=c.id;
            full.collections.push_back(std::move(c));
        }
        const auto receipt=catalog->commit(std::move(full),passwords,[]{return admitted;});
        QCOMPARE(receipt.error,CollectionImportError::None);QCOMPARE(receipt.collectionsAdded,std::size_t(64));
        const auto calls=passwords.calls;
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::Capacity);
        QCOMPARE(passwords.calls,calls);QCOMPARE(QDir(temporary.path()).entryList({"*.qkr"}).size(),64);
    }
    void metadataConflict_data() {
        QTest::addColumn<int>("field");QTest::newRow("source")<<0;QTest::newRow("label")<<1;
        QTest::newRow("dates")<<2;QTest::newRow("alias")<<3;QTest::newRow("attributes")<<4;
    }
    void metadataConflict() {
        QFETCH(int,field);QTemporaryDir temporary;Passwords passwords;
        auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        QCOMPARE(catalog->commit(batch(),passwords,[]{return admitted;}).error,CollectionImportError::None);
        const auto before=disk(temporary.path()+"/catalog.json");auto changed=batch();
        if(field==0) changed.collections[0].sourceId="/changed";
        if(field==1) changed.collections[0].label="Changed";
        if(field==2) changed.collections[0].modified=21;
        if(field==3) changed.aliases["default"]="legacy1";
        if(field==4) changed.collections[0].items[0].attributes["source"]="changed";
        QCOMPARE(catalog->commit(std::move(changed),passwords,[]{return admitted;}).error,CollectionImportError::Conflict);
        QCOMPARE(disk(temporary.path()+"/catalog.json"),before);
    }
    void orphanRecoveryAndExclusiveWriter() {
        QTemporaryDir temporary;auto catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        QVERIFY_EXCEPTION_THROWN(openCollectionImportCatalog(temporary.path()),std::runtime_error);
        auto p=password();CollectionStore orphan(temporary.path().toStdString(),"orphan");
        QCOMPARE(orphan.create(p.bytes(),{8192,1,1}),StoreError::None);QCOMPARE(orphan.save(),StoreError::None);
        catalog.reset();catalog=openCollectionImportCatalog(temporary.path(),{8192,1,1});
        QVERIFY(!QFile::exists(temporary.path()+"/orphan.qkr"));
    }
};
QTEST_GUILESS_MAIN(CollectionImportTest)
#include "tst_collection_import.moc"
