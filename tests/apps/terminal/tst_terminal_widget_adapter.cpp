// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_ansi_palette.h"
#include "ui/terminal_window.h"
#include "ui/terminal_profile_dialog.h"
#include "session/process_liveness.h"
#include "ui/terminal_widget_adapter.h"

#include <qtermwidget.h>
#include <QCoreApplication>
#include <QAction>
#include <QFontDatabase>
#include <QListWidget>
#include <QElapsedTimer>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <sys/wait.h>

using QindaQt::Apps::Terminal::builtinDefaultProfile;
using QindaQt::Apps::Terminal::TerminalAppearanceAdapter;
using QindaQt::Apps::Terminal::TerminalContentScheme;
using QindaQt::Apps::Terminal::TerminalSessionBackend;
using QindaQt::Apps::Terminal::TerminalLaunchRequest;
using QindaQt::Apps::Terminal::TerminalSearchDirection;
using QindaQt::Apps::Terminal::TerminalSearchQuery;
using QindaQt::Apps::Terminal::TerminalViewAppearance;
using QindaQt::Apps::Terminal::TerminalWidgetAdapter;

namespace {

QFont testMonoFont() {
  return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

TerminalViewAppearance darkAppearance() {
  return TerminalAppearanceAdapter::derive(QPalette(),
                                           TerminalContentScheme::Dark, false,
                                           testMonoFont());
}

TerminalViewAppearance lightAppearance() {
  return TerminalAppearanceAdapter::derive(QPalette(),
                                           TerminalContentScheme::Light, false,
                                           testMonoFont());
}

} // namespace

class TerminalWidgetAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void productionWindowPaintsInteractivePrompt();
  void profilesDialogUsesStockPlatformChrome();
  void liveShellDirectoryAndKeyboardInput();
  void ansiColorsPaintAcrossLiveSchemeChanges();
  void explicitProfileSchemeSurvivesDesktopRefresh();
  void zoomIsLocalAndSurvivesAppearance();
  void blankGridDoesNotPublishCopyAvailability();
  void customSchemePaintsRequestedTerminalBackground();
  void realPtyOutputSupportsBoundedSearchAndVisibleLinks();
  void osc8SequencesExposeNoHiddenLinkTarget();
};

void TerminalWidgetAdapterTest::profilesDialogUsesStockPlatformChrome() {
  using namespace QindaQt::Apps::Terminal;
  // ADR-0116: the dialog installs no palette, font, or stylesheet of its own;
  // it and its native list child track the application palette, which is the
  // platform theme's product. No Terminal code seeds or re-derives them.
  const QPalette previous = QGuiApplication::palette();
  QPalette distinct(previous);
  distinct.setColor(QPalette::Window, QColor("#33442f"));
  distinct.setColor(QPalette::WindowText, QColor("#f2f2f2"));
  distinct.setColor(QPalette::Base, QColor("#22311f"));
  distinct.setColor(QPalette::Text, QColor("#f2f2f2"));
  QGuiApplication::setPalette(distinct);
  TerminalProfileDialog dialog({}, {}, false);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  QVERIFY(dialog.styleSheet().isEmpty());
  QCOMPARE(dialog.palette().color(QPalette::Window),
           distinct.color(QPalette::Window));
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("terminalProfileList"));
  QVERIFY(list);
  QCOMPARE(list->palette().color(QPalette::Base), distinct.color(QPalette::Base));
  QCOMPARE(dialog.font(), QGuiApplication::font());

  // A live desktop palette change propagates to the open dialog on its own.
  QGuiApplication::setPalette(previous);
  QTRY_COMPARE(dialog.palette().color(QPalette::Window),
               previous.color(QPalette::Window));
}

