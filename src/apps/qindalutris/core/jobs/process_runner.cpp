// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_runner.h"

#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

#include <unistd.h>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kTrackIntervalMs = 1000;

// AGENT-GUARD: umu-run is Python. A PYTHONPATH/PYTHONHOME/PYTHONSTARTUP from
// the session or from a planner/recipe overlay would change which code umu
// runs, so no PYTHON* variable ever reaches a job process.
bool isReservedVariable(const QString &name) {
  return name.startsWith(QLatin1String("PYTHON"));
}

QString resolveProgram(const QString &program) {
  if (program.isEmpty()) {
    return {};
  }
  if (program.contains(QLatin1Char('/'))) {
    const QFileInfo info(program);
    return info.isFile() && info.isExecutable() ? info.absoluteFilePath() : QString();
  }
  return QStandardPaths::findExecutable(program);
}

} // namespace

ProcessRunner::ProcessRunner(QObject *parent) : QObject(parent) {}
ProcessRunner::~ProcessRunner() = default;

QProcessRunner::QProcessRunner(ProcessContainment containment, QObject *parent)
    : ProcessRunner(parent), m_timeout(new QTimer(this)),
      m_supervisor(new ProcessTreeSupervisor(this)) {
  if (containment != ProcessContainment::ProcessGroup) {
    m_tools = detectUserScopeTools();
  }
  m_timeout->setSingleShot(true);
  connect(m_timeout, &QTimer::timeout, this, [this] {
    if (m_process != nullptr && !m_stopping && !m_exited) {
      m_result.timedOut = true;
      m_result.error = QStringLiteral("The program took too long and was stopped.");
      beginStop();
    }
  });
  connect(m_supervisor, &ProcessTreeSupervisor::settled, this, &QProcessRunner::onSettled);
}

QProcessRunner::~QProcessRunner() {
  ++m_generation;
  // The supervisor's destructor stops a still-running tree (blocking,
  // bounded) and joins its worker; only then is the dead main process reaped.
  delete m_supervisor;
  m_supervisor = nullptr;
  releaseProcess();
}

void QProcessRunner::setStopTimings(int graceMs, int killWaitMs) {
  m_graceMs = graceMs;
  m_killWaitMs = killWaitMs;
}

void QProcessRunner::emitLater(ProcessRunResult result) {
  const quint64 generation = m_generation;
  QTimer::singleShot(0, this, [this, generation, result] {
    if (generation == m_generation) {
      Q_EMIT finished(result);
    }
  });
}

void QProcessRunner::start(const ProcessRunSpec &spec) {
  ++m_generation;
  if (m_process != nullptr) {
    ProcessRunResult busy;
    busy.error = QStringLiteral("Another program is already running for this job.");
    emitLater(busy);
    return;
  }
  m_result = {};
  m_limit = spec.maxOutputBytes;
  m_target = {};
  m_stopping = false;
  m_exited = false;
  const QString program = resolveProgram(spec.program);
  if (program.isEmpty() || spec.timeoutMs <= 0) {
    ProcessRunResult refused;
    refused.error = spec.timeoutMs <= 0
                        ? QStringLiteral("Refused to run a program without a time limit.")
                        : QStringLiteral("The program %1 was not found.").arg(spec.program);
    emitLater(refused);
    return;
  }

  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  for (const QString &key : environment.keys()) {
    const bool unsetByPrefix = std::any_of(
        spec.unsetEnvironmentPrefixes.cbegin(), spec.unsetEnvironmentPrefixes.cend(),
        [&key](const QString &prefix) { return !prefix.isEmpty() && key.startsWith(prefix); });
    if (isReservedVariable(key) || unsetByPrefix || spec.unsetEnvironment.contains(key)) {
      environment.remove(key);
    }
  }
  for (auto it = spec.environment.constBegin(); it != spec.environment.constEnd(); ++it) {
    if (!isReservedVariable(it.key())) {
      environment.insert(it.key(), it.value());
    }
  }
  m_process = new QProcess(this);
  if (m_tools.available()) {
    m_target.scopeUnit = newScopeUnitName();
    m_target.tools = m_tools;
    m_process->setProgram(m_tools.systemdRun);
    m_process->setArguments(systemdRunScopeArguments(m_target.scopeUnit, program, spec.arguments));
  } else {
    m_process->setProgram(program);
    m_process->setArguments(spec.arguments);
  }
  // Own process group in both modes: the fallback's kill target, and a
  // backstop for a cancel that lands before systemd has registered the scope.
  m_process->setChildProcessModifier([] { ::setpgid(0, 0); });
  m_process->setProcessEnvironment(environment);
  m_process->setProcessChannelMode(QProcess::SeparateChannels);
  if (!spec.workingDirectory.isEmpty()) {
    m_process->setWorkingDirectory(spec.workingDirectory);
  }
  connect(m_process, &QProcess::readyReadStandardOutput, this, &QProcessRunner::collect);
  connect(m_process, &QProcess::readyReadStandardError, this, &QProcessRunner::collect);
  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart && m_process != nullptr && !m_stopping &&
        !m_supervisor->isWatching()) {
      m_result.error = m_process->errorString();
      m_timeout->stop();
      releaseProcess();
      emitLater(m_result);
    }
  });
  connect(m_process, &QProcess::finished, this, &QProcessRunner::onProcessFinished);
  m_timeout->start(spec.timeoutMs);
  m_process->start();
  if (m_process == nullptr) {
    return; // failed to start; the result is already queued
  }
  m_process->closeWriteChannel();
  m_target.mainPid = m_process->processId();
  m_target.processGroup = m_target.mainPid; // setpgid(0, 0) in the child
  if (const auto main = liveProcessIdentity(m_target.mainPid)) {
    m_target.known = {*main};
  }
  m_supervisor->watch(m_target, m_graceMs, m_killWaitMs);
}

