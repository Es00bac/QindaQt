// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/network_mount_manager.h"

#include "network/mount_unit.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;

namespace {

// Records what the manager asked of the service manager. No real systemctl is
// ever run: a row that ran one would touch the developer's own user manager.
class RecordingUnits final : public SystemdUserUnits {
public:
  [[nodiscard]] QString reload() override {
    ++m_reloads;
    return m_reloadRefusal;
  }
  [[nodiscard]] QString enable(const QString &unitName) override {
    m_enabled.append(unitName);
    return {};
  }
  [[nodiscard]] QString disable(const QString &unitName) override {
    m_disabled.append(unitName);
    return {};
  }

  int m_reloads = 0;
  QString m_reloadRefusal;
  QStringList m_enabled;
  QStringList m_disabled;
};

[[nodiscard]] NetworkLocationRecord location(const QString &address,
                                             const QString &name, bool mount) {
  NetworkLocationRecord record;
  record.url = QUrl(address);
  record.id = NetworkLocationsStore::identityFor(record.url);
  record.name = name;
  record.mountAtLogin = mount;
  return record;
}

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

class TestNetworkMountManager final : public QObject {
  Q_OBJECT

private slots:
  void writesEnablesAndCountsAUnit();
  void isIdempotent();
  void retiresAUnitWhoseLocationStoppedAskingForOne();
  void neverTouchesAUnitItDoesNotOwn();
  void reportsARefusalWithoutLosingTheRest();
};

void TestNetworkMountManager::writesEnablesAndCountsAUnit() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto units = std::make_unique<RecordingUnits>();
  auto *rawUnits = units.get();
  NetworkMountManager manager(temporary.filePath(QStringLiteral("systemd")),
                              temporary.path(), std::move(units));
  QSignalSpy mounts(&manager, &NetworkMountManager::mountsChanged);

  manager.synchronize({location(QStringLiteral("sftp://qinda/mnt/storage"),
                                QStringLiteral("Storage"), true),
                       location(QStringLiteral("sftp://qinda/home/cabewse"),
                                QStringLiteral("Home"), false)});

  const QString expected =
      MountUnit::escapePath(temporary.path() + QStringLiteral("/Network/Storage")) +
      QStringLiteral(".mount");
  QCOMPARE(manager.ownedUnitNames(), QStringList{expected});
  QCOMPARE(manager.mountedLocationCount(), 1);
  QCOMPARE(mounts.count(), 1);
  QCOMPARE(rawUnits->m_enabled, QStringList{expected});
  QCOMPARE(rawUnits->m_reloads, 1);
  QVERIFY(manager.lastError().isEmpty());

  QFile written(QDir(temporary.filePath(QStringLiteral("systemd"))).filePath(expected));
  QVERIFY(written.open(QIODevice::ReadOnly));
  const QString contents = QString::fromUtf8(written.readAll());
  QVERIFY(contents.contains(QStringLiteral("What=qinda:/mnt/storage")));
  QVERIFY(contents.contains(QStringLiteral("Type=fuse.sshfs")));
}

void TestNetworkMountManager::isIdempotent() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto units = std::make_unique<RecordingUnits>();
  auto *rawUnits = units.get();
  NetworkMountManager manager(temporary.filePath(QStringLiteral("systemd")),
                              temporary.path(), std::move(units));
  const QVector<NetworkLocationRecord> locations = {
      location(QStringLiteral("sftp://qinda/mnt/storage"), QStringLiteral("Storage"),
               true)};

  manager.synchronize(locations);
  QCOMPARE(rawUnits->m_reloads, 1);
  // A second identical pass rewrites nothing and asks systemd for nothing,
  // so opening a window does not churn the service manager.
  manager.synchronize(locations);
  QCOMPARE(rawUnits->m_reloads, 1);
  QCOMPARE(rawUnits->m_enabled.size(), 1);
  QCOMPARE(manager.mountedLocationCount(), 1);
}

void TestNetworkMountManager::retiresAUnitWhoseLocationStoppedAskingForOne() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto units = std::make_unique<RecordingUnits>();
  auto *rawUnits = units.get();
  NetworkMountManager manager(temporary.filePath(QStringLiteral("systemd")),
                              temporary.path(), std::move(units));
  manager.synchronize({location(QStringLiteral("sftp://qinda/mnt/storage"),
                                QStringLiteral("Storage"), true)});
  const QString unitName = manager.ownedUnitNames().constFirst();

  manager.synchronize({location(QStringLiteral("sftp://qinda/mnt/storage"),
                                QStringLiteral("Storage"), false)});
  QVERIFY(manager.ownedUnitNames().isEmpty());
  QCOMPARE(manager.mountedLocationCount(), 0);
  // Disabled before the file went away, so systemd's default.target.wants
  // symlink does not outlive the unit.
  QCOMPARE(rawUnits->m_disabled, QStringList{unitName});
  QVERIFY(!QFile::exists(
      QDir(temporary.filePath(QStringLiteral("systemd"))).filePath(unitName)));

  // Removing every location also removes the unit.
  manager.synchronize({});
  QVERIFY(manager.ownedUnitNames().isEmpty());
}

void TestNetworkMountManager::neverTouchesAUnitItDoesNotOwn() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString unitsDirectory = temporary.filePath(QStringLiteral("systemd"));
  // AGENT-GUARD: the ownership prefix is the whole rule. A unit the user wrote
  // by hand, and a mount unit for somewhere else entirely, must survive.
  QVERIFY(writeFile(QDir(unitsDirectory).filePath(QStringLiteral("my-backup.mount")),
                    QByteArray("[Mount]\n")));
  QVERIFY(writeFile(QDir(unitsDirectory).filePath(QStringLiteral("mnt-data.mount")),
                    QByteArray("[Mount]\n")));

  auto units = std::make_unique<RecordingUnits>();
  auto *rawUnits = units.get();
  NetworkMountManager manager(unitsDirectory, temporary.path(), std::move(units));
  manager.synchronize({});

  QVERIFY(manager.ownedUnitNames().isEmpty());
  QVERIFY(QFile::exists(QDir(unitsDirectory).filePath(QStringLiteral("my-backup.mount"))));
  QVERIFY(QFile::exists(QDir(unitsDirectory).filePath(QStringLiteral("mnt-data.mount"))));
  QVERIFY(rawUnits->m_disabled.isEmpty());
  QVERIFY(manager.ownedPrefix().endsWith(QStringLiteral("Network-")));
}

void TestNetworkMountManager::reportsARefusalWithoutLosingTheRest() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto units = std::make_unique<RecordingUnits>();
  auto *rawUnits = units.get();
  rawUnits->m_reloadRefusal = QStringLiteral("no service manager here");
  NetworkMountManager manager(temporary.filePath(QStringLiteral("systemd")),
                              temporary.path(), std::move(units));

  // One location cannot be mounted at all (smb); the other still gets its
  // unit, and the refusal is reported rather than swallowed.
  manager.synchronize({location(QStringLiteral("smb://nas/share"),
                                QStringLiteral("NAS"), true),
                       location(QStringLiteral("sftp://qinda/mnt/storage"),
                                QStringLiteral("Storage"), true)});
  QCOMPARE(manager.ownedUnitNames().size(), 1);
  QVERIFY(!manager.lastError().isEmpty());
  QVERIFY(manager.lastError().contains(QStringLiteral("SFTP")));
  manager.clearLastError();
  QVERIFY(manager.lastError().isEmpty());
}

QTEST_MAIN(TestNetworkMountManager)
#include "tst_network_mount_manager.moc"
