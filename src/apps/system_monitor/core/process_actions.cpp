// SPDX-License-Identifier: LGPL-3.0-or-later

#include "process_actions.h"

#include "procfs_reader.h"

#include <cerrno>
#include <cstring>
#include <limits>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace QindaQt::SystemMonitor {
namespace {

QString systemError(const QString &operation, qint64 pid, int errorNumber) {
  return QStringLiteral("%1 process %2 failed: %3")
      .arg(operation, QString::number(pid),
           QString::fromLocal8Bit(std::strerror(errorNumber)));
}

QString verifyIdentity(const QString &procRoot, qint64 pid,
                       quint64 startTicks) {
  QString readError;
  const auto current = ProcfsReader(procRoot).readProcess(pid, &readError);
  if (!current) {
    return readError;
  }
  if (current->startTicks != startTicks) {
    return QStringLiteral(
               "Process %1 identity changed (expected start tick %2, found %3)")
        .arg(pid)
        .arg(startTicks)
        .arg(current->startTicks);
  }
  return {};
}

int openPidFileDescriptor(qint64 pid) {
#ifdef SYS_pidfd_open
  return int(syscall(SYS_pidfd_open, pid_t(pid), 0));
#else
  Q_UNUSED(pid)
  errno = ENOSYS;
  return -1;
#endif
}

int sendPidFileDescriptorSignal(int pidfd, int signalNumber) {
#ifdef SYS_pidfd_send_signal
  return int(syscall(SYS_pidfd_send_signal, pidfd, signalNumber, nullptr, 0));
#else
  Q_UNUSED(pidfd)
  Q_UNUSED(signalNumber)
  errno = ENOSYS;
  return -1;
#endif
}

QString sendSignal(const QString &procRoot, qint64 pid, quint64 startTicks,
                   int signalNumber, const QString &operation) {
  const QString firstCheck = verifyIdentity(procRoot, pid, startTicks);
  if (!firstCheck.isEmpty()) {
    return firstCheck;
  }

  const int pidfd = openPidFileDescriptor(pid);
  if (pidfd >= 0) {
    const QString pinnedCheck = verifyIdentity(procRoot, pid, startTicks);
    if (!pinnedCheck.isEmpty()) {
      close(pidfd);
      return pinnedCheck;
    }
    if (sendPidFileDescriptorSignal(pidfd, signalNumber) == 0) {
      close(pidfd);
      return {};
    }
    const int savedError = errno;
    close(pidfd);
    return systemError(operation, pid, savedError);
  }

  return systemError(QStringLiteral("Open identity handle for"), pid, errno);
}

} // namespace

QString applyProcessAction(const QString &procRoot, qint64 pid,
                           quint64 startTicks, const QString &action,
                           int value) {
  if (pid <= 0 || pid > std::numeric_limits<pid_t>::max() || startTicks == 0) {
    return QStringLiteral("Process identity requires a representable positive "
                          "PID and start tick");
  }
  if (action == QStringLiteral("terminate")) {
    return sendSignal(procRoot, pid, startTicks, SIGTERM,
                      QStringLiteral("Terminate"));
  }
  if (action == QStringLiteral("kill")) {
    return sendSignal(procRoot, pid, startTicks, SIGKILL,
                      QStringLiteral("Kill"));
  }
  if (action == QStringLiteral("stop")) {
    return sendSignal(procRoot, pid, startTicks, SIGSTOP,
                      QStringLiteral("Stop"));
  }
  if (action == QStringLiteral("continue")) {
    return sendSignal(procRoot, pid, startTicks, SIGCONT,
                      QStringLiteral("Continue"));
  }
  if (action != QStringLiteral("nice")) {
    return QStringLiteral("Unsupported process action: %1").arg(action);
  }
  if (value < -20 || value > 19) {
    return QStringLiteral("Nice value %1 is outside -20..19").arg(value);
  }
  const QString identityCheck = verifyIdentity(procRoot, pid, startTicks);
  if (!identityCheck.isEmpty()) {
    return identityCheck;
  }
  errno = 0;
  if (::setpriority(PRIO_PROCESS, id_t(pid), value) != 0) {
    return systemError(QStringLiteral("Set priority for"), pid, errno);
  }
  return verifyIdentity(procRoot, pid, startTicks);
}

} // namespace QindaQt::SystemMonitor
