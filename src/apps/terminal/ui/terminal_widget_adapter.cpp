// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "ui/terminal_appearance.h"

#include <qtermwidget.h>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <vector>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-NOTE: Keyboard bytes are buffered between emulator bursts and a child
// that is not reading (flow control, a stopped pager). The cap keeps the GUI
// thread responsive and memory bounded; overflow drops newest bytes, which is
// the honest degradation for keystrokes and is recorded in the wiki.
constexpr qsizetype kMaxKeyboardBufferBytes = 64 * 1024;

// Close every descriptor the child could inherit above stderr, including the
// PTY master held by the widget and Qt-internal pipes. The loop bound covers
// realistic descriptor ceilings without consulting rlimits after fork.
constexpr int kChildFdScanLimit = 4096;

struct ExecStrings {
  QByteArray program;
  std::vector<QByteArray> arguments;
  std::vector<QByteArray> environment;
  QByteArray workingDirectory;
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

void resetSignalDispositions() {
  struct sigaction defaults;
  std::memset(&defaults, 0, sizeof(defaults));
  sigemptyset(&defaults.sa_mask);
  defaults.sa_handler = SIG_DFL;
  for (int signalNumber = 1; signalNumber < NSIG; ++signalNumber) {
    sigaction(signalNumber, &defaults, nullptr);
  }
}

// Runs only in the forked child and must stay async-signal-safe until
// execve: no Qt, no allocation, no errno-dependent formatting. Diagnostics
// are fixed literals because formatting functions are not as-safe.
[[noreturn]] void execChild(const ExecStrings &strings, int slaveFd) {
  if (::setsid() == -1) {
    ::_exit(126);
  }
  if (::ioctl(slaveFd, TIOCSCTTY, nullptr) == -1) {
    writeAllToStderr("qindaqt-terminal: cannot set controlling terminal\n");
    ::_exit(126);
  }
  ::dup2(slaveFd, STDIN_FILENO);
  ::dup2(slaveFd, STDOUT_FILENO);
  ::dup2(slaveFd, STDERR_FILENO);
  if (slaveFd > STDERR_FILENO) {
    ::close(slaveFd);
  }
  for (int fd = STDERR_FILENO + 1; fd < kChildFdScanLimit; ++fd) {
    ::close(fd);
  }
  resetSignalDispositions();
  if (!strings.workingDirectory.isEmpty() &&
      ::chdir(strings.workingDirectory.constData()) == -1) {
    writeAllToStderr("qindaqt-terminal: cannot enter working directory\n");
    ::_exit(126);
  }

  std::vector<char *> argv;
  argv.reserve(strings.arguments.size() + 2);
  argv.push_back(const_cast<char *>(strings.program.constData()));
  for (const QByteArray &argument : strings.arguments) {
    argv.push_back(const_cast<char *>(argument.constData()));
  }
  argv.push_back(nullptr);

  std::vector<char *> environment;
  environment.reserve(strings.environment.size() + 1);
  for (const QByteArray &entry : strings.environment) {
    environment.push_back(const_cast<char *>(entry.constData()));
  }
  environment.push_back(nullptr);

  ::execve(strings.program.constData(), argv.data(), environment.data());
  writeAllToStderr("qindaqt-terminal: cannot run the configured shell\n");
  ::_exit(127);
}

} // namespace

