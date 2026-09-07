// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link_opener.h"
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include "qindaqt/themes/theme_loader.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTabBar>
#include <QtTest>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalViewAppearance appearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load test theme");
  }
  return *TerminalAppearanceAdapter::fromTheme(theme.theme).appearance;
}

class UiBackend final : public TerminalSessionBackend {
public:
  QWidget view;
  TerminalSearchQuery lastQuery;
  TerminalSearchDirection lastDirection = TerminalSearchDirection::Initial;
  int searchCalls = 0;
  int clearCalls = 0;
  int matchIndex = 0;
  int linkIndex = 0;
  QList<TerminalLink> links{
      {TerminalLinkKind::WebUrl, QStringLiteral("https://example.test"),
       QStringLiteral("https://example.test"), QStringLiteral("Exact web target")},
      {TerminalLinkKind::LocalPath, QStringLiteral("/home/user/file"),
       QStringLiteral("/home/user/file"), QStringLiteral("Exact local target")}};

  StartOutcome start(const TerminalLaunchRequest &) override {
    return {.ok = true, .diagnostic = {}};
  }
  void requestShutdown() override {}
  ProcessId shellProcessId() const override { return 8181; }
  QWidget *terminalWidget() override { return &view; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}

  TerminalSearchResult searchScrollback(
      const TerminalSearchQuery &query,
      TerminalSearchDirection direction) override {
    ++searchCalls;
    lastQuery = query;
    lastDirection = direction;
    if (query.pattern == QStringLiteral("missing")) {
      return {.accepted = true,
              .found = false,
              .current = 0,
              .total = 0,
              .wrapped = false,
              .diagnostic = QStringLiteral("No matches")};
    }
    bool wrapped = false;
    if (direction == TerminalSearchDirection::Initial) {
      matchIndex = 0;
    } else if (direction == TerminalSearchDirection::Next) {
      wrapped = matchIndex == 1;
      matchIndex = (matchIndex + 1) % 2;
    } else {
      wrapped = matchIndex == 0;
      matchIndex = (matchIndex + 1) % 2;
    }
    return {.accepted = true,
            .found = true,
            .current = matchIndex + 1,
            .total = 2,
            .wrapped = wrapped,
            .diagnostic = {}};
  }
  void clearScrollbackSearch() override { ++clearCalls; }
  TerminalLinkSelection selectVisibleLink(int delta) override {
    if (links.isEmpty()) {
      return {};
    }
    linkIndex = qBound(0, linkIndex, static_cast<int>(links.size()) - 1);
    const int old = linkIndex;
    const int count = static_cast<int>(links.size());
    linkIndex = (linkIndex + delta + count) % count;
    return {.found = true,
            .current = linkIndex + 1,
            .total = count,
            .wrapped = delta < 0 ? linkIndex > old : linkIndex < old,
            .link = links.at(linkIndex)};
  }
  TerminalLinkSelection currentVisibleLink() override {
    if (links.isEmpty()) {
      return {};
    }
    linkIndex = qBound(0, linkIndex, static_cast<int>(links.size()) - 1);
    return {.found = true,
            .current = linkIndex + 1,
            .total = static_cast<int>(links.size()),
            .link = links.at(linkIndex)};
  }
};

class ExitMonitor final : public ProcessMonitor {
public:
  ProcessExitInfo reap(ProcessId) override {
    return {.state = ProcessState::Running,
            .signaled = false,
            .code = 0,
            .statusKnown = true};
  }
  ProcessGroupState processGroupState(ProcessId) override {
    return ProcessGroupState::Empty;
  }
  bool signalProcessGroup(ProcessId, int) override { return true; }
};

class RecordingSpawner final : public TerminalLinkSpawner {
public:
  QStringList arguments;
  int calls = 0;
  bool spawn(const QString &, const QStringList &argv, QString *) override {
    ++calls;
    arguments = argv;
    return true;
  }
};

class AcceptingConfirmation final : public TerminalLinkConfirmation {
public:
  int calls = 0;
  bool confirm(const TerminalLink &, QWidget *) override {
    ++calls;
    return true;
  }
};

