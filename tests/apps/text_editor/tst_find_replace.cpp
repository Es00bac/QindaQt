// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "find/find_replace_engine.h"
#include "ui/editor_window.h"
#include "ui/find_replace_bar.h"


#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTest>

using namespace QindaQt::Apps::TextEditor;

namespace {

DocumentStoreFactory localFactory() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}

} // namespace

class FindReplaceTest final : public QObject {
  Q_OBJECT

private slots:
  void findsWithOptionsAndWrap();
  void hostilePatternsFailWithinBound();
  void replaceAllIsOneUndoStep();
  void noMatchHasTextStatus();
};

void FindReplaceTest::findsWithOptionsAndWrap() {
  FindOptions options{.pattern = QStringLiteral("cat")};
  FindResult result = FindReplaceEngine::find(QStringLiteral("Cat scatter cat"),
                                              options, 1, FindDirection::Next);
  QCOMPARE(result.matches.size(), 3);
  QCOMPARE(result.currentIndex, 1);

  options.wholeWord = true;
  result = FindReplaceEngine::find(QStringLiteral("Cat scatter cat"), options,
                                   15, FindDirection::Next);
  QCOMPARE(result.matches.size(), 2);
  QCOMPARE(result.currentIndex, 0);
  QVERIFY(result.wrapped);

  options.caseSensitive = true;
  result = FindReplaceEngine::find(QStringLiteral("Cat cat"), options, 7,
                                   FindDirection::Previous);
  QCOMPARE(result.matches.size(), 1);
  QCOMPARE(result.currentIndex, 0);
}

void FindReplaceTest::hostilePatternsFailWithinBound() {
  QElapsedTimer timer;
  timer.start();
  const FindResult nested = FindReplaceEngine::find(
      QString(200'000, u'a') + u'!',
      {.pattern = QStringLiteral("(a+)+$"), .regularExpression = true}, 0,
      FindDirection::Next);
  QVERIFY(timer.elapsed() < 100);
  QCOMPARE(nested.error, FindError::UnsafeRegularExpression);

  const FindResult tooLong = FindReplaceEngine::find(
      QStringLiteral("text"), {.pattern = QString(257, u'x')}, 0,
      FindDirection::Next);
  QCOMPARE(tooLong.error, FindError::PatternTooLong);

  const FindResult invalid = FindReplaceEngine::find(
      QStringLiteral("text"),
      {.pattern = QStringLiteral("[z-a]"), .regularExpression = true}, 0,
      FindDirection::Next);
  QCOMPARE(invalid.error, FindError::InvalidRegularExpression);
}

void FindReplaceTest::replaceAllIsOneUndoStep() {
  EditorWindow window(localFactory());
  window.editor()->setPlainText(QStringLiteral("cat and cat"));
  window.findBar()->open(true);
  window.findBar()->findEditor()->setText(QStringLiteral("cat"));
  window.findBar()->replaceEditor()->setText(QStringLiteral("dog"));
  auto *replaceAll =
      window.findChild<QPushButton *>(QStringLiteral("replaceAllButton"));
  QVERIFY(replaceAll != nullptr);
  replaceAll->click();
  QCOMPARE(window.editor()->toPlainText(), QStringLiteral("dog and dog"));
  window.editor()->undo();
  QCOMPARE(window.editor()->toPlainText(), QStringLiteral("cat and cat"));
}

void FindReplaceTest::noMatchHasTextStatus() {
  EditorWindow window(localFactory());
  window.show();
  window.editor()->setPlainText(QStringLiteral("alpha"));
  window.findBar()->open(false);
  window.findBar()->findEditor()->setText(QStringLiteral("omega"));
  window.findChild<QPushButton *>(QStringLiteral("findNextButton"))->click();
  const auto *status = window.findChild<QLabel *>(QStringLiteral("findStatus"));
  QCOMPARE(status->text(), QStringLiteral("No matches"));
  QVERIFY(!status->accessibleDescription().isEmpty());
}

QTEST_MAIN(FindReplaceTest)
#include "tst_find_replace.moc"
