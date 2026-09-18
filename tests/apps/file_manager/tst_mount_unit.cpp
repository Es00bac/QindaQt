// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/mount_unit.h"

#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] NetworkLocationRecord sftpLocation(const QString &address,
                                                 const QString &name) {
  NetworkLocationRecord record;
  record.url = QUrl(address);
  record.id = NetworkLocationsStore::identityFor(record.url);
  record.name = name;
  record.mountAtLogin = true;
  return record;
}

} // namespace

class TestMountUnit final : public QObject {
  Q_OBJECT

private slots:
  void escapesPathsTheWaySystemdDoes_data();
  void escapesPathsTheWaySystemdDoes();
  void buildsAUnitThatNamesItsOwnMountPoint();
  void carriesThePortAndTheRemotePath();
  void refusesWhatCannotBeMounted();
  void reducesALocationNameToOneSafeDirectory_data();
  void reducesALocationNameToOneSafeDirectory();
};

void TestMountUnit::escapesPathsTheWaySystemdDoes_data() {
  QTest::addColumn<QString>("path");
  QTest::addColumn<QString>("escaped");
  // Checked against `systemd-escape --path` for each case.
  QTest::newRow("root") << QStringLiteral("/") << QStringLiteral("-");
  QTest::newRow("plain") << QStringLiteral("/home/cabewse/Network/Storage")
                         << QStringLiteral("home-cabewse-Network-Storage");
  QTest::newRow("trailing slash") << QStringLiteral("/home/cabewse/")
                                  << QStringLiteral("home-cabewse");
  QTest::newRow("double slash") << QStringLiteral("/home//cabewse")
                                << QStringLiteral("home-cabewse");
  QTest::newRow("space") << QStringLiteral("/mnt/My Files")
                         << QStringLiteral("mnt-My\\x20Files");
  QTest::newRow("dash") << QStringLiteral("/mnt/a-b")
                        << QStringLiteral("mnt-a\\x2db");
  QTest::newRow("dot kept") << QStringLiteral("/mnt/a.b")
                            << QStringLiteral("mnt-a.b");
  QTest::newRow("leading dot escaped") << QStringLiteral("/.config")
                                       << QStringLiteral("\\x2econfig");
}

void TestMountUnit::escapesPathsTheWaySystemdDoes() {
  QFETCH(QString, path);
  QFETCH(QString, escaped);
  QCOMPARE(MountUnit::escapePath(path), escaped);
}

void TestMountUnit::buildsAUnitThatNamesItsOwnMountPoint() {
  const MountUnitResult unit =
      MountUnit::build(sftpLocation(QStringLiteral("sftp://qinda/mnt/storage"),
                                    QStringLiteral("Storage")),
                       QStringLiteral("/home/cabewse"));
  QVERIFY2(unit.ok(), qPrintable(unit.message));
  QCOMPARE(unit.mountPoint, QStringLiteral("/home/cabewse/Network/Storage"));
  // AGENT-GUARD: systemd resolves a .mount unit's Where= from its own name,
  // so the two must agree exactly or the unit refuses to load.
  QCOMPARE(unit.unitName,
           MountUnit::escapePath(unit.mountPoint) + QStringLiteral(".mount"));
  QCOMPARE(unit.unitName, QStringLiteral("home-cabewse-Network-Storage.mount"));

  QVERIFY(unit.contents.contains(QStringLiteral("Where=/home/cabewse/Network/Storage")));
  QVERIFY(unit.contents.contains(QStringLiteral("What=qinda:/mnt/storage")));
  QVERIFY(unit.contents.contains(QStringLiteral("Type=fuse.sshfs")));
  QVERIFY(unit.contents.contains(QStringLiteral("WantedBy=default.target")));
  QVERIFY(unit.contents.contains(QStringLiteral("reconnect")));
  // No credential can appear: the address is userinfo-free by construction.
  QVERIFY(!unit.contents.contains(QLatin1Char('@')));
}

void TestMountUnit::carriesThePortAndTheRemotePath() {
  const MountUnitResult ported =
      MountUnit::build(sftpLocation(QStringLiteral("sftp://qinda:2222/srv"),
                                    QStringLiteral("Srv")),
                       QStringLiteral("/home/cabewse"));
  QVERIFY2(ported.ok(), qPrintable(ported.message));
  QVERIFY(ported.contents.contains(QStringLiteral("port=2222")));
  QVERIFY(ported.contents.contains(QStringLiteral("What=qinda:/srv")));

  // A bare authority mounts the server's root, not an empty path.
  const MountUnitResult bare = MountUnit::build(
      sftpLocation(QStringLiteral("sftp://qinda"), QStringLiteral("Qinda")),
      QStringLiteral("/home/cabewse"));
  QVERIFY2(bare.ok(), qPrintable(bare.message));
  QVERIFY(bare.contents.contains(QStringLiteral("What=qinda:/")));
  QVERIFY(!bare.contents.contains(QStringLiteral("port=")));
}

void TestMountUnit::refusesWhatCannotBeMounted() {
  const MountUnitResult smb = MountUnit::build(
      sftpLocation(QStringLiteral("smb://nas/share"), QStringLiteral("NAS")),
      QStringLiteral("/home/cabewse"));
  QVERIFY(!smb.ok());
  QCOMPARE(smb.error, MountUnitError::UnsupportedScheme);
  QVERIFY(smb.unitName.isEmpty());

  const MountUnitResult relativeHome =
      MountUnit::build(sftpLocation(QStringLiteral("sftp://qinda/mnt"),
                                    QStringLiteral("Mnt")),
                       QStringLiteral("cabewse"));
  QVERIFY(!relativeHome.ok());
  QCOMPARE(relativeHome.error, MountUnitError::UnusableHome);

  const MountUnitResult unnamed = MountUnit::build(
      sftpLocation(QStringLiteral("sftp://qinda/mnt"), QStringLiteral("  /  ")),
      QStringLiteral("/home/cabewse"));
  QVERIFY(!unnamed.ok());
  QCOMPARE(unnamed.error, MountUnitError::UnusableName);
}

void TestMountUnit::reducesALocationNameToOneSafeDirectory_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("directory");
  QTest::newRow("plain") << QStringLiteral("Storage") << QStringLiteral("Storage");
  QTest::newRow("spaces kept") << QStringLiteral("Storage (desktop)")
                               << QStringLiteral("Storage (desktop)");
  QTest::newRow("trimmed") << QStringLiteral("  Storage  ")
                           << QStringLiteral("Storage");
  // AGENT-GUARD: a separator would let a location name choose its own mount
  // point anywhere in the tree.
  QTest::newRow("slash dropped") << QStringLiteral("../../etc")
                                 << QStringLiteral("etc");
  QTest::newRow("backslash dropped") << QStringLiteral("a\\b") << QStringLiteral("ab");
  QTest::newRow("leading dots dropped") << QStringLiteral("...hidden")
                                        << QStringLiteral("hidden");
  QTest::newRow("only dots refused") << QStringLiteral("..") << QString();
  QTest::newRow("empty refused") << QStringLiteral("   ") << QString();
}

void TestMountUnit::reducesALocationNameToOneSafeDirectory() {
  QFETCH(QString, name);
  QFETCH(QString, directory);
  QCOMPARE(MountUnit::directoryNameFor(name), directory);
}

QTEST_APPLESS_MAIN(TestMountUnit)
#include "tst_mount_unit.moc"
