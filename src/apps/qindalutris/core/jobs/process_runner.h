// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;
class QTimer;

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the bounded, cancellable child-process seam for long jobs
// (ADR-0275 section 6; the posture of ADR-0062/ADR-0231's launch seam): one
// program plus one argv vector plus typed environment overlays over the
// session environment. There is NO shell -- no system(), no command string.
// It is used for `tar` (Proton extraction) and, under the name
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
  bool crashed = false;
  int exitCode = -1;
  QByteArray standardOutput;
  QByteArray standardError;
  bool outputTruncated = false;
  QString error; // why it did not start / was stopped; empty otherwise
};

// Semantics: start() must not be called while running and never emits
// synchronously; exactly one finished() follows each start() unless cancel()
// comes first, after which nothing more is emitted. cancel() kills the child.
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

// Production runner over QProcess (argv only, separate channels).
class QProcessRunner final : public ProcessRunner {
  Q_OBJECT
public:
  explicit QProcessRunner(QObject *parent = nullptr);
  ~QProcessRunner() override;

  void start(const ProcessRunSpec &spec) override;
  void cancel() override;

private:
  void collect();
  void complete();
  void emitLater(ProcessRunResult result);

  QProcess *m_process = nullptr;
  QTimer *m_timeout = nullptr;
  ProcessRunResult m_result;
  qsizetype m_limit = 0;
  quint64 m_generation = 0;
};

} // namespace QindaQt::QindaLutris
