// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include <QDir>
#include <QFile>
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

[[nodiscard]] QVariantMap rowNamed(const QVariantList &rows, const QString &label) {
  for (const QVariant &row : rows) {
    const QVariantMap map = row.toMap();
    if (map.value(QStringLiteral("label")).toString() == label) {
      return map;
    }
  }
  return {};
}

} // namespace

// Failure-before coverage for DesktopContentsController: the boundary's own
// typed-failure paths (missing target, non-regular target) are the only
// launch outcomes a hostile/offline test environment can assert on
// deterministically, mirroring tst_desktop_file_boundary.cpp's own launch
// tests. A real successful launch depends on the desktop's configured
// handlers and is out of scope here for the same reason it is there.
class DesktopContentsControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void cleanup();

  void listsRealFilesAndFoldersFromTheInjectedRoot();
  void hiddenEntriesAreOmittedFromThePresentation();
  void refreshPicksUpABoundedChange();
  void openSafelyRejectsAMissingTarget();
  void openSafelyRejectsADirectoryTarget();
  void emptyRootProducesZeroRowsWithoutFeedback();
  void unresolvedRootProducesZeroRowsWithFeedbackInsteadOfBlocking();

private:
  std::unique_ptr<QTemporaryDir> m_root;
};

void DesktopContentsControllerTests::init() {
  m_root = std::make_unique<QTemporaryDir>();
  QVERIFY(m_root->isValid());
}

void DesktopContentsControllerTests::cleanup() { m_root.reset(); }

void DesktopContentsControllerTests::listsRealFilesAndFoldersFromTheInjectedRoot() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Notes.txt"))));
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Projects"))));

  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 2);
  QCOMPARE(controller.feedback(), QString());

  const QVariantMap folder = rowNamed(controller.rows(), QStringLiteral("Projects"));
  QVERIFY(!folder.isEmpty());
  QCOMPARE(folder.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("folder"));
  QCOMPARE(folder.value(QStringLiteral("isDirectory")).toBool(), true);
  QCOMPARE(folder.value(QStringLiteral("path")).toString(),
           m_root->filePath(QStringLiteral("Projects")));
  QCOMPARE(folder.value(QStringLiteral("id")).toString(),
           folder.value(QStringLiteral("path")).toString());
  QVERIFY(folder.value(QStringLiteral("accessibleName")).toString().contains(
      QStringLiteral("Projects")));

  const QVariantMap file = rowNamed(controller.rows(), QStringLiteral("Notes.txt"));
  QVERIFY(!file.isEmpty());
  QCOMPARE(file.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("text-x-generic"));
  QCOMPARE(file.value(QStringLiteral("isDirectory")).toBool(), false);

  // Directories sort before files (LocalDirectoryLister's contract).
  QCOMPARE(controller.rows().first().toMap().value(QStringLiteral("label")).toString(),
           QStringLiteral("Projects"));
}

void DesktopContentsControllerTests::hiddenEntriesAreOmittedFromThePresentation() {
  QVERIFY(writeFile(m_root->filePath(QStringLiteral("Visible.txt"))));
  QVERIFY(writeFile(m_root->filePath(QStringLiteral(".hidden"))));

  DesktopContentsController controller(m_root->path());
  QCOMPARE(controller.rows().size(), 1);
  QCOMPARE(controller.rows().first().toMap().value(QStringLiteral("label")).toString(),
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
  QCOMPARE(controller.rows().first().toMap().value(QStringLiteral("label")).toString(),
           QStringLiteral("Later.txt"));
}

void DesktopContentsControllerTests::openSafelyRejectsAMissingTarget() {
  DesktopContentsController controller(m_root->path());
  QVERIFY(!controller.open(m_root->filePath(QStringLiteral("ghost.txt"))));
  QVERIFY(!controller.feedback().isEmpty());
}

void DesktopContentsControllerTests::openSafelyRejectsADirectoryTarget() {
  QVERIFY(QDir().mkpath(m_root->filePath(QStringLiteral("Projects"))));
  DesktopContentsController controller(m_root->path());

  QSignalSpy feedbackChanged(&controller,
                            &DesktopContentsController::feedbackChanged);
  QVERIFY(!controller.open(m_root->filePath(QStringLiteral("Projects"))));
  QVERIFY(!controller.feedback().isEmpty());
  QCOMPARE(feedbackChanged.size(), 1);

  controller.clearFeedback();
  QCOMPARE(controller.feedback(), QString());
}

void DesktopContentsControllerTests::emptyRootProducesZeroRowsWithoutFeedback() {
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

QTEST_GUILESS_MAIN(DesktopContentsControllerTests)
#include "tst_desktop_contents_controller.moc"
