// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
using namespace QindaQt::Services::SecretPortal;
using namespace qindaqt::keyring;
namespace {
bool failDirectorySync=false;
std::span<const unsigned char> password() {static const unsigned char value[]="synthetic-import-password";return {value,sizeof(value)-1};}
LegacyPortalRecord record(const QString &app="org.example.App",const QString &wallet="synthetic-network-wallet",std::size_t size=64) {
    LegacyPortalRecord value;value.appId=app;value.sourceWallet=wallet;value.secret=SecureBuffer(size);
    for(std::size_t index=0;index<size;++index) value.secret.bytes()[index]=static_cast<unsigned char>((index*17U)&255U);
    return value;
}
std::vector<LegacyPortalRecord> batch(LegacyPortalRecord value) {std::vector<LegacyPortalRecord> records;records.emplace_back(std::move(value));return records;}
QByteArray file(const QString &root) {QFile input(root+"/login.qkr");if(!input.open(QIODevice::ReadOnly)) return {};return input.readAll();}
bool exact(const Item *item) {
    if(!item || item->secret.size()!=64) return false;
    for(std::size_t index=0;index<64;++index) if(item->secret.bytes()[index]!=static_cast<unsigned char>((index*17U)&255U)) return false;
    return true;
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    struct stat state{};
    if(failDirectorySync && fstat(fd,&state)==0 && S_ISDIR(state.st_mode)) {errno=EIO;return -1;}
    return __real_fsync(fd);
}
class LegacyImportTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exactBytesProvenanceAndIdempotentRestart() {
        QTemporaryDir root;CollectionStore store(root.path().toStdString(),"login");QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);QCOMPARE(store.save(),StoreError::None);
        auto result=commitLegacyImport(store,batch(record()));QVERIFY(result.error==LegacyImportError::None);QCOMPARE(result.added,std::size_t{1});
        const auto id=applicationItemId("org.example.App").toStdString();QVERIFY(exact(store.item(id)));QVERIFY(matchesLegacySecret(*store.item(id),"org.example.App"));
        const auto sealed=file(root.path());result=commitLegacyImport(store,batch(record()));QCOMPARE(result.added,std::size_t{0});QCOMPARE(result.unchanged,std::size_t{1});QVERIFY(sealed==file(root.path()));
        store.lock();CollectionStore restored(root.path().toStdString(),"login");QCOMPARE(restored.load(),StoreError::None);QCOMPARE(restored.unlock(password()),StoreError::None);QVERIFY(exact(restored.item(id)));
        std::vector<LegacyPortalRecord> other;other.emplace_back(record("org.example.app"));other.emplace_back(record("org.example.Other"));result=commitLegacyImport(restored,std::move(other));QCOMPARE(result.added,std::size_t{2});QCOMPARE(restored.search({}).ids.size(),std::size_t{3});
    }
    void malformedDuplicateAndLockedPlansRejectWithoutMutation() {
        QTemporaryDir root;CollectionStore store(root.path().toStdString(),"login");QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);QCOMPARE(store.save(),StoreError::None);const auto sealed=file(root.path());
        for(std::size_t size:{std::size_t{0},std::size_t{32},std::size_t{63},std::size_t{65}}) {const auto result=commitLegacyImport(store,batch(record("org.example.App","wallet",size)));QVERIFY(result.error==LegacyImportError::InvalidInput);}
        for(const auto &app:QStringList{"","unknown","org.example.App\n"}) {const auto result=commitLegacyImport(store,batch(record(app)));QVERIFY(result.error==LegacyImportError::InvalidInput);}
        auto result=commitLegacyImport(store,batch(record("org.example.App","")));QVERIFY(result.error==LegacyImportError::InvalidInput);
        std::vector<LegacyPortalRecord> duplicate;duplicate.emplace_back(record());duplicate.emplace_back(record());result=commitLegacyImport(store,std::move(duplicate));QVERIFY(result.error==LegacyImportError::InvalidInput);
        QVERIFY(sealed==file(root.path()));QVERIFY(store.search({}).ids.empty());store.lock();result=commitLegacyImport(store,batch(record()));QCOMPARE(result.persistence,StoreError::Locked);
    }
    void wholeBatchConflictsPreserveFreshAndLegacyRecords() {
        QTemporaryDir root;CollectionStore store(root.path().toStdString(),"login");QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);
        auto fresh=newApplicationSecret("org.example.Fresh");QCOMPARE(store.put(std::move(fresh)),StoreError::None);QCOMPARE(store.save(),StoreError::None);
        const auto sealed=file(root.path());std::vector<LegacyPortalRecord> conflict;conflict.emplace_back(record("org.example.New"));conflict.emplace_back(record("org.example.Fresh"));
        auto result=commitLegacyImport(store,std::move(conflict));QVERIFY(result.error==LegacyImportError::Conflict);QVERIFY(sealed==file(root.path()));QVERIFY(!store.item(applicationItemId("org.example.New").toStdString()));
        result=commitLegacyImport(store,batch(record()));QCOMPARE(result.added,std::size_t{1});const auto original=file(root.path());
        result=commitLegacyImport(store,batch(record("org.example.App","different-wallet")));QVERIFY(result.error==LegacyImportError::Conflict);
        auto changed=record();changed.secret.bytes()[63]^=1;result=commitLegacyImport(store,batch(std::move(changed)));QVERIFY(result.error==LegacyImportError::Conflict);QVERIFY(original==file(root.path()));
    }
    void rejectedCommitPreservesOldMemoryAndDisk_data() {QTest::addColumn<bool>("throws");QTest::newRow("false")<<false;QTest::newRow("throw")<<true;}
    void rejectedCommitPreservesOldMemoryAndDisk() {
        QFETCH(bool,throws);QTemporaryDir root;bool permit=true;CollectionStore store(root.path().toStdString(),"login",[&]{if(!permit && throws) throw std::runtime_error("synthetic cancellation");return permit;});
        QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);auto fresh=newApplicationSecret("org.example.Retained");QCOMPARE(store.put(std::move(fresh)),StoreError::None);QCOMPARE(store.save(),StoreError::None);
        const auto id=applicationItemId("org.example.Retained").toStdString();const auto *retained=store.item(id);const auto original=file(root.path());permit=false;
        const auto result=commitLegacyImport(store,batch(record()));QVERIFY(result.error==LegacyImportError::Unavailable);QCOMPARE(result.persistence,StoreError::IoError);QVERIFY(store.item(id)==retained);QVERIFY(!store.locked());QVERIFY(original==file(root.path()));QVERIFY(!store.item(applicationItemId("org.example.App").toStdString()));
    }
    void genericBatchRejectsExistingDuplicateAndAggregateBounds() {
        QTemporaryDir root;CollectionStore store(root.path().toStdString(),"login");QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);QCOMPARE(store.put(newApplicationSecret("org.example.Retained")),StoreError::None);QCOMPARE(store.save(),StoreError::None);const auto original=file(root.path());
        std::vector<Item> existing;existing.emplace_back(newApplicationSecret("org.example.Retained"));QCOMPARE(store.insertBatchAndSave(std::move(existing)),StoreError::InvalidInput);
        std::vector<Item> duplicate;duplicate.emplace_back(newApplicationSecret("org.example.Duplicate"));duplicate.emplace_back(newApplicationSecret("org.example.Duplicate"));QCOMPARE(store.insertBatchAndSave(std::move(duplicate)),StoreError::InvalidInput);
        std::vector<Item> oversized;for(int index=0;index<4;++index) {Item item;item.id="large"+std::to_string(index);item.secret=SecureBuffer(1024*1024);oversized.emplace_back(std::move(item));}
        QCOMPARE(store.insertBatchAndSave(std::move(oversized)),StoreError::InvalidInput);QVERIFY(original==file(root.path()));QCOMPARE(store.search({}).ids.size(),std::size_t{1});
    }
    void unknownDurabilityReloadsLockedWithoutFalseSuccess() {
        QTemporaryDir root;CollectionStore store(root.path().toStdString(),"login");QCOMPARE(store.create(password(),{8192,1,1}),StoreError::None);QCOMPARE(store.save(),StoreError::None);
        failDirectorySync=true;const auto result=commitLegacyImport(store,batch(record()));failDirectorySync=false;
        QCOMPARE(result.persistence,StoreError::DurabilityUnknown);QVERIFY(result.error!=LegacyImportError::None);QCOMPARE(result.added,std::size_t{0});QVERIFY(store.locked());QVERIFY(!store.item(applicationItemId("org.example.App").toStdString()));
        QCOMPARE(store.unlock(password()),StoreError::None);QVERIFY(exact(store.item(applicationItemId("org.example.App").toStdString())));
    }
};
QTEST_GUILESS_MAIN(LegacyImportTest)
#include "tst_legacy_import.moc"
