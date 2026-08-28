// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "session/pty_bridge.h"
#include "ui/terminal_appearance.h"

#include <qtermwidget.h>

#include <QDir>
#include <QFile>
#include <QEvent>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>
#include <utility>
#include <vector>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-NOTE: Buffers toward the rendering widget are bounded (64 KiB,
// drop-newest) so sustained backpressure cannot block the GUI thread or grow
// without limit; the wiki records this degradation.
constexpr qsizetype kMaxWidgetOutputBufferBytes = 64 * 1024;

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
    const ssize_t chunk =
        ::write(STDERR_FILENO, message + written,
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
  if (::dup2(slave, STDIN_FILENO) == -1 ||
      ::dup2(slave, STDOUT_FILENO) == -1 ||
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
    const TerminalViewAppearance &appearance, QObject *parent)
    : TerminalSessionBackend(parent), m_appearance(appearance) {
  // AGENT-NOTE: startnow is deliberately 0 and setShellProgram/setArgs are
  // never used: the widget must not spawn its own child (ADR-0040). The
  // child runs on the adapter's own bridge PTY; the widget's teletype slave
  // receives child output for rendering only.
  m_widget = new QTermWidget(0, nullptr);
  m_widget->setAttribute(Qt::WA_StyledBackground, false);
  m_widget->startTerminalTeletype();
  m_widget->installEventFilter(this);

  // Private output channel into the widget's teletype slave: slave write ->
  // widget master read -> emulator. O_NONBLOCK here is safe because this
  // descriptor is never inherited by the child (ADR-0040).
  const int widgetSlaveFd = m_widget->getPtySlaveFd();
  if (widgetSlaveFd >= 0) {
    m_widgetSlaveFd = ::dup(widgetSlaveFd);
    if (m_widgetSlaveFd >= 0) {
      ::fcntl(m_widgetSlaveFd, F_SETFD, FD_CLOEXEC);
      const int flags = ::fcntl(m_widgetSlaveFd, F_GETFL, 0);
      if (flags >= 0) {
        ::fcntl(m_widgetSlaveFd, F_SETFL, flags | O_NONBLOCK);
      }
      makeWidgetTransportByteTransparent();
      m_widgetOutputNotifier =
          new QSocketNotifier(m_widgetSlaveFd, QSocketNotifier::Write, this);
      m_widgetOutputNotifier->setEnabled(false);
      connect(m_widgetOutputNotifier, &QSocketNotifier::activated, this,
              [this] { flushChildOutputToWidget(); });
    }
  }

  // The child's own PTY: keyboard/paste master-writes reach the child as
  // input; child output and echo are pumped from the bridge master into the
  // widget channel above.
  m_bridge = new TerminalPtyBridge(
      [this](const char *data, int length) {
        forwardChildOutput(data, length);
      },
      this);
  const auto opened = m_bridge->open();
  m_bridgeDiagnostic = opened.diagnostic;
  m_slavePath = opened.slavePath;

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

void TerminalWidgetAdapter::makeWidgetTransportByteTransparent() {
  // AGENT-CONTRACT (P2: double line discipline, ADR-0040): the bytes this
  // adapter writes into the widget's teletype slave are already
  // line-disciplined child output from the bridge PTY. qtermwidget opens its
  // PTY with default termios, whose OPOST/ONLCR would transform that output
  // a second time (LF -> CRLF), so the transport must be byte-transparent.
  // Fail-closed: when the output processing cannot be cleared AND the
  // clearing verified, the transport stays unusable and start() refuses with
  // a typed diagnostic instead of silently rendering mutated bytes.
  termios settings{};
  if (::tcgetattr(m_widgetSlaveFd, &settings) != 0) {
    m_transportDiagnostic = QStringLiteral(
        "Cannot read the rendering teletype settings");
    return;
  }
  // OPOST off disables every output translation; the cast keeps the bitwise
  // complement unsigned so -Wsign-conversion stays clean.
  settings.c_oflag &= static_cast<tcflag_t>(~OPOST);
  if (::tcsetattr(m_widgetSlaveFd, TCSANOW, &settings) != 0) {
    m_transportDiagnostic = QStringLiteral(
        "Cannot clear rendering teletype output processing");
    return;
  }
  termios verified{};
  if (::tcgetattr(m_widgetSlaveFd, &verified) != 0 ||
      (verified.c_oflag & OPOST) != 0) {
    m_transportDiagnostic = QStringLiteral(
        "Rendering teletype output processing could not be disabled");
  }
}

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

void TerminalWidgetAdapter::applyAppearance() {
  if (m_widget == nullptr) {
    return;
  }
  // AGENT-NOTE (P3-3): the scheme document is per-instance (pid + counter)
  // and installed atomically. The temporary file is created with NewOnly —
  // an exclusive create that fails instead of truncating through a
  // pre-planted symlink — and the install removes a crash-stale target
  // (possible after PID reuse) before renaming, so a stale file can never
  // make the install fail or race its contents.
  const QString cacheDirectory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (!cacheDirectory.isEmpty() && QDir().mkpath(cacheDirectory)) {
    static int instanceCounter = 0;
    // AGENT-CONTRACT (qtermwidget 2.4): setColorScheme(path) attempts a
    // custom-file load only when the final path ends in `.colorscheme`.
    // Another extension silently selects its built-in white default even
    // when the file exists and contains a valid Konsole document.
    const QString baseName =
        QStringLiteral("qindaqt-terminal-scheme-%1-%2.colorscheme")
            .arg(::getpid())
            .arg(++instanceCounter);
    const QString targetPath = QDir(cacheDirectory).filePath(baseName);
    const QString temporaryPath = targetPath + QStringLiteral(".tmp");
    {
      QFile schemeFile(temporaryPath);
      if (schemeFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        const QByteArray document =
            TerminalColorSchemeDocument::render(m_appearance).toUtf8();
        if (schemeFile.write(document) == document.size() &&
            schemeFile.flush()) {
          schemeFile.close();
          // Replace-safe install (P3-3): a stale target can only exist after
          // PID reuse from a crashed instance of this same pid/counter name,
          // so removing it first is safe; QFile::rename does not overwrite.
          if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
          }
          if (QFile::rename(temporaryPath, targetPath)) {
            m_schemePath = targetPath;
            m_widget->setColorScheme(m_schemePath);
          }
        }
      }
    }
    QFile::remove(temporaryPath);
  }
  // Font and window palette come from the same QST generation even when the
  // scheme file failed; the widget then keeps its built-in scheme, which is
  // a visible but non-fatal degradation recorded in the wiki.
  m_widget->setTerminalFont(m_appearance.terminalFont);
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
  return TerminalSessionBackend::eventFilter(watched, event);
}

TerminalSessionBackend::StartOutcome TerminalWidgetAdapter::start(
    const TerminalLaunchRequest &request) {
  if (m_shutdownRequested) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Session is shutting down")};
  }
  if (m_childPid != 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Session backend is single-use")};
  }
  if (m_bridge == nullptr || !m_bridge->isOpen() || m_widgetSlaveFd < 0) {
    return {.ok = false,
            .diagnostic = m_bridgeDiagnostic.isEmpty()
                              ? QStringLiteral(
                                    "Terminal channel is unavailable")
                              : m_bridgeDiagnostic};
  }
  if (!m_transportDiagnostic.isEmpty()) {
    // Fail-closed byte-transparency gate (P2: double line discipline): a
    // transforming transport would corrupt exact child output bytes.
    return {.ok = false, .diagnostic = m_transportDiagnostic};
  }

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
  strings.environment.reserve(
      static_cast<size_t>(request.environment.size()));
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

void TerminalWidgetAdapter::closeChildChannel() {
  if (m_bridge != nullptr) {
    // Master close delivers SIGHUP to the child session; the session layer
    // adds bounded process-group escalation on top of this.
    m_bridge->closeChildChannel();
  }
  if (m_widgetOutputNotifier != nullptr) {
    m_widgetOutputNotifier->setEnabled(false);
    m_widgetOutputNotifier->deleteLater();
    m_widgetOutputNotifier = nullptr;
  }
  m_widgetOutputBuffer.clear();
  if (m_widgetSlaveFd >= 0) {
    ::close(m_widgetSlaveFd);
    m_widgetSlaveFd = -1;
  }
}

void TerminalWidgetAdapter::forwardChildOutput(const char *data,
                                               int length) {
  if (data == nullptr || length <= 0 || m_widgetSlaveFd < 0) {
    return;
  }
  if (m_widgetOutputBuffer.size() >= kMaxWidgetOutputBufferBytes) {
    return;
  }
  const qsizetype room =
      kMaxWidgetOutputBufferBytes - m_widgetOutputBuffer.size();
  m_widgetOutputBuffer.append(data, qMin<qsizetype>(length, room));
  flushChildOutputToWidget();
}

void TerminalWidgetAdapter::flushChildOutputToWidget() {
  while (!m_widgetOutputBuffer.isEmpty() && m_widgetSlaveFd >= 0) {
    const ssize_t written =
        ::write(m_widgetSlaveFd, m_widgetOutputBuffer.constData(),
                static_cast<size_t>(m_widgetOutputBuffer.size()));
    if (written > 0) {
      m_widgetOutputBuffer.remove(0, written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (m_widgetOutputNotifier != nullptr) {
        m_widgetOutputNotifier->setEnabled(true);
      }
      return;
    }
    m_widgetOutputBuffer.clear();
    return;
  }
  if (m_widgetOutputNotifier != nullptr) {
    m_widgetOutputNotifier->setEnabled(false);
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
