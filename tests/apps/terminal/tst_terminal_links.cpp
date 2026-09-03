// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link.h"
#include "links/terminal_link_opener.h"

#include <QtTest>

using namespace QindaQt::Apps::Terminal;

namespace {

class RecordingSpawner final : public TerminalLinkSpawner {
public:
  QString program;
  QStringList arguments;
  int calls = 0;
  bool succeeds = true;

  bool spawn(const QString &absoluteProgram, const QStringList &argv,
             QString *diagnostic) override {
    ++calls;
    program = absoluteProgram;
    arguments = argv;
    if (!succeeds && diagnostic != nullptr) {
      *diagnostic = QStringLiteral("recorded failure");
    }
    return succeeds;
  }
};

class RecordingConfirmation final : public TerminalLinkConfirmation {
public:
  TerminalLink seen;
  int calls = 0;
  bool accepted = true;

  bool confirm(const TerminalLink &link, QWidget *) override {
    ++calls;
    seen = link;
    return accepted;
  }
};

} // namespace

class TerminalLinksTest final : public QObject {
  Q_OBJECT

private slots:
  void detectsUrlsPathsAndTrimsPresentationPunctuation();
  void stripsControlsAndPreservesHostnameSpelling();
  void rejectsOverlongAndUnsupportedTargets();
  void openerConfirmsExactTargetAndUsesOneArgvElement();
  void openerCancellationAndFailureNeverSpawnOrShellParse();
};

void TerminalLinksTest::detectsUrlsPathsAndTrimsPresentationPunctuation() {
  const auto links = detectTerminalLinks(QStringLiteral(
      "See (https://example.test/a_(b)). '/home/user/My File.txt' "
      "and https://example.test/end.,"));
  QCOMPARE(links.size(), 3);
  QCOMPARE(links.at(0).target, QStringLiteral("https://example.test/a_(b)"));
  QCOMPARE(links.at(1).kind, TerminalLinkKind::LocalPath);
  QCOMPARE(links.at(1).target, QStringLiteral("/home/user/My File.txt"));
  QCOMPARE(links.at(2).target, QStringLiteral("https://example.test/end"));
}

void TerminalLinksTest::stripsControlsAndPreservesHostnameSpelling() {
  const auto links = detectTerminalLinks(
      QStringLiteral("https://xn--bcher-kva.example\nhttps://раypal.example ") +
      QStringLiteral("https://exam") + QChar(0x0007) +
      QStringLiteral("ple.test"));
  QCOMPARE(links.size(), 3);
  QCOMPARE(links.at(0).display,
           QStringLiteral("https://xn--bcher-kva.example"));
  QCOMPARE(links.at(1).display, QStringLiteral("https://раypal.example"));
  QVERIFY(links.at(0).tooltip.contains(QStringLiteral("exactly as printed")));
  QCOMPARE(links.at(2).target, QStringLiteral("https://example.test"));
  QVERIFY(!links.at(2).target.contains(QChar(0x0007)));
}

void TerminalLinksTest::rejectsOverlongAndUnsupportedTargets() {
  const QString overlong = QStringLiteral("https://example.test/") +
                           QString(kTerminalLinkTargetLimit, QLatin1Char('x'));
  const auto links = detectTerminalLinks(
      overlong + QStringLiteral(" ftp://example.test relative/path"));
  QVERIFY(links.isEmpty());
}

void TerminalLinksTest::openerConfirmsExactTargetAndUsesOneArgvElement() {
  RecordingSpawner spawner;
  RecordingConfirmation confirmation;
  TerminalLinkOpener opener(QStringLiteral("/bin/true"), &spawner,
                            &confirmation);
  const TerminalLink link =
      detectTerminalLinks(QStringLiteral("https://example.test/a;touch bad"))
          .constFirst();
  const auto result = opener.open(link, nullptr);
  QVERIFY(result.started);
  QCOMPARE(confirmation.calls, 1);
  QCOMPARE(confirmation.seen.target, link.target);
  QCOMPARE(spawner.calls, 1);
  QCOMPARE(spawner.program, QStringLiteral("/bin/true"));
  QCOMPARE(spawner.arguments, QStringList{link.target});
}

void TerminalLinksTest::openerCancellationAndFailureNeverSpawnOrShellParse() {
  RecordingSpawner spawner;
  RecordingConfirmation confirmation;
  confirmation.accepted = false;
  TerminalLinkOpener opener(QStringLiteral("/bin/true"), &spawner,
                            &confirmation);
  const TerminalLink link = detectTerminalLinks(
                                QStringLiteral("/home/user/a;echo-owned"))
                                .constFirst();
  auto result = opener.open(link, nullptr);
  QVERIFY(result.cancelled);
  QCOMPARE(spawner.calls, 0);

  confirmation.accepted = true;
  spawner.succeeds = false;
  result = opener.open(link, nullptr);
  QVERIFY(!result.started);
  QCOMPARE(spawner.calls, 1);
  QCOMPARE(spawner.arguments, QStringList{QStringLiteral("/home/user/a;echo-owned")});

  TerminalLinkOpener relativeProgram(QStringLiteral("xdg-open"), &spawner,
                                     &confirmation);
  result = relativeProgram.open(link, nullptr);
  QVERIFY(!result.started);
  QCOMPARE(spawner.calls, 1);
}

QTEST_MAIN(TerminalLinksTest)
#include "tst_terminal_links.moc"