void TerminalWidgetAdapterTest::productionWindowPaintsInteractivePrompt() {
  using namespace QindaQt::Apps::Terminal;
  PosixProcessMonitor monitor;
  TerminalWidgetAdapter *adapter = nullptr;
  // Gentoo's normal prompt sends an OSC title terminated by BEL. Use an
  // isolated equivalent so this regression never depends on host shell files.
  TerminalSessionContext context{
      {QStringLiteral("PATH=/usr/bin:/bin"), QStringLiteral("LANG=C.UTF-8"),
       QStringLiteral("PS1=\\[\\e]0;QindaQt prompt\\a\\]PROMPT> ")},
      QStringLiteral("/bin/bash"),
      {QStringLiteral("--noprofile"), QStringLiteral("--norc"), QStringLiteral("-i")},
      QStringLiteral("/tmp")};
  // AGENT-CONTRACT: The native harness retires detached PTY descendants by
  // their unique private runtime, so preserve that marker in this lean fixture.
  if (qEnvironmentVariableIsSet("XDG_RUNTIME_DIR"))
    context.baseEnvironment.append(QStringLiteral("XDG_RUNTIME_DIR=") +
                                   qEnvironmentVariable("XDG_RUNTIME_DIR"));
  auto collection = std::make_unique<TerminalSessionCollection>(context,
      [&adapter](const TerminalProfile &profile) {
        auto result = std::make_unique<TerminalWidgetAdapter>(darkAppearance(), profile);
        adapter = result.get(); return result;
      }, &monitor, TeardownBounds{});
  TerminalWindow window(std::move(collection), darkAppearance(), nullptr);
  window.resize(800, 500); window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QTest::qWait(75);
  window.newSessionWithDefaultProfile();
  QVERIFY(adapter);
  const auto contains = [&adapter](const QString &text) {
    return adapter->searchScrollback({.pattern=text}, TerminalSearchDirection::Initial).found;
  };
  QTRY_VERIFY(contains(QStringLiteral("PROMPT>")));
  window.applyAppearance(darkAppearance());
  auto *input = adapter->terminalWidget()->focusProxy();
  QVERIFY(input);
  QTest::keyClicks(input, QStringLiteral("printf '%s%s\\n' INTERACTIVE- KEYBOARD-PROOF"));
  QTest::keyClick(input, Qt::Key_Return);
  QTRY_VERIFY(contains(QStringLiteral("INTERACTIVE-KEYBOARD-PROOF")));
  adapter->clearScrollbackSearch();
  QTest::qWait(100);
  const auto rendered = adapter->terminalWidget()->grab().toImage();
  if (qEnvironmentVariableIsSet("QINDAQT_TEST_CAPTURE"))
    rendered.save(qEnvironmentVariable("QINDAQT_TEST_CAPTURE"));
  int bright = 0;
  for (int y=0; y<rendered.height(); ++y) for (int x=40; x<rendered.width(); ++x)
    if (rendered.pixelColor(x,y).lightness()>120) ++bright;
  QVERIFY2(bright>100, "Production window parsed output but painted no glyphs");
  if (qEnvironmentVariableIsSet("QINDAQT_TEST_WINDOW_CAPTURE")) {
    const QString path = qEnvironmentVariable("QINDAQT_TEST_WINDOW_CAPTURE");
    QVERIFY(window.grab().save(path + QStringLiteral(".png")));
    auto *find = window.findChild<QAction *>(QStringLiteral("viewFindAction"));
    QVERIFY(find);
    find->trigger();
    QTest::qWait(60);
    QVERIFY(window.grab().save(path + QStringLiteral("-find.png")));
    auto profile = builtinDefaultProfile();
    profile.id = QStringLiteral("capture-local");
    profile.name = QStringLiteral("Local shell");
    TerminalProfileDialog profiles({profile}, profile.id, false, &window);
    profiles.findChild<QListWidget *>(QStringLiteral("terminalProfileList"))->setCurrentRow(1);
    profiles.show();
    QTest::qWait(60);
    QVERIFY(profiles.grab().save(path + QStringLiteral("-profiles.png")));
  }
}

