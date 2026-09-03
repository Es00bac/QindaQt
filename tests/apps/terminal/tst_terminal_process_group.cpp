// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_session.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <memory>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace QindaQt::Apps::Terminal;

namespace {

bool processExists(pid_t pid) {
  if (pid <= 0) {
    return false;
  }
  errno = 0;
  return ::kill(pid, 0) == 0 || errno == EPERM;
}

// AGENT-GUARD: This reproduction deliberately creates a hostile descendant.
// Every return path runs this cleanup, including assertion-style failures and
// the event-loop timeout, so the registered test cannot leak its fixture.
class FixtureCleanup final {
public:
  ~FixtureCleanup() {
    if (group > 0) {
      static_cast<void>(::killpg(group, SIGKILL));
    }
    if (descendant > 0) {
      static_cast<void>(::kill(descendant, SIGKILL));
    }
    if (leader > 0) {
      static_cast<void>(::kill(leader, SIGKILL));
    }
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 1000) {
      if (leader > 0) {
        int status = 0;
        const pid_t reaped = ::waitpid(leader, &status, WNOHANG);
        if (reaped == leader || (reaped < 0 && errno == ECHILD)) {
          leader = 0;
        }
      }
      if (leader == 0 && !processExists(descendant)) {
        break;
      }
      QThread::msleep(10);
    }
  }

  void disarm() {
    leader = 0;
    descendant = 0;
    group = 0;
  }

  pid_t leader = 0;
  pid_t descendant = 0;
  pid_t group = 0;
};

class ProcessGroupBackend final : public TerminalSessionBackend {
public:
  explicit ProcessGroupBackend(FixtureCleanup &cleanup) : m_cleanup(cleanup) {}

  StartOutcome start(const TerminalLaunchRequest &) override {
    int descendantPipe[2] = {-1, -1};
    if (::pipe(descendantPipe) != 0) {
      return {false, QStringLiteral("pipe failed")};
    }
    const pid_t leader = ::fork();
    if (leader < 0) {
      ::close(descendantPipe[0]);
      ::close(descendantPipe[1]);
      return {false, QStringLiteral("fork failed")};
    }
    if (leader == 0) {
      ::close(descendantPipe[0]);
      if (::setsid() < 0) {
        ::_exit(120);
      }
      const pid_t descendant = ::fork();
      if (descendant < 0) {
        ::_exit(121);
      }
      if (descendant == 0) {
        ::signal(SIGHUP, SIG_IGN);
        ::signal(SIGTERM, SIG_IGN);
        const pid_t me = ::getpid();
        const ssize_t writeSize =
            ::write(descendantPipe[1], &me, sizeof(descendant));
        if (writeSize != static_cast<ssize_t>(sizeof(descendant))) {
          ::_exit(122);
        }
        for (;;) {
          ::pause();
        }
      }
      ::close(descendantPipe[1]);
      for (;;) {
        ::pause();
      }
    }

    ::close(descendantPipe[1]);
    m_pid = leader;
    m_cleanup.leader = leader;
    m_cleanup.group = leader;
    const ssize_t readSize = ::read(descendantPipe[0], &m_cleanup.descendant,
                                    sizeof(m_cleanup.descendant));
    ::close(descendantPipe[0]);
    if (readSize != static_cast<ssize_t>(sizeof(m_cleanup.descendant))) {
      return {false, QStringLiteral("descendant handshake failed")};
    }
    return {true, {}};
  }

  void requestShutdown() override {
    if (m_pid > 0) {
      static_cast<void>(::killpg(m_pid, SIGHUP));
    }
  }
  ProcessId shellProcessId() const override { return m_pid; }
  QWidget *terminalWidget() override { return nullptr; }
  void copySelectionToClipboard() override {}
  void pasteClipboardToSession() override {}
  void pastePrimarySelectionToSession() override {}
  void selectAllInView() override {}
  void clearView() override {}
  bool hasSelectedText() const override { return false; }
  void sendTextToSession(const QString &) override {}

private:
  FixtureCleanup &m_cleanup;
  pid_t m_pid = 0;
};

} // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  FixtureCleanup cleanup;
  PosixProcessMonitor monitor;
  TerminalSession session(
      [&cleanup](const TerminalProfile &) {
        return std::make_unique<ProcessGroupBackend>(cleanup);
      },
      &monitor, TeardownBounds{100, 100, 1000, 10});
  TerminalLaunchRequest request;
  request.program = QStringLiteral("/bin/true");
  if (!session.start(request)) {
    return 2;
  }

  // Reap the leader first while its HUP/TERM-immune descendant keeps the
  // process group alive. start() must not overwrite that retained ownership.
  static_cast<void>(::kill(cleanup.leader, SIGHUP));
  QElapsedTimer reapDeadline;
  reapDeadline.start();
  while (session.state() == TerminalSession::State::Running &&
         reapDeadline.elapsed() < 1000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(5);
  }
  if (session.state() != TerminalSession::State::Exited ||
      !processExists(cleanup.descendant) || session.start(request)) {
    return 3;
  }

  bool finished = false;
  bool clean = false;
  QObject::connect(&session, &TerminalSession::shutdownFinished, &application,
                   [&](bool result, const QString &) {
                     finished = true;
                     clean = result;
                     application.quit();
                   });
  session.beginShutdown();
  QTimer::singleShot(4000, &application, &QCoreApplication::quit);
  application.exec();

  const bool descendantAlive = processExists(cleanup.descendant);
  const bool passed =
      finished && clean &&
      session.state() == TerminalSession::State::ShutdownComplete &&
      !descendantAlive;
  if (passed) {
    cleanup.disarm();
  }
  return passed ? 0 : 1;
}
