// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_widget_adapter.h"

#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <sys/wait.h>

using QindaQt::Apps::Terminal::builtinDefaultProfile;
using QindaQt::Apps::Terminal::TerminalAppearanceAdapter;
using QindaQt::Apps::Terminal::TerminalSessionBackend;
using QindaQt::Apps::Terminal::TerminalLaunchRequest;
using QindaQt::Apps::Terminal::TerminalSearchDirection;
using QindaQt::Apps::Terminal::TerminalSearchQuery;
using QindaQt::Apps::Terminal::TerminalViewAppearance;
using QindaQt::Apps::Terminal::TerminalWidgetAdapter;

namespace {

TerminalViewAppearance darkAppearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load qinda-dark: %s", qPrintable(theme.error));
  }
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    qFatal("Could not derive Terminal appearance: %s",
           qPrintable(appearance.diagnostic));
  }
  return *appearance.appearance;
}

} // namespace

class TerminalWidgetAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void blankGridDoesNotPublishCopyAvailability();
  void customSchemePaintsRequestedTerminalBackground();
  void realPtyOutputSupportsBoundedSearchAndVisibleLinks();
};

void TerminalWidgetAdapterTest::blankGridDoesNotPublishCopyAvailability() {
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  QSignalSpy selectionSpy(&adapter, &TerminalSessionBackend::selectionChanged);

  adapter.selectAllInView();

  QCOMPARE(selectionSpy.count(), 1);
  QCOMPARE(selectionSpy.at(0).at(0).toBool(), false);
  QVERIFY(!adapter.hasSelectedText());
}

void TerminalWidgetAdapterTest::
    customSchemePaintsRequestedTerminalBackground() {
  const TerminalViewAppearance appearance = darkAppearance();
  QCOMPARE(appearance.terminalBackground, QColor(QStringLiteral("#171a18")));

  // The adapter owns and deletes its widget. Declare the host first so the
  // adapter is destroyed first and QLayout never becomes a competing owner.
  QWidget host;
  TerminalWidgetAdapter adapter(appearance, builtinDefaultProfile());
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(adapter.terminalWidget());
  host.resize(640, 400);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

  const QImage rendered = host.grab().toImage();
  QVERIFY(!rendered.isNull());
  const QColor center = QColor::fromRgba(
      rendered.pixel(rendered.width() / 2, rendered.height() / 2));
  QCOMPARE(center.rgb(), appearance.terminalBackground.rgb());
}

void TerminalWidgetAdapterTest::
    realPtyOutputSupportsBoundedSearchAndVisibleLinks() {
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  const TerminalLaunchRequest request{
      .program = QStringLiteral("/bin/sh"),
      .arguments = {QStringLiteral("-c"),
                    QStringLiteral("printf 'Alpha needle alpha needle\\n"
                                   "https://example.test /home/user/file\\n'")},
      .workingDirectory = {},
      .environment = {QStringLiteral("PATH=/usr/bin:/bin"),
                      QStringLiteral("LANG=C.UTF-8"),
                      QStringLiteral("TERM=xterm-256color")},
      .title = {}};
  const auto started = adapter.start(request);
  QVERIFY2(started.ok, qPrintable(started.diagnostic));
  QTest::qWait(150);
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

  const TerminalSearchQuery literal{.pattern = QStringLiteral("needle"),
                                    .caseSensitive = true,
                                    .regularExpression = false};
  auto result =
      adapter.searchScrollback(literal, TerminalSearchDirection::Initial);
  QVERIFY2(result.accepted, qPrintable(result.diagnostic));
  QCOMPARE(result.total, 2);
  QCOMPARE(result.current, 1);
  QVERIFY(adapter.hasSelectedText());

  // AGENT-NOTE: Regression for review P2-1. Escape clears renderer highlight
  // through this boundary but must retain the session's logical position, so
  // backward navigation from match one wraps to match two.
  const TerminalSearchQuery regexQuery{
      .pattern = QStringLiteral("n.e+dle"),
      .caseSensitive = true,
      .regularExpression = true};
  result =
      adapter.searchScrollback(regexQuery, TerminalSearchDirection::Initial);
  QCOMPARE(result.current, 1);
  adapter.clearScrollbackSearch();
  result = adapter.searchScrollback(literal,
                                    TerminalSearchDirection::Previous);
  QCOMPARE(result.current, 2);
  QVERIFY(result.wrapped);

  result =
      adapter.searchScrollback(literal, TerminalSearchDirection::Initial);
  QCOMPARE(result.current, 1);
  result = adapter.searchScrollback(literal, TerminalSearchDirection::Next);
  QCOMPARE(result.current, 2);
  result = adapter.searchScrollback(literal, TerminalSearchDirection::Next);
  QCOMPARE(result.current, 1);
  QVERIFY(result.wrapped);
  result =
      adapter.searchScrollback(literal, TerminalSearchDirection::Previous);
  QCOMPARE(result.current, 2);
  QVERIFY(result.wrapped);

  result = adapter.searchScrollback(
      {.pattern = QStringLiteral("ALPHA"), .caseSensitive = true},
      TerminalSearchDirection::Initial);
  QVERIFY(!result.found);
  QCOMPARE(result.diagnostic, QStringLiteral("No matches"));
  result = adapter.searchScrollback(
      {.pattern = QStringLiteral("alpha"), .caseSensitive = false},
      TerminalSearchDirection::Initial);
  QCOMPARE(result.total, 2);

  QElapsedTimer hostileTimer;
  hostileTimer.start();
  result = adapter.searchScrollback(
      {.pattern = QStringLiteral("(a+)+$"),
       .caseSensitive = true,
       .regularExpression = true},
      TerminalSearchDirection::Initial);
  QVERIFY(!result.accepted);
  QVERIFY(hostileTimer.elapsed() < 100);

  auto link = adapter.currentVisibleLink();
  QVERIFY(link.found);
  QCOMPARE(link.total, 2);
  QCOMPARE(link.link.target, QStringLiteral("https://example.test"));
  link = adapter.selectVisibleLink(1);
  QCOMPARE(link.link.target, QStringLiteral("/home/user/file"));

  int status = 0;
  QCOMPARE(::waitpid(static_cast<pid_t>(adapter.shellProcessId()), &status, 0),
           static_cast<pid_t>(adapter.shellProcessId()));
  QVERIFY(WIFEXITED(status));
}

QTEST_MAIN(TerminalWidgetAdapterTest)
#include "tst_terminal_widget_adapter.moc"
