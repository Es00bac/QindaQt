// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/pty_bridge.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <utility>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-NOTE: Shared bound for both bridge buffers. Sustained backpressure
// drops newest bytes instead of blocking the GUI thread or growing without
// limit; the wiki records this degradation honestly.
constexpr qsizetype kMaxBridgeBufferBytes = 64 * 1024;

} // namespace

TerminalPtyBridge::TerminalPtyBridge(OutputSink sink, QObject *parent)
    : QObject(parent), m_sink(std::move(sink)) {}

TerminalPtyBridge::~TerminalPtyBridge() { closeChildChannel(); }

TerminalPtyBridge::OpenResult TerminalPtyBridge::open() {
  if (m_masterFd >= 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Bridge is already open"),
            .slavePath = {}};
  }
  // AGENT-GUARD: O_CLOEXEC keeps the master out of every child; the slave is
  // deliberately NOT opened here. The child opens the slave path itself, so
  // no descriptor sharing (and no shared O_NONBLOCK flag) can reach it.
  const int master = ::posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (master < 0) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Cannot open a pseudo-terminal"),
            .slavePath = {}};
  }
  if (::grantpt(master) == -1 || ::unlockpt(master) == -1) {
    const int savedErrno = errno;
    ::close(master);
    errno = savedErrno;
    return {.ok = false,
            .diagnostic = QStringLiteral("Cannot unlock a pseudo-terminal"),
            .slavePath = {}};
  }
  char slaveName[128];
  if (::ptsname_r(master, slaveName, sizeof(slaveName)) != 0) {
    const int savedErrno = errno;
    ::close(master);
    errno = savedErrno;
    return {.ok = false,
            .diagnostic = QStringLiteral("Cannot resolve the pseudo-terminal "
                                         "slave path"),
            .slavePath = {}};
  }

  m_masterFd = master;
  m_childOutputClosed = false;
  const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
  if (flags >= 0) {
    // Master-only nonblocking mode: the child never references this open
    // file description, so its stdio stays blocking.
    ::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);
  }

  m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read,
                                       this);
  connect(m_readNotifier, &QSocketNotifier::activated, this,
          [this] { pumpMasterToSink(); });
  m_writeNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Write,
                                        this);
  m_writeNotifier->setEnabled(false);
  connect(m_writeNotifier, &QSocketNotifier::activated, this,
          [this] { flushInput(); });

  return {.ok = true,
          .diagnostic = {},
          .slavePath = QString::fromLatin1(slaveName)};
}

void TerminalPtyBridge::writeInput(const char *data, int length) {
  if (data == nullptr || length <= 0 || m_masterFd < 0) {
    return;
  }
  appendBounded(m_inputBuffer, data, length);
  flushInput();
}

void TerminalPtyBridge::setChildWindowSize(int columns, int rows) {
  if (m_masterFd < 0 || columns <= 0 || rows <= 0) {
    return;
  }
  winsize size{};
  size.ws_col = static_cast<unsigned short>(
      qBound(1, columns, static_cast<int>(0xFFFF)));
  size.ws_row = static_cast<unsigned short>(
      qBound(1, rows, static_cast<int>(0xFFFF)));
  ::ioctl(m_masterFd, TIOCSWINSZ, &size);
}

void TerminalPtyBridge::closeChildChannel() {
  delete m_readNotifier;
  m_readNotifier = nullptr;
  delete m_writeNotifier;
  m_writeNotifier = nullptr;
  m_inputBuffer.clear();
  if (m_masterFd >= 0) {
    // Closing the last master makes the kernel deliver SIGHUP to the child
    // session's foreground process group; bounded escalation stays with the
    // session layer.
    ::close(m_masterFd);
    m_masterFd = -1;
  }
}

void TerminalPtyBridge::appendBounded(QByteArray &buffer, const char *data,
                                      int length) {
  if (buffer.size() >= kMaxBridgeBufferBytes) {
    return;
  }
  const qsizetype room = kMaxBridgeBufferBytes - buffer.size();
  buffer.append(data, qMin<qsizetype>(length, room));
}

void TerminalPtyBridge::flushInput() {
  while (!m_inputBuffer.isEmpty() && m_masterFd >= 0) {
    const ssize_t written =
        ::write(m_masterFd, m_inputBuffer.constData(),
                static_cast<size_t>(m_inputBuffer.size()));
    if (written > 0) {
      m_inputBuffer.remove(0, written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (m_writeNotifier != nullptr) {
        m_writeNotifier->setEnabled(true);
      }
      return;
    }
    // Master gone or unusable: drop pending keystrokes; exit truth is the
    // session reap's job.
    m_inputBuffer.clear();
    break;
  }
  if (m_writeNotifier != nullptr) {
    m_writeNotifier->setEnabled(false);
  }
}

void TerminalPtyBridge::pumpMasterToSink() {
  if (m_masterFd < 0 || m_childOutputClosed) {
    return;
  }
  char chunk[8192];
  for (;;) {
    const ssize_t received =
        ::read(m_masterFd, chunk, sizeof(chunk));
    if (received > 0) {
      if (m_sink) {
        m_sink(chunk, static_cast<int>(received));
      }
      if (received < static_cast<ssize_t>(sizeof(chunk))) {
        return;
      }
      continue;
    }
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return; // Drained; the notifier stays armed for live output.
    }
    // AGENT-GUARD (P1: EIO hot loop): 0 (EOF) or any other errno is
    // terminal for this generation. Linux reports EIO once the last slave
    // descriptor is gone and keeps the master POLLHUP-readable forever, so
    // leaving the notifier enabled here spins the GUI thread. The master is
    // deliberately NOT closed: closeChildChannel() is its only owner and the
    // teardown SIGHUP path, and exit truth stays with the session's
    // ProcessMonitor reap — this quiescence is not an exit publication.
    m_childOutputClosed = true;
    if (m_readNotifier != nullptr) {
      m_readNotifier->setEnabled(false);
    }
    return;
  }
}

} // namespace QindaQt::Apps::Terminal
