// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "app_shell/terminal_app_shell_bridge.h"
#include "profiles/terminal_profile_settings.h"
#include "session/terminal_launch_policy.h"
#include "ui/terminal_tab_bar.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QVBoxLayout>

namespace QindaQt::Apps::Terminal {
namespace {

[[nodiscard]] QString signalName(int signalNumber) {
  switch (signalNumber) {
  case 1:
    return QStringLiteral("SIGHUP");
  case 2:
    return QStringLiteral("SIGINT");
  case 3:
    return QStringLiteral("SIGQUIT");
  case 4:
    return QStringLiteral("SIGILL");
  case 6:
    return QStringLiteral("SIGABRT");
  case 8:
    return QStringLiteral("SIGFPE");
  case 9:
    return QStringLiteral("SIGKILL");
  case 10:
    return QStringLiteral("SIGUSR1");
  case 11:
    return QStringLiteral("SIGSEGV");
  case 12:
    return QStringLiteral("SIGUSR2");
  case 13:
    return QStringLiteral("SIGPIPE");
  case 14:
    return QStringLiteral("SIGALRM");
  case 15:
    return QStringLiteral("SIGTERM");
  default:
    return QStringLiteral("signal %1").arg(signalNumber);
  }
}

} // namespace

TerminalWindow::TerminalWindow(
    std::unique_ptr<TerminalSessionCollection> sessions,
    const TerminalViewAppearance &appearance, const QStringList &themeIds,
    TerminalProfileSettings *profileSettings, QWidget *parent)
    : QMainWindow(parent), m_sessions(std::move(sessions)),
      m_profileSettings(profileSettings), m_themeIds(themeIds),
      m_appearance(appearance) {
  setObjectName(QStringLiteral("qindaqtTerminalWindow"));
  setAccessibleName(QStringLiteral("QindaQt Terminal"));
  setAccessibleDescription(
      QStringLiteral("Terminal sessions running the configured shell"));
  setWindowTitle(QStringLiteral("QindaQt Terminal"));
  setPalette(m_appearance.windowPalette);
  setFont(m_appearance.interfaceFont);

  auto *container = new QWidget(this);
  container->setObjectName(QStringLiteral("qindaqtTerminalContainer"));
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);
  m_tabBar = new TerminalTabBar(container);
  containerLayout->addWidget(m_tabBar);
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
  }

  connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
    const QVariant tab = m_tabBar->tabData(index);
    auto *session = tab.value<TerminalSession *>();
    if (session != nullptr && session != m_activeSession) {
      setActiveSession(session);
    }
    updateTabActionStates();
  });
  connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
    if (auto *session = m_tabBar->tabData(index).value<TerminalSession *>()) {
      closeSessionFromPresentation(session);
    }
  });

  updateViewActionStates();
  updateTabActionStates();
}

TerminalWindow::~TerminalWindow() = default;

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
  statusBar()->addPermanentWidget(m_statusLabel);
  statusBar()->setAccessibleName(QStringLiteral("Terminal status bar"));
}

