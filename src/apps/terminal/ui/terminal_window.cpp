// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "app_shell/terminal_app_shell_bridge.h"
#include "profiles/terminal_profile_settings.h"
#include "session/terminal_launch_policy.h"
#include "ui/terminal_find_bar.h"
#include "qindaqt/controls/application_icon.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QVBoxLayout>

namespace QindaQt::Apps::Terminal {

TerminalWindow::TerminalWindow(
    std::unique_ptr<TerminalSessionCollection> sessions,
    const TerminalViewAppearance &appearance,
    TerminalProfileSettings *profileSettings, TerminalLinkOpener *linkOpener,
    NewTerminalLauncher newTerminalLauncher, QWidget *parent)
    : QMainWindow(parent), m_sessions(std::move(sessions)),
      m_profileSettings(profileSettings), m_linkOpener(linkOpener),
      m_appearance(appearance),
      m_newTerminalLauncher(std::move(newTerminalLauncher)) {
  setObjectName(QStringLiteral("qindaqtTerminalWindow"));
  setAccessibleName(QStringLiteral("QindaQt Terminal"));
  setAccessibleDescription(
      QStringLiteral("Terminal sessions running the configured shell"));
  setWindowTitle(QStringLiteral("QindaQt Terminal"));
  // AGENT-CONTRACT (ADR-0116): no palette, font, or stylesheet is installed
  // here. Window chrome, menus, dialogs, the status bar, and the find bar
  // inherit the Qt platform theme palette and Fusion.
  setWindowIcon(QindaQt::Controls::applicationIcon(QStringLiteral("utilities-terminal")));

  auto *container = new QWidget(this);
  container->setObjectName(QStringLiteral("qindaqtTerminalContainer"));
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);
  buildFindBar();
  containerLayout->addWidget(m_findBar);
  m_terminalHolder = new QWidget(container);
  m_terminalHolder->setObjectName(QStringLiteral("qindaqtTerminalHolder"));
  m_terminalLayout = new QVBoxLayout(m_terminalHolder);
  m_terminalLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->addWidget(m_terminalHolder, 1);
  setCentralWidget(container);

  buildActions();
  buildMenus();
  buildStatusBar();
  wireCollection();

  m_appShellBridge = new TerminalAppShellBridge(this);
  publishAppShellProjection();
  rebuildProfileMenu();
  if (m_profileSettings != nullptr) {
    connect(m_profileSettings, &TerminalProfileSettings::profilesChanged, this,
            &TerminalWindow::rebuildProfileMenu);
    connect(m_profileSettings, &TerminalProfileSettings::applyFinished, this,
            &TerminalWindow::presentProfileApplyResult);
  }

  updateViewActionStates();
}

TerminalWindow::~TerminalWindow() = default;

void TerminalWindow::applyAppearance(const TerminalViewAppearance &appearance) {
  m_appearance = appearance;
  setWindowIcon(QindaQt::Controls::applicationIcon(QStringLiteral("utilities-terminal")));
  // The find bar tints its icons from its own palette, which already tracks
  // the platform theme; only the icon re-resolution needs a nudge.
  m_findBar->refreshIcons();
  // Content owns its profile font and ANSI palette; chrome inherits Qt.
  m_sessions->setAppearance(appearance);
  if (m_activeSession)
    updateStatusForState(m_activeSession->state());
}

void TerminalWindow::refreshDesktopContentAppearance() {
  applyAppearance(terminalDesktopContentAppearance());
}

void TerminalWindow::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
  switch (event->type()) {
  // Live platform palette/font/style changes: chrome repaints on its own;
  // only the terminal content derivation consumes the new platform truth.
  case QEvent::ApplicationPaletteChange:
  case QEvent::PaletteChange:
  case QEvent::ApplicationFontChange:
  case QEvent::FontChange:
  case QEvent::StyleChange:
  case QEvent::ThemeChange:
    refreshDesktopContentAppearance();
    break;
  default:
    break;
  }
}

QindaQt::AppShell::ApplicationCoordinator &
TerminalWindow::appShellCoordinator() {
  return m_appShellBridge->coordinator();
}

void TerminalWindow::prepareApplicationQuitFlow(QGuiApplication &application) {
  // AGENT-GUARD: Flipping this single Qt default is what keeps window close
  // from terminating the event loop during the bounded teardown escalation.
  // Idempotent by design; the regression row documents that Qt's default is
  // true so the flip can never silently become a no-op.
  application.setQuitOnLastWindowClosed(false);
}

void TerminalWindow::connectQuitAfterCloseShutdown(
    QCoreApplication &application) const {
  // The queued connection is deliberate: quit must observe every session's
  // terminal state, not merely the close intent.
  QObject::connect(this, &TerminalWindow::closeShutdownFinished, &application,
                   &QCoreApplication::quit, Qt::QueuedConnection);
}