struct Harness final {
  ExitMonitor monitor;
  RecordingSpawner spawner;
  AcceptingConfirmation confirmation;
  TerminalLinkOpener opener{QStringLiteral("/bin/true"), &spawner,
                            &confirmation};
  UiBackend *backend = nullptr;
  QList<UiBackend *> backends;

  std::unique_ptr<TerminalWindow> window() {
    TerminalSession::BackendFactory factory = [this](const TerminalProfile &) {
      auto created = std::make_unique<UiBackend>();
      backend = created.get();
      backends.append(created.get());
      return std::unique_ptr<TerminalSessionBackend>(std::move(created));
    };
    TerminalSessionContext context{
        .baseEnvironment = {QStringLiteral("PATH=/usr/bin"),
                            QStringLiteral("LANG=C.UTF-8")},
        .fallbackProgram = QStringLiteral("/bin/true"),
        .fallbackArguments = {},
        .workingDirectory = {}};
    auto sessions = std::make_unique<TerminalSessionCollection>(
        context, factory, &monitor, TeardownBounds{10, 10, 10, 1});
    auto result = std::make_unique<TerminalWindow>(
        std::move(sessions), appearance(), QStringList{}, nullptr, &opener);
    result->newSessionWithDefaultProfile();
    return result;
  }
};

} // namespace

class TerminalSearchLinksUiTest final : public QObject {
  Q_OBJECT

private slots:
  void findBarHasKeyboardParityStatusAndFocusReturn();
  void findTextAndVisibilityAreWindowLocalAndVolatile();
  void linksTraverseCopyOpenAndPopulateContextMenu();
  void staleLinkSelectionCannotCopyOrOpen();
};

