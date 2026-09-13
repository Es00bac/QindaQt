// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using QindaQt::Shell::DesktopSurface::DesktopContentsController;

namespace {

[[nodiscard]] bool writeFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly);
}

[[nodiscard]] QVariantMap rowNamed(const QVariantList &rows,
                                   const QString &label) {
  for (const QVariant &row : rows) {
    const QVariantMap map = row.toMap();
    if (map.value(QStringLiteral("label")).toString() == label) {
      return map;
    }
  }
  return {};
}

// A stand-in qindaqt-file-manager recording its exact argv (NUL-terminated),
// renamed into place so a reader never sees a partial record.
[[nodiscard]] QString writeRecordingProgram(const QString &directory,
                                            const QString &record) {
  const QString program = directory + QStringLiteral("/qindaqt-file-manager");
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  file.write(
      QStringLiteral(
          "#!/bin/sh\nprintf '%s\\0' \"$@\" > '%1.tmp' && mv '%1.tmp' '%1'\n")
          .arg(record)
          .toLocal8Bit());
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);
  return program;
}

[[nodiscard]] QByteArray recorded(const QString &record) {
  QFile file(record);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

// Folder activation is proven end to end against a recording stand-in for
// qindaqt-file-manager; regular files keep FileBoundary::launchLocalFile, whose
// typed pre-flight failures are the only file-launch outcomes an offline test
// can assert without invoking the desktop's configured handlers.
class DesktopContentsControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void cleanup();

  void listsRealFilesAndFoldersFromTheInjectedRoot();
  void hiddenEntriesAreOmittedFromThePresentation();
  void refreshPicksUpABoundedChange();
  void openSafelyRejectsAMissingTarget();
  void openingAListedFolderStartsFileManagerWithItsCanonicalPath();
  void folderActivationFailsTruthfullyWithoutLaunching();
  void regularFileActivationKeepsTheDefaultHandlerPath();
  void emptyRootProducesZeroRowsWithoutFeedback();
  void unresolvedRootProducesZeroRowsWithFeedbackInsteadOfBlocking();
  void renamesAListedEntryThroughTheIdentityBoundary();
  void rowsCarryTheIdentityFieldsTheBatchContractsConsume();
  void trashesOnlyListedEntriesThroughTheIdentityBoundary();
  void clipboardOperationsFailClosedWithoutAClipboard();

private:
  std::unique_ptr<QTemporaryDir> m_root;
  std::unique_ptr<QTemporaryDir> m_bin;
  QString m_record;
  QString m_program;
};

void DesktopContentsControllerTests::init() {
  m_root = std::make_unique<QTemporaryDir>();
  m_bin = std::make_unique<QTemporaryDir>();
  QVERIFY(m_root->isValid() && m_bin->isValid());
  m_record = m_bin->filePath(QStringLiteral("argv"));
  m_program = writeRecordingProgram(m_bin->path(), m_record);
  QVERIFY(!m_program.isEmpty());
}

void DesktopContentsControllerTests::cleanup() {
  m_root.reset();
  m_bin.reset();
}

void DesktopContentsControllerTests::
    listsRealFilesAndFoldersFromTheInjectedRoot() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Notes.txt"))));
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Projects"))));

  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 2);
  QCOMPARE(controller.feedback(), QString());

  const QVariantMap folder =
      rowNamed(controller.rows(), QStringLiteral("Projects"));
  QVERIFY(!folder.isEmpty());
  QCOMPARE(folder.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("folder"));
  QCOMPARE(folder.value(QStringLiteral("isDirectory")).toBool(), true);
  QCOMPARE(folder.value(QStringLiteral("path")).toString(),
           m_root->filePath(QStringLiteral("Projects")));
  QCOMPARE(folder.value(QStringLiteral("id")).toString(),
           folder.value(QStringLiteral("path")).toString());
  QVERIFY(folder.value(QStringLiteral("accessibleName"))
              .toString()
              .contains(QStringLiteral("Projects")));

  const QVariantMap file =
      rowNamed(controller.rows(), QStringLiteral("Notes.txt"));
  QVERIFY(!file.isEmpty());
  QCOMPARE(file.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("text-x-generic"));
  QCOMPARE(file.value(QStringLiteral("isDirectory")).toBool(), false);

  // Directories sort before files (LocalDirectoryLister's contract).
  QCOMPARE(controller.rows()
               .first()
               .toMap()
               .value(QStringLiteral("label"))
               .toString(),
           QStringLiteral("Projects"));
}

void DesktopContentsControllerTests::
    hiddenEntriesAreOmittedFromThePresentation() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Visible.txt"))));
  QVERIFY(writeFile(m_root->filePath(QStringLiteral(".hidden"))));

  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 1);
  QCOMPARE(controller.rows()
               .first()
               .toMap()
               .value(QStringLiteral("label"))
               .toString(),
           QStringLiteral("Visible.txt"));
}

