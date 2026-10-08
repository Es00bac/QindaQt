// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation/recovery_mount_admission.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace QindaQt::Apps::FileManager;
class RecoveryMountTests final : public QObject {
  Q_OBJECT
private slots:
  void actualPinnedAdmissionAndMoveLifetime();
  void pathReplacementAndPermissionRevocation();
  void symlinkAncestorRefused();
  void actualDistinctFilesystems();
};
void RecoveryMountTests::actualPinnedAdmissionAndMoveLifetime() {
  QTemporaryDir directory;
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(directory.path(), error);
  QVERIFY2(live.has_value(), qPrintable(error.diagnostic));
  QVERIFY(live->observation().mountId != 0);
  QVERIFY(live->privateCurrent().ok());
  auto moved = std::move(*live);
  QCOMPARE(live->current().error, MutationError::Changed);
  QVERIFY(moved.current().ok());
  QCOMPARE(::fcntl(moved.descriptor(), F_GETFD) & FD_CLOEXEC, FD_CLOEXEC);
}
void RecoveryMountTests::pathReplacementAndPermissionRevocation() {
  QTemporaryDir root;
  const auto path = root.filePath(QStringLiteral("private"));
  QVERIFY(QDir().mkdir(path));
  QCOMPARE(::chmod(QFile::encodeName(path).constData(), 0700), 0);
  MutationResult error;
  auto live = RecoveryDirectoryAdmission::open(path, error);
  QVERIFY(live);
  QCOMPARE(::chmod(QFile::encodeName(path).constData(), 0755), 0);
  QCOMPARE(live->current().error, MutationError::Changed);
  live = RecoveryDirectoryAdmission::open(path, error);
  QVERIFY(live);
  QCOMPARE(live->privateCurrent().error, MutationError::PermissionDenied);
  QVERIFY(QDir().rename(path, path + QStringLiteral("-old")));
  QVERIFY(QDir().mkdir(path));
  QCOMPARE(live->current().error, MutationError::Changed);
}
void RecoveryMountTests::symlinkAncestorRefused() {
  QTemporaryDir root;
  const auto real = root.filePath(QStringLiteral("real"));
  const auto link = root.filePath(QStringLiteral("link"));
  QVERIFY(QDir().mkpath(real + QStringLiteral("/child")));
  QCOMPARE(::symlink(QFile::encodeName(real).constData(),
                      QFile::encodeName(link).constData()), 0);
  MutationResult error;
  QVERIFY(!RecoveryDirectoryAdmission::open(link + QStringLiteral("/child"), error));
  QVERIFY(!error.ok());
}
void RecoveryMountTests::actualDistinctFilesystems() {
  QTemporaryDir disk;
  QTemporaryDir shared(QStringLiteral("/dev/shm/qindaqt-recovery-test-XXXXXX"));
  QVERIFY(disk.isValid());
  QVERIFY2(shared.isValid(), "Real distinct-device fixture requires writable private /dev/shm.");
  MutationResult error;
  auto left = RecoveryDirectoryAdmission::open(disk.path(), error);
  auto right = RecoveryDirectoryAdmission::open(shared.path(), error);
  QVERIFY(left && right);
  QVERIFY2(left->observation().device != right->observation().device,
           "These fixture paths are not distinct real devices; no cross-device pass.");
  QVERIFY(left->observation().mountId != right->observation().mountId);
}
QTEST_GUILESS_MAIN(RecoveryMountTests)
#include "tst_recovery_mount.moc"
