// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/document_editor.h"
#include <QTest>
#include <QTextBlock>
using namespace QindaQt::Apps::TextEditor;
class DocumentEditorTest final : public QObject {
  Q_OBJECT
private slots:
  void indentRoundTripAndUndo() {
    DocumentEditor editor;
    editor.setPlainText(QStringLiteral("one\ntwo\nthree"));
    auto cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(8, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    editor.indentLines(false);
    QCOMPARE(editor.toPlainText(), QStringLiteral("    one\n    two\nthree"));
    editor.indentLines(true);
    QCOMPARE(editor.toPlainText(), QStringLiteral("one\ntwo\nthree"));
    editor.undo();
    QCOMPARE(editor.toPlainText(), QStringLiteral("    one\n    two\nthree"));
    editor.undo();
    QCOMPARE(editor.toPlainText(), QStringLiteral("one\ntwo\nthree"));
  }
  void autoIndentAndTabStops() {
    DocumentEditor editor;
    editor.setPlainText(QStringLiteral("    hello"));
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("    hello\n    "));
    editor.undo();
    QCOMPARE(editor.toPlainText(), QStringLiteral("    hello"));
    editor.setPlainText(QStringLiteral("ab"));
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QStringLiteral("ab  "));
  }
  void syntaxDoesNotChangeTextOrDirtyState() {
    DocumentEditor editor;
    editor.setPlainText(QStringLiteral("def example():\n    return 42\n"));
    editor.document()->setModified(false);
    editor.setDocumentPath(QStringLiteral("example.py"));
    QTest::qWait(5);
    QCOMPARE(editor.syntaxName(), QStringLiteral("Python"));
    QVERIFY(!editor.document()->isModified());
    QCOMPARE(editor.toPlainText(),
             QStringLiteral("def example():\n    return 42\n"));
    editor.setDocumentPath(QStringLiteral("untitled.unknown-extension"));
    QCOMPARE(editor.syntaxName(), QStringLiteral("Plain text"));
  }
  void navigationAndGutter() {
    DocumentEditor editor;
    const int small = editor.gutterWidth();
    editor.setPlainText(QString(100, QLatin1Char('\n')));
    QVERIFY(editor.gutterWidth() > small);
    editor.goToLine(42);
    QCOMPARE(editor.textCursor().blockNumber(), 41);
    editor.goToLine(999);
    QCOMPARE(editor.textCursor().blockNumber(), 100);
    editor.goToLine(-1);
    QCOMPARE(editor.textCursor().blockNumber(), 0);
  }
  void zoomSurvivesAppearanceWithoutEditing() {
    DocumentEditor editor;
    QFont base(QStringLiteral("monospace"), 12);
    editor.setBaseFont(base);
    editor.zoomText(2);
    QCOMPARE(editor.font().pointSizeF(), 14.0);
    base.setPointSize(14);
    editor.setBaseFont(base);
    QCOMPARE(editor.font().pointSizeF(), 16.0);
    editor.resetZoom();
    QCOMPARE(editor.font().pointSizeF(), 14.0);
    QVERIFY(editor.document()->isEmpty());
  }
};
QTEST_MAIN(DocumentEditorTest)
#include "tst_document_editor.moc"
