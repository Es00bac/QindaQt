// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_runner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

namespace QindaQt::QindaLutris {

ProcessRunner::ProcessRunner(QObject *parent) : QObject(parent) {}
ProcessRunner::~ProcessRunner() = default;

QProcessRunner::QProcessRunner(QObject *parent)
    : ProcessRunner(parent), m_timeout(new QTimer(this)) {
  m_timeout->setSingleShot(true);
  connect(m_timeout, &QTimer::timeout, this, [this] {
    if (m_process != nullptr) {
      m_result.timedOut = true;
      m_result.error = QStringLiteral("The program took too long and was stopped.");
      m_process->kill();
    }
  });
}

QProcessRunner::~QProcessRunner() { cancel(); }

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
  if (spec.program.isEmpty() || spec.timeoutMs <= 0) {
    ProcessRunResult refused;
    refused.error = spec.program.isEmpty()
                        ? QStringLiteral("No program was given.")
                        : QStringLiteral("Refused to run a program without a time limit.");
    emitLater(refused);
    return;
  }

  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  for (auto it = spec.environment.constBegin(); it != spec.environment.constEnd(); ++it) {
    environment.insert(it.key(), it.value());
  }
  m_process = new QProcess(this);
  m_process->setProgram(spec.program);
  m_process->setArguments(spec.arguments);
  m_process->setProcessEnvironment(environment);
  m_process->setProcessChannelMode(QProcess::SeparateChannels);
  m_process->setInputChannelMode(QProcess::ManagedInputChannel);
  if (!spec.workingDirectory.isEmpty()) {
    m_process->setWorkingDirectory(spec.workingDirectory);
  }
  connect(m_process, &QProcess::readyReadStandardOutput, this, &QProcessRunner::collect);
  connect(m_process, &QProcess::readyReadStandardError, this, &QProcessRunner::collect);
  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart && m_process != nullptr) {
      m_result.error = m_process->errorString();
      complete();
    }
  });
  connect(m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
    complete();
  });
  m_timeout->start(spec.timeoutMs);
  m_process->start();
  // AGENT-GUARD: QProcess reports FailedToStart synchronously from start(),
  // and complete() has already released the process by then.
  if (m_process != nullptr) {
    m_process->closeWriteChannel();
  }
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

void QProcessRunner::complete() {
  if (m_process == nullptr) {
    return;
  }
  m_timeout->stop();
  collect();
  QProcess *process = m_process;
  m_process = nullptr;
  if (m_result.error.isEmpty() || m_result.timedOut) {
    m_result.started = true;
    m_result.crashed = process->exitStatus() == QProcess::CrashExit && !m_result.timedOut;
    m_result.exitCode = process->exitCode();
  }
  process->disconnect(this);
  process->deleteLater();
  emitLater(m_result);
}

void QProcessRunner::cancel() {
  ++m_generation;
  m_timeout->stop();
  if (m_process != nullptr) {
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->kill();
    process->waitForFinished(3000);
    process->deleteLater();
  }
}

} // namespace QindaQt::QindaLutris
