// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "app_shell/terminal_action_catalog.h"
#include "app_shell/terminal_app_shell_bridge.h"
#include "profiles/terminal_profile_settings.h"
#include "ui/terminal_profile_apply_status.h"
#include "ui/terminal_profile_dialog.h"
#include "ui/terminal_tab_bar.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include <cstdio>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-GUARD: Plain Ctrl+letter sequences belong to readline and terminal
// flow control. Character-based window shortcuts stay Shift-modified; F3 is
// the non-character find-navigation convention.
constexpr auto kNewTabShortcut = "Ctrl+Shift+T";
constexpr auto kCloseTabShortcut = "Ctrl+Shift+W";
constexpr auto kNextTabShortcut = "Ctrl+Shift+Right";
constexpr auto kPreviousTabShortcut = "Ctrl+Shift+Left";
constexpr auto kMoveTabLeftShortcut = "Ctrl+Shift+Alt+Left";
constexpr auto kMoveTabRightShortcut = "Ctrl+Shift+Alt+Right";
constexpr auto kManageProfilesShortcut = "Ctrl+Shift+P";
constexpr auto kRestartShortcut = "Ctrl+Shift+R";
constexpr auto kCopyShortcut = "Ctrl+Shift+C";
constexpr auto kPasteShortcut = "Ctrl+Shift+V";
constexpr auto kPasteSelectionShortcut = "Ctrl+Shift+Insert";
constexpr auto kSelectAllShortcut = "Ctrl+Shift+A";
constexpr auto kClearShortcut = "Ctrl+Shift+K";
constexpr auto kFindShortcut = "Ctrl+Shift+F";
constexpr auto kFindNextShortcut = "F3";
constexpr auto kFindPreviousShortcut = "Shift+F3";
constexpr auto kLinkNextShortcut = "Ctrl+Shift+L";
constexpr auto kLinkPreviousShortcut = "Ctrl+Shift+Alt+L";
constexpr auto kLinkCopyShortcut = "Ctrl+Shift+Y";
constexpr auto kLinkOpenShortcut = "Ctrl+Shift+O";
constexpr auto kQuitShortcut = "Ctrl+Shift+Q";

} // namespace

