// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "ui/editor_window.h"

#include <QAction>
#include <QFile>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

namespace {

[[nodiscard]] DocumentStoreFactory localFactory() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}

} // namespace

class EditorPrintingTest final : public QObject {
  Q_OBJECT

private slots:
  void printActionsExistInFileMenuAndCatalogShape();
  void printActionsFollowDocumentContent();
  void pdfRenderWritesTheDocument();
  void pdfRenderRejectsAnEmptyTarget();
};

void EditorPrintingTest::printActionsExistInFileMenuAndCatalogShape() {
  EditorWindow window(localFactory());
  auto *print = window.findChild<QAction *>(QStringLiteral("filePrintAction"));
  auto *preview =
      window.findChild<QAction *>(QStringLiteral("filePrintPreviewAction"));
  QVERIFY(print);
  QVERIFY(preview);
  QCOMPARE(print->shortcut(), QKeySequence(QKeySequence::Print));
  QCOMPARE(preview->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+P")));
  QCOMPARE(print->shortcutContext(), Qt::WindowShortcut);
  auto *fileMenu = window.findChild<QMenu *>(QStringLiteral("fileMenu"));
  QVERIFY(fileMenu);
  QVERIFY(fileMenu->actions().contains(print));
  QVERIFY(fileMenu->actions().contains(preview));
  QVERIFY(fileMenu->actions().indexOf(print) <
          fileMenu->actions().indexOf(
              window.findChild<QAction *>(QStringLiteral("fileQuitAction"))));
}

void EditorPrintingTest::printActionsFollowDocumentContent() {
  EditorWindow window(localFactory());
  auto *print = window.findChild<QAction *>(QStringLiteral("filePrintAction"));
  auto *preview =
      window.findChild<QAction *>(QStringLiteral("filePrintPreviewAction"));
  // An empty document would print a blank page; both actions stay disabled.
  QVERIFY(!print->isEnabled());
  QVERIFY(!preview->isEnabled());
  window.editor()->insertPlainText(QStringLiteral("a page worth printing"));
  QVERIFY(print->isEnabled());
  QVERIFY(preview->isEnabled());
  window.editor()->undo();
  QVERIFY(!print->isEnabled());
  QVERIFY(!preview->isEnabled());
}

void EditorPrintingTest::pdfRenderWritesTheDocument() {
  EditorWindow window(localFactory());
  window.editor()->insertPlainText(
      QStringLiteral("QindaQt print pipeline fixture\nsecond line\n"));
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString output = directory.filePath(QStringLiteral("document.pdf"));
  QVERIFY(window.printDocumentToPdfFile(output));
  QFile file(output);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();
  QVERIFY(bytes.startsWith("%PDF-"));
  // A paginated two-line document in the document font must be a nontrivial
  // payload; this floor also catches an accidentally empty renderer.
  QVERIFY(bytes.size() > 1000);
}

void EditorPrintingTest::pdfRenderRejectsAnEmptyTarget() {
  EditorWindow window(localFactory());
  window.editor()->insertPlainText(QStringLiteral("content"));
  QVERIFY(!window.printDocumentToPdfFile(QString()));
}

QTEST_MAIN(EditorPrintingTest)
#include "tst_editor_printing.moc"
