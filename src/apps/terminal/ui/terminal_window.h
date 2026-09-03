// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "profiles/terminal_profile.h"
#include "session/terminal_session_collection.h"
#include "session/terminal_session_types.h"
#include "ui/terminal_appearance.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QPalette>
#include <QVBoxLayout>
#include <QVariantList>
#include <memory>

class QMenu;

namespace QindaQt::Apps::Terminal {

class TerminalAppShellBridge;
class TerminalProfileSettings;
class TerminalTabBar;

// AGENT-CONTRACT: TerminalWindow owns presentation only: actions, menus,
// the tab strip, status reporting, focus, accessibility metadata, and the
// AppShell action projection. It never touches PTYs, processes, the
// rendering library, or Settings1 transport; the injected collection owns
// session lifecycles and the injected profile settings owns persistence.
// Every window shortcut is Shift-modified on purpose: plain Ctrl+C/S/Q/A
// and friends must reach the child shell (readline job and flow control),
// so a window-level binding on them would be a functional regression.
class TerminalWindow final : public QMainWindow {
  Q_OBJECT

public:
  // The window takes ownership of the collection. profileSettings is
  // injected, not owned, and may be null (persistence unavailable: only the
  // built-in profile is offered). themeIds enumerates the installed QindaQt
  // themes offered by the profile dialog.
  TerminalWindow(std::unique_ptr<TerminalSessionCollection> sessions,
                 const TerminalViewAppearance &appearance,
                 const QStringList &themeIds,
                 TerminalProfileSettings *profileSettings,
                 QWidget *parent = nullptr);
  ~TerminalWindow() override;

  // AGENT-CONTRACT: This is the application wiring seam for teardown-first
  // quit, and main() must call it before the first window is shown. Qt's
  // quitOnLastWindowClosed default is true; closing the terminal window
  // hides it while bounded session escalations are still running, so the
  // default would end the event loop before a single escalation tick and
  // orphan any SIGHUP-immune child. The only permitted quit path is
  // connectQuitAfterCloseShutdown(), which fires strictly after every
  // session reached a terminal state. See docs/wiki/apps/terminal.md.
  static void prepareApplicationQuitFlow(QGuiApplication &application);

  // Connects closeShutdownFinished -> QCoreApplication::quit as a queued
  // connection. Never call quit from any other path.
  void connectQuitAfterCloseShutdown(QCoreApplication &application) const;

  [[nodiscard]] TerminalSessionCollection *sessions() const {
    return m_sessions.get();
  }
  // The active session; null when no tab exists.
  [[nodiscard]] TerminalSession *session() const { return m_activeSession; }

  // Adds one session with the current default profile (used for the first
  // tab and the New Tab command).
  void newSessionWithDefaultProfile();

signals:
  // Emitted when the close path requested shutdown of every session and all
  // of them reached a terminal state cleanly; never emitted while any
  // SIGKILL survivor remains, so connecting QApplication::quit() here
  // preserves the teardown guarantee.
  void closeShutdownFinished();

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void buildActions();
  void buildMenus();
  void buildStatusBar();
  void wireCollection();
  void wireSessionPresentation(TerminalSession *session);
  void publishAppShellProjection();

  void setActiveSession(TerminalSession *session);
  void attachSessionView(TerminalSession *session);
  void detachSessionView();
  void addSessionWithProfile(const TerminalProfile &profile);
  void closeSessionFromPresentation(TerminalSession *session);
  void closeActiveSession();
  void activateRelativeTab(int delta);
  void moveActiveTab(int delta);
  void manageProfiles();
  void rebuildProfileMenu();

  void updateViewActionStates();
  void updateTabActionStates();
  void updateStatusForState(TerminalSession::State state);
  void showExitStatus(const TerminalExitStatus &status);
  void presentProfileApplyResult(const QVariantList &ledger);
  void showStatusMessage(const QString &text, bool danger);
  void showStatusMessage(const QString &text, bool danger,
                         const QPalette &palette);
  void updateWindowTitle();
  [[nodiscard]] QString displayTitle(const TerminalSession *session) const;
  [[nodiscard]] int tabIndexOf(const TerminalSession *session) const;
  [[nodiscard]] TerminalProfile currentDefaultProfile() const;
  void requestCloseShutdown();

  std::unique_ptr<TerminalSessionCollection> m_sessions;
  TerminalProfileSettings *m_profileSettings = nullptr;
  QStringList m_themeIds;
  TerminalViewAppearance m_appearance;
  TerminalAppShellBridge *m_appShellBridge = nullptr;
  TerminalTabBar *m_tabBar = nullptr;
  QWidget *m_terminalHolder = nullptr;
  QVBoxLayout *m_terminalLayout = nullptr;
  QWidget *m_terminalView = nullptr;
  QLabel *m_statusLabel = nullptr;
  QMenu *m_profileMenu = nullptr;
  QAction *m_newTabAction = nullptr;
  QAction *m_closeTabAction = nullptr;
  QAction *m_nextTabAction = nullptr;
  QAction *m_previousTabAction = nullptr;
  QAction *m_moveTabLeftAction = nullptr;
  QAction *m_moveTabRightAction = nullptr;
  QAction *m_manageProfilesAction = nullptr;
  QAction *m_restartAction = nullptr;
  QAction *m_copyAction = nullptr;
  QAction *m_pasteAction = nullptr;
  QAction *m_pasteSelectionAction = nullptr;
  QAction *m_selectAllAction = nullptr;
  QAction *m_clearAction = nullptr;
  QAction *m_quitAction = nullptr;
  TerminalSession *m_activeSession = nullptr;
  QHash<const TerminalSession *, bool> m_selectionBySession;
  QHash<const TerminalSession *, QString> m_titlesBySession;
  bool m_quitRequested = false;
};

} // namespace QindaQt::Apps::Terminal
