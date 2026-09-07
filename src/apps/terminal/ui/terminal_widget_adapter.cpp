// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "session/pty_bridge.h"
#include "ui/terminal_appearance.h"

#include <qtermwidget.h>

#include <QContextMenuEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QFile>
#include <QResizeEvent>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <utility>
#include <vector>

namespace QindaQt::Apps::Terminal {
namespace {

// Child descriptor sweep fallback bound when close_range(2) is unavailable.
// The comment claims a bounded sweep, not "every descriptor", on purpose.
constexpr int kChildFdScanLimit = 4096;

bool hasSemanticSelection(const QString &selection) {
  // AGENT-GUARD (qtermwidget 2.4 live behavior): Select All over a pristine
  // grid returns one LF even though no cell contains user-visible content.
  // Treat only line separators as qtermwidget's structural row encoding;
  // spaces and tabs remain copyable because they can be intentional terminal
  // output. See docs/wiki/apps/terminal.md#action-and-accessibility-contract.
  for (const QChar character : selection) {
    if (character != QLatin1Char('\n') && character != QLatin1Char('\r') &&
        character != QChar::LineSeparator &&
        character != QChar::ParagraphSeparator) {
      return true;
    }
  }
  return false;
}

struct ExecStrings {
  QByteArray program;
  std::vector<QByteArray> arguments;
  std::vector<QByteArray> environment;
  QByteArray workingDirectory;
  std::vector<char *> argv;
  std::vector<char *> envp;
};

void writeAllToStderr(const char *message) {
  const auto length = static_cast<ssize_t>(std::strlen(message));
  ssize_t written = 0;
  while (written < length) {
    const ssize_t chunk = ::write(STDERR_FILENO, message + written,
                                  static_cast<size_t>(length - written));
    if (chunk <= 0) {
      return;
    }
    written += chunk;
  }
}

void resetSignalDispositionsAndMask() {
  sigset_t empty;
  sigemptyset(&empty);
  // The inherited mask is deliberately reset (P3-2): a blocked SIGCHLD or
  // SIGWINCH in the parent must not leak into the freshly exec'd shell.
  ::sigprocmask(SIG_SETMASK, &empty, nullptr);
  struct sigaction defaults;
  std::memset(&defaults, 0, sizeof(defaults));
  sigemptyset(&defaults.sa_mask);
  defaults.sa_handler = SIG_DFL;
  for (int signalNumber = 1; signalNumber < NSIG; ++signalNumber) {
    sigaction(signalNumber, &defaults, nullptr);
  }
}

void closeChildDescriptors() {
#ifdef __linux__
  // Linux-wide close primitive; the loop below is the fallback for every
  // close_range failure.
  if (::close_range(STDERR_FILENO + 1, ~0U, 0) == 0) {
    return;
  }
  // AGENT-GUARD (P3-2): fall through on ANY close_range error — ENOSYS on
  // old kernels, EINVAL on bounds, but also sandbox/SELinux EPERM. Returning
  // here would leak every inherited descriptor into the exec'd shell.
#endif
  for (int fd = STDERR_FILENO + 1; fd < kChildFdScanLimit; ++fd) {
    ::close(fd);
  }
}

// Runs only in the forked child and stays async-signal-safe until execve:
// no Qt, no allocation, no errno-dependent formatting. argv/envp pointer
// arrays are built before fork (P2-2). Diagnostics are fixed literals
// because formatting functions are not as-safe. Every dup/setup step is
// checked so a partially wired stdio never reaches execve.
[[noreturn]] void execChildInBridge(const char *slavePath,
                                    const ExecStrings &strings) {
  // AGENT-CONTRACT (ADR-0040, P3-4): setsid() runs BEFORE the slave opens —
  // the child becomes a session leader with no controlling terminal, and
  // opening the slave then acquires it as the controlling TTY; TIOCSCTTY
  // asserts that explicitly. Reordering the open before setsid silently
  // breaks this conventional acquisition order and the accepted wording.
  if (::setsid() == -1) {
    writeAllToStderr("qindaqt-terminal: cannot create the session\n");
    ::_exit(126);
  }
  const int slave = ::open(slavePath, O_RDWR);
  if (slave < 0) {
    writeAllToStderr("qindaqt-terminal: cannot open the terminal device\n");
    ::_exit(126);
  }
  if (::ioctl(slave, TIOCSCTTY, nullptr) == -1) {
    writeAllToStderr("qindaqt-terminal: cannot set controlling terminal\n");
    ::_exit(126);
  }
  if (::dup2(slave, STDIN_FILENO) == -1 || ::dup2(slave, STDOUT_FILENO) == -1 ||
      ::dup2(slave, STDERR_FILENO) == -1) {
    writeAllToStderr("qindaqt-terminal: cannot wire standard streams\n");
    ::_exit(126);
  }
  if (slave > STDERR_FILENO) {
    ::close(slave);
  }
  resetSignalDispositionsAndMask();
  closeChildDescriptors();
  if (!strings.workingDirectory.isEmpty() &&
      ::chdir(strings.workingDirectory.constData()) == -1) {
    writeAllToStderr("qindaqt-terminal: cannot enter working directory\n");
    ::_exit(126);
  }
  ::execve(strings.program.constData(), strings.argv.data(),
           strings.envp.data());
  writeAllToStderr("qindaqt-terminal: cannot run the configured shell\n");
  ::_exit(127);
}

} // namespace

TerminalWidgetAdapter::TerminalWidgetAdapter(
    const TerminalViewAppearance &appearance, const TerminalProfile &profile,
    QObject *parent)
    : TerminalSessionBackend(parent), m_appearance(appearance),
      m_profile(profile) {
  // AGENT-NOTE: startnow is deliberately 0 and setShellProgram/setArgs are
  // never used: the widget must not spawn its own child (ADR-0040). Teletype
  // startup is deferred to start(), after TerminalSession has published and
  // the window has attached this widget to its final layout. Initializing a
  // parentless QTermWidget while a Wayland event loop is live leaves its
  // ScreenWindow detached from the subsequently reparented display: bytes
  // reach emulation and the cursor paints, but every glyph stays invisible.
  m_widget = new QTermWidget(0, nullptr);
  m_widget->setAttribute(Qt::WA_StyledBackground, false);
  m_widget->installEventFilter(this);
  if (m_widget->focusProxy() != nullptr) {
    m_widget->focusProxy()->installEventFilter(this);
  }
  initializeSearchSurface();
  // AGENT-GUARD: BEL also terminates OSC title/control sequences. Never
  // remove it from PTY bytes: doing so swallows interactive shell prompts.
  // qtermwidget emits parsed bell notifications; sound policy belongs here.
  connect(m_widget, &QTermWidget::bell, this, [this](const QString &) {
    if (m_profile.bellPolicy == TerminalProfile::BellPolicy::Audible)
      QApplication::beep();
  });

  connect(m_widget, &QTermWidget::sendData, this,
          [this](const char *data, int length) {
            if (m_bridge != nullptr) {
              m_bridge->writeInput(data, length);
            }
          });
  connect(m_widget, &QTermWidget::copyAvailable, this,
          &TerminalSessionBackend::selectionChanged);
  connect(m_widget, &QTermWidget::titleChanged, this, [this] {
    emit titleChanged(m_widget != nullptr ? m_widget->title() : QString());
  });
  // QTermWidget::finished() is deliberately not connected: the widget owns no
  // child in teletype mode, so the session's ProcessMonitor reap is the only
  // exit authority (AGENT-CONTRACT in terminal_session_backend.h).

  applyAppearance();
}

QWidget *TerminalWidgetAdapter::terminalWidget() { return m_widget; }

TerminalWidgetAdapter::~TerminalWidgetAdapter() {
  closeChildChannel();
  if (m_widget != nullptr) {
    // Widget disposal stops the emulator/scrollback side; the teardown
    // SIGHUP itself comes from the bridge master close above.
    delete m_widget;
    m_widget = nullptr;
  }
  if (!m_schemePath.isEmpty()) {
    QFile::remove(m_schemePath);
  }
}

bool TerminalWidgetAdapter::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_widget && event->type() == QEvent::Resize) {
    // AGENT-NOTE (ADR-0040): the child lives on the bridge PTY, so its
    // winsize is applied explicitly. The filter sees the resize before the
    // display relayouts; a zero-delay pass reads the settled emulator grid
    // and programs TIOCSWINSZ (the kernel then raises SIGWINCH).
    QTimer::singleShot(0, this, [this] {
      if (m_bridge != nullptr && m_bridge->isOpen() && m_widget != nullptr) {
        m_bridge->setChildWindowSize(m_widget->screenColumnsCount(),
                                     m_widget->screenLinesCount());
      }
    });
  }
  const bool watchedTerminalSurface =
      watched == m_widget ||
      (m_widget != nullptr && watched == m_widget->focusProxy());
  if (watchedTerminalSurface && event->type() == QEvent::ContextMenu) {
    const auto *contextEvent = static_cast<QContextMenuEvent *>(event);
    if (!refreshVisibleLinks().isEmpty()) {
      emit linkContextRequested(contextEvent->globalPos());
      return true;
    }
  }
  return TerminalSessionBackend::eventFilter(watched, event);
}