void TerminalWindow::buildActions() {
  const auto addTerminalAction =
      [this](QAction **action, const QString &objectName, const QString &text,
             const char *shortcut, const QString &statusTip) {
        *action = new QAction(text, this);
        (*action)->setObjectName(objectName);
        (*action)->setShortcut(QKeySequence(QLatin1String(shortcut)));
        (*action)->setShortcutContext(Qt::WindowShortcut);
        (*action)->setStatusTip(statusTip);
        (*action)->setToolTip(statusTip);
      };

  addTerminalAction(&m_newTabAction, QStringLiteral("tabNewAction"),
                    QStringLiteral("New Tab"), kNewTabShortcut,
                    QStringLiteral("Open a new terminal tab with the default "
                                   "profile"));
  addTerminalAction(&m_closeTabAction, QStringLiteral("tabCloseAction"),
                    QStringLiteral("Close Tab"), kCloseTabShortcut,
                    QStringLiteral("Close the active terminal tab"));
  addTerminalAction(&m_nextTabAction, QStringLiteral("tabNextAction"),
                    QStringLiteral("Next Tab"), kNextTabShortcut,
                    QStringLiteral("Switch to the next terminal tab"));
  addTerminalAction(&m_previousTabAction, QStringLiteral("tabPreviousAction"),
                    QStringLiteral("Previous Tab"), kPreviousTabShortcut,
                    QStringLiteral("Switch to the previous terminal tab"));
  addTerminalAction(&m_moveTabLeftAction, QStringLiteral("tabMoveLeftAction"),
                    QStringLiteral("Move Tab Left"), kMoveTabLeftShortcut,
                    QStringLiteral("Move the active terminal tab one "
                                   "position left"));
  addTerminalAction(&m_moveTabRightAction, QStringLiteral("tabMoveRightAction"),
                    QStringLiteral("Move Tab Right"), kMoveTabRightShortcut,
                    QStringLiteral("Move the active terminal tab one "
                                   "position right"));
  addTerminalAction(&m_manageProfilesAction,
                    QStringLiteral("profileManageAction"),
                    QStringLiteral("Manage Profiles…"), kManageProfilesShortcut,
                    QStringLiteral("Edit terminal profiles and tab restore"));
  addTerminalAction(&m_restartAction, QStringLiteral("sessionRestartAction"),
                    QStringLiteral("Restart Session"), kRestartShortcut,
                    QStringLiteral("Close this session and start a fresh "
                                   "one with the same profile"));
  addTerminalAction(&m_copyAction, QStringLiteral("editCopyAction"),
                    QStringLiteral("Copy"), kCopyShortcut,
                    QStringLiteral("Copy the terminal selection to the "
                                   "clipboard"));
  addTerminalAction(&m_pasteAction, QStringLiteral("editPasteAction"),
                    QStringLiteral("Paste"), kPasteShortcut,
                    QStringLiteral("Paste the clipboard into the terminal"));
  addTerminalAction(&m_pasteSelectionAction,
                    QStringLiteral("editPasteSelectionAction"),
                    QStringLiteral("Paste Selection"), kPasteSelectionShortcut,
                    QStringLiteral("Paste the primary selection into the "
                                   "terminal"));
  addTerminalAction(&m_selectAllAction, QStringLiteral("editSelectAllAction"),
                    QStringLiteral("Select All"), kSelectAllShortcut,
                    QStringLiteral("Select the entire terminal buffer"));
  addTerminalAction(&m_clearAction, QStringLiteral("viewClearAction"),
                    QStringLiteral("Clear Display"), kClearShortcut,
                    QStringLiteral("Clear the terminal display and "
                                   "scrollback"));
  addTerminalAction(&m_findAction, QStringLiteral("viewFindAction"),
                    QStringLiteral("Find…"), kFindShortcut,
                    QStringLiteral("Find text in this session's scrollback"));
  addTerminalAction(&m_findNextAction, QStringLiteral("viewFindNextAction"),
                    QStringLiteral("Find Next"), kFindNextShortcut,
                    QStringLiteral("Select the next scrollback match"));
  addTerminalAction(&m_findPreviousAction,
                    QStringLiteral("viewFindPreviousAction"),
                    QStringLiteral("Find Previous"), kFindPreviousShortcut,
                    QStringLiteral("Select the previous scrollback match"));
  addTerminalAction(&m_linkNextAction, QStringLiteral("linkNextAction"),
                    QStringLiteral("Select Next Link"), kLinkNextShortcut,
                    QStringLiteral("Select the next link in visible output"));
  addTerminalAction(&m_linkPreviousAction,
                    QStringLiteral("linkPreviousAction"),
                    QStringLiteral("Select Previous Link"),
                    kLinkPreviousShortcut,
                    QStringLiteral("Select the previous link in visible output"));
  addTerminalAction(&m_linkCopyAction, QStringLiteral("linkCopyAction"),
                    QStringLiteral("Copy Link"), kLinkCopyShortcut,
                    QStringLiteral("Copy the selected link exactly"));
  addTerminalAction(&m_linkOpenAction, QStringLiteral("linkOpenAction"),
                    QStringLiteral("Open Link…"), kLinkOpenShortcut,
                    QStringLiteral("Confirm and open the selected link"));
  addTerminalAction(&m_quitAction, QStringLiteral("fileQuitAction"),
                    QStringLiteral("Quit"), kQuitShortcut,
                    QStringLiteral("Close every session and quit"));

  addTerminalAction(&m_zoomInAction, QStringLiteral("viewZoomInAction"),
                    tr("Zoom In"), "Ctrl+Shift++", tr("Increase terminal text size"));
  addTerminalAction(&m_zoomOutAction, QStringLiteral("viewZoomOutAction"),
                    tr("Zoom Out"), "Ctrl+Shift+-", tr("Decrease terminal text size"));
  addTerminalAction(&m_zoomResetAction, QStringLiteral("viewZoomResetAction"),
                    tr("Actual Size"), "Ctrl+Shift+0", tr("Restore profile text size"));
  connect(m_zoomInAction, &QAction::triggered, this, [this] { if (m_activeSession) m_activeSession->zoomText(1); });
  connect(m_zoomOutAction, &QAction::triggered, this, [this] { if (m_activeSession) m_activeSession->zoomText(-1); });
  connect(m_zoomResetAction, &QAction::triggered, this, [this] { if (m_activeSession) m_activeSession->resetZoom(); });

  connect(m_newTabAction, &QAction::triggered, this,
          [this] { newSessionWithDefaultProfile(); });
  connect(m_closeTabAction, &QAction::triggered, this,
          [this] { closeActiveSession(); });
  connect(m_nextTabAction, &QAction::triggered, this,
          [this] { activateRelativeTab(1); });
  connect(m_previousTabAction, &QAction::triggered, this,
          [this] { activateRelativeTab(-1); });
  connect(m_moveTabLeftAction, &QAction::triggered, this,
          [this] { moveActiveTab(-1); });
  connect(m_moveTabRightAction, &QAction::triggered, this,
          [this] { moveActiveTab(1); });
  connect(m_manageProfilesAction, &QAction::triggered, this,
          [this] { manageProfiles(); });
  connect(m_restartAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      static_cast<void>(m_activeSession->restart());
    }
  });
  connect(m_copyAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      m_activeSession->copySelectionToClipboard();
    }
  });
  connect(m_pasteAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      m_activeSession->pasteClipboardToSession();
    }
  });
  connect(m_pasteSelectionAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      m_activeSession->pastePrimarySelectionToSession();
    }
  });
  connect(m_selectAllAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      m_activeSession->selectAllInView();
    }
  });
  connect(m_clearAction, &QAction::triggered, this, [this] {
    if (m_activeSession != nullptr) {
      m_activeSession->clearView();
    }
  });
  connect(m_findAction, &QAction::triggered, this, &TerminalWindow::showFindBar);
  connect(m_findNextAction, &QAction::triggered, this,
          [this] { runSearch(TerminalSearchDirection::Next); });
  connect(m_findPreviousAction, &QAction::triggered, this,
          [this] { runSearch(TerminalSearchDirection::Previous); });
  connect(m_linkNextAction, &QAction::triggered, this,
          [this] { selectRelativeLink(1); });
  connect(m_linkPreviousAction, &QAction::triggered, this,
          [this] { selectRelativeLink(-1); });
  connect(m_linkCopyAction, &QAction::triggered, this,
          &TerminalWindow::copyCurrentLink);
  connect(m_linkOpenAction, &QAction::triggered, this,
          &TerminalWindow::openCurrentLink);
  connect(m_quitAction, &QAction::triggered, this, &TerminalWindow::close);
}