void TerminalWindow::wireCollection() {
  connect(m_sessions.get(), &TerminalSessionCollection::sessionAdded, this,
          [this](TerminalSession *session) {
            wireSessionPresentation(session);
            m_tabBar->addTab(displayTitle(session));
            m_tabBar->setTabData(m_tabBar->count() - 1,
                                 QVariant::fromValue(session));
            if (m_activeSession == nullptr) {
              setActiveSession(session);
            }
            updateTabActionStates();
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionRemoved, this,
          [this](TerminalSession *session) {
            const int index = tabIndexOf(session);
            if (index >= 0) {
              m_tabBar->removeTab(index);
            }
            m_selectionBySession.remove(session);
            m_titlesBySession.remove(session);
            disconnect(session, nullptr, this, nullptr);
            if (session == m_activeSession) {
              m_activeSession = nullptr;
              detachSessionView();
              TerminalSession *next = nullptr;
              if (m_tabBar->count() > 0) {
                const int clamped = qBound(0, index, m_tabBar->count() - 1);
                next = m_tabBar->tabData(clamped).value<TerminalSession *>();
              }
              if (next != nullptr) {
                setActiveSession(next);
              } else {
                showStatusMessage(QStringLiteral("No session"), false);
                updateWindowTitle();
              }
            }
            updateTabActionStates();
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionMoved, this,
          [this](TerminalSession *session, int newIndex) {
            const int from = tabIndexOf(session);
            if (from >= 0 && from != newIndex) {
              m_tabBar->moveTab(from, newIndex);
            }
          });
  connect(m_sessions.get(), &TerminalSessionCollection::sessionTitleChanged,
          this, [this](TerminalSession *session, const QString &title) {
            m_titlesBySession.insert(session, title);
            const int index = tabIndexOf(session);
            if (index >= 0) {
              m_tabBar->setTabText(index, displayTitle(session));
            }
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
            updateTabActionStates();
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
}

void TerminalWindow::setActiveSession(TerminalSession *session) {
  if (session == nullptr || session == m_activeSession) {
    return;
  }
  m_activeSession = session;
  detachSessionView();
  attachSessionView(session);
  const int index = tabIndexOf(session);
  if (index >= 0 && m_tabBar->currentIndex() != index) {
    m_tabBar->setCurrentIndex(index);
  }
  updateStatusForState(session->state());
  updateViewActionStates();
  updateTabActionStates();
  updateWindowTitle();
}

void TerminalWindow::attachSessionView(TerminalSession *session) {
  QWidget *widget = session->terminalWidget();
  if (widget == nullptr) {
    return;
  }
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
  addSessionWithProfile(currentDefaultProfile());
}

void TerminalWindow::addSessionWithProfile(const TerminalProfile &profile) {
  // The collection validates, resolves through the launch policy, and
  // emits sessionAdded; refusal diagnostics arrive via sessionAddRejected.
  static_cast<void>(m_sessions->addSession(profile));
}

void TerminalWindow::closeActiveSession() {
  closeSessionFromPresentation(m_activeSession);
}

void TerminalWindow::closeSessionFromPresentation(TerminalSession *session) {
  if (session == nullptr) {
    return;
  }
  // AGENT-GUARD: Closing the final tab is application quit intent, not merely
  // an empty-window mutation. Route it through closeEvent so teardown-first
  // quit and survivor refusal remain identical for the tab button, shortcut,
  // File > Quit, and window decoration.
  if (m_sessions->count() == 1) {
    close();
    return;
  }
  m_sessions->requestCloseSession(session);
}

void TerminalWindow::activateRelativeTab(int delta) {
  const int count = m_tabBar->count();
  if (count < 2) {
    return;
  }
  const int next = (m_tabBar->currentIndex() + delta + count) % count;
  if (auto *session = m_tabBar->tabData(next).value<TerminalSession *>()) {
    setActiveSession(session);
  }
}

void TerminalWindow::moveActiveTab(int delta) {
  const int from = m_tabBar->currentIndex();
  const int target = from + delta;
  if (m_activeSession == nullptr || target < 0 || target >= m_tabBar->count()) {
    return;
  }
  m_sessions->moveSession(m_activeSession, target);
}

void TerminalWindow::updateStatusForState(TerminalSession::State state) {
  QString text;
  QPalette palette = m_appearance.windowPalette;
  switch (state) {
  case TerminalSession::State::Idle:
    text = QStringLiteral("No session");
    break;
  case TerminalSession::State::Running:
    text = QStringLiteral("Session running");
    break;
  case TerminalSession::State::Exited:
    // The typed exit status and the Exited state arrive in the same tick;
    // rendering the generic state text here would overwrite the
    // code/signal/unknown detail before the user can read it.
    if (m_activeSession != nullptr &&
        m_activeSession->lastExit().kind != TerminalExitStatus::Kind::None) {
      showExitStatus(m_activeSession->lastExit());
      updateViewActionStates();
      return;
    }
    text = QStringLiteral("Session ended");
    break;
  case TerminalSession::State::ShuttingDown:
    text = QStringLiteral("Closing session…");
    break;
  case TerminalSession::State::ShutdownComplete:
    text = QStringLiteral("Session closed");
    break;
  case TerminalSession::State::ShutdownFailed:
    text = QStringLiteral("Session close failed");
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  }
  showStatusMessage(text, false, palette);
  updateViewActionStates();
}

void TerminalWindow::showExitStatus(const TerminalExitStatus &status) {
  QString text;
  QPalette palette = m_appearance.windowPalette;
  switch (status.kind) {
  case TerminalExitStatus::Kind::Normal:
    text = QStringLiteral("Session exited (code %1)").arg(status.code);
    break;
  case TerminalExitStatus::Kind::Signal:
    text =
        QStringLiteral("Session terminated by %1").arg(signalName(status.code));
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::UnknownExit:
    // Another reaper consumed the status; the truth is "exited, code
    // unknown", never a fabricated normal status.
    text = QStringLiteral("Session exited (status unknown)");
    palette.setColor(QPalette::WindowText,
                     m_appearance.statusWarningForeground);
    break;
  case TerminalExitStatus::Kind::StartFailed:
    text = QStringLiteral("Error: %1").arg(status.diagnostic);
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::None:
    return;
  }
  showStatusMessage(text, false, palette);
}

void TerminalWindow::showStatusMessage(const QString &text, bool danger) {
  QPalette palette = m_appearance.windowPalette;
  if (danger) {
    palette.setColor(QPalette::WindowText, m_appearance.statusDangerForeground);
  }
  showStatusMessage(text, danger, palette);
}

void TerminalWindow::showStatusMessage(const QString &text, bool,
                                       const QPalette &palette) {
  if (m_statusLabel == nullptr) {
    return;
  }
  m_statusLabel->setText(text);
  m_statusLabel->setPalette(palette);
  // NF-T5: every visible text change must also update the screen-reader
  // name.
  m_statusLabel->setAccessibleName(
      QStringLiteral("Session status: %1").arg(text));
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
  const int index = m_sessions->indexOf(session);
  return QStringLiteral("Session %1").arg(index + 1);
}

int TerminalWindow::tabIndexOf(const TerminalSession *session) const {
  for (int index = 0; index < m_tabBar->count(); ++index) {
    if (m_tabBar->tabData(index).value<TerminalSession *>() == session) {
      return index;
    }
  }
  return -1;
}

void TerminalWindow::requestCloseShutdown() {
  // AGENT-GUARD: The application must not exit before every session reached
  // a terminal state, or a surviving child would defeat the teardown
  // guarantee. Hiding the window and waiting for allSessionsClosed is what
  // keeps close deterministic under the bounded escalation; a failed
  // escalation re-shows the window and refuses the quit.
  m_quitRequested = true;
  hide();
  m_sessions->requestCloseAll();
}

void TerminalWindow::closeEvent(QCloseEvent *event) {
  // AGENT-GUARD (P1: Restart→Close): every non-refused close — including
  // one that arrives while a Restart's teardown is already in flight —
  // reaches TerminalSession::beginShutdown() through the collection, which
  // cancels pending restarts. A SIGKILL survivor anywhere in the tab list
  // refuses the close: ownership of the survivor is retained, so closing
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