TerminalSessionBackend::StartOutcome
TerminalWidgetAdapter::start(const TerminalLaunchRequest &request) {
  if (m_shutdownRequested) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Session is shutting down")};
  }
  if (m_childPid != 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Session backend is single-use")};
  }
  if (m_bridge == nullptr && !initializeChannels()) {
    return {.ok = false,
            .diagnostic = !m_transportDiagnostic.isEmpty()
                              ? m_transportDiagnostic
                              : m_bridgeDiagnostic};
  }
  if (m_bridge == nullptr || !m_bridge->isOpen() || m_widgetSlaveFd < 0) {
    return {.ok = false,
            .diagnostic =
                m_bridgeDiagnostic.isEmpty()
                    ? QStringLiteral("Terminal channel is unavailable")
                    : m_bridgeDiagnostic};
  }
  if (!m_transportDiagnostic.isEmpty()) {
    // Fail-closed byte-transparency gate (P2: double line discipline): a
    // transforming transport would corrupt exact child output bytes.
    return {.ok = false, .diagnostic = m_transportDiagnostic};
  }

  // The production collection attaches this widget immediately before
  // start(). Deliver its actual attached geometry to qtermwidget's internal
  // TerminalDisplay synchronously; otherwise a fast prompt is parsed on the
  // constructor grid and disappears when the queued first resize arrives.
  QCoreApplication::sendPostedEvents(m_widget, QEvent::Resize);
  QResizeEvent attachedResize(m_widget->size(), m_widget->size());
  QCoreApplication::sendEvent(m_widget, &attachedResize);

  // AGENT-NOTE: All byte conversion and pointer-array construction happens
  // before fork so the child's pre-exec path stays allocation-free (P2-2).
  // toLocal8Bit preserves filesystem/locale byte semantics for execve; the
  // child environment's effective locale is forced to UTF-8 by the launch
  // policy.
  ExecStrings strings;
  strings.program = request.program.toLocal8Bit();
  strings.arguments.reserve(static_cast<size_t>(request.arguments.size()));
  for (const QString &argument : request.arguments) {
    strings.arguments.push_back(argument.toLocal8Bit());
  }
  strings.environment.reserve(static_cast<size_t>(request.environment.size()));
  for (const QString &entry : request.environment) {
    strings.environment.push_back(entry.toLocal8Bit());
  }
  strings.workingDirectory = request.workingDirectory.toLocal8Bit();
  // AGENT-GUARD (P1 strict compile): argv/envp alias the QByteArray buffers,
  // each of which has refcount 1 right after construction. Never append to
  // arguments/environment after the pointer arrays are built, and never copy
  // ExecStrings (a copy shares the buffers, and a later non-const data()
  // would detach and dangle); execChildInBridge takes a const reference, so
  // no copy exists. The loops iterate non-const QByteArray on purpose: the
  // non-const data() is the honest char* without a const_cast.
  strings.argv.push_back(strings.program.data());
  for (QByteArray &argument : strings.arguments) {
    strings.argv.push_back(argument.data());
  }
  strings.argv.push_back(nullptr);
  for (QByteArray &entry : strings.environment) {
    strings.envp.push_back(entry.data());
  }
  strings.envp.push_back(nullptr);

  const QByteArray slavePathUtf8 = m_slavePath.toUtf8();
  const pid_t pid = ::fork();
  if (pid < 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Cannot create the terminal child")};
  }
  if (pid == 0) {
    execChildInBridge(slavePathUtf8.constData(), strings);
  }

  m_childPid = static_cast<ProcessId>(pid);
  return {.ok = true, .diagnostic = {}};
}