void TerminalWindow::buildMenus() {
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu->setObjectName(QStringLiteral("fileMenu"));
  fileMenu->addAction(m_newTabAction);
  m_profileMenu = fileMenu->addMenu(QStringLiteral("New Tab With Profil&e"));
  m_profileMenu->setObjectName(QStringLiteral("profileNewTabMenu"));
  fileMenu->addAction(m_closeTabAction);
  fileMenu->addSeparator();
  fileMenu->addAction(m_quitAction);

  auto *sessionMenu = menuBar()->addMenu(QStringLiteral("&Session"));
  sessionMenu->setObjectName(QStringLiteral("sessionMenu"));
  sessionMenu->addAction(m_restartAction);
  sessionMenu->addSeparator();
  sessionMenu->addAction(m_previousTabAction);
  sessionMenu->addAction(m_nextTabAction);
  sessionMenu->addSeparator();
  sessionMenu->addAction(m_moveTabLeftAction);
  sessionMenu->addAction(m_moveTabRightAction);
  sessionMenu->addSeparator();
  sessionMenu->addAction(m_manageProfilesAction);

  auto *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
  editMenu->setObjectName(QStringLiteral("editMenu"));
  editMenu->addAction(m_copyAction);
  editMenu->addAction(m_pasteAction);
  editMenu->addAction(m_pasteSelectionAction);
  editMenu->addAction(m_selectAllAction);
  editMenu->addSeparator();
  editMenu->addAction(m_linkCopyAction);
  editMenu->addAction(m_linkOpenAction);

  auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
  viewMenu->setObjectName(QStringLiteral("viewMenu"));
  viewMenu->addActions({m_zoomInAction, m_zoomOutAction, m_zoomResetAction});
  viewMenu->addSeparator();
  viewMenu->addAction(m_clearAction);
  viewMenu->addSeparator();
  viewMenu->addAction(m_findAction);
  viewMenu->addAction(m_findNextAction);
  viewMenu->addAction(m_findPreviousAction);
  viewMenu->addSeparator();
  viewMenu->addAction(m_linkPreviousAction);
  viewMenu->addAction(m_linkNextAction);
}