void QProcessRunner::collect() {
  if (m_process == nullptr) {
    return;
  }
  const auto take = [this](QByteArray chunk, QByteArray &sink) {
    const qsizetype room = m_limit - sink.size();
    if (chunk.size() > room) {
      m_result.outputTruncated = true;
      chunk.truncate(room > 0 ? room : 0);
    }
    sink.append(chunk);
  };
  take(m_process->readAllStandardOutput(), m_result.standardOutput);
  take(m_process->readAllStandardError(), m_result.standardError);
}

void QProcessRunner::onProcessFinished() {
  if (m_process == nullptr) {
    return;
  }
  collect();
  if (m_stopping || m_exited) {
    return; // the supervisor decides when the whole tree is gone
  }
  m_exited = true;
  m_timeout->stop();
  m_result.started = true;
  m_result.crashed = m_process->exitStatus() == QProcess::CrashExit;
  m_result.exitCode = m_process->exitCode();
  m_supervisor->mainProcessExited(); // leftovers are stopped before finished()
}

void QProcessRunner::beginStop() {
  m_stopping = true;
  m_timeout->stop();
  m_supervisor->requestStop();
}

void QProcessRunner::onSettled(const TreeStopOutcome &outcome) {
  if (m_process == nullptr) {
    return;
  }
  collect();
  m_result.started = true;
  m_result.treeStopped = outcome.proven;
  m_result.trackedStopped = outcome.trackedGone;
  m_result.stoppedLeftovers = outcome.hadLeftovers;
  m_result.stopDetail = outcome.detail;
  if (m_stopping && m_process->state() == QProcess::NotRunning) {
    m_result.exitCode = m_process->exitCode();
  }
  m_stopping = false;
  m_exited = false;
  releaseProcess();
  emitLater(m_result);
}

void QProcessRunner::releaseProcess() {
  if (m_process == nullptr) {
    return;
  }
  QProcess *process = m_process;
  m_process = nullptr;
  process->disconnect(this);
  if (process->state() != QProcess::NotRunning) {
    // Already signalled by the stopper; this only reaps the main process.
    process->kill();
    process->waitForFinished(1000);
  }
  process->deleteLater();
}

void QProcessRunner::cancel() {
  if (m_process == nullptr || m_stopping || m_exited) {
    return; // idle, already stopping, or already cleaning up after an exit
  }
  m_result.cancelled = true;
  m_result.error = QStringLiteral("Stopped because the job was cancelled.");
  beginStop();
}

} // namespace QindaQt::QindaLutris
