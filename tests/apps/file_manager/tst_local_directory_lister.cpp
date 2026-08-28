// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/local_directory_lister.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using QindaQt::Apps::FileManager::ListingError;
using QindaQt::Apps::FileManager::LocalDirectoryLister;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = {}) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

} // namespace

class TestLocalDirectoryLister final : public QObject {
  Q_OBJECT

private slots:
  void listsAndSortsDirectoriesBeforeFilesCaseInsensitively();
  void flagsHiddenSymlinkAndUnreadableEntries();
  void missingPathIsNotFound();
  void regularFileIsNotADirectory();
  void unreadableDirectoryIsPermissionDenied();
  void emptyDirectoryListsCleanly();
};

void TestLocalDirectoryLister::listsAndSortsDirectoriesBeforeFilesCaseInsensitively() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Zeta")));
  QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("alpha")));
  QVERIFY(writeFile(dir.filePath(QStringLiteral("Banana.txt"))));
  QVERIFY(writeFile(dir.filePath(QStringLiteral("apple.txt"))));

  LocalDirectoryLister lister;
  const auto result = lister.list(dir.path());
  QVERIFY(result.ok());
  QCOMPARE(result.entries.size(), 4);
  // Directories first (case-insensitive alpha < Zeta), then files
  // (case-insensitive apple.txt < Banana.txt).
  QCOMPARE(result.entries.at(0).name, QStringLiteral("alpha"));
  QCOMPARE(result.entries.at(1).name, QStringLiteral("Zeta"));
  QCOMPARE(result.entries.at(2).name, QStringLiteral("apple.txt"));
  QCOMPARE(result.entries.at(3).name, QStringLiteral("Banana.txt"));
  QVERIFY(result.entries.at(0).isDirectory);
  QVERIFY(!result.entries.at(2).isDirectory);
}

void TestLocalDirectoryLister::flagsHiddenSymlinkAndUnreadableEntries() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(writeFile(dir.filePath(QStringLiteral(".hidden")), "secret"));
  QVERIFY(writeFile(dir.filePath(QStringLiteral("target.txt")), "target"));
#ifdef Q_OS_UNIX
  QVERIFY(QFile::link(dir.filePath(QStringLiteral("target.txt")),
                      dir.filePath(QStringLiteral("link.txt"))));
#endif

  LocalDirectoryLister lister;
  const auto result = lister.list(dir.path());
  QVERIFY(result.ok());

  bool sawHidden = false;
  bool sawLink = false;
  for (const auto &entry : result.entries) {
    if (entry.name == QLatin1String(".hidden")) {
      sawHidden = true;
      QVERIFY(entry.isHidden);
    }
    if (entry.name == QLatin1String("target.txt")) {
      QVERIFY(!entry.isHidden);
      QVERIFY(!entry.isSymlink);
    }
#ifdef Q_OS_UNIX
    if (entry.name == QLatin1String("link.txt")) {
      sawLink = true;
      QVERIFY(entry.isSymlink);
      QVERIFY(!entry.isDirectory);
    }
#endif
  }
  QVERIFY(sawHidden);
#ifdef Q_OS_UNIX
  QVERIFY(sawLink);
#else
  Q_UNUSED(sawLink);
#endif
}

void TestLocalDirectoryLister::missingPathIsNotFound() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  LocalDirectoryLister lister;
  const auto result = lister.list(dir.filePath(QStringLiteral("does-not-exist")));
  QVERIFY(!result.ok());
  QCOMPARE(result.error, ListingError::NotFound);
}

void TestLocalDirectoryLister::regularFileIsNotADirectory() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString filePath = dir.filePath(QStringLiteral("plain.txt"));
  QVERIFY(writeFile(filePath, "hello"));

  LocalDirectoryLister lister;
  const auto result = lister.list(filePath);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, ListingError::NotADirectory);
}

void TestLocalDirectoryLister::unreadableDirectoryIsPermissionDenied() {
#ifdef Q_OS_UNIX
  if (::geteuid() == 0) {
    QSKIP("root bypasses POSIX permission bits; this case cannot be exercised as root");
  }
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString lockedPath = dir.filePath(QStringLiteral("locked"));
  QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("locked")));
  QVERIFY(QFile::setPermissions(lockedPath, QFileDevice::Permissions{}));

  LocalDirectoryLister lister;
  const auto result = lister.list(lockedPath);
  // Restore permissions before any assertion can throw, so QTemporaryDir can
  // still clean up the fixture on failure.
  QFile::setPermissions(lockedPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);
  QVERIFY(!result.ok());
  QCOMPARE(result.error, ListingError::PermissionDenied);
#else
  QSKIP("POSIX permission bits are not exercised on this platform");
#endif
}

void TestLocalDirectoryLister::emptyDirectoryListsCleanly() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  LocalDirectoryLister lister;
  const auto result = lister.list(dir.path());
  QVERIFY(result.ok());
  QVERIFY(result.entries.isEmpty());
  QVERIFY(!result.truncated);
}

QTEST_APPLESS_MAIN(TestLocalDirectoryLister)
#include "tst_local_directory_lister.moc"
