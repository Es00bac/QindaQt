// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/network_locations_store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] NetworkLocationRecord recordFor(const QString &address,
                                              const QString &name,
                                              bool showInPlaces = true,
                                              bool mountAtLogin = false) {
  NetworkLocationRecord record;
  record.url = QUrl(address);
  record.id = NetworkLocationsStore::identityFor(record.url);
  record.name = name;
  record.showInPlaces = showInPlaces;
  record.mountAtLogin = mountAtLogin;
  return record;
}

[[nodiscard]] bool writeRaw(const QString &directory, const QByteArray &bytes,
                            const int version = 2) {
  QDir().mkpath(directory);
  QFile file(QDir(directory).filePath(
      QStringLiteral("network-locations-v%1.json").arg(version)));
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

class TestNetworkLocationsStore final : public QObject {
  Q_OBJECT

private slots:
  void firstRunIsAbsentWithoutDiagnostic();
  void roundTripsEveryField();
  void refusesUnusableAddresses();
  void refusesDuplicateAndOverlongInventories();
  void refusesForeignSchemaAndUnknownKeys();
  void refusesASymlinkedStateRoot();
  void migratesAVersionOneInventory();
  void refusesMountAtLoginOnAnythingButSftp();
};

void TestNetworkLocationsStore::firstRunIsAbsentWithoutDiagnostic() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const NetworkLocationsStore store(temporary.filePath(QStringLiteral("state")));
  const auto loaded = store.load();
  QVERIFY(!loaded.ok());
  QCOMPARE(loaded.error, NetworkLocationsError::Absent);
  // A first run must show nothing: an empty diagnostic is what keeps the
  // banner hidden until something is really wrong.
  QVERIFY(loaded.diagnostic.isEmpty());
  QVERIFY(loaded.locations.isEmpty());
}

void TestNetworkLocationsStore::roundTripsEveryField() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const NetworkLocationsStore store(temporary.filePath(QStringLiteral("state")));
  const QVector<NetworkLocationRecord> written = {
      recordFor(QStringLiteral("sftp://qinda/mnt/storage"),
                QStringLiteral("Storage (desktop)"), true, true),
      recordFor(QStringLiteral("smb://nas:4455/share"), QStringLiteral("NAS"), false),
  };
  QVERIFY(store.store(written).ok());
  const auto loaded = store.load();
  QVERIFY2(loaded.ok(), qPrintable(loaded.diagnostic));
  QCOMPARE(loaded.locations, written);
  QCOMPARE(loaded.locations.at(0).mountAtLogin, true);
  QCOMPARE(loaded.locations.at(1).showInPlaces, false);
  QCOMPARE(loaded.locations.at(1).mountAtLogin, false);
  QCOMPARE(loaded.locations.at(1).url.port(), 4455);
  QVERIFY(!loaded.migratedFromV1);

  // The file is the schema the wiki documents, not an implementation detail.
  QFile file(store.filePath());
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
  QCOMPARE(object.value(QStringLiteral("version")).toInt(),
           NetworkLocationsStore::schemaVersion);
  QVERIFY(store.filePath().endsWith(QStringLiteral("network-locations-v2.json")));
  QCOMPARE(object.value(QStringLiteral("locations")).toArray().size(), 2);
  QCOMPARE(object.value(QStringLiteral("locations")).toArray().at(0).toObject()
               .value(QStringLiteral("url")).toString(),
           QStringLiteral("sftp://qinda/mnt/storage"));
}

void TestNetworkLocationsStore::refusesUnusableAddresses() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const NetworkLocationsStore store(temporary.filePath(QStringLiteral("state")));

  // AGENT-GUARD: a credential must never reach the inventory, an unsupported
  // scheme must never be stored as browsable, and a non-canonical spelling
  // must never be silently repaired into a different folder.
  const QStringList refusedAddresses = {
      QStringLiteral("sftp://user:secret@qinda/mnt"),
      QStringLiteral("sftp://user@qinda/mnt"),
      QStringLiteral("ftp://qinda/mnt"),
      QStringLiteral("file:///home/cabewse"),
      QStringLiteral("sftp:///mnt/storage"),
      QStringLiteral("sftp://qinda/mnt/../etc"),
      QStringLiteral("sftp://qinda/mnt/storage/"),
  };
  for (const QString &address : refusedAddresses) {
    const auto result = store.store({recordFor(address, QStringLiteral("x"))});
    QVERIFY2(!result.ok(), qPrintable(address));
    QCOMPARE(result.error, NetworkLocationsError::Malformed);
  }

  NetworkLocationRecord unnamed =
      recordFor(QStringLiteral("sftp://qinda/mnt"), QString());
  QCOMPARE(store.store({unnamed}).error, NetworkLocationsError::Malformed);

  // An id that does not match its own URL is a corrupted record, not a
  // renameable one: identity is derived, never supplied.
  NetworkLocationRecord mismatched =
      recordFor(QStringLiteral("sftp://qinda/mnt"), QStringLiteral("Mnt"));
  mismatched.id = QStringLiteral("something-else");
  QCOMPARE(store.store({mismatched}).error, NetworkLocationsError::Malformed);
}

