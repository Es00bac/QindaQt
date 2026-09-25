// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_tree.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

#include <csignal>
#include <sys/types.h>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kPollMs = 100;
constexpr int kSystemctlTimeoutMs = 2000;

struct StatLine {
  qint64 ppid = 0;
  qint64 pgrp = 0;
  quint64 startTime = 0;
  bool zombie = false;
};

// /proc/<pid>/stat: "pid (comm) S ppid ... starttime(22) ...". comm may hold
// spaces and parentheses, so fields are counted after the LAST ')'.
std::optional<StatLine> readStat(qint64 pid) {
  QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  const QByteArray text = file.read(4096);
  const qsizetype close = text.lastIndexOf(')');
  if (close < 0) {
    return std::nullopt;
  }
  const QList<QByteArray> fields = text.mid(close + 2).split(' ');
  // fields[0] = state (field 3), fields[1] = ppid (4), fields[19] = starttime (22)
  if (fields.size() < 20) {
    return std::nullopt;
  }
  StatLine line;
  line.zombie = fields.at(0) == "Z" || fields.at(0) == "X";
  line.ppid = fields.at(1).toLongLong();
  line.pgrp = fields.at(2).toLongLong();
  line.startTime = fields.at(19).toULongLong();
  return line;
}

struct QuickRun {
  int exitCode = -1;
  QByteArray output;
};

QuickRun runQuick(const QString &program, const QStringList &arguments) {
  QuickRun out;
  if (program.isEmpty()) {
    return out;
  }
  QProcess process;
  process.setProgram(program);
  process.setArguments(arguments);
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start();
  if (!process.waitForFinished(kSystemctlTimeoutMs)) {
    process.kill();
    process.waitForFinished(500);
    return out;
  }
  out.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  out.output = process.readAll().trimmed();
  return out;
}

void refreshKnownImpl(StopTarget &target) {
  // Dead entries are dropped so a long run cannot grow the set without bound;
  // roots are only verified identities, never a bare (maybe recycled) pid.
  target.known.removeIf([](const ProcessIdentity &process) { return !isProcessAlive(process); });
  QVector<qint64> roots;
  for (const ProcessIdentity &process : std::as_const(target.known)) {
    roots.append(process.pid);
  }
  QVector<ProcessIdentity> found = liveDescendants(roots);
  if (target.processGroup > 1) {
    found += liveGroupMembers(target.processGroup);
  }
  for (const ProcessIdentity &child : std::as_const(found)) {
    if (!target.known.contains(child)) {
      target.known.append(child);
    }
  }
}

void signalTree(const StopTarget &target, int signalNumber, const QString &signalName) {
  // The direct child first: umu-run forwards SIGTERM to its own session.
  for (const ProcessIdentity &process : target.known) {
    if (process.pid == target.mainPid) {
      signalProcess(process, signalNumber);
    }
  }
  if (!target.scopeUnit.isEmpty()) {
    (void)runQuick(target.tools.systemctl, systemctlKillArguments(target.scopeUnit, signalName));
  }
  for (const ProcessIdentity &process : target.known) {
    signalProcess(process, signalNumber);
  }
  if (target.processGroup > 1) {
    ::kill(static_cast<pid_t>(-target.processGroup), signalNumber);
  }
}

bool trackedGone(const StopTarget &target) {
  for (const ProcessIdentity &process : target.known) {
    if (isProcessAlive(process)) {
      return false;
    }
  }
  return true;
}

} // namespace

void trackProcessTree(StopTarget &target) { refreshKnownImpl(target); }

std::optional<ProcessIdentity> liveProcessIdentity(qint64 pid) {
  if (pid <= 0) {
    return std::nullopt;
  }
  const auto stat = readStat(pid);
  if (!stat || stat->zombie) {
    return std::nullopt;
  }
  return ProcessIdentity{pid, stat->startTime};
}

bool isProcessAlive(const ProcessIdentity &process) {
  const auto now = liveProcessIdentity(process.pid);
  return now && now->startTime == process.startTime;
}

QVector<ProcessIdentity> liveDescendants(const QVector<qint64> &rootPids, int maxProcesses) {
  QHash<qint64, QVector<qint64>> children;
  QHash<qint64, quint64> starts;
  const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  int seen = 0;
  for (const QString &entry : entries) {
    bool numeric = false;
    const qint64 pid = entry.toLongLong(&numeric);
    if (!numeric || ++seen > maxProcesses) {
      continue;
    }
    const auto stat = readStat(pid);
    if (!stat || stat->zombie) {
      continue;
    }
    children[stat->ppid].append(pid);
    starts.insert(pid, stat->startTime);
  }
  QVector<ProcessIdentity> out;
  QVector<qint64> queue = rootPids;
  QHash<qint64, bool> visited;
  while (!queue.isEmpty() && out.size() < maxProcesses) {
    const qint64 parent = queue.takeFirst();
    for (const qint64 child : children.value(parent)) {
      if (visited.contains(child)) {
        continue;
      }
      visited.insert(child, true);
      out.append({child, starts.value(child)});
      queue.append(child);
    }
  }
  return out;
}

