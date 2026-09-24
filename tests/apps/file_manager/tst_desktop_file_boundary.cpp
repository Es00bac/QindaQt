// SPDX-License-Identifier: GPL-3.0-or-later
#include "public/desktop_file_boundary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

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

[[nodiscard]] Desktop::ListedIdentity identityOf(const ListingResult &listing,
                                                const QString &name) {
  for (const DirectoryEntry &entry : listing.entries) {
    if (entry.name == name) {
      return {entry.device, entry.inode};
    }
  }
  return {};
}

// A stand-in qindaqt-file-manager that records its exact argv, one
// NUL-terminated element per argument, then renames the record into place so
// a reader never observes a partial write.
[[nodiscard]] QString writeRecordingProgram(const QString &directory, const QString &record) {
  const QString program = directory + QStringLiteral("/qindaqt-file-manager");
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  file.write(QStringLiteral("#!/bin/sh\nprintf '%s\\0' \"$@\" > '%1.tmp' && mv '%1.tmp' '%1'\n")
                 .arg(record)
                 .toLocal8Bit());
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  return program;
}

[[nodiscard]] QStringList recordedArguments(const QString &record) {
  QFile file(record);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  QStringList arguments;
  for (const QByteArray &argument : file.readAll().split('\0')) {
    arguments.append(QString::fromLocal8Bit(argument));
  }
  if (!arguments.isEmpty() && arguments.constLast().isEmpty()) {
    arguments.removeLast();
  }
  return arguments;
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
  void folderOpenStartsFileManagerWithTheCanonicalDirectoryArgv();
  void folderOpenRefusesMissingReplacedAndNonDirectoryTargetsWithoutLaunch();
  void folderOpenReportsMissingProgramAndRefusedLaunch();
  void getInfoStartsFileManagerSelectingTheItemWithItsProperties();
  void getInfoRefusesReplacedMissingAndRootItemsWithoutLaunch();

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

void TestDesktopFileBoundary::folderOpenStartsFileManagerWithTheCanonicalDirectoryArgv() {
  QTemporaryDir root;
  QTemporaryDir bin;
  QVERIFY(root.isValid() && bin.isValid());
  // Shell metacharacters must reach File Manager as one literal argument.
  const QString hostileName = QStringLiteral("Shell $(touch PWNED); `id` \"q\" 'x' *");
  QVERIFY(QDir(root.path()).mkdir(hostileName));
  QVERIFY(QFile::link(root.filePath(hostileName), root.filePath(QStringLiteral("Link"))));
  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(root.path());
  QVERIFY(listing.ok());
  const QString canonical = QFileInfo(root.filePath(hostileName)).canonicalFilePath();

  QStringList programs;
  QList<QStringList> arguments;
  const Desktop::ProcessStarter capture = [&](const QString &program, const QStringList &argv) {
    programs.append(program);
    arguments.append(argv);
    return true;
  };
  const QString record = bin.filePath(QStringLiteral("argv"));
  const QString program = writeRecordingProgram(bin.path(), record);
  QVERIFY(!program.isEmpty());
  const Desktop::FolderOpenResult direct = Desktop::FileBoundary::openLocalFolder(
      root.filePath(hostileName), identityOf(listing, hostileName), {program}, capture);
  QVERIFY2(direct.ok(), qPrintable(direct.diagnostic));
  QCOMPARE(programs, QStringList{program});
  QCOMPARE(arguments, QList<QStringList>{QStringList{canonical}});

  // A listed symlink opens its canonical target, still as one argument.
  const Desktop::FolderOpenResult link = Desktop::FileBoundary::openLocalFolder(
      root.filePath(QStringLiteral("Link")), identityOf(listing, QStringLiteral("Link")),
      {program}, capture);
  QVERIFY2(link.ok(), qPrintable(link.diagnostic));
  QCOMPARE(link.canonicalPath, canonical);
  QCOMPARE(arguments.constLast(), QStringList{canonical});

  // The production starter runs the real program with that argv and no shell.
  // The child inherits a fresh working directory, so an interpolated
  // `touch PWNED` could only land there and no earlier run leaves a marker.
  QTemporaryDir workingDirectory;
  QVERIFY(workingDirectory.isValid());
  const QString previousDirectory = QDir::currentPath();
  const auto restoreDirectory =
      qScopeGuard([&previousDirectory] { QDir::setCurrent(previousDirectory); });
  QVERIFY(QDir::setCurrent(workingDirectory.path()));
  const Desktop::FolderOpenResult started = Desktop::FileBoundary::openLocalFolder(
      root.filePath(hostileName), identityOf(listing, hostileName), {program});
  QVERIFY2(started.ok(), qPrintable(started.diagnostic));
  QTRY_COMPARE(recordedArguments(record), QStringList{canonical});
  QVERIFY(!QFile::exists(workingDirectory.filePath(QStringLiteral("PWNED"))));
  QVERIFY(!QFile::exists(root.filePath(QStringLiteral("PWNED"))));
}

void TestDesktopFileBoundary::folderOpenRefusesMissingReplacedAndNonDirectoryTargetsWithoutLaunch() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Gone")));
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Swapped")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("notes.txt"))));
  QVERIFY(QFile::link(root.filePath(QStringLiteral("absent")), root.filePath(QStringLiteral("Dangling"))));
  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(root.path());
  QVERIFY(listing.ok());

  QVERIFY(QDir(root.filePath(QStringLiteral("Gone"))).removeRecursively());
  QVERIFY(QDir(root.path()).rename(QStringLiteral("Swapped"), QStringLiteral("Moved")));
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Swapped")));

  int starts = 0;
  const Desktop::ProcessStarter refuseToRun = [&starts](const QString &, const QStringList &) {
    ++starts;
    return true;
  };
  const QStringList programs{QStringLiteral("/bin/true")};
  const auto open = [&](const QString &name, Desktop::ListedIdentity identity) {
    return Desktop::FileBoundary::openLocalFolder(root.filePath(name), identity, programs,
                                                  refuseToRun);
  };
  QCOMPARE(open(QStringLiteral("Gone"), identityOf(listing, QStringLiteral("Gone"))).error,
           Desktop::FolderOpenError::NotFound);
  QCOMPARE(open(QStringLiteral("Swapped"), identityOf(listing, QStringLiteral("Swapped"))).error,
           Desktop::FolderOpenError::Replaced);
  QCOMPARE(open(QStringLiteral("notes.txt"), identityOf(listing, QStringLiteral("notes.txt"))).error,
           Desktop::FolderOpenError::NotDirectory);
  QCOMPARE(open(QStringLiteral("Dangling"), identityOf(listing, QStringLiteral("Dangling"))).error,
           Desktop::FolderOpenError::NotFound);
  const Desktop::FolderOpenResult relative = Desktop::FileBoundary::openLocalFolder(
      QStringLiteral("Moved"), identityOf(listing, QStringLiteral("Swapped")), programs, refuseToRun);
  QCOMPARE(relative.error, Desktop::FolderOpenError::NotFound);
  QVERIFY(!relative.diagnostic.isEmpty());
  QCOMPARE(starts, 0);
}

