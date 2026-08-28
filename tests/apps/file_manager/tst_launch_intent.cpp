// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/launch_intent.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using QindaQt::Apps::FileManager::DesktopFileLauncher;
using QindaQt::Apps::FileManager::LaunchError;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = "x") {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

} // namespace

class TestLaunchIntent final : public QObject {
  Q_OBJECT

private slots:
  void missingPathIsRejected();
  void directoryIsRejectedAsNotRegular();
  void danglingSymlinkIsRejectedAsMissing();
  void validSymlinkResolvesToItsCanonicalTarget();
  void unreadableFileIsRejected();
  void validRegularFilePassesValidation();
};

void TestLaunchIntent::missingPathIsRejected() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto result =
      DesktopFileLauncher::validateRegularFile(dir.filePath(QStringLiteral("ghost")), nullptr);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::NotFound);
}

void TestLaunchIntent::directoryIsRejectedAsNotRegular() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto result = DesktopFileLauncher::validateRegularFile(dir.path(), nullptr);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::NotRegularFile);
}

void TestLaunchIntent::danglingSymlinkIsRejectedAsMissing() {
#ifdef Q_OS_UNIX
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString target = dir.filePath(QStringLiteral("gone"));
  const QString link = dir.filePath(QStringLiteral("dangling"));
  QVERIFY(writeFile(target));
  QVERIFY(QFile::link(target, link));
  QVERIFY(QFile::remove(target));

  const auto result = DesktopFileLauncher::validateRegularFile(link, nullptr);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::NotFound);
#else
  QSKIP("symlinks are not exercised on this platform");
#endif
}

void TestLaunchIntent::validSymlinkResolvesToItsCanonicalTarget() {
#ifdef Q_OS_UNIX
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString target = dir.filePath(QStringLiteral("real.txt"));
  const QString link = dir.filePath(QStringLiteral("alias.txt"));
  QVERIFY(writeFile(target));
  QVERIFY(QFile::link(target, link));

  QString canonicalPath;
  const auto result = DesktopFileLauncher::validateRegularFile(link, &canonicalPath);
  QVERIFY(result.ok());
  QCOMPARE(QFileInfo(canonicalPath).canonicalFilePath(), QFileInfo(target).canonicalFilePath());
#else
  QSKIP("symlinks are not exercised on this platform");
#endif
}

void TestLaunchIntent::unreadableFileIsRejected() {
#ifdef Q_OS_UNIX
  if (::geteuid() == 0) {
    QSKIP("root bypasses POSIX permission bits; this case cannot be exercised as root");
  }
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("locked.txt"));
  QVERIFY(writeFile(path));
  QVERIFY(QFile::setPermissions(path, QFileDevice::Permissions{}));

  const auto result = DesktopFileLauncher::validateRegularFile(path, nullptr);
  QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::Unreadable);
#else
  QSKIP("POSIX permission bits are not exercised on this platform");
#endif
}

void TestLaunchIntent::validRegularFilePassesValidation() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("plain.txt"));
  QVERIFY(writeFile(path));

  QString canonicalPath;
  const auto result = DesktopFileLauncher::validateRegularFile(path, &canonicalPath);
  QVERIFY(result.ok());
  QVERIFY(!canonicalPath.isEmpty());
}

QTEST_APPLESS_MAIN(TestLaunchIntent)
#include "tst_launch_intent.moc"