void TerminalWindow::publishAppShellProjection() {
  const auto error = m_appShellBridge->publishActionCatalog();
  if (!error.ok()) {
    std::fprintf(stderr,
                 "qindaqt-terminal: AppShell catalog was rejected: %s\n",
                 qPrintable(error.message));
    std::fflush(stderr);
  }
  const QHash<QString, QAction *> targets{
      {QString::fromLatin1(AppShellActionIds::SessionNewTab), m_newTabAction},
      {QString::fromLatin1(AppShellActionIds::SessionCloseTab),
       m_closeTabAction},
      {QString::fromLatin1(AppShellActionIds::SessionRestart), m_restartAction},
      {QString::fromLatin1(AppShellActionIds::SessionManageProfiles),
       m_manageProfilesAction},
      {QString::fromLatin1(AppShellActionIds::SessionNextTab), m_nextTabAction},
      {QString::fromLatin1(AppShellActionIds::SessionPreviousTab),
       m_previousTabAction},
      {QString::fromLatin1(AppShellActionIds::SessionMoveTabLeft),
       m_moveTabLeftAction},
      {QString::fromLatin1(AppShellActionIds::SessionMoveTabRight),
       m_moveTabRightAction},
      {QString::fromLatin1(AppShellActionIds::EditCopy), m_copyAction},
      {QString::fromLatin1(AppShellActionIds::EditPaste), m_pasteAction},
      {QString::fromLatin1(AppShellActionIds::EditPasteSelection),
       m_pasteSelectionAction},
      {QString::fromLatin1(AppShellActionIds::EditSelectAll),
       m_selectAllAction},
      {QString::fromLatin1(AppShellActionIds::ViewClear), m_clearAction},
      {QStringLiteral("view.zoom-in"), m_zoomInAction},
      {QStringLiteral("view.zoom-out"), m_zoomOutAction},
      {QStringLiteral("view.zoom-reset"), m_zoomResetAction},
      {QString::fromLatin1(AppShellActionIds::ViewFind), m_findAction},
      {QString::fromLatin1(AppShellActionIds::ViewFindNext), m_findNextAction},
      {QString::fromLatin1(AppShellActionIds::ViewFindPrevious),
       m_findPreviousAction},
      {QString::fromLatin1(AppShellActionIds::LinkNext), m_linkNextAction},
      {QString::fromLatin1(AppShellActionIds::LinkPrevious),
       m_linkPreviousAction},
      {QString::fromLatin1(AppShellActionIds::LinkCopy), m_linkCopyAction},
      {QString::fromLatin1(AppShellActionIds::LinkOpen), m_linkOpenAction},
      {QString::fromLatin1(AppShellActionIds::FileQuit), m_quitAction},
  };
  m_appShellBridge->bindActivationTargets(targets);
  for (auto it = targets.begin(); it != targets.end(); ++it) {
    const QString id = it.key();
    static_cast<void>(
        m_appShellBridge->setActionEnabled(id, it.value()->isEnabled()));
    connect(
        it.value(), &QAction::enabledChanged, this, [this, id](bool enabled) {
          static_cast<void>(m_appShellBridge->setActionEnabled(id, enabled));
        });
  }
}

void TerminalWindow::manageProfiles() {
  if (m_profileSettings == nullptr || !m_profileSettings->baselineReceived()) {
    showStatusMessage(
        QStringLiteral("Profiles are unavailable: Settings1 has no current "
                       "confirmed baseline"),
        true);
    return;
  }
  TerminalProfileDialog dialog(
      m_profileSettings->userProfiles(), m_profileSettings->defaultProfileId(),
      m_profileSettings->restoreTabsPolicy(), m_themeIds, this);
  connect(&dialog, &TerminalProfileDialog::applyRequested, &dialog,
          [this, &dialog] {
            if (!m_profileSettings->applyProfiles(dialog.userProfiles(),
                                                  dialog.defaultProfileId(),
                                                  dialog.restoreTabs())) {
              const QString failure = QStringLiteral(
                  "Error: Profiles could not be saved right now; Settings1 "
                  "is unavailable or a save is in progress");
              showStatusMessage(failure, true);
              dialog.finishApply(false, failure);
              return;
            }
            showStatusMessage(QStringLiteral("Saving terminal profiles…"),
                              false);
          });
  connect(m_profileSettings, &TerminalProfileSettings::applyFinished, &dialog,
          [&dialog](const QVariantList &ledger) {
            const TerminalProfileApplyStatus status =
                terminalProfileApplyStatus(ledger);
            dialog.finishApply(status.allApplied, status.text);
          });
  static_cast<void>(dialog.exec());
}