void TerminalWidgetAdapterTest::explicitProfileSchemeSurvivesDesktopRefresh() {
  auto profile = builtinDefaultProfile();
  profile.id = QStringLiteral("custom-profile");
  profile.colorSchemeId = QStringLiteral("light");
  const auto light = lightAppearance();
  QWidget host;
  TerminalWidgetAdapter adapter(light, profile);
  auto *layout = new QVBoxLayout(&host);
  layout->addWidget(adapter.terminalWidget());
  host.resize(500, 300); host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  const auto backgroundPainted = [&adapter](const QColor &background) {
    const QImage image = adapter.terminalWidget()->grab().toImage();
    return image.pixelColor(image.width() / 2, image.height() / 2).rgb() == background.rgb();
  };
  // A desktop-scheme refresh does not overwrite the profile's pinned scheme.
  adapter.setAppearance(darkAppearance());
  QTRY_VERIFY(backgroundPainted(light.terminalBackground));
  // High contrast temporarily takes precedence over the pinned scheme…
  const auto high =
      TerminalAppearanceAdapter::derive(QPalette(), TerminalContentScheme::Dark,
                                        true, testMonoFont());
  adapter.setAppearance(high);
  QTRY_VERIFY(backgroundPainted(high.terminalBackground));
  // …and disabling it restores the profile's choice without a shell restart.
  adapter.setAppearance(darkAppearance());
  QTRY_VERIFY(backgroundPainted(light.terminalBackground));
}

void TerminalWidgetAdapterTest::ansiColorsPaintAcrossLiveSchemeChanges() {
  using namespace QindaQt::Apps::Terminal;
  QWidget host;
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  auto *layout = new QVBoxLayout(&host);
  layout->addWidget(adapter.terminalWidget());
  host.resize(800, 500);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  // The short-lived owned shell exits immediately after printing; reap before
  // asserting pixels so no rendering failure can leave a child behind.
  QString script;
  for (int index = 0; index < 16; ++index) {
    const int code = index < 8 ? 30 + index : 90 + index - 8;
    script += QStringLiteral("printf '\\033[%1m████████\\033[0m slot%2\\n'; ").arg(code).arg(index);
  }
  const TerminalLaunchRequest request{
      .program = QStringLiteral("/bin/sh"),
      .arguments = {QStringLiteral("-c"), script},
      .workingDirectory = QStringLiteral("/"),
      .environment = {QStringLiteral("PATH=/usr/bin:/bin"), QStringLiteral("LANG=C.UTF-8"),
                      QStringLiteral("TERM=xterm-256color")}, .title = {}};
  QVERIFY(adapter.start(request).ok);
  int status = 0;
  const auto pid = static_cast<pid_t>(adapter.shellProcessId());
  pid_t reaped = 0;
  QTRY_VERIFY(reaped == pid || (reaped = ::waitpid(pid, &status, WNOHANG)) == pid);
  QVERIFY(WIFEXITED(status));
  QCOMPARE(WEXITSTATUS(status), 0);
  QTRY_VERIFY(adapter.searchScrollback({.pattern = QStringLiteral("slot15")},
                                     TerminalSearchDirection::Initial).found);
  adapter.clearScrollbackSearch();
  const QList<TerminalViewAppearance> appearances{
      darkAppearance(), lightAppearance(),
      TerminalAppearanceAdapter::derive(QPalette(), TerminalContentScheme::Dark,
                                        true, testMonoFont())};
  for (const auto &appearance : appearances) {
    adapter.setAppearance(appearance);
    QTest::qWait(60);
    const QImage image = adapter.terminalWidget()->grab().toImage();
    for (int index = 0; index < 16; ++index) {
      int painted = 0;
      for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
          if (image.pixelColor(x, y).rgb() == appearance.ansi[index].rgb())
            ++painted;
      QVERIFY2(painted > 10, qPrintable(QStringLiteral("%1 ANSI %2 was not painted")
                                      .arg(appearance.schemeId).arg(index)));
    }
  }
}

