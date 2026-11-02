// SPDX-License-Identifier: GPL-3.0-or-later
// Identity generation/persistence, SPKI pin and the trust store. Uses only
// QTemporaryDir state.
#include <qindaqt/services/link_transport/identity.h>
#include <qindaqt/services/link_transport/trust_store.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt::Link;

class LinkIdentityTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void pinMatchesOpenssl()
    {
        QTemporaryDir dir;
        const DeviceIdentity id = generateIdentity(QStringLiteral("pin-test"));
        QVERIFY(id.isValid());
        QCOMPARE(id.key.algorithm(), QSsl::Ec);
        QFile cert(dir.filePath(QStringLiteral("c.pem")));
        QVERIFY(cert.open(QIODevice::WriteOnly));
        cert.write(id.certificate.toPem());
        cert.close();
        QProcess sh;
        sh.start(QStringLiteral("/bin/sh"),
                 {QStringLiteral("-c"),
                  QStringLiteral("openssl x509 -in '%1' -pubkey -noout | openssl pkey -pubin -outform DER | sha256sum")
                      .arg(cert.fileName())});
        QVERIFY(sh.waitForFinished(10000));
        QCOMPARE(sh.exitCode(), 0);
        const QString expected = QString::fromLatin1(sh.readAllStandardOutput()).split(QLatin1Char(' ')).first();
        QCOMPARE(deviceIdFor(id.certificate), expected);
        QCOMPARE(id.spkiDigest().size(), 32);
    }

    void persistsWithRestrictedPermissions()
    {
        QTemporaryDir root;
        const QString dir = root.filePath(QStringLiteral("qindalink"));
        IdentityStore store(dir);
        QString error;
        const DeviceIdentity first = store.loadOrCreate(QStringLiteral("t"), &error);
        QVERIFY2(first.isValid(), qPrintable(error));
        QCOMPARE(QFileInfo(dir).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                                 | QFileDevice::WriteGroup | QFileDevice::WriteOther),
                 QFileDevice::Permissions());
        const auto keyPerms = QFileInfo(dir + QStringLiteral("/identity-key.pem")).permissions();
        QVERIFY(!(keyPerms & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther)));
        QVERIFY(keyPerms.testFlag(QFileDevice::ReadOwner) && keyPerms.testFlag(QFileDevice::WriteOwner));
        const DeviceIdentity second = store.loadOrCreate(QStringLiteral("t"), &error);
        QCOMPARE(second.spkiDigest(), first.spkiDigest());
    }

    void unreadableIdentityFailsClosed()
    {
        QTemporaryDir root;
        IdentityStore store(root.path());
        QVERIFY(store.loadOrCreate(QStringLiteral("t")).isValid());
        QFile key(root.filePath(QStringLiteral("identity-key.pem")));
        QVERIFY(key.open(QIODevice::WriteOnly | QIODevice::Truncate));
        key.write("garbage");
        key.close();
        QString error;
        QVERIFY(!store.loadOrCreate(QStringLiteral("t"), &error).isValid());
        QVERIFY(error.contains(QLatin1String("refusing")));
        QFile cert(root.filePath(QStringLiteral("identity-cert.pem")));
        QVERIFY(cert.open(QIODevice::ReadOnly));
        QVERIFY(cert.readAll().startsWith("-----BEGIN CERTIFICATE"));  // not regenerated
    }

    void failedTrustStoreWriteRollsBack()
    {
        QTemporaryDir root;
        TrustStore s(root.path());
        QVERIFY(s.load());
        PairedDevice d;
        d.id = QString(64, QLatin1Char('b'));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("devices.json")));  // rename onto a directory fails
        QVERIFY(!s.upsert(d));
        QVERIFY(!s.contains(d.id));
    }

    void trustStoreRoundTripIsAtomic()
    {
        QTemporaryDir root;
        TrustStore a(root.filePath(QStringLiteral("d")));
        QVERIFY(a.load());
        PairedDevice d;
        d.id = QString(64, QLatin1Char('a'));
        d.name = QStringLiteral("Phone");
        d.model = QStringLiteral("M");
        d.pairedAt = 5;
        QVERIFY(a.upsert(d));
        d.name = QStringLiteral("Renamed");
        QVERIFY(a.upsert(d));
        TrustStore b(root.filePath(QStringLiteral("d")));
        QVERIFY(b.load());
        QCOMPARE(b.devices().size(), 1);
        QCOMPARE(b.find(d.id)->name, QStringLiteral("Renamed"));
        QCOMPARE(QDir(root.filePath(QStringLiteral("d"))).entryList(QDir::Files).size(), 1);  // no temp leftovers
        QVERIFY(b.remove(d.id));
        TrustStore c(root.filePath(QStringLiteral("d")));
        QVERIFY(c.load());
        QVERIFY(c.devices().isEmpty());
    }

    void corruptTrustStoreFailsClosed()
    {
        QTemporaryDir root;
        QFile f(root.filePath(QStringLiteral("devices.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json");
        f.close();
        TrustStore s(root.path());
        QVERIFY(!s.load());
        QVERIFY(s.devices().isEmpty());
    }
};

QTEST_GUILESS_MAIN(LinkIdentityTest)
#include "tst_link_identity.moc"