QVector<ProcessIdentity> liveGroupMembers(qint64 processGroup, int maxProcesses) {
  QVector<ProcessIdentity> out;
  if (processGroup <= 1) {
    return out;
  }
  const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  int seen = 0;
  for (const QString &entry : entries) {
    bool numeric = false;
    const qint64 pid = entry.toLongLong(&numeric);
    if (!numeric || ++seen > maxProcesses) {
      continue;
    }
    const auto stat = readStat(pid);
    if (stat && !stat->zombie && stat->pgrp == processGroup) {
      out.append({pid, stat->startTime});
    }
  }
  return out;
}

void signalProcess(const ProcessIdentity &process, int signalNumber) {
  if (process.pid > 1 && isProcessAlive(process)) {
    ::kill(static_cast<pid_t>(process.pid), signalNumber);
  }
}

QString newScopeUnitName(const QString &prefix) {
  return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QStringList systemdRunScopeArguments(const QString &unit, const QString &program,
                                     const QStringList &arguments) {
  QStringList out{QStringLiteral("--user"), QStringLiteral("--scope"), QStringLiteral("--quiet"),
                  QStringLiteral("--collect"), QStringLiteral("--unit=") + unit,
                  QStringLiteral("--"), program};
  out.append(arguments);
  return out;
}

QStringList systemctlKillArguments(const QString &unit, const QString &signalName) {
  return {QStringLiteral("--user"), QStringLiteral("kill"),
          QStringLiteral("--signal=") + signalName, unit + QStringLiteral(".scope")};
}

UserScopeTools detectUserScopeTools() {
  UserScopeTools tools;
  const QString run = QStandardPaths::findExecutable(QStringLiteral("systemd-run"));
  const QString ctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
  if (run.isEmpty() || ctl.isEmpty()) {
    return tools;
  }
  const QuickRun state =
      runQuick(ctl, {QStringLiteral("--user"), QStringLiteral("is-system-running")});
  static const QList<QByteArray> usable{"running", "degraded", "starting"};
  if (usable.contains(state.output)) {
    tools.systemdRun = run;
    tools.systemctl = ctl;
  }
  return tools;
}

bool isScopeActive(const UserScopeTools &tools, const QString &unit) {
  if (!tools.available() || unit.isEmpty()) {
    return false;
  }
  const QuickRun state = runQuick(
      tools.systemctl,
      {QStringLiteral("--user"), QStringLiteral("is-active"), unit + QStringLiteral(".scope")});
  static const QList<QByteArray> busy{"active", "activating", "deactivating", "reloading"};
  return busy.contains(state.output);
}

TreeStopOutcome stopProcessTree(StopTarget &target, int graceMs, int killWaitMs) {
  TreeStopOutcome outcome;
  trackProcessTree(target);
  const qsizetype signalled = target.known.size();
  signalTree(target, SIGTERM, QStringLiteral("SIGTERM"));
  bool gone = false;
  for (int phase = 0; phase < 2 && !gone; ++phase) {
    const qint64 deadline =
        QDateTime::currentMSecsSinceEpoch() + (phase == 0 ? graceMs : killWaitMs);
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
      trackProcessTree(target);
      if (trackedGone(target) &&
          (target.scopeUnit.isEmpty() || !isScopeActive(target.tools, target.scopeUnit))) {
        gone = true;
        break;
      }
      QThread::msleep(kPollMs);
    }
    if (!gone && phase == 0) {
      signalTree(target, SIGKILL, QStringLiteral("SIGKILL"));
    }
  }
  outcome.trackedGone = trackedGone(target);
  outcome.proven = !target.scopeUnit.isEmpty() && outcome.trackedGone &&
                   !isScopeActive(target.tools, target.scopeUnit);
  outcome.detail = describeOutcome(target, outcome, signalled);
  return outcome;
}

QString describeOutcome(const StopTarget &target, const TreeStopOutcome &outcome,
                        qsizetype signalled) {
  QString state;
  if (outcome.proven) {
    state = QStringLiteral("process tree stopped (scope %1 gone)").arg(target.scopeUnit);
  } else if (outcome.trackedGone && target.scopeUnit.isEmpty()) {
    state = QStringLiteral("stopped as far as tracked (process-group fallback: a process that "
                           "detached before it was tracked cannot be ruled out)");
  } else {
    state = QStringLiteral("some processes may still be running");
  }
  return QStringLiteral("%1; %2 processes signalled, %3 still alive")
      .arg(state)
      .arg(signalled)
      .arg(target.known.size());
}

} // namespace QindaQt::QindaLutris