void TestDesktopFileBoundary::folderOpenReportsMissingProgramAndRefusedLaunch() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Folder")));
  QVERIFY(writeFile(root.filePath(QStringLiteral("not-executable"))));
  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(root.path());
  const Desktop::ListedIdentity identity = identityOf(listing, QStringLiteral("Folder"));

  int starts = 0;
  const Desktop::ProcessStarter counting = [&starts](const QString &, const QStringList &) {
    ++starts;
    return false;
  };
  const QStringList unusable{QStringLiteral("qindaqt-file-manager"),
                             root.filePath(QStringLiteral("missing/qindaqt-file-manager")),
                             root.filePath(QStringLiteral("not-executable"))};
  const Desktop::FolderOpenResult missing = Desktop::FileBoundary::openLocalFolder(
      root.filePath(QStringLiteral("Folder")), identity, unusable, counting);
  QCOMPARE(missing.error, Desktop::FolderOpenError::NotInstalled);
  QCOMPARE(starts, 0);

  const Desktop::FolderOpenResult refused = Desktop::FileBoundary::openLocalFolder(
      root.filePath(QStringLiteral("Folder")), identity, {QStringLiteral("/bin/true")}, counting);
  QCOMPARE(refused.error, Desktop::FolderOpenError::LaunchRefused);
  QVERIFY(!refused.diagnostic.isEmpty());
  QCOMPARE(starts, 1);

  const QStringList defaults = Desktop::FileBoundary::fileManagerProgramCandidates();
  QVERIFY(!defaults.isEmpty());
  QCOMPARE(defaults.constFirst(),
           QCoreApplication::applicationDirPath() + QStringLiteral("/qindaqt-file-manager"));
  for (const QString &candidate : defaults) {
    QVERIFY(QFileInfo(candidate).isAbsolute());
    QVERIFY(candidate.endsWith(QStringLiteral("/qindaqt-file-manager")));
  }
}

