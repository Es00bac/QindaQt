// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixtures.h"
#include <QTest>
#include <QTemporaryDir>
#include <sys/stat.h>
#include <unistd.h>

using namespace fixture;
class StorageFailuresTest : public QObject {
    Q_OBJECT
private:
    bool seed(const QString &temporary) {
        CollectionStore store(root(temporary), "login");
        return store.create(password(), fastKdf()) == StoreError::None
            && store.put(item()) == StoreError::None && store.save() == StoreError::None;
    }
    static void number(QByteArray &bytes, int offset, unsigned int value) {
        for (int i = 0; i < 4; ++i)
            bytes[offset+i] = static_cast<char>(value >> (24 - 8*i));
    }
private slots:
    void authenticatedTampering_data() {
        QTest::addColumn<int>("offset");
        QTest::newRow("valid-KDF-parameter") << 11;
        QTest::newRow("salt") << 20;
        QTest::newRow("nonce") << 36;
        QTest::newRow("search-key") << 48;
        QTest::newRow("index-item-id") << 96;
        QTest::newRow("index-digest") << 108;
        QTest::newRow("ciphertext") << -17;
        QTest::newRow("tag") << -1;
    }
    void authenticatedTampering() {
        QFETCH(int, offset);
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        auto original = read(path(temporary.path()));
        const auto index = offset >= 0 ? offset : static_cast<int>(original.size()) + offset;
        original[index] = static_cast<char>(original[index] ^ 1);
        QVERIFY(write(path(temporary.path()), original));
        CollectionStore store(root(temporary.path()), "login");
        const auto loaded = store.load();
        // Malformed ordering/framing can reject before KDF; valid tampered
        // metadata must fail authentication and never publish decrypted secrets.
        if (loaded == StoreError::None)
            QCOMPARE(store.unlock(password()), StoreError::AuthenticationFailed);
        else QCOMPARE(loaded, StoreError::InvalidFormat);
        QVERIFY(store.locked());
        QVERIFY(!store.secret("item-one"));
        QVERIFY(!store.search({}).authenticated);
    }
    void hostileHeaders_data() {
        QTest::addColumn<int>("offset");
        QTest::addColumn<unsigned int>("value");
        QTest::newRow("KDF-too-small") << 8 << 1U;
        QTest::newRow("KDF-memory-overflow") << 8 << 0xffffffffU;
        QTest::newRow("KDF-iterations-unbounded") << 12 << 0xffffffffU;
        QTest::newRow("KDF-lanes-unbounded") << 16 << 0xffffffffU;
        QTest::newRow("index-size-overflow") << 80 << 0xffffffffU;
        QTest::newRow("cipher-size-overflow") << 84 << 0xffffffffU;
        QTest::newRow("item-count-overflow") << 88 << 0xffffffffU;
        QTest::newRow("id-size-overflow") << 92 << 0xffffffffU;
        QTest::newRow("attribute-count-overflow") << 104 << 0xffffffffU;
    }
    void hostileHeaders() {
        QFETCH(int, offset);
        QFETCH(unsigned int, value);
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        auto bytes = read(path(temporary.path()));
        number(bytes, offset, value);
        QVERIFY(write(path(temporary.path()), bytes));
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.load(), StoreError::InvalidFormat);
        QVERIFY(store.locked());
        QVERIFY(!store.secret("item-one"));
    }
    void everyTruncationAndTrailingBytes() {
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        const auto original = read(path(temporary.path()));
        for (qsizetype size = 0; size < original.size(); ++size) {
            QVERIFY(write(path(temporary.path()), original.left(size)));
            CollectionStore store(root(temporary.path()), "login");
            QCOMPARE(store.load(), StoreError::InvalidFormat);
            QVERIFY(store.locked());
        }
        QVERIFY(write(path(temporary.path()), original + "trailing"));
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.load(), StoreError::InvalidFormat);
        auto unknown = original;
        unknown[7] = '2';
        QVERIFY(write(path(temporary.path()), unknown));
        QCOMPARE(store.load(), StoreError::InvalidFormat);
    }
    void atomicCancellationPreservesOriginalAndRekey() {
        QTemporaryDir temporary;
        bool allow = true;
        CollectionStore store(root(temporary.path()), "login", [&] { return allow; });
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        const auto original = read(path(temporary.path()));
        allow = false;
        QCOMPARE(store.put(item("new-item")), StoreError::None);
        QCOMPARE(store.save(), StoreError::IoError);
        QCOMPARE(read(path(temporary.path())), original);
        QCOMPARE(QDir(QString::fromStdString(root(temporary.path()))).entryList(
                     QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{"login.qkr"});
        QCOMPARE(store.rekey(otherPassword(), fastKdf()), StoreError::IoError);
        QCOMPARE(read(path(temporary.path())), original);
        QCOMPARE(store.load(), StoreError::None);
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(!store.secret("new-item"));
        QCOMPARE(store.unlock(otherPassword()), StoreError::AuthenticationFailed);
    }
    void throwingBarrierPreservesOriginal() {
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        const auto original = read(path(temporary.path()));
        CollectionStore store(root(temporary.path()), "login", []() -> bool { throw 1; });
        QCOMPARE(store.load(), StoreError::None);
        QCOMPARE(store.unlock(password()), StoreError::None);
        QCOMPARE(store.put(item("second")), StoreError::None);
        QCOMPARE(store.save(), StoreError::IoError);
        QCOMPARE(read(path(temporary.path())), original);
    }
    void permissionsAndSymlinkSafety() {
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        struct stat directory{}, file{};
        const auto directoryPath = root(temporary.path());
        const auto filePath = path(temporary.path()).toStdString();
        QCOMPARE(stat(directoryPath.c_str(), &directory), 0);
        QCOMPARE(stat(filePath.c_str(), &file), 0);
        QCOMPARE(directory.st_mode & 07777, mode_t(0700));
        QCOMPARE(file.st_mode & 07777, mode_t(0600));
        CollectionStore store(directoryPath, "login");
        QCOMPARE(chmod(filePath.c_str(), 0644), 0);
        QCOMPARE(store.load(), StoreError::IoError);
        QCOMPARE(chmod(filePath.c_str(), 0600), 0);
        QCOMPARE(chmod(directoryPath.c_str(), 0755), 0);
        QCOMPARE(store.load(), StoreError::IoError);
        QCOMPARE(chmod(directoryPath.c_str(), 0700), 0);
        const auto original = read(path(temporary.path()));
        const auto target = (temporary.path() + "/target").toStdString();
        QCOMPARE(rename(filePath.c_str(), target.c_str()), 0);
        QCOMPARE(symlink(target.c_str(), filePath.c_str()), 0);
        QCOMPARE(store.load(), StoreError::IoError);
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.save(), StoreError::IoError);
        QCOMPARE(read(QString::fromStdString(target)), original);
        const auto linkDir = (temporary.path() + "/linked-directory").toStdString();
        QCOMPARE(symlink(directoryPath.c_str(), linkDir.c_str()), 0);
        CollectionStore linked(linkDir, "login");
        QCOMPARE(linked.load(), StoreError::IoError);
        QCOMPARE(linked.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(linked.save(), StoreError::IoError);
    }
    void hardLinksAndCreateCannotOverwrite() {
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        const auto original = read(path(temporary.path()));
        CollectionStore duplicate(root(temporary.path()), "login");
        QCOMPARE(duplicate.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(duplicate.save(), StoreError::IoError);
        QCOMPARE(read(path(temporary.path())), original);
        const auto file = path(temporary.path()).toStdString();
        const auto alias = (temporary.path() + "/hardlink").toStdString();
        QCOMPARE(link(file.c_str(), alias.c_str()), 0);
        QCOMPARE(duplicate.load(), StoreError::IoError);
        QCOMPARE(unlink(alias.c_str()), 0);
        QCOMPARE(duplicate.load(), StoreError::None);
    }
    void oversizeFileAndFifoFailWithoutBlocking() {
        QTemporaryDir temporary;
        QVERIFY(seed(temporary.path()));
        QFile file(path(temporary.path()));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.resize(8 * 1024 * 1024 + 1));
        file.close();
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.load(), StoreError::IoError);
        const auto name = path(temporary.path()).toStdString();
        QCOMPARE(unlink(name.c_str()), 0);
        QCOMPARE(mkfifo(name.c_str(), 0600), 0);
        QCOMPARE(store.load(), StoreError::IoError);
    }
};
QTEST_GUILESS_MAIN(StorageFailuresTest)
#include "tst_storage_failures.moc"