void TerminalWindow::buildStatusBar() {
  m_statusLabel = new QLabel(QStringLiteral("No session"), this);
  m_statusLabel->setObjectName(QStringLiteral("qindaqtTerminalStatus"));
  m_statusLabel->setAccessibleName(QStringLiteral("Session status"));
  statusBar()->addWidget(m_statusLabel, 1);
  statusBar()->setSizeGripEnabled(false);
  statusBar()->setAccessibleName(QStringLiteral("Terminal status bar"));
}

void TerminalWindow::wireCollection() {
  connect(m_sessions.get(), &TerminalSessionCollection::sessionAdded, this,
          [this](TerminalSession *session) {
            // AGENT-GUARD: TerminalWindow deliberately permits one session.
            // Containers own tab and split topology, so a second shell must
            // be a separate Terminal process launched by launchNewTerminal.
            if (m_activeSession != nullptr) {
              showStatusMessage(QStringLiteral("Error: this window already has a shell"),
                                true);
              return;
            }
            wireSessionPresentation(session);
            setActiveSession(session);
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionRemoved, this,
          [this](TerminalSession *session) {
            m_selectionBySession.remove(session);
            m_titlesBySession.remove(session);
            m_searchBySession.remove(session);
            m_searchResultBySession.remove(session);
            m_findVisibleBySession.remove(session);
            m_linkBySession.remove(session);
            disconnect(session, nullptr, this, nullptr);
            if (session == m_activeSession) {
              m_activeSession = nullptr;
              detachSessionView();
              showStatusMessage(QStringLiteral("No session"), false);
              updateWindowTitle();
            }
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionTitleChanged,
          this, [this](TerminalSession *session, const QString &title) {
            m_titlesBySession.insert(session, title);
            if (session == m_activeSession) {
              updateWindowTitle();
            }
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionAddRejected,
          this, [this](const QString &diagnostic) {
            showStatusMessage(QStringLiteral("Error: %1").arg(diagnostic),
                              true);
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionCloseFailed,
          this, [this](TerminalSession *session, const QString &diagnostic) {
            if (session == m_activeSession) {
              showStatusMessage(QStringLiteral("Error: %1").arg(diagnostic),
                                true);
            }
          });
  connect(m_sessions.get(), &TerminalSessionCollection::allSessionsClosed, this,
          [this](bool clean, const QString &diagnostic) {
            updateViewActionStates();
            if (!clean) {
              // A SIGKILL survivor stays owned: quit is refused and the
              // failure stays visible in the re-shown window (P1-2).
              m_quitRequested = false;
              show();
              showStatusMessage(QStringLiteral("Error: %1").arg(diagnostic),
                                true);
              return;
            }
            if (m_quitRequested) {
              emit closeShutdownFinished();
            }
          });
}

void TerminalWindow::wireSessionPresentation(TerminalSession *session) {
  connect(session, &TerminalSession::terminalWidgetChanged, this,
          [this, session](QWidget *widget) {
            if (widget != nullptr && session == m_activeSession) {
              attachSessionView(session);
            }
          });
  connect(session, &TerminalSession::viewDisposalRequested, this,
          [this, session] {
            // Synchronous detach before the adapter destroys the view; the
            // layout must not outlive a widget it indexes.
            if (session == m_activeSession) {
              detachSessionView();
            }
            m_selectionBySession.insert(session, false);
            if (session == m_activeSession) {
              updateViewActionStates();
            }
          });
  connect(session, &TerminalSession::stateChanged, this,
          [this, session](TerminalSession::State state) {
            if (session == m_activeSession) {
              updateStatusForState(state);
            }
          });
  connect(session, &TerminalSession::sessionFinished, this,
          [this, session](const TerminalExitStatus &status) {
            if (session == m_activeSession) {
              showExitStatus(status);
            }
          });
  connect(session, &TerminalSession::selectionAvailable, this,
          [this, session](bool hasSelection) {
            m_selectionBySession.insert(session, hasSelection);
            if (session == m_activeSession) {
              updateViewActionStates();
            }
          });
  connect(session, &TerminalSession::linkContextRequested, this,
          [this, session](const QPoint &position) {
            if (session == m_activeSession) {
              showLinkContextMenu(position);
            }
          });
}

void TerminalWindow::setActiveSession(TerminalSession *session) {
  if (session == nullptr || session == m_activeSession) {
    return;
  }
  m_activeSession = session;
  detachSessionView();
  attachSessionView(session);
  updateStatusForState(session->state());
  updateViewActionStates();
  updateWindowTitle();
  restoreSearchPresentation();
}

void TerminalWindow::attachSessionView(TerminalSession *session) {
  QWidget *widget = session->terminalWidget();
  if (widget == nullptr) {
    return;
  }
  // Apply the retained live appearance before the first prompt, including
  // when a restart constructs a fresh renderer after the settings snapshot.
  session->setAppearance(m_appearance);
  m_terminalLayout->addWidget(widget);
  m_terminalView = widget;
  widget->setAccessibleName(QStringLiteral("Terminal session"));
  widget->setAccessibleDescription(
      QStringLiteral("Shell output and keyboard input for the running "
                     "terminal session"));
  widget->setFocusPolicy(Qt::StrongFocus);
  widget->setFocus();
}

void TerminalWindow::detachSessionView() {
  if (m_terminalView != nullptr) {
    m_terminalLayout->removeWidget(m_terminalView);
    m_terminalView = nullptr;
  }
}

void TerminalWindow::newSessionWithDefaultProfile() {
  startSession(currentDefaultProfile());
}

void TerminalWindow::startSession(const TerminalProfile &profile) {
  if (m_sessions->count() != 0) {
    showStatusMessage(QStringLiteral("Error: this window already has a shell"),
                      true);
    return;
  }
  static_cast<void>(m_sessions->addSession(profile));
}

void TerminalWindow::launchNewTerminal(const TerminalProfile &profile) {
  const QString directory = m_activeSession != nullptr
                                ? m_activeSession->workingDirectory()
                                : m_sessions->context().workingDirectory;
  if (!m_newTerminalLauncher) {
    showStatusMessage(QStringLiteral("Error: opening another Terminal is unavailable"),
                      true);
    return;
  }
  const QString error = m_newTerminalLauncher(profile, directory);
  if (!error.isEmpty()) {
    showStatusMessage(QStringLiteral("Error: %1").arg(error), true);
  }
}

void TerminalWindow::updateWindowTitle() {
  const QString title =
      m_activeSession != nullptr ? displayTitle(m_activeSession) : QString();
  setWindowTitle(title.isEmpty()
                     ? QStringLiteral("QindaQt Terminal")
                     : QStringLiteral("%1 — QindaQt Terminal").arg(title));
}

QString TerminalWindow::displayTitle(const TerminalSession *session) const {
  const QString title = m_titlesBySession.value(session);
  if (!title.isEmpty()) {
    return title;
  }
  return QStringLiteral("Terminal");
}

void TerminalWindow::requestCloseShutdown() {
  // AGENT-GUARD: The application must not exit before every session reached
  // a terminal state, or a surviving child would defeat the teardown
  // guarantee. Hiding the window and waiting for allSessionsClosed is what
  // keeps close deterministic under the bounded escalation; a failed
  // escalation re-shows the window and refuses the quit.
  //
  // Snapshot the restorable launch state now: when closeShutdownFinished
  // fires the sessions are already removed, and workingDirectory() reads the
  // live child cwd through the process monitor only while the session runs.
  // The first snapshot wins; a re-entry (Restart close racing a user close)
  // must not overwrite it with a mid-shutdown state.
  if (!m_restoreEntryAtClose.has_value() && m_activeSession != nullptr) {
    TerminalRestoreEntry entry;
    entry.profileId = m_activeSession->profile().id;
    entry.workingDirectory = m_activeSession->workingDirectory();
    if (entry.workingDirectory.isEmpty()) {
      entry.workingDirectory = m_sessions->context().workingDirectory;
    }
    if (!entry.profileId.isEmpty() && !entry.workingDirectory.isEmpty()) {
      m_restoreEntryAtClose = entry;
    }
  }
  m_quitRequested = true;
  hide();
  m_sessions->requestCloseAll();
}

void TerminalWindow::closeEvent(QCloseEvent *event) {
  // AGENT-GUARD (P1: Restart→Close): every non-refused close — including
  // one that arrives while a Restart's teardown is already in flight —
  // reaches TerminalSession::beginShutdown() through the collection, which
  // cancels pending restarts. A SIGKILL survivor in the owned session
  // refuses the close: ownership of that survivor is retained, so closing
  // (and the quit it would trigger) stays refused until the child is gone.
  const auto sessions = m_sessions.get();
  for (int index = 0; index < sessions->count(); ++index) {
    if (sessions->sessionAt(index)->state() ==
        TerminalSession::State::ShutdownFailed) {
      showStatusMessage(
          QStringLiteral("Error: a session child survived teardown; close "
                         "is refused"),
          true);
      event->ignore();
      return;
    }
  }
  requestCloseShutdown();
  event->accept();
}

void TerminalWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (m_terminalView != nullptr) {
    const QSize clamped = TerminalLaunchPolicy::clampViewSize(
        event->size().width(), event->size().height());
    m_terminalView->resize(clamped);
  }
}

} // namespace QindaQt::Apps::Terminal