// ADR-0273: the Desktop's Get Info starts File Manager on the item's folder
// with the item selected and its properties open, on the one reveal command
// line File Manager's main.cpp reads.
void TestDesktopFileBoundary::getInfoStartsFileManagerSelectingTheItemWithItsProperties() {
  QTemporaryDir root;
  QTemporaryDir bin;
  QVERIFY(root.isValid() && bin.isValid());
  const QString hostileName = QStringLiteral("--help $(touch PWNED) 'x'.txt");
  QVERIFY(writeFile(root.filePath(hostileName)));
  QVERIFY(QFile::link(root.filePath(QStringLiteral("absent")),
                      root.filePath(QStringLiteral("Dangling"))));
  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(root.path());
  QVERIFY(listing.ok());
  const QString folder = QFileInfo(root.path()).canonicalFilePath();

  QStringList programs;
  QList<QStringList> arguments;
  const Desktop::ProcessStarter capture = [&](const QString &program, const QStringList &argv) {
    programs.append(program);
    arguments.append(argv);
    return true;
  };
  const QString record = bin.filePath(QStringLiteral("argv"));
  const QString program = writeRecordingProgram(bin.path(), record);
  QVERIFY(!program.isEmpty());
  const Desktop::FolderOpenResult info = Desktop::FileBoundary::revealLocalItem(
      root.filePath(hostileName), identityOf(listing, hostileName), true, {program}, capture);
  QVERIFY2(info.ok(), qPrintable(info.diagnostic));
  QCOMPARE(info.canonicalPath, folder);
  QCOMPARE(programs, QStringList{program});
  // "--select=" keeps a name that starts with "--" a value, never an option.
  const QStringList expected{QStringLiteral("--select=") + hostileName,
                             QStringLiteral("--show-properties"), folder};
  QCOMPARE(arguments, QList<QStringList>{expected});
  QCOMPARE(Desktop::FileBoundary::revealArguments({folder, {hostileName}, true}), expected);

  // Selecting without properties drops the flag; a dangling link is an item.
  const Desktop::FolderOpenResult select = Desktop::FileBoundary::revealLocalItem(
      root.filePath(QStringLiteral("Dangling")), identityOf(listing, QStringLiteral("Dangling")),
      false, {program}, capture);
  QVERIFY2(select.ok(), qPrintable(select.diagnostic));
  QCOMPARE(arguments.constLast(),
           (QStringList{QStringLiteral("--select=Dangling"), folder}));

  // The production starter runs the real program with exactly that argv.
  QTemporaryDir workingDirectory;
  QVERIFY(workingDirectory.isValid());
  const QString previousDirectory = QDir::currentPath();
  const auto restoreDirectory =
      qScopeGuard([&previousDirectory] { QDir::setCurrent(previousDirectory); });
  QVERIFY(QDir::setCurrent(workingDirectory.path()));
  const Desktop::FolderOpenResult started = Desktop::FileBoundary::revealLocalItem(
      root.filePath(hostileName), identityOf(listing, hostileName), true, {program});
  QVERIFY2(started.ok(), qPrintable(started.diagnostic));
  QTRY_COMPARE(recordedArguments(record), expected);
  QVERIFY(!QFile::exists(workingDirectory.filePath(QStringLiteral("PWNED"))));
}

void TestDesktopFileBoundary::getInfoRefusesReplacedMissingAndRootItemsWithoutLaunch() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  QVERIFY(writeFile(root.filePath(QStringLiteral("gone.txt"))));
  QVERIFY(writeFile(root.filePath(QStringLiteral("swapped.txt"))));
  const ListingResult listing = Desktop::FileBoundary::listLocalFolder(root.path());
  QVERIFY(listing.ok());
  QVERIFY(QFile::remove(root.filePath(QStringLiteral("gone.txt"))));
  QVERIFY(QFile::rename(root.filePath(QStringLiteral("swapped.txt")),
                        root.filePath(QStringLiteral("moved.txt"))));
  QVERIFY(writeFile(root.filePath(QStringLiteral("swapped.txt"))));

  int starts = 0;
  const Desktop::ProcessStarter counting = [&starts](const QString &, const QStringList &) {
    ++starts;
    return true;
  };
  const QStringList programs{QStringLiteral("/bin/true")};
  const auto info = [&](const QString &path, Desktop::ListedIdentity identity) {
    return Desktop::FileBoundary::revealLocalItem(path, identity, true, programs, counting);
  };
  QCOMPARE(info(root.filePath(QStringLiteral("gone.txt")),
                identityOf(listing, QStringLiteral("gone.txt")))
               .error,
           Desktop::FolderOpenError::NotFound);
  QCOMPARE(info(root.filePath(QStringLiteral("swapped.txt")),
                identityOf(listing, QStringLiteral("swapped.txt")))
               .error,
           Desktop::FolderOpenError::Replaced);
  QCOMPARE(info(QStringLiteral("swapped.txt"), identityOf(listing, QStringLiteral("swapped.txt")))
               .error,
           Desktop::FolderOpenError::NotFound);
  // "/" is no folder's item: with its true identity the refusal is the name.
  struct stat rootStatus {};
  QCOMPARE(::lstat("/", &rootStatus), 0);
  QCOMPARE(info(QStringLiteral("/"), {static_cast<quint64>(rootStatus.st_dev),
                                      static_cast<quint64>(rootStatus.st_ino)})
               .error,
           Desktop::FolderOpenError::NotFound);
  QCOMPARE(starts, 0);
}

QTEST_GUILESS_MAIN(TestDesktopFileBoundary)
#include "tst_desktop_file_boundary.moc"