void TerminalWidgetAdapterTest::liveShellDirectoryAndKeyboardInput() {
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  const TerminalLaunchRequest request{
      .program = QStringLiteral("/bin/sh"),
      .arguments = {QStringLiteral("-c"), QStringLiteral("cd /tmp; printf 'directory-ready\\n'; read answer; printf 'received:%s\\n' \"$answer\"")},
      .workingDirectory = QStringLiteral("/"),
      .environment = {QStringLiteral("PATH=/usr/bin:/bin"), QStringLiteral("LANG=C.UTF-8"), QStringLiteral("TERM=xterm-256color")},
      .title = {}};
  QVERIFY(adapter.start(request).ok);
  const auto contains = [&adapter](const QString &text) {
    return adapter.searchScrollback({.pattern = text}, TerminalSearchDirection::Initial).found;
  };
  QTRY_VERIFY(contains(QStringLiteral("directory-ready")));
  QindaQt::Apps::Terminal::PosixProcessMonitor monitor;
  QCOMPARE(monitor.workingDirectory(adapter.shellProcessId()), QStringLiteral("/tmp"));
  adapter.sendTextToSession(QStringLiteral("keyboard-proof\n"));
  QTRY_VERIFY(contains(QStringLiteral("received:keyboard-proof")));
  int status = 0;
  const auto pid = static_cast<pid_t>(adapter.shellProcessId());
  pid_t reaped = 0;
  QTRY_VERIFY(reaped == pid || (reaped = ::waitpid(pid, &status, WNOHANG)) == pid);
  QVERIFY(WIFEXITED(status));
  QCOMPARE(WEXITSTATUS(status), 0);
}

void TerminalWidgetAdapterTest::zoomIsLocalAndSurvivesAppearance() {
  auto profile = builtinDefaultProfile();
  profile.fontSize = 12;
  TerminalWidgetAdapter first(darkAppearance(), profile);
  TerminalWidgetAdapter second(darkAppearance(), profile);
  auto *widget = qobject_cast<QTermWidget *>(first.terminalWidget());
  auto *other = qobject_cast<QTermWidget *>(second.terminalWidget());
  QVERIFY(widget); QVERIFY(other);
  first.setZoomSteps(3);
  QCOMPARE(widget->getTerminalFont().pointSize(), 15);
  QCOMPARE(other->getTerminalFont().pointSize(), 12);
  first.setAppearance(darkAppearance());
  QCOMPARE(widget->getTerminalFont().pointSize(), 15);
  first.setZoomSteps(0);
  QCOMPARE(widget->getTerminalFont().pointSize(), 12);
  // A different profile size applies directly; zoom stays per-window.
  auto resizedProfile = profile;
  resizedProfile.fontSize = 18;
  TerminalWidgetAdapter third(darkAppearance(), resizedProfile);
  auto *thirdWidget = qobject_cast<QTermWidget *>(third.terminalWidget());
  QVERIFY(thirdWidget);
  QCOMPARE(thirdWidget->getTerminalFont().pointSize(), 18);
  QCOMPARE(other->getTerminalFont().pointSize(), 12);
}

