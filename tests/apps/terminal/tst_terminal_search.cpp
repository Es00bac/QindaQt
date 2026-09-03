// SPDX-License-Identifier: GPL-3.0-or-later
#include "search/terminal_search.h"

#include <QElapsedTimer>
#include <QtTest>

using namespace QindaQt::Apps::Terminal;

class TerminalSearchTest final : public QObject {
  Q_OBJECT

private slots:
  void literalCaseAndNoMatchTruth();
  void boundedRegexSubsetWorks();
  void hostileRegexIsRejectedBeforeExecution();
  void patternAndMatchBoundsFailClosed();
};

void TerminalSearchTest::literalCaseAndNoMatchTruth() {
  const QString text = QStringLiteral("Alpha alpha ALPHA\nneedle\n");
  auto scan = scanTerminalText(
      text, {.pattern = QStringLiteral("alpha"), .caseSensitive = false});
  QVERIFY(scan.result.accepted);
  QVERIFY(scan.result.found);
  QCOMPARE(scan.result.total, 3);

  scan = scanTerminalText(
      text, {.pattern = QStringLiteral("Alpha"), .caseSensitive = true});
  QCOMPARE(scan.result.total, 1);
  scan = scanTerminalText(
      text, {.pattern = QStringLiteral("absent"), .caseSensitive = true});
  QVERIFY(scan.result.accepted);
  QVERIFY(!scan.result.found);
  QCOMPARE(scan.result.diagnostic, QStringLiteral("No matches"));
}

void TerminalSearchTest::boundedRegexSubsetWorks() {
  const auto scan = scanTerminalText(
      QStringLiteral("cat cot cut dog"),
      {.pattern = QStringLiteral("c[ao]t|cut"),
       .caseSensitive = true,
       .regularExpression = true});
  QVERIFY(scan.result.accepted);
  QCOMPARE(scan.result.total, 3);
}

void TerminalSearchTest::hostileRegexIsRejectedBeforeExecution() {
  const QString hostileText(4 * 1024 * 1024, QLatin1Char('a'));
  QElapsedTimer timer;
  timer.start();
  const auto nested = scanTerminalText(
      hostileText,
      {.pattern = QStringLiteral("(a+)+$"),
       .caseSensitive = true,
       .regularExpression = true});
  QVERIFY(!nested.result.accepted);
  QVERIFY(nested.result.diagnostic.contains(QStringLiteral("complex")));
  QVERIFY2(timer.elapsed() < 100,
           qPrintable(QStringLiteral("hostile rejection took %1 ms")
                          .arg(timer.elapsed())));

  const auto extension = scanTerminalText(
      hostileText,
      {.pattern = QStringLiteral("(?=a)"),
       .caseSensitive = true,
       .regularExpression = true});
  QVERIFY(!extension.result.accepted);
  const auto controlVerb = scanTerminalText(
      hostileText,
      {.pattern = QStringLiteral("(*UTF)a"),
       .caseSensitive = true,
       .regularExpression = true});
  QVERIFY(!controlVerb.result.accepted);
}

void TerminalSearchTest::patternAndMatchBoundsFailClosed() {
  auto scan = scanTerminalText(
      QStringLiteral("text"),
      {.pattern = QString(kTerminalSearchPatternLimit + 1, QLatin1Char('x'))});
  QVERIFY(!scan.result.accepted);
  scan = scanTerminalText(
      QString(kTerminalSearchMatchLimit + 1, QLatin1Char('x')),
      {.pattern = QStringLiteral("x"), .caseSensitive = true});
  QVERIFY(!scan.result.accepted);
  QVERIFY(scan.result.diagnostic.contains(QStringLiteral("10000")));
}

QTEST_APPLESS_MAIN(TerminalSearchTest)
#include "tst_terminal_search.moc"
