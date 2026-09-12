// SPDX-License-Identifier: GPL-3.0-or-later
#include "public/desktop_file_boundary.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = "payload") {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

// Mirrors the QML entry snapshot MutationController::identityFromMap()
// expects: decimal-string device/inode/size/modified-time/mode fields.
[[nodiscard]] QVariantMap identityMapFor(const DirectoryEntry &entry) {
  return {{QStringLiteral("device"), QString::number(entry.device)},
          {QStringLiteral("inode"), QString::number(entry.inode)},
          {QStringLiteral("identitySize"), QString::number(entry.identitySize)},
          {QStringLiteral("modifiedNanoseconds"), QString::number(entry.modifiedNanoseconds)},
          {QStringLiteral("mode"), QString::number(entry.mode)}};
}

} // namespace

class TestDesktopFileBoundary final : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void listsARealLocalFolder();
  void listingMissingFolderReportsTypedError();
  void launchRejectsAMissingFileBeforeDispatch();
  void launchRejectsADirectoryAsNotRegular();
  void mutationControllerTrashesARealFileUsingListedIdentity();

private:
  bool m_hadPreviousXdgDataHome = false;
  QByteArray m_previousXdgDataHome;
};

void TestDesktopFileBoundary::init() {
  m_hadPreviousXdgDataHome = qEnvironmentVariableIsSet("XDG_DATA_HOME");
  if (m_hadPreviousXdgDataHome) {
    m_previousXdgDataHome = qgetenv("XDG_DATA_HOME");
  }
}

void TestDesktopFileBoundary::cleanup() {
  if (m_hadPreviousXdgDataHome) {
    qputenv("XDG_DATA_HOME", m_previousXdgDataHome);
  } else {
    qunsetenv("XDG_DATA_HOME");
  }
}

void TestDesktopFileBoundary::listsARealLocalFolder() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(writeFile(dir.filePath(QStringLiteral("note.txt"))));

  const ListingResult result = Desktop::FileBoundary::listLocalFolder(dir.path());
  QVERIFY(result.ok());
  QCOMPARE(result.entries.size(), 1);
  QCOMPARE(result.entries.first().name, QStringLiteral("note.txt"));
  QVERIFY(!result.entries.first().isDirectory);
  QVERIFY(result.entries.first().device > 0);
  QVERIFY(result.entries.first().inode > 0);
}

void TestDesktopFileBoundary::listingMissingFolderReportsTypedError() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const ListingResult result =
      Desktop::FileBoundary::listLocalFolder(dir.filePath(QStringLiteral("ghost")));
  QVERIFY(!result.ok());
  QCOMPARE(result.error, ListingError::NotFound);
}

void TestDesktopFileBoundary::launchRejectsAMissingFileBeforeDispatch() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const LaunchResult result =
      Desktop::FileBoundary::launchLocalFile(dir.filePath(QStringLiteral("ghost.txt")));
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::NotFound);
}

void TestDesktopFileBoundary::launchRejectsADirectoryAsNotRegular() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const LaunchResult result = Desktop::FileBoundary::launchLocalFile(dir.path());
  QVERIFY(!result.ok());
  QCOMPARE(result.error, LaunchError::NotRegularFile);
}

void TestDesktopFileBoundary::mutationControllerTrashesARealFileUsingListedIdentity() {
  QTemporaryDir dataHomeDir;
  QTemporaryDir workDir;
  QVERIFY(dataHomeDir.isValid());
  QVERIFY(workDir.isValid());
  qputenv("XDG_DATA_HOME", dataHomeDir.path().toLocal8Bit());

  const QString filePath = workDir.filePath(QStringLiteral("discard.txt"));
  QVERIFY(writeFile(filePath));

  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(workDir.path());
  QVERIFY(listing.ok());
  QCOMPARE(listing.entries.size(), 1);
  const DirectoryEntry entry = listing.entries.first();

  const std::unique_ptr<MutationController> controller =
      Desktop::FileBoundary::createLocalMutationController();
  QVERIFY(controller != nullptr);

  QSignalSpy stateChanged(controller.get(), &MutationController::stateChanged);
  QVERIFY(controller->trashItem(entry.absolutePath, identityMapFor(entry)));
  QTRY_VERIFY(!controller->busy());
  QVERIFY(!stateChanged.isEmpty());
  QCOMPARE(controller->failureCode(), QStringLiteral("none"));
  QVERIFY(!QFile::exists(filePath));
  QVERIFY(QDir(dataHomeDir.path()).exists(QStringLiteral("Trash")));
}

QTEST_GUILESS_MAIN(TestDesktopFileBoundary)
#include "tst_desktop_file_boundary.moc"