void TerminalWidgetAdapterTest::osc8SequencesExposeNoHiddenLinkTarget() {
  // AGENT-CONTRACT (pinned behavior): qtermwidget 2.4.x parses OSC-8 sequences
  // only as far as processWindowAttributeChange(), which routes attribute 8
  // into Session::setUserTitle; that ignores every attribute other than 0/1/2,
  // so no hyperlink target or cell association is ever exposed
  // (lib/Vt102Emulation.cpp, lib/Session.cpp at 2.4.0). The link layer
  // therefore acts on printed text only, and an OSC-8 sequence can neither
  // create a link nor smuggle a hidden target behind printed text.
  QWidget host;
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  host.resize(720, 320);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  // Match the proven production ordering: let the top-level settle before the
  // adapter attaches, so the renderer's first grid is the real layout.
  QTest::qWait(75);
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  layout->addWidget(adapter.terminalWidget());
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
  const TerminalLaunchRequest request{
      .program = QStringLiteral("/bin/sh"),
      .arguments = {QStringLiteral("-c"),
                    QStringLiteral("printf 'osc8 probe line\\n"
                                   "\\033]8;;https://hidden.example\\a"
                                   "https://printed.example"
                                   "\\033]8;;\\a\\n'")},
      .workingDirectory = {},
      .environment = {QStringLiteral("PATH=/usr/bin:/bin"),
                      QStringLiteral("LANG=C.UTF-8"),
                      QStringLiteral("TERM=xterm-256color")},
      .title = {}};
  const auto started = adapter.start(request);
  QVERIFY2(started.ok, qPrintable(started.diagnostic));
  QSignalSpy titleSpy(&adapter, &TerminalSessionBackend::titleChanged);
  QTest::qWait(150);
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
  adapter.clearScrollbackSearch();

  // The printed text is what the link layer traverses; the OSC-8 target
  // "https://hidden.example" must never become the actionable target.
  QTRY_VERIFY(adapter.currentVisibleLink().found);
  const auto link = adapter.currentVisibleLink();
  QCOMPARE(link.total, 1);
  QCOMPARE(link.link.target, QStringLiteral("https://printed.example"));

  // The OSC-8 attribute was dropped by the emulation: no title update fired.
  QVERIFY(titleSpy.isEmpty());

  int status = 0;
  QCOMPARE(::waitpid(static_cast<pid_t>(adapter.shellProcessId()), &status, 0),
           static_cast<pid_t>(adapter.shellProcessId()));
  QVERIFY(WIFEXITED(status));
}

void TerminalWidgetAdapterTest::blankGridDoesNotPublishCopyAvailability() {  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  QSignalSpy selectionSpy(&adapter, &TerminalSessionBackend::selectionChanged);

  adapter.selectAllInView();

  QCOMPARE(selectionSpy.count(), 1);
  QCOMPARE(selectionSpy.at(0).at(0).toBool(), false);
  QVERIFY(!adapter.hasSelectedText());
}

void TerminalWidgetAdapterTest::
    customSchemePaintsRequestedTerminalBackground() {
  const TerminalViewAppearance appearance = darkAppearance();
  // Compare the loaded theme against the renderer, so identity refreshes do
  // not turn this behavior gate into an obsolete hard-coded color assertion.

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

  // Inspect only qtermwidget's pixels. The host chrome contains menu/tab/status
  // text and would let an empty terminal viewport satisfy this regression.
  const QImage rendered = adapter.terminalWidget()->grab().toImage();
  QVERIFY(!rendered.isNull());
  const QColor center = QColor::fromRgba(
      rendered.pixel(rendered.width() / 2, rendered.height() / 2));
  QCOMPARE(center.rgb(), appearance.terminalBackground.rgb());
}

void TerminalWidgetAdapterTest::
    realPtyOutputSupportsBoundedSearchAndVisibleLinks() {
  QWidget host;
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  host.resize(720, 320);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  // Hostile production ordering: Settings1 resolves after the top-level is
  // already exposed, so the adapter is constructed and attached late. On the
  // unrepaired tree qtermwidget enters teletype mode while parentless; bytes
  // become searchable but Wayland paints no glyphs after the live reparent.
  QTest::qWait(75);
  TerminalWidgetAdapter adapter(darkAppearance(), builtinDefaultProfile());
  layout->addWidget(adapter.terminalWidget());
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
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

  const QImage rendered = host.grab().toImage();
  QVERIFY(!rendered.isNull());
  qsizetype foregroundLikePixels = 0;
  for (int y = 0; y < rendered.height(); ++y) {
    // Exclude the first cell: qtermwidget's pristine block cursor alone is
    // bright enough to make a blank renderer look nonempty.
    for (int x = 40; x < rendered.width(); ++x) {
      const QColor pixel = QColor::fromRgba(rendered.pixel(x, y));
      if (pixel.lightness() > 120) {
        ++foregroundLikePixels;
      }
    }
  }
  // Hostile regression control for the real symptom: searchable scrollback
  // is insufficient if the production renderer paints an empty viewport.
  QVERIFY2(foregroundLikePixels > 100,
           "child output reached scrollback but painted no visible glyphs");

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