void TestNetworkLocationsStore::refusesDuplicateAndOverlongInventories() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const NetworkLocationsStore store(temporary.filePath(QStringLiteral("state")));

  const auto duplicate = recordFor(QStringLiteral("sftp://qinda/mnt"),
                                   QStringLiteral("Mnt"));
  QCOMPARE(store.store({duplicate, duplicate}).error,
           NetworkLocationsError::Malformed);

  QVector<NetworkLocationRecord> tooMany;
  for (int index = 0; index <= NetworkLocationsStore::maximumLocations; ++index) {
    tooMany.append(recordFor(QStringLiteral("sftp://qinda/mnt/%1").arg(index),
                             QStringLiteral("Folder %1").arg(index)));
  }
  QCOMPARE(store.store(tooMany).error, NetworkLocationsError::Malformed);

  QVector<NetworkLocationRecord> longName = {recordFor(
      QStringLiteral("sftp://qinda/mnt"),
      QString(NetworkLocationsStore::maximumNameLength + 1, QLatin1Char('n')))};
  QCOMPARE(store.store(longName).error, NetworkLocationsError::Malformed);
}

void TestNetworkLocationsStore::refusesForeignSchemaAndUnknownKeys() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const NetworkLocationsStore store(directory);

  QVERIFY(writeRaw(directory, QByteArray("not json at all")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);

  QVERIFY(writeRaw(directory, QByteArray("{\"version\":3,\"locations\":[]}")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);

  QVERIFY(writeRaw(directory, QByteArray("{\"locations\":[]}")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);

  // AGENT-CONTRACT: an entry written by a newer schema is refused whole
  // rather than half-understood, so a v2 field can never be silently lost.
  QVERIFY(writeRaw(directory,
                   QByteArray("{\"version\":2,\"locations\":[{\"name\":\"n\","
                              "\"url\":\"sftp://q/m\",\"showInPlaces\":true,"
                              "\"mountAtLogin\":true,\"openInPlace\":true}]}")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);

  QVERIFY(writeRaw(directory,
                   QByteArray("{\"version\":2,\"locations\":[{\"name\":\"n\","
                              "\"url\":\"sftp://q/m\"}]}")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);

  // A stored address that no longer canonicalizes refuses the whole
  // inventory: a partial load would quietly drop a user's saved location.
  QVERIFY(writeRaw(directory,
                   QByteArray("{\"version\":2,\"locations\":[{\"name\":\"n\","
                              "\"url\":\"sftp://user@q/m\",\"showInPlaces\":true,"
                              "\"mountAtLogin\":false}]}")));
  QCOMPARE(store.load().error, NetworkLocationsError::Malformed);
}

void TestNetworkLocationsStore::refusesASymlinkedStateRoot() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString real = temporary.filePath(QStringLiteral("real"));
  QVERIFY(QDir().mkpath(real));
  const QString linked = temporary.filePath(QStringLiteral("linked"));
  QVERIFY(QFile::link(real, linked));

  const NetworkLocationsStore store(linked);
  const auto written =
      store.store({recordFor(QStringLiteral("sftp://qinda/mnt"), QStringLiteral("Mnt"))});
  QVERIFY(!written.ok());
  QCOMPARE(written.error, NetworkLocationsError::InvalidRoot);
  QCOMPARE(store.load().error, NetworkLocationsError::InvalidRoot);
}

void TestNetworkLocationsStore::migratesAVersionOneInventory() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));
  const NetworkLocationsStore store(directory);

  // A v1 file has no mountAtLogin. It is read once, reported as migrated, and
  // the records default the new field off -- nobody is opted into a mount
  // they never asked for.
  QVERIFY(writeRaw(directory,
                   QByteArray("{\"version\":1,\"locations\":[{\"name\":\"Storage\","
                              "\"url\":\"sftp://qinda/mnt/storage\","
                              "\"showInPlaces\":true}]}"),
                   1));
  const auto migrated = store.load();
  QVERIFY2(migrated.ok(), qPrintable(migrated.diagnostic));
  QVERIFY(migrated.migratedFromV1);
  QCOMPARE(migrated.locations.size(), 1);
  QCOMPARE(migrated.locations.constFirst().name, QStringLiteral("Storage"));
  QCOMPARE(migrated.locations.constFirst().mountAtLogin, false);

  // Writing it back produces the v2 document, and the v1 file is left where
  // it is so a downgrade still finds its own inventory.
  QVERIFY(store.store(migrated.locations).ok());
  const auto reread = store.load();
  QVERIFY2(reread.ok(), qPrintable(reread.diagnostic));
  QVERIFY(!reread.migratedFromV1);
  QCOMPARE(reread.locations, migrated.locations);
  QVERIFY(QFile::exists(
      QDir(directory).filePath(QStringLiteral("network-locations-v1.json"))));

  // A malformed v1 file is not treated as a first run in disguise.
  QTemporaryDir second;
  QVERIFY(second.isValid());
  const QString brokenDirectory = second.filePath(QStringLiteral("state"));
  QVERIFY(writeRaw(brokenDirectory, QByteArray("{\"version\":1}"), 1));
  QCOMPARE(NetworkLocationsStore(brokenDirectory).load().error,
           NetworkLocationsError::Absent);
}

void TestNetworkLocationsStore::refusesMountAtLoginOnAnythingButSftp() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const NetworkLocationsStore store(temporary.filePath(QStringLiteral("state")));
  // AGENT-GUARD: sshfs is the only mount this knob knows, so storing it on an
  // smb location would promise a mount nothing can perform.
  const auto refused = store.store({recordFor(QStringLiteral("smb://nas/share"),
                                              QStringLiteral("NAS"), true, true)});
  QVERIFY(!refused.ok());
  QCOMPARE(refused.error, NetworkLocationsError::Malformed);
  QVERIFY(store.store({recordFor(QStringLiteral("smb://nas/share"),
                                 QStringLiteral("NAS"), true, false)})
              .ok());
}

QTEST_MAIN(TestNetworkLocationsStore)
#include "tst_network_locations_store.moc"