void DesktopContentsControllerTests::refreshPicksUpABoundedChange() {
  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 0);

  QSignalSpy rowsChanged(&controller, &DesktopContentsController::rowsChanged);
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Later.txt"))));
  controller.refresh();

  QCOMPARE(rowsChanged.size(), 1);
  QCOMPARE(controller.rows().size(), 1);
  QCOMPARE(controller.rows()
               .first()
               .toMap()
               .value(QStringLiteral("label"))
               .toString(),
           QStringLiteral("Later.txt"));
}

void DesktopContentsControllerTests::openSafelyRejectsAMissingTarget() {
  DesktopContentsController controller(m_root->path());
  QVERIFY(!controller.open(m_root->filePath(QStringLiteral("ghost.txt"))));
  QVERIFY(!controller.feedback().isEmpty());
}

void DesktopContentsControllerTests::
    openingAListedFolderStartsFileManagerWithItsCanonicalPath() {
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Projects"))));
  DesktopContentsController controller(m_root->path(), {m_program});
  const QString canonical =
      QFileInfo(m_root->filePath(QStringLiteral("Projects")))
          .canonicalFilePath();

  QVERIFY2(controller.open(m_root->filePath(QStringLiteral("Projects"))),
           qPrintable(controller.feedback()));
  QCOMPARE(controller.feedback(), QString());
  QTRY_COMPARE(recorded(m_record), canonical.toLocal8Bit() + '\0');
}

void DesktopContentsControllerTests::
    folderActivationFailsTruthfullyWithoutLaunching() {
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Replaced"))));
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Removed"))));
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Other"))));
  DesktopContentsController controller(m_root->path(), {m_program});

  // A listed folder replaced by a file, a removed folder, and a path that was
  // never listed each report feedback and never fall back to another folder.
  QVERIFY(
      QDir(m_root->filePath(QStringLiteral("Replaced"))).removeRecursively());
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Replaced"))));
  QVERIFY(
      QDir(m_root->filePath(QStringLiteral("Removed"))).removeRecursively());
  for (const QString &name :
       {QStringLiteral("Replaced"), QStringLiteral("Removed"),
        QStringLiteral("Unlisted")}) {
    QSignalSpy feedbackChanged(&controller,
                               &DesktopContentsController::feedbackChanged);
    QVERIFY(!controller.open(m_root->filePath(name)));
    QVERIFY2(!controller.feedback().isEmpty(), qPrintable(name));
    controller.clearFeedback();
  }

  // No installed File Manager is a typed refusal too.
  DesktopContentsController uninstalled(m_root->path(), QStringList{});
  QVERIFY(!uninstalled.open(m_root->filePath(QStringLiteral("Other"))));
  QVERIFY(uninstalled.feedback().contains(QStringLiteral("File Manager")));
  QTest::qWait(100);
  QVERIFY(!QFile::exists(m_record));
}

void DesktopContentsControllerTests::
    regularFileActivationKeepsTheDefaultHandlerPath() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Locked.txt"))));
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Became-folder.txt"))));
  QVERIFY(QFile::setPermissions(m_root->filePath(QStringLiteral("Locked.txt")),
                                QFileDevice::WriteOwner));
  DesktopContentsController controller(m_root->path(), {m_program});
  QVERIFY(QFile::remove(m_root->filePath(QStringLiteral("Became-folder.txt"))));
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Became-folder.txt"))));

  // Both outcomes carry launchLocalFile's own diagnostics and never reach the
  // File Manager program.
  QVERIFY(
      !controller.open(m_root->filePath(QStringLiteral("Became-folder.txt"))));
  QVERIFY(controller.feedback().endsWith(QStringLiteral("is not a file")));
  if (QFileInfo(m_root->filePath(QStringLiteral("Locked.txt"))).isReadable()) {
    QSKIP("running with permission override; unreadable-file refusal not "
          "observable");
  }
  QVERIFY(!controller.open(m_root->filePath(QStringLiteral("Locked.txt"))));
  QVERIFY(controller.feedback().endsWith(QStringLiteral("cannot be read")));
  QTest::qWait(100);
  QVERIFY(!QFile::exists(m_record));
}

void DesktopContentsControllerTests::
    emptyRootProducesZeroRowsWithoutFeedback() {
  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 0);
  QCOMPARE(controller.feedback(), QString());
}

void DesktopContentsControllerTests::
    unresolvedRootProducesZeroRowsWithFeedbackInsteadOfBlocking() {
  DesktopContentsController controller(
      m_root->filePath(QStringLiteral("does-not-exist")));
  QCOMPARE(controller.rows().size(), 0);
  QVERIFY(!controller.feedback().isEmpty());
}

