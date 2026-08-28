// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_window.h"

#include "session/terminal_launch_policy.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QMenuBar>
#include <QResizeEvent>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstdio>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-GUARD: These defaults exist for keyboard semantics, not decoration.
// None of them uses a plain Ctrl+<letter> sequence: readline owns Ctrl+C/S/Q/A
// and friends inside the child, so window shortcuts must stay Shift-modified
// or they would steal flow control from every interactive program.
constexpr auto kRestartShortcut = "Ctrl+Shift+R";
constexpr auto kCopyShortcut = "Ctrl+Shift+C";
constexpr auto kPasteShortcut = "Ctrl+Shift+V";
constexpr auto kPasteSelectionShortcut = "Ctrl+Shift+Insert";
constexpr auto kSelectAllShortcut = "Ctrl+Shift+A";
constexpr auto kClearShortcut = "Ctrl+Shift+K";
constexpr auto kQuitShortcut = "Ctrl+Shift+Q";

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

TerminalWindow::TerminalWindow(std::unique_ptr<TerminalSession> session,
                               const TerminalViewAppearance &appearance,
                               QWidget *parent)
    : QMainWindow(parent), m_session(std::move(session)),
      m_appearance(appearance) {
  setObjectName(QStringLiteral("qindaqtTerminalWindow"));
  setAccessibleName(QStringLiteral("QindaQt Terminal"));
  setAccessibleDescription(
      QStringLiteral("Interactive terminal session running the configured "
                     "shell"));
  setWindowTitle(QStringLiteral("QindaQt Terminal"));
  setPalette(m_appearance.windowPalette);
  setFont(m_appearance.interfaceFont);

  m_terminalHolder = new QWidget(this);
  m_terminalHolder->setObjectName(QStringLiteral("qindaqtTerminalHolder"));
  m_terminalLayout = new QVBoxLayout(m_terminalHolder);
  m_terminalLayout->setContentsMargins(0, 0, 0, 0);
  setCentralWidget(m_terminalHolder);

  buildActions();
  buildMenus();
  buildStatusBar();

  connect(m_session.get(), &TerminalSession::terminalWidgetChanged, this,
          &TerminalWindow::embedTerminalWidget);
  connect(m_session.get(), &TerminalSession::viewDisposalRequested, this,
          [this] {
            // Synchronous detach before the adapter destroys the view; the
            // layout must not outlive a widget it indexes.
            if (m_terminalView != nullptr) {
              m_terminalLayout->removeWidget(m_terminalView);
              m_terminalView = nullptr;
            }
          });
  connect(m_session.get(), &TerminalSession::stateChanged, this,
          [this](TerminalSession::State state) {
            updateStatusForState(state);
          });
  connect(m_session.get(), &TerminalSession::sessionFinished, this,
          [this](const TerminalExitStatus &status) {
            showExitStatus(status);
          });
  connect(m_session.get(), &TerminalSession::titleReceived, this,
          [this](const QString &title) {
            setWindowTitle(title.isEmpty()
                               ? QStringLiteral("QindaQt Terminal")
                               : QStringLiteral("%1 — QindaQt Terminal")
                                     .arg(title));
          });
  connect(m_session.get(), &TerminalSession::shutdownFinished, this,
          &TerminalWindow::reportShutdownOutcome);
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
  // The queued connection is deliberate: quit must observe the session's
  // terminal state, not merely the close intent.
  QObject::connect(this, &TerminalWindow::closeShutdownFinished, &application,
                   &QCoreApplication::quit, Qt::QueuedConnection);
}

