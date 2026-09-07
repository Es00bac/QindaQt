// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace QindaQt::Apps::Terminal {
QString PosixProcessMonitor::workingDirectory(ProcessId pid) {
  if (pid <= 0) return {};
  const QString path = QFileInfo(QStringLiteral("/proc/%1/cwd").arg(pid)).symLinkTarget();
  return QFileInfo(path).isDir() ? path : QString{};
}

namespace {

ProcessGroupState scanProcForGroup(ProcessId processGroupId) {
  const QDir proc(QStringLiteral("/proc"));
  if (!proc.exists()) {
    return ProcessGroupState::Unknown;
  }
  const QStringList entries =
      proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  bool sawProcessEntry = false;
  bool scanIncomplete = false;
  bool capturedLeaderExistsOutsideGroup = false;
  for (const QString &entry : entries) {
    bool numeric = false;
    const qlonglong entryPid = entry.toLongLong(&numeric);
    if (!numeric) {
      continue;
    }
    sawProcessEntry = true;
    QFile statFile(proc.filePath(entry + QStringLiteral("/stat")));
    if (!statFile.open(QIODevice::ReadOnly)) {
      // A process may disappear between enumeration and open. A persistent
      // unreadable entry means the scan cannot prove that group absent.
      scanIncomplete = scanIncomplete || QFileInfo::exists(statFile.fileName());
      continue;
    }
    const QByteArray stat = statFile.readAll();
    const qsizetype commandEnd = stat.lastIndexOf(')');
    if (commandEnd < 0 || commandEnd + 2 >= stat.size()) {
      scanIncomplete = true;
      continue;
    }
    // Fields after comm are: state, ppid, pgrp, session, ... . The command
    // itself may contain spaces or parentheses, hence the last ')' anchor.
    const QList<QByteArray> fields = stat.sliced(commandEnd + 2).split(' ');
    if (fields.size() < 3) {
      scanIncomplete = true;
      continue;
    }
    bool groupOk = false;
    const qlonglong group = fields.at(2).toLongLong(&groupOk);
    if (!groupOk) {
      scanIncomplete = true;
      continue;
    }
    if (group == processGroupId) {
      return ProcessGroupState::NonEmpty;
    }
    if (entryPid == processGroupId) {
      // The forked child can briefly exist before setsid() establishes the
      // promised group. Treat that startup window (or a failed setsid zombie)
      // as uncertain, never as proof that teardown has completed.
      capturedLeaderExistsOutsideGroup = true;
    }
  }
  if (capturedLeaderExistsOutsideGroup || scanIncomplete) {
    return ProcessGroupState::Unknown;
  }
  return sawProcessEntry ? ProcessGroupState::Empty
                         : ProcessGroupState::Unknown;
}

} // namespace

ProcessExitInfo PosixProcessMonitor::reap(ProcessId pid) {
  int status = 0;
  const pid_t result = waitpid(static_cast<pid_t>(pid), &status, WNOHANG);
  if (result == static_cast<pid_t>(pid)) {
    ProcessExitInfo info;
    info.state = ProcessState::Exited;
    info.statusKnown = true;
    if (WIFSIGNALED(status)) {
      info.signaled = true;
      info.code = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
      info.signaled = false;
      info.code = WEXITSTATUS(status);
    } else {
      // Stopped/continued children are not terminal exits; treat as running
      // so the session keeps waiting for a real terminal disposition.
      info.state = ProcessState::Running;
    }
    return info;
  }
  if (result == 0) {
    return {.state = ProcessState::Running, .signaled = false, .code = 0};
  }
  // result < 0: ECHILD means the exit status was already reaped elsewhere —
  // reported as Exited with statusKnown=false (never a fabricated normal
  // code); any other errno is Unknown so callers treat the process
  // conservatively as alive rather than signaling on a guess.
  const bool alreadyReaped = (errno == ECHILD);
  return {.state = alreadyReaped ? ProcessState::Exited : ProcessState::Unknown,
          .signaled = false,
          .code = 0,
          .statusKnown = !alreadyReaped};
}

ProcessGroupState
PosixProcessMonitor::processGroupState(ProcessId processGroupId) {
  if (processGroupId <= 0) {
    return ProcessGroupState::Unknown;
  }
  errno = 0;
  if (::killpg(static_cast<pid_t>(processGroupId), 0) == 0 || errno == EPERM) {
    return ProcessGroupState::NonEmpty;
  }
  if (errno != ESRCH) {
    return ProcessGroupState::Unknown;
  }
  // AGENT-GUARD: ESRCH from killpg alone is insufficient evidence. A zombie
  // descendant still owns the captured pgrp in /proc until its new parent
  // reaps it. Releasing the id before this scan says clean while a member
  // remains, recreating Dina St Johnston's P1-1 leak.
  return scanProcForGroup(processGroupId);
}

bool PosixProcessMonitor::signalProcessGroup(ProcessId processGroupId,
                                             int signalNumber) {
  if (processGroupId <= 0) {
    return false;
  }
  // AGENT-CONTRACT: TerminalSession retains the captured setsid-created group
  // id until processGroupState() proves it empty. The group leader may already
  // be reaped, so revalidating getpgid(leader) here would strand descendants.
  return ::killpg(static_cast<pid_t>(processGroupId), signalNumber) == 0;
}

} // namespace QindaQt::Apps::Terminal
