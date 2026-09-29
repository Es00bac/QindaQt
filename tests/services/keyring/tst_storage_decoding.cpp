// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixtures.h"
#include "format_p.h"
#include <QTest>
#include <QTemporaryDir>

using namespace fixture;
using namespace qindaqt::keyring::detail;
class StorageDecodingTest : public QObject {
    Q_OBJECT
private:
    static bool writeEnvelope(const QString &file, Envelope &e, SecureBuffer &plain,
                              const SecureBuffer &key) {
        e.aad = makeAad(e, plain.size());
        if (!encrypt(key.bytes(), e.nonce, e.aad, plain.bytes(), e.cipher, e.tag)) return false;
        Bytes bytes = e.aad;
        bytes.insert(bytes.end(), e.cipher.begin(), e.cipher.end());
        bytes.insert(bytes.end(), e.tag.begin(), e.tag.end());
        return write(file, {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())});
    }
private slots:
    void authenticatedHostilePayload_data() {
        QTest::addColumn<int>("offset");
        QTest::newRow("payload-item-count") << 0;
        QTest::newRow("payload-id-length") << 4;
        QTest::newRow("payload-label-length") << 16;
    }
    void authenticatedHostilePayload() {
        QFETCH(int, offset);
        QTemporaryDir temporary;
        CollectionStore seed(root(temporary.path()), "login");
        QCOMPARE(seed.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(seed.put(item()), StoreError::None);
        QCOMPARE(seed.save(), StoreError::None);
        seed.lock();
        const auto bytes = read(path(temporary.path()));
        Envelope e;
        QVERIFY(parseEnvelope({reinterpret_cast<const unsigned char *>(bytes.data()),
                               static_cast<std::size_t>(bytes.size())}, e));
        SecureBuffer key, decrypted;
        QVERIFY(derive(password(), e.salt, e.parameters, key));
        QVERIFY(decrypt(key.bytes(), e.nonce, e.aad, e.cipher, e.tag, decrypted));
        SecureBuffer payload(e.cipher.size());
        std::copy_n(decrypted.bytes().begin(), payload.size(), payload.bytes().begin());
        for (int i = 0; i < 4; ++i) payload.bytes()[static_cast<std::size_t>(offset + i)] = 0xff;
        QVERIFY(writeEnvelope(path(temporary.path()), e, payload, key));
        CollectionStore rejected(root(temporary.path()), "login");
        QCOMPARE(rejected.load(), StoreError::None);
        QCOMPARE(rejected.unlock(password()), StoreError::InvalidFormat);
        QVERIFY(rejected.locked());
        QVERIFY(!rejected.secret("item-one"));
    }
    void authenticatedIndexStillMustMatchPayload() {
        QTemporaryDir temporary;
        CollectionStore seed(root(temporary.path()), "login");
        QCOMPARE(seed.create(password(), fastKdf()), StoreError::None);
        QCOMPARE(seed.put(item()), StoreError::None);
        QCOMPARE(seed.save(), StoreError::None);
        seed.lock();
        const auto bytes = read(path(temporary.path()));
        Envelope e;
        QVERIFY(parseEnvelope({reinterpret_cast<const unsigned char *>(bytes.data()),
                               static_cast<std::size_t>(bytes.size())}, e));
        SecureBuffer key, decrypted;
        QVERIFY(derive(password(), e.salt, e.parameters, key));
        QVERIFY(decrypt(key.bytes(), e.nonce, e.aad, e.cipher, e.tag, decrypted));
        SecureBuffer payload(e.cipher.size());
        std::copy_n(decrypted.bytes().begin(), payload.size(), payload.bytes().begin());
        e.index[0].id = "forged-id"; // Author with password, incompatible schema.
        QVERIFY(writeEnvelope(path(temporary.path()), e, payload, key));
        CollectionStore rejected(root(temporary.path()), "login");
        QCOMPARE(rejected.load(), StoreError::None);
        QCOMPARE(rejected.search({}).ids, std::vector<std::string>{"forged-id"});
        QVERIFY(!rejected.search({}).authenticated);
        QCOMPARE(rejected.unlock(password()), StoreError::InvalidFormat);
        QVERIFY(rejected.locked());
        QVERIFY(!rejected.secret("item-one"));
    }
};
QTEST_GUILESS_MAIN(StorageDecodingTest)
#include "tst_storage_decoding.moc"