void TerminalWindow::buildActions() {
  const auto addTerminalAction = [this](QAction **action,
                                        const QString &objectName,
                                        const QString &text,
                                        const char *shortcut,
                                        const QString &statusTip) {
    *action = new QAction(text, this);
    (*action)->setObjectName(objectName);
    (*action)->setShortcut(QKeySequence(QLatin1String(shortcut)));
    (*action)->setShortcutContext(Qt::WindowShortcut);
    (*action)->setStatusTip(statusTip);
    (*action)->setToolTip(statusTip);
  };

  addTerminalAction(&m_restartAction, QStringLiteral("sessionRestartAction"),
                    QStringLiteral("Restart Session"), kRestartShortcut,
                    QStringLiteral("Close this session and start a fresh "
                                   "one with the same shell"));
  addTerminalAction(&m_copyAction, QStringLiteral("editCopyAction"),
                    QStringLiteral("Copy"), kCopyShortcut,
                    QStringLiteral("Copy the terminal selection to the "
                                   "clipboard"));
  addTerminalAction(&m_pasteAction, QStringLiteral("editPasteAction"),
                    QStringLiteral("Paste"), kPasteShortcut,
                    QStringLiteral("Paste the clipboard into the terminal"));
  addTerminalAction(&m_pasteSelectionAction,
                    QStringLiteral("editPasteSelectionAction"),
                    QStringLiteral("Paste Selection"),
                    kPasteSelectionShortcut,
                    QStringLiteral("Paste the primary selection into the "
                                   "terminal"));
  addTerminalAction(&m_selectAllAction, QStringLiteral("editSelectAllAction"),
                    QStringLiteral("Select All"), kSelectAllShortcut,
                    QStringLiteral("Select the entire terminal buffer"));
  addTerminalAction(&m_clearAction, QStringLiteral("viewClearAction"),
                    QStringLiteral("Clear Display"), kClearShortcut,
                    QStringLiteral("Clear the terminal display and "
                                   "scrollback"));
  addTerminalAction(&m_quitAction, QStringLiteral("fileQuitAction"),
                    QStringLiteral("Quit"), kQuitShortcut,
                    QStringLiteral("Close the session and quit"));

  connect(m_restartAction, &QAction::triggered, this, [this] {
    // A rejected restart has already published its typed failure through
    // sessionFinished, which the status bar renders; the boolean is the
    // running-state answer only.
    static_cast<void>(m_session->restart());
  });
  connect(m_copyAction, &QAction::triggered, this,
          [this] { m_session->copySelectionToClipboard(); });
  m_copyAction->setEnabled(false);
  connect(m_pasteAction, &QAction::triggered, this,
          [this] { m_session->pasteClipboardToSession(); });
  connect(m_pasteSelectionAction, &QAction::triggered, this,
          [this] { m_session->pastePrimarySelectionToSession(); });
  connect(m_selectAllAction, &QAction::triggered, this,
          [this] { m_session->selectAllInView(); });
  connect(m_clearAction, &QAction::triggered, this,
          [this] { m_session->clearView(); });
  connect(m_quitAction, &QAction::triggered, this,
          &TerminalWindow::close);

  connect(m_session.get(), &TerminalSession::selectionAvailable, this,
          [this](bool hasSelection) { m_copyAction->setEnabled(hasSelection); });
}

void TerminalWindow::buildMenus() {
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu->setObjectName(QStringLiteral("sessionMenu"));
  fileMenu->addAction(m_restartAction);
  fileMenu->addSeparator();
  fileMenu->addAction(m_quitAction);

  auto *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
  editMenu->setObjectName(QStringLiteral("editMenu"));
  editMenu->addAction(m_copyAction);
  editMenu->addAction(m_pasteAction);
  editMenu->addAction(m_pasteSelectionAction);
  editMenu->addAction(m_selectAllAction);

  auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
  viewMenu->setObjectName(QStringLiteral("viewMenu"));
  viewMenu->addAction(m_clearAction);
}

void TerminalWindow::buildStatusBar() {
  m_statusLabel = new QLabel(QStringLiteral("No session"), this);
  m_statusLabel->setObjectName(QStringLiteral("qindaqtTerminalStatus"));
  m_statusLabel->setAccessibleName(QStringLiteral("Session status"));
  statusBar()->addPermanentWidget(m_statusLabel);
  statusBar()->setAccessibleName(QStringLiteral("Terminal status bar"));
}