TerminalWidgetAdapter::TerminalWidgetAdapter(
    const TerminalViewAppearance &appearance, QObject *parent)
    : TerminalSessionBackend(parent), m_appearance(appearance) {
  // AGENT-NOTE: startnow is deliberately 0 and setShellProgram/setArgs are
  // never used: the widget must not spawn its own child (ADR-0028); this
  // adapter owns fork/execve so exit codes and process-group teardown stay
  // in QindaQt hands.
  m_widget = new QTermWidget(0, nullptr);
  m_widget->setAttribute(Qt::WA_StyledBackground, false);
  m_widget->startTerminalTeletype();

  const int widgetSlaveFd = m_widget->getPtySlaveFd();
  if (widgetSlaveFd >= 0) {
    m_slaveFd = ::dup(widgetSlaveFd);
    if (m_slaveFd >= 0) {
      ::fcntl(m_slaveFd, F_SETFD, FD_CLOEXEC);
      const int flags = ::fcntl(m_slaveFd, F_GETFL, 0);
      if (flags >= 0) {
        ::fcntl(m_slaveFd, F_SETFL, flags | O_NONBLOCK);
      }
      m_slaveWriteNotifier =
          new QSocketNotifier(m_slaveFd, QSocketNotifier::Write, this);
      m_slaveWriteNotifier->setEnabled(false);
      connect(m_slaveWriteNotifier, &QSocketNotifier::activated, this,
              [this] { flushKeyboardBuffer(); });
    }
  }

  connect(m_widget, &QTermWidget::sendData, this,
          [this](const char *data, int length) {
            forwardKeyboardBytes(data, length);
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

TerminalWidgetAdapter::~TerminalWidgetAdapter() {
  closeChildChannel();
  if (m_widget != nullptr) {
    // Closing the master is the SIGHUP teardown path; the session layer adds
    // bounded process-group escalation on top of this.
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
  const QString cacheDirectory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (!cacheDirectory.isEmpty() &&
      QDir().mkpath(cacheDirectory)) {
    QFile schemeFile(
        QDir(cacheDirectory).filePath(QStringLiteral("terminal-scheme.ini")));
    if (schemeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      const QByteArray document = TerminalColorSchemeDocument::render(
                                          m_appearance)
                                      .toUtf8();
      if (schemeFile.write(document) == document.size()) {
        m_schemePath = schemeFile.fileName();
        m_widget->setColorScheme(m_schemePath);
      }
    }
  }
  // Font and window palette come from the same QST generation even when the
  // scheme file failed; the widget then keeps its built-in scheme, which is a
  // visible but non-fatal degradation recorded in the wiki.
  m_widget->setTerminalFont(m_appearance.terminalFont);
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
  if (m_slaveFd < 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Terminal channel is unavailable")};
  }

  // AGENT-NOTE: Byte conversion happens before fork so the child's pre-exec
  // path stays allocation-free. toLocal8Bit preserves filesystem/locale byte
  // semantics for execve; the child environment itself was already forced to
  // a UTF-8 locale by the launch policy.
  ExecStrings strings;
  strings.program = request.program.toLocal8Bit();
  strings.arguments.reserve(request.arguments.size());
  for (const QString &argument : request.arguments) {
    strings.arguments.push_back(argument.toLocal8Bit());
  }
  strings.environment.reserve(request.environment.size());
  for (const QString &entry : request.environment) {
    strings.environment.push_back(entry.toLocal8Bit());
  }
  strings.workingDirectory = request.workingDirectory.toLocal8Bit();

  const pid_t pid = ::fork();
  if (pid < 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Cannot create the terminal child")};
  }
  if (pid == 0) {
    execChild(strings, m_slaveFd);
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
  if (m_slaveWriteNotifier != nullptr) {
    m_slaveWriteNotifier->setEnabled(false);
    m_slaveWriteNotifier->deleteLater();
    m_slaveWriteNotifier = nullptr;
  }
  m_keyboardBuffer.clear();
  if (m_slaveFd >= 0) {
    ::close(m_slaveFd);
    m_slaveFd = -1;
  }
}

void TerminalWidgetAdapter::forwardKeyboardBytes(const char *data,
                                                 int length) {
  if (data == nullptr || length <= 0 || m_slaveFd < 0) {
    return;
  }
  if (m_keyboardBuffer.size() >= kMaxKeyboardBufferBytes) {
    // Sustained backpressure drops newest bytes instead of blocking the GUI
    // thread or growing unbounded (bounded-buffer contract in the wiki).
    return;
  }
  const qsizetype room = kMaxKeyboardBufferBytes - m_keyboardBuffer.size();
  m_keyboardBuffer.append(data, qMin<qsizetype>(length, room));
  flushKeyboardBuffer();
}

void TerminalWidgetAdapter::flushKeyboardBuffer() {
  while (!m_keyboardBuffer.isEmpty() && m_slaveFd >= 0) {
    const ssize_t written = ::write(
        m_slaveFd, m_keyboardBuffer.constData(),
        static_cast<size_t>(m_keyboardBuffer.size()));
    if (written > 0) {
      m_keyboardBuffer.remove(0, written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      // Signal interruption is not channel state; retry the same bytes.
      continue;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (m_slaveWriteNotifier != nullptr) {
        m_slaveWriteNotifier->setEnabled(true);
      }
      return;
    }
    // Channel is broken (child gone and channel closed): drop pending bytes
    // rather than looping; exit detection is the reap's job.
    m_keyboardBuffer.clear();
    return;
  }
  if (m_slaveWriteNotifier != nullptr) {
    m_slaveWriteNotifier->setEnabled(false);
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
  // Public extent-based selection: from the first history column to the last
  // screen column. qtermwidget 2.4 exposes no simpler whole-buffer call.
  const int rows =
      m_widget->historyLinesCount() + m_widget->screenLinesCount();
  m_widget->setSelectionStart(0, 0);
  m_widget->setSelectionEnd(rows, m_widget->screenColumnsCount());
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
  return !selection.isEmpty();
}

void TerminalWidgetAdapter::sendTextToSession(const QString &text) {
  const QByteArray bytes = text.toUtf8();
  forwardKeyboardBytes(bytes.constData(), static_cast<int>(bytes.size()));
}

} // namespace QindaQt::Apps::Terminal