void DesktopContentsControllerTests::
    renamesAListedEntryThroughTheIdentityBoundary() {
  const QString original = m_root->filePath(QStringLiteral("Old name.txt"));
  const QString renamed = m_root->filePath(QStringLiteral("New name.txt"));
  QVERIFY(writeFile(original));
  DesktopContentsController controller(m_root->path());
  const QVariantMap row =
      rowNamed(controller.rows(), QStringLiteral("Old name.txt"));
  QVERIFY(!row.value(QStringLiteral("layoutKey")).toString().isEmpty());

  QVERIFY2(controller.rename(original, QStringLiteral("New name.txt")),
           qPrintable(controller.feedback()));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(renamed), 5000);
  QTRY_VERIFY_WITH_TIMEOUT(
      !rowNamed(controller.rows(), QStringLiteral("New name.txt")).isEmpty(),
      5000);
  QCOMPARE(rowNamed(controller.rows(), QStringLiteral("New name.txt"))
               .value(QStringLiteral("layoutKey")),
           row.value(QStringLiteral("layoutKey")));
  QVERIFY(!QFileInfo::exists(original));
}

void DesktopContentsControllerTests::
    rowsCarryTheIdentityFieldsTheBatchContractsConsume() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Notes.txt"))));
  DesktopContentsController controller(m_root->path());
  const QVariantMap row =
      rowNamed(controller.rows(), QStringLiteral("Notes.txt"));
  for (const char *key :
       {"device", "inode", "identitySize", "modifiedNanoseconds", "mode"}) {
    const QString value = row.value(QLatin1String(key)).toString();
    QVERIFY2(!value.isEmpty(), key);
    // Decimal-string round trip is exactly what the batch contract parses.
    bool numeric = false;
    value.toULongLong(&numeric);
    QVERIFY2(numeric, key);
  }
  QCOMPARE(row.value(QStringLiteral("path")).toString(),
           m_root->filePath(QStringLiteral("Notes.txt")));
}

void DesktopContentsControllerTests::
    trashesOnlyListedEntriesThroughTheIdentityBoundary() {
  const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
  QTemporaryDir dataHome;
  QVERIFY(dataHome.isValid());
  qputenv("XDG_DATA_HOME", dataHome.path().toLocal8Bit());
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("First.txt"))));
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Second.txt"))));
  const QString ghost = m_root->filePath(QStringLiteral("Ghost.txt"));

  {
    DesktopContentsController controller(m_root->path());
    const QVariantList rows = controller.rows();
    QCOMPARE(rows.size(), 2);
    QVERIFY2(controller.trashEntries(rows), qPrintable(controller.feedback()));
    const QDir trashFiles(dataHome.filePath(QStringLiteral("Trash/files")));
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("First.txt")),
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("Second.txt")),
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.rows().isEmpty(), 5000);
    QVERIFY(!QFileInfo::exists(m_root->filePath(QStringLiteral("First.txt"))));
    QVERIFY(!QFileInfo::exists(m_root->filePath(QStringLiteral("Second.txt"))));

    // A path the listing never reported is refused before anything mutates;
    // the two legitimately trashed files stay exactly where they landed.
    QVERIFY(!controller.trashEntries(
        {QVariantMap{{QStringLiteral("path"), ghost}}}));
    QVERIFY(controller.feedback().contains(QStringLiteral("not on the Desktop")));
    QCOMPARE(trashFiles.entryList(QDir::Files | QDir::NoDotAndDotDot).size(), 2);
    QVERIFY(!trashFiles.exists(QStringLiteral("Ghost.txt")));
  }
  qputenv("XDG_DATA_HOME", previousDataHome);
}

void DesktopContentsControllerTests::
    clipboardOperationsFailClosedWithoutAClipboard() {
  // This row runs GUI-less: no QGuiApplication exists, so the composed
  // clipboard stays absent and every clipboard entry refuses closed with
  // feedback instead of crashing or dispatching.
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Notes.txt"))));
  DesktopContentsController controller(m_root->path());
  const QVariantList rows = controller.rows();
  QCOMPARE(rows.size(), 1);
  QVERIFY(!controller.canPaste());
  QCOMPARE(controller.clipboardMode(), QStringLiteral("none"));
  QVERIFY(!controller.copySelection(rows));
  QVERIFY(controller.feedback().contains(QStringLiteral("clipboard")));
  QVERIFY(!controller.cutSelection(rows));
  QVERIFY(!controller.pasteIntoDesktop());
}

QTEST_GUILESS_MAIN(DesktopContentsControllerTests)
#include "tst_desktop_contents_controller.moc"