void TerminalWidgetAdapter::requestShutdown() {
  if (m_shutdownRequested) {
    return;
  }
  m_shutdownRequested = true;
  closeChildChannel();
  if (m_widget != nullptr) {
    delete m_widget;
    m_widget = nullptr;
  }
}

void TerminalWidgetAdapter::copySelectionToClipboard() {
  if (m_widget != nullptr) {
    m_widget->copyClipboard();
  }
}

void TerminalWidgetAdapter::pasteClipboardToSession() {
  if (m_widget != nullptr) {
    m_widget->pasteClipboard();
  }
}

void TerminalWidgetAdapter::pastePrimarySelectionToSession() {
  if (m_widget != nullptr) {
    m_widget->pasteSelection();
  }
}

void TerminalWidgetAdapter::selectAllInView() {
  if (m_widget == nullptr) {
    return;
  }
  // AGENT-GUARD (P1-4): qtermwidget rows are zero-based and it does not clamp
  // the end row, so the end row must be the last valid row
  // (history + screen - 1); the columns value deliberately keeps upstream's
  // one-past-column convention. Availability is the adapter's real
  // hasSelectedText() answer (P2: a blank buffer must not enable Copy),
  // never an unconditional true.
  const int lastValidRow =
      m_widget->historyLinesCount() + m_widget->screenLinesCount() - 1;
  m_widget->setSelectionStart(0, 0);
  if (lastValidRow >= 0) {
    m_widget->setSelectionEnd(lastValidRow, m_widget->screenColumnsCount());
  }
  emit selectionChanged(hasSelectedText());
}

void TerminalWidgetAdapter::clearView() {
  if (m_widget != nullptr) {
    m_widget->clear();
  }
}

bool TerminalWidgetAdapter::hasSelectedText() const {
  if (m_widget == nullptr) {
    return false;
  }
  const QString selection =
      const_cast<QTermWidget *>(m_widget)->selectedText(false);
  return hasSemanticSelection(selection);
}

void TerminalWidgetAdapter::sendTextToSession(const QString &text) {
  const QByteArray bytes = text.toUtf8();
  if (m_bridge != nullptr) {
    m_bridge->writeInput(bytes.constData(), static_cast<int>(bytes.size()));
  }
}

} // namespace QindaQt::Apps::Terminal
