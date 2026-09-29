// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixtures.h"
#include <QTest>
#include <QTemporaryDir>

using namespace fixture;
class CollectionStoreTest : public QObject {
    Q_OBJECT
private slots:
    void encryptedRoundTrip_data() {
        QTest::addColumn<QString>("collection");
        QTest::newRow("default-login") << QString("login");
        QTest::newRow("custom-fixture") << QString("custom-collection");
    }
    void encryptedRoundTrip() {
        QFETCH(QString, collection);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        CollectionStore store(root(temporary.path()), collection.toStdString());
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        const auto ciphertext = read(path(temporary.path(), collection));
        QVERIFY(!ciphertext.contains("fixture-secret-only"));
        QVERIFY(!ciphertext.contains("fixture label"));
        QVERIFY(!ciphertext.contains("fixture-account"));
        store.lock();
        QVERIFY(store.locked());
        QVERIFY(!store.secret("item-one"));
        QCOMPARE(store.save(), StoreError::Locked);
        QCOMPARE(store.put(item("another")), StoreError::Locked);
        const auto lockedMatch = store.search({{"account", "fixture-account"}});
        QVERIFY(lockedMatch.authenticated);
        QCOMPARE(lockedMatch.ids, std::vector<std::string>{"item-one"});
        CollectionStore reopened(root(temporary.path()), collection.toStdString());
        QCOMPARE(reopened.load(), StoreError::None);
        QVERIFY(!reopened.search({{"account", "fixture-account"}}).authenticated);
        QCOMPARE(reopened.search({{"account", "fixture-account"}}).ids, lockedMatch.ids);
        QCOMPARE(reopened.unlock(password()), StoreError::None);
        const auto *value = reopened.secret("item-one");
        QVERIFY(value);
        const std::string expected = "fixture-secret-only";
        QCOMPARE(value->size(), expected.size());
        QVERIFY(std::equal(value->bytes().begin(), value->bytes().end(), expected.begin()));
        QCOMPARE(reopened.item("item-one")->metadata.label, std::string("fixture label"));
        QCOMPARE(reopened.item("item-one")->metadata.created, std::uint64_t(17));
        QCOMPARE(reopened.item("item-one")->metadata.modified, std::uint64_t(23));
        QVERIFY(reopened.search({}).authenticated);
    }
    void wrongPasswordDropsPriorSecrets() {
        QTemporaryDir temporary;
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        QCOMPARE(store.unlock(otherPassword()), StoreError::AuthenticationFailed);
        QVERIFY(store.locked());
        QVERIFY(!store.secret("item-one"));
        QVERIFY(!store.item("item-one"));
        QVERIFY(!store.search({}).authenticated);
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(store.secret("item-one"));
    }
    void emptySecretAndSubsetSearch() {
        QTemporaryDir temporary;
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item("empty", "")), StoreError::None);
        auto second = item("second");
        second.attributes["service"] = "other-service";
        QCOMPARE(store.put(std::move(second)), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        store.lock();
        QCOMPARE(store.search({}).ids.size(), std::size_t(2));
        QCOMPARE(store.search({{"service", "fixture-service"}}).ids, std::vector<std::string>{"empty"});
        QCOMPARE(store.search({{"service", "other-service"}, {"account", "fixture-account"}}).ids,
                 std::vector<std::string>{"second"});
        QVERIFY(store.search({{"service", "missing"}}).ids.empty());
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(store.secret("empty"));
        QCOMPARE(store.secret("empty")->size(), std::size_t(0));
        QCOMPARE(store.erase("empty"), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        QCOMPARE(store.load(), StoreError::None);
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(!store.secret("empty"));
        QVERIFY(store.secret("second"));
    }
    void freshNonceAndSaltRekey() {
        QTemporaryDir temporary;
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        const auto first = read(path(temporary.path()));
        QCOMPARE(store.save(), StoreError::None);
        const auto second = read(path(temporary.path()));
        QVERIFY(first.mid(36,12) != second.mid(36,12));
        QCOMPARE(first.mid(20,16), second.mid(20,16));
        QCOMPARE(store.rekey(otherPassword(), fastKdf()), StoreError::None);
        const auto third = read(path(temporary.path()));
        QVERIFY(second.mid(20,16) != third.mid(20,16));
        QCOMPARE(second.mid(48,32), third.mid(48,32)); // Independent search key.
        QCOMPARE(store.load(), StoreError::None);
        QCOMPARE(store.unlock(password()), StoreError::AuthenticationFailed);
        QCOMPARE(store.unlock(otherPassword()), StoreError::None);
        QVERIFY(store.secret("item-one"));
    }
    void defaultKdfWorks() {
        QTemporaryDir temporary;
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        const auto file = read(path(temporary.path()));
        QCOMPARE(file.mid(8,12).toHex(), QByteArray("000100000000000300000001"));
        QCOMPARE(store.load(), StoreError::None);
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(store.secret("item-one"));
    }
    void lockDiscardsUnsavedChangesAndFailedLoadRetiresSecrets() {
        QTemporaryDir temporary;
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(store.put(item()), StoreError::None);
        QCOMPARE(store.save(), StoreError::None);
        QCOMPARE(store.put(item("unsaved")), StoreError::None);
        QCOMPARE(store.search({}).ids.size(), std::size_t(2));
        store.lock();
        QCOMPARE(store.search({}).ids, std::vector<std::string>{"item-one"});
        QCOMPARE(store.unlock(password()), StoreError::None);
        QVERIFY(!store.secret("unsaved"));
        QVERIFY(store.secret("item-one"));
        auto broken = read(path(temporary.path()));
        broken[0] = static_cast<char>(broken[0] ^ 1);
        QVERIFY(write(path(temporary.path()), broken));
        QCOMPARE(store.load(), StoreError::InvalidFormat);
        QVERIFY(store.locked());
        QVERIFY(!store.secret("item-one"));
        QVERIFY(store.search({}).ids.empty());
        QVERIFY(!store.search({}).authenticated);
    }
    void invalidInputsAndBounds() {
        QTemporaryDir temporary;
        CollectionStore invalid(root(temporary.path()), "../escape");
        QCOMPARE(invalid.create(password(), fastKdf()), StoreError::InvalidInput);
        CollectionStore store(root(temporary.path()), "login");
        QCOMPARE(store.create(password(), {1,1,1}), StoreError::InvalidInput);
        QCOMPARE(store.create(password(), fastKdf()), StoreError::None);
        auto invalidItem = item("../escape");
        QCOMPARE(store.put(std::move(invalidItem)), StoreError::InvalidInput);
        invalidItem = item();
        invalidItem.attributes.emplace("", "value");
        QCOMPARE(store.put(std::move(invalidItem)), StoreError::InvalidInput);
        invalidItem = item();
        for (int i = 0; i < 32; ++i) invalidItem.attributes.emplace(std::to_string(i), "value");
        QCOMPARE(store.put(std::move(invalidItem)), StoreError::InvalidInput);
        invalidItem = item();
        invalidItem.metadata.contentType.clear();
        QCOMPARE(store.put(std::move(invalidItem)), StoreError::InvalidInput);
        QVERIFY(store.search({}).ids.empty());
        for (int i = 0; i < 3; ++i) {
            auto large = item(std::to_string(i), "");
            large.secret = SecureBuffer(1024 * 1024);
            QCOMPARE(store.put(std::move(large)), StoreError::None);
        }
        auto tooMany = item("four", "");
        tooMany.secret = SecureBuffer(1024 * 1024);
        QCOMPARE(store.put(std::move(tooMany)), StoreError::InvalidInput);
    }
};
QTEST_GUILESS_MAIN(CollectionStoreTest)
#include "tst_collection_store.moc"