void TerminalWindow::rebuildProfileMenu() {
  if (m_profileMenu == nullptr) {
    return;
  }
  m_profileMenu->clear();
  const auto addProfileEntry = [this](const TerminalProfile &profile) {
    QAction *action = m_profileMenu->addAction(
        QStringLiteral("New Tab With \"%1\"").arg(profile.name));
    action->setObjectName(
        QStringLiteral("newTabWithProfileAction-%1").arg(profile.id));
    connect(action, &QAction::triggered, this,
            [this, profile] { addSessionWithProfile(profile); });
  };
  addProfileEntry(builtinDefaultProfile());
  if (m_profileSettings != nullptr && m_profileSettings->baselineReceived()) {
    const QList<TerminalProfile> profiles = m_profileSettings->userProfiles();
    for (const TerminalProfile &profile : profiles) {
      addProfileEntry(profile);
    }
  }
  updateTabActionStates();
}

TerminalProfile TerminalWindow::currentDefaultProfile() const {
  return m_profileSettings != nullptr && m_profileSettings->baselineReceived()
             ? m_profileSettings->defaultProfile()
             : builtinDefaultProfile();
}

void TerminalWindow::updateViewActionStates() {
  const TerminalSession *active = m_activeSession;
  const auto state =
      active != nullptr ? active->state() : TerminalSession::State::Idle;
  const bool viewLive = active != nullptr &&
                        active->terminalWidget() != nullptr &&
                        state != TerminalSession::State::ShuttingDown;
  const bool generationLive = state == TerminalSession::State::Running;
  const bool hasSelection =
      active != nullptr && m_selectionBySession.value(active, false);
  m_copyAction->setEnabled(hasSelection && viewLive);
  m_pasteAction->setEnabled(generationLive);
  m_pasteSelectionAction->setEnabled(generationLive);
  m_selectAllAction->setEnabled(viewLive);
  m_clearAction->setEnabled(viewLive);
  m_zoomInAction->setEnabled(viewLive);
  m_zoomOutAction->setEnabled(viewLive);
  m_zoomResetAction->setEnabled(viewLive);
  m_findAction->setEnabled(viewLive);
  const bool hasSearch = active != nullptr &&
                         !m_searchBySession.value(active).pattern.isEmpty();
  m_findNextAction->setEnabled(viewLive && hasSearch);
  m_findPreviousAction->setEnabled(viewLive && hasSearch);
  m_linkNextAction->setEnabled(viewLive);
  m_linkPreviousAction->setEnabled(viewLive);
  const bool hasLink = active != nullptr &&
                       m_linkBySession.value(active).found;
  m_linkCopyAction->setEnabled(viewLive && hasLink);
  m_linkOpenAction->setEnabled(viewLive && hasLink && m_linkOpener != nullptr);
  m_restartAction->setEnabled(active != nullptr &&
                              state != TerminalSession::State::ShuttingDown &&
                              state != TerminalSession::State::ShutdownFailed);
}

void TerminalWindow::updateTabActionStates() {
  const int count = m_sessions->count();
  const int current = m_tabBar->currentIndex();
  m_newTabAction->setEnabled(count < TerminalSessionCollection::kMaxSessions);
  m_closeTabAction->setEnabled(count > 0);
  m_nextTabAction->setEnabled(count > 1);
  m_previousTabAction->setEnabled(count > 1);
  m_moveTabLeftAction->setEnabled(count > 1 && current > 0);
  m_moveTabRightAction->setEnabled(count > 1 && current >= 0 &&
                                   current + 1 < count);
  m_manageProfilesAction->setEnabled(m_profileSettings != nullptr &&
                                     m_profileSettings->baselineReceived());
}

} // namespace QindaQt::Apps::Terminal
