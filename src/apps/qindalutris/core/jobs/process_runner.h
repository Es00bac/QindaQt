// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "process_tree.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;
class QTimer;

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the bounded, cancellable child-process seam for long jobs
// (ADR-0275 sections 4b and 6; the posture of ADR-0062/ADR-0231's launch
// seam): one program plus one argv vector plus typed environment overlays
// over the session environment. There is NO shell -- no system(), no command
// string. It is used for `tar` (Proton extraction) and, under the name
// InstallerRunner, for vendor installers planned by the umu plan builder.
struct ProcessRunSpec final {
  QString program;
  QStringList arguments;
  QHash<QString, QString> environment; // overlays; never a replacement
  QString workingDirectory;            // empty: inherit
  // AGENT-GUARD: every run is bounded. A spec with timeoutMs <= 0 is refused
  // by the production runner rather than run forever.
  int timeoutMs = 0;
  // Each of stdout/stderr keeps at most this many leading bytes; the result
  // says when more was produced so callers can refuse partial data.
  qsizetype maxOutputBytes = 64 * 1024;

  friend bool operator==(const ProcessRunSpec &, const ProcessRunSpec &) = default;
};

struct ProcessRunResult final {
  bool started = false;
  bool timedOut = false;
  bool cancelled = false;
  bool crashed = false;
  // After a timeout or cancel: true only when the WHOLE process tree was
  // confirmed gone. False means something may still run; callers must not
  // delete files the tree might still use.
  bool treeStopped = true;
  int exitCode = -1;
  QByteArray standardOutput;
  QByteArray standardError;
  bool outputTruncated = false;
  QString error;      // why it did not start / was stopped; empty otherwise
  QString stopDetail; // for the details log after a stop
};

// Semantics every implementation keeps:
//  - start() must not be called while running and never emits synchronously;
//    exactly one finished() follows each start().
//  - cancel() begins stopping the WHOLE tree and returns at once; finished()
//    then arrives with cancelled = true once the tree is gone or the SIGKILL
//    escalation ended (treeStopped says which). cancel() when idle is a no-op.
//  - a timeout stops the tree the same way (timedOut = true).
// Threading: confined to the owning thread.
class ProcessRunner : public QObject {
  Q_OBJECT
public:
  explicit ProcessRunner(QObject *parent = nullptr);
  ~ProcessRunner() override;

  virtual void start(const ProcessRunSpec &spec) = 0;
  virtual void cancel() = 0;

Q_SIGNALS:
  void finished(const QindaQt::QindaLutris::ProcessRunResult &result);
};

// How QProcessRunner contains the tree it starts.
enum class ProcessContainment {
  Auto,         // a systemd user scope when a user manager answers, else group
  SystemdScope, // require the scope (falls back to group if tools are missing)
  ProcessGroup, // own process group + /proc descendant tracking only
};

// Production runner over QProcess (argv only, separate channels). Every run
// is contained per process_tree.h; overlays and the inherited environment
// are stripped of PYTHON* (see the guard in process_runner.cpp).
class QProcessRunner final : public ProcessRunner {
  Q_OBJECT
public:
  explicit QProcessRunner(ProcessContainment containment = ProcessContainment::Auto,
                          QObject *parent = nullptr);
  ~QProcessRunner() override;

  void start(const ProcessRunSpec &spec) override;
  void cancel() override;

  // Grace between SIGTERM and SIGKILL, and the wait after SIGKILL.
  void setStopTimings(int graceMs, int killWaitMs);
  [[nodiscard]] bool usesSystemdScope() const { return m_tools.available(); }
  // The scope unit of the current/last run (empty in the fallback).
  [[nodiscard]] QString scopeUnit() const { return m_target.scopeUnit; }

private:
  void collect();
  void onProcessFinished();
  void beginStop();
  void onStopped(bool treeGone, const QString &detail);
  void releaseProcess();
  void emitLater(ProcessRunResult result);

  UserScopeTools m_tools;
  QProcess *m_process = nullptr;
  QTimer *m_timeout = nullptr;
  QTimer *m_tracker = nullptr;
  ProcessTreeStopper *m_stopper = nullptr;
  StopTarget m_target;
  ProcessRunResult m_result;
  qsizetype m_limit = 0;
  quint64 m_generation = 0;
  int m_graceMs = 5000;
  int m_killWaitMs = 3000;
  bool m_stopping = false;
};

} // namespace QindaQt::QindaLutris