void TerminalWindow::embedTerminalWidget(QWidget *widget) {
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
  updateStatusForState(m_session->state());
}

void TerminalWindow::updateStatusForState(TerminalSession::State state) {
  m_restartAction->setEnabled(state != TerminalSession::State::ShuttingDown);
  QString text;
  switch (state) {
  case TerminalSession::State::Idle:
    text = QStringLiteral("No session");
    break;
  case TerminalSession::State::Running:
    text = QStringLiteral("Session running");
    break;
  case TerminalSession::State::Exited:
    // AGENT-GUARD: The typed exit status and the Exited state arrive in the
    // same tick (publishExit runs before setState). Rendering the generic
    // state text here would overwrite the code/signal/start-failure detail
    // before the user can read it, so the exit detail is the Exited state's
    // visible text.
    if (m_session->lastExit().kind == TerminalExitStatus::Kind::None) {
      text = QStringLiteral("Session ended");
    } else {
      showExitStatus(m_session->lastExit());
      return;
    }
    break;
  case TerminalSession::State::ShuttingDown:
    text = QStringLiteral("Closing session…");
    break;
  case TerminalSession::State::ShutdownComplete:
    text = QStringLiteral("Session closed");
    break;
  case TerminalSession::State::ShutdownFailed:
    text = QStringLiteral("Session close incomplete");
    break;
  }
  if (m_statusLabel != nullptr) {
    m_statusLabel->setText(text);
    m_statusLabel->setPalette(
        m_appearance.windowPalette); // Neutral states use window palette.
    // NF-T5: every visible text change must also update the screen-reader
    // name; showExitStatus does the same for exit severities.
    m_statusLabel->setAccessibleName(
        QStringLiteral("Session status: %1").arg(text));
  }
}

void TerminalWindow::showExitStatus(const TerminalExitStatus &status) {
  if (m_statusLabel == nullptr) {
    return;
  }
  QString text;
  QPalette palette = m_appearance.windowPalette;
  switch (status.kind) {
  case TerminalExitStatus::Kind::Normal:
    text = QStringLiteral("Session exited (code %1)").arg(status.code);
    break;
  case TerminalExitStatus::Kind::Signal:
    text = QStringLiteral("Session terminated by %1")
               .arg(signalName(status.code));
    palette.setColor(QPalette::WindowText,
                     m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::StartFailed:
    text = QStringLiteral("Error: %1").arg(status.diagnostic);
    palette.setColor(QPalette::WindowText,
                     m_appearance.statusDangerForeground);
    break;
  case TerminalExitStatus::Kind::None:
    return;
  }
  m_statusLabel->setText(text);
  m_statusLabel->setPalette(palette);
  m_statusLabel->setAccessibleName(
      QStringLiteral("Session status: %1").arg(text));
}

void TerminalWindow::reportShutdownOutcome(bool clean,
                                           const QString &diagnostic) {
  if (!clean) {
    // The window is hidden at this point; stderr is the honest channel for a
    // teardown failure that UI can no longer show.
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(diagnostic));
    std::fflush(stderr);
  }
  if (m_quitRequested) {
    emit closeShutdownFinished();
  }
}

void TerminalWindow::requestCloseShutdown() {
  // AGENT-GUARD: The application must not exit before the session reached a
  // terminal state, or a surviving child would defeat the teardown
  // guarantee. Hiding the window and waiting for shutdownFinished is what
  // keeps close deterministic under the bounded escalation.
  m_quitRequested = true;
  hide();
  m_session->beginShutdown();
}

void TerminalWindow::closeEvent(QCloseEvent *event) {
  if (m_session->state() == TerminalSession::State::ShuttingDown) {
    m_quitRequested = true;
    event->accept();
    return;
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
