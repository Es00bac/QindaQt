// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_window.h"

#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAction>
#include <QTabBar>
#include <QWidget>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::Terminal;

namespace {

TerminalViewAppearance testAppearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load test theme: %s", qPrintable(theme.error));
  }
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    qFatal("Could not derive appearance: %s",
           qPrintable(appearance.diagnostic));
  }
  return *appearance.appearance;
}

class RunningMonitor final : public ProcessMonitor {
public:
  ProcessExitInfo reap(ProcessId pid) override {
    return m_stopped.contains(pid)
               ? ProcessExitInfo{ProcessState::Exited, false, 0, true}
               : ProcessExitInfo{ProcessState::Running, false, 0, true};
  }
  bool signalProcessGroup(ProcessId, int) override { return true; }
  void stop(ProcessId pid) { m_stopped.insert(pid); }

private:
  QSet<ProcessId> m_stopped;
};

class TabBackend final : public TerminalSessionBackend {
public:
  TabBackend(ProcessId pid, RunningMonitor &monitor)
      : m_pid(pid), m_monitor(monitor) {}
  ~TabBackend() override { delete m_view; }
  StartOutcome start(const TerminalLaunchRequest &) override {
    m_view = new QWidget;
    return {.ok = true, .diagnostic = {}};
  }
  void requestShutdown() override {
    m_monitor.stop(m_pid);
    delete m_view;
    m_view = nullptr;
  }
  ProcessId shellProcessId() const override { return m_pid; }
  QWidget *terminalWidget() override { return m_view; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}

private:
  ProcessId m_pid = 0;
  RunningMonitor &m_monitor;
  QWidget *m_view = nullptr;
};

std::unique_ptr<TerminalWindow> makeWindow(RunningMonitor &monitor) {
  int nextPid = 9000;
  TerminalSession::BackendFactory factory =
      [&monitor, nextPid](const TerminalProfile &) mutable {
        return std::make_unique<TabBackend>(nextPid++, monitor);
      };
  TerminalSessionContext context{{}, QStringLiteral("/bin/true"), {}, {}};
  auto sessions = std::make_unique<TerminalSessionCollection>(
      context, std::move(factory), &monitor, TeardownBounds{2, 2, 2, 1});
  auto window = std::make_unique<TerminalWindow>(
      std::move(sessions), testAppearance(),
      QStringList{QStringLiteral("qinda-dark")}, nullptr);
  window->newSessionWithDefaultProfile();
  return window;
}

} // namespace

class TerminalTabsTest final : public QObject {
  Q_OBJECT

private slots:
  void keyboardTraversalMoveCloseAndAccessibilityStayCoherent();
};

void TerminalTabsTest::
    keyboardTraversalMoveCloseAndAccessibilityStayCoherent() {
  RunningMonitor monitor;
  auto window = makeWindow(monitor);
  const auto action = [&window](const char *name) {
    return window->findChild<QAction *>(QLatin1String(name));
  };
  QAction *newTab = action("tabNewAction");
  QAction *next = action("tabNextAction");
  QAction *previous = action("tabPreviousAction");
  QAction *moveLeft = action("tabMoveLeftAction");
  QAction *close = action("tabCloseAction");
  QVERIFY(newTab && next && previous && moveLeft && close);
  QCOMPARE(newTab->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+T")));
  QCOMPARE(next->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+Right")));
  QCOMPARE(previous->shortcut(),
           QKeySequence(QStringLiteral("Ctrl+Shift+Left")));
  QCOMPARE(moveLeft->shortcut(),
           QKeySequence(QStringLiteral("Ctrl+Shift+Alt+Left")));
  QCOMPARE(close->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+W")));
  QCOMPARE(newTab->shortcutContext(), Qt::WindowShortcut);

  newTab->trigger();
  QTRY_COMPARE(window->sessions()->count(), 2);
  TerminalSession *first = window->sessions()->sessionAt(0);
  TerminalSession *second = window->sessions()->sessionAt(1);

  next->trigger();
  QCOMPARE(window->session(), second);
  moveLeft->trigger();
  QCOMPARE(window->sessions()->sessionAt(0), second);
  previous->trigger();
  QCOMPARE(window->session(), first);
  close->trigger();
  QTRY_COMPARE(window->sessions()->count(), 1);
  QCOMPARE(window->session(), second);

  auto *tabs = window->findChild<QTabBar *>(QStringLiteral("terminalTabBar"));
  QVERIFY(tabs != nullptr);
  QCOMPARE(tabs->accessibleName(), QStringLiteral("Terminal tabs"));
  QAccessibleInterface *accessible =
      QAccessible::queryAccessibleInterface(tabs);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::PageTabList);
  int pageTabs = 0;
  for (int index = 0; index < accessible->childCount(); ++index) {
    QAccessibleInterface *child = accessible->child(index);
    QVERIFY(child != nullptr);
    if (child->role() == QAccessible::PageTab) {
      ++pageTabs;
      QVERIFY(!child->text(QAccessible::Name).isEmpty());
    }
  }
  QCOMPARE(pageTabs, 1);
}

QTEST_MAIN(TerminalTabsTest)
#include "tst_terminal_tabs.moc"