void TerminalSearchLinksUiTest::findBarHasKeyboardParityStatusAndFocusReturn() {
  Harness harness;
  auto window = harness.window();

  auto *find = window->findChild<QAction *>(QStringLiteral("viewFindAction"));
  auto *next =
      window->findChild<QAction *>(QStringLiteral("viewFindNextAction"));
  auto *previous =
      window->findChild<QAction *>(QStringLiteral("viewFindPreviousAction"));
  QVERIFY(find && next && previous);
  QCOMPARE(find->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+F")));
  QCOMPARE(next->shortcut(), QKeySequence(QStringLiteral("F3")));
  QCOMPARE(previous->shortcut(), QKeySequence(QStringLiteral("Shift+F3")));

  find->trigger();
  auto *editor =
      window->findChild<QLineEdit *>(QStringLiteral("terminalFindText"));
  auto *status =
      window->findChild<QLabel *>(QStringLiteral("terminalFindStatus"));
  auto *regex =
      window->findChild<QCheckBox *>(QStringLiteral("terminalFindRegex"));
  auto *matchCase =
      window->findChild<QCheckBox *>(QStringLiteral("terminalFindCase"));
  QVERIFY(editor && status && regex && matchCase);
  QCOMPARE(editor->focusPolicy(), Qt::StrongFocus);
  QTest::keyClicks(editor, QStringLiteral("needle"));
  QTRY_COMPARE(status->text(), QStringLiteral("Match 1 of 2"));
  matchCase->setChecked(true);
  regex->setChecked(true);
  QVERIFY(harness.backend->lastQuery.caseSensitive);
  QVERIFY(harness.backend->lastQuery.regularExpression);
  next->trigger();
  QCOMPARE(harness.backend->lastDirection, TerminalSearchDirection::Next);
  previous->trigger();
  QCOMPARE(harness.backend->lastDirection, TerminalSearchDirection::Previous);

  editor->selectAll();
  QTest::keyClicks(editor, QStringLiteral("missing"));
  QTRY_COMPARE(status->text(), QStringLiteral("No matches"));
  QVERIFY(status->accessibleName().contains(QStringLiteral("No matches")));
  QTest::keyClick(editor, Qt::Key_Escape);
  QVERIFY(window->findChild<QWidget *>(QStringLiteral("terminalFindBar"))
              ->isHidden());
  QCOMPARE(harness.backend->clearCalls, 1);
}

void TerminalSearchLinksUiTest::findTextAndVisibilityAreWindowLocalAndVolatile() {
  Harness harness;
  auto window = harness.window();
  auto *find = window->findChild<QAction *>(QStringLiteral("viewFindAction"));
  auto *editor =
      window->findChild<QLineEdit *>(QStringLiteral("terminalFindText"));
  auto *bar = window->findChild<QWidget *>(QStringLiteral("terminalFindBar"));
  auto *status =
      window->findChild<QLabel *>(QStringLiteral("terminalFindStatus"));
  QVERIFY(find && editor && bar && status);
  QVERIFY(window->findChild<QTabBar *>() == nullptr);

  find->trigger();
  QTest::keyClicks(editor, QStringLiteral("first-only"));
  QTRY_COMPARE(status->text(), QStringLiteral("Match 1 of 2"));
  // A repeated initial-session request cannot create a hidden second shell or
  // disturb the sole window's volatile search state.
  window->newSessionWithDefaultProfile();
  QCOMPARE(harness.backends.size(), 1);
  QCOMPARE(editor->text(), QStringLiteral("first-only"));
  QVERIFY(!bar->isHidden());
  QCOMPARE(status->text(), QStringLiteral("Match 1 of 2"));
  QCOMPARE(status->accessibleName(),
           QStringLiteral("Search status: Match 1 of 2"));
}

void TerminalSearchLinksUiTest::staleLinkSelectionCannotCopyOrOpen() {
  Harness harness;
  auto window = harness.window();
  auto *next = window->findChild<QAction *>(QStringLiteral("linkNextAction"));
  auto *copy = window->findChild<QAction *>(QStringLiteral("linkCopyAction"));
  auto *open = window->findChild<QAction *>(QStringLiteral("linkOpenAction"));
  QVERIFY(next && copy && open);

  next->trigger();
  QApplication::clipboard()->setText(QStringLiteral("unchanged"));
  harness.backend->links = {
      {TerminalLinkKind::WebUrl, QStringLiteral("https://new.example"),
       QStringLiteral("https://new.example"), QStringLiteral("new")}};

  // AGENT-NOTE: Regression for review P2-2. Activation must refresh current
  // viewport truth and stop when it differs from the traversed selection.
  copy->trigger();
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("unchanged"));
  harness.backend->links = {
      {TerminalLinkKind::WebUrl, QStringLiteral("https://newer.example"),
       QStringLiteral("https://newer.example"), QStringLiteral("newer")}};
  open->trigger();
  QCOMPARE(harness.confirmation.calls, 0);
  QCOMPARE(harness.spawner.calls, 0);
}

void TerminalSearchLinksUiTest::linksTraverseCopyOpenAndPopulateContextMenu() {
  Harness harness;
  auto window = harness.window();
  auto *next = window->findChild<QAction *>(QStringLiteral("linkNextAction"));
  auto *copy = window->findChild<QAction *>(QStringLiteral("linkCopyAction"));
  auto *open = window->findChild<QAction *>(QStringLiteral("linkOpenAction"));
  QVERIFY(next && copy && open);
  next->trigger();
  QVERIFY(copy->isEnabled());
  QVERIFY(open->isEnabled());
  copy->trigger();
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("/home/user/file"));
  open->trigger();
  QCOMPARE(harness.confirmation.calls, 1);
  QCOMPARE(harness.spawner.calls, 1);
  QCOMPARE(harness.spawner.arguments,
           QStringList{QStringLiteral("/home/user/file")});

  QVERIFY(QMetaObject::invokeMethod(
      harness.backend, "linkContextRequested", Qt::DirectConnection,
      Q_ARG(QPoint, QPoint(20, 20))));
  QTRY_VERIFY(window->findChild<QMenu *>(
                  QStringLiteral("terminalLinkContextMenu")) != nullptr);
}

QTEST_MAIN(TerminalSearchLinksUiTest)
#include "tst_terminal_search_links_ui.moc"
