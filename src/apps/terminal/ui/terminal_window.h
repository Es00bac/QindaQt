// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "links/terminal_link.h"
#include "profiles/terminal_profile.h"
#include "search/terminal_search.h"
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

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::Terminal {

class TerminalAppShellBridge;
class TerminalFindBar;
class TerminalLinkOpener;
class TerminalProfileSettings;
class TerminalTabBar;

// AGENT-CONTRACT: TerminalWindow owns presentation only: actions, menus,
// the tab strip, status reporting, focus, accessibility metadata, and the
// AppShell action projection. It never touches PTYs, processes, the
// rendering library, or Settings1 transport; the injected collection owns
// session lifecycles and the injected profile settings owns persistence.
// Every character-based window shortcut is Shift-modified on purpose: plain
// Ctrl+C/S/Q/A and friends must reach the child shell (readline job and flow
// control), so a window-level binding on them would be a functional
// regression. F3 remains the non-character find-navigation convention.
class TerminalWindow final : public QMainWindow {
  Q_OBJECT

public:
  // The window takes ownership of the collection. profileSettings is
  // injected, not owned, and may be null (persistence unavailable: only the
  // built-in profile is offered). linkOpener is likewise borrowed, may be
  // null, and must outlive the window. themeIds enumerates the installed
  // QindaQt themes offered by the profile dialog.
  TerminalWindow(std::unique_ptr<TerminalSessionCollection> sessions,
                 const TerminalViewAppearance &appearance,
                 const QStringList &themeIds,
                 TerminalProfileSettings *profileSettings,
                 TerminalLinkOpener *linkOpener = nullptr,
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

  // AGENT-CONTRACT: the application-side composition seam for the first-party
  // global-menu export. main() reads this coordinator after show() and hands
  // it to QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport; the
  // window keeps every enablement/lifecycle decision itself.
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &
  appShellCoordinator();

  // Adds one session with the current default profile (used for the first
  // tab and the New Tab command).
  void newSessionWithDefaultProfile();
  void applyAppearance(const TerminalViewAppearance &appearance);

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
  void buildFindBar();

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
  void showFindBar();
  void closeFindBar();
  void runSearch(TerminalSearchDirection direction);
  void restoreSearchPresentation();
  void selectRelativeLink(int delta);
  void copyCurrentLink();
  void openCurrentLink();
  void showLinkContextMenu(const QPoint &globalPosition);
  void presentLinkSelection(const TerminalLinkSelection &selection);

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
  TerminalLinkOpener *m_linkOpener = nullptr;
  QStringList m_themeIds;
  TerminalViewAppearance m_appearance;
  TerminalAppShellBridge *m_appShellBridge = nullptr;
  TerminalTabBar *m_tabBar = nullptr;
  QWidget *m_terminalHolder = nullptr;
  QVBoxLayout *m_terminalLayout = nullptr;
  TerminalFindBar *m_findBar = nullptr;
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
  QAction *m_zoomInAction = nullptr;
  QAction *m_zoomOutAction = nullptr;
  QAction *m_zoomResetAction = nullptr;
  QAction *m_findAction = nullptr;
  QAction *m_findNextAction = nullptr;
  QAction *m_findPreviousAction = nullptr;
  QAction *m_linkNextAction = nullptr;
  QAction *m_linkPreviousAction = nullptr;
  QAction *m_linkCopyAction = nullptr;
  QAction *m_linkOpenAction = nullptr;
  QAction *m_quitAction = nullptr;
  TerminalSession *m_activeSession = nullptr;
  QHash<const TerminalSession *, bool> m_selectionBySession;
  QHash<const TerminalSession *, QString> m_titlesBySession;
  QHash<const TerminalSession *, TerminalSearchQuery> m_searchBySession;
  QHash<const TerminalSession *, TerminalSearchResult> m_searchResultBySession;
  QHash<const TerminalSession *, bool> m_findVisibleBySession;
  QHash<const TerminalSession *, TerminalLinkSelection> m_linkBySession;
  bool m_quitRequested = false;
};

} // namespace QindaQt::Apps::Terminal
