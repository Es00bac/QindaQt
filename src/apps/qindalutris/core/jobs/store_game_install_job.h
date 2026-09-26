// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_log.h"
#include "store_clients.h"

#include <QObject>
#include <QString>

namespace QindaQt::QindaLutris {

class ProcessRunner;
struct ProcessRunResult;

struct StoreGameInstallRequest final {
  StoreClient client = StoreClient::Epic;
  StoreClientBinaries binaries;
  QString storesRoot;
  QString gameId;
  QString title;   // for messages only
  QString baseDir; // absolute; the client makes the game's folder inside
};

struct StoreGameInstallResult final {
  bool ok = false;
  bool cancelled = false;
  QString message; // one plain sentence
  StoreGameInstall install;
};

// AGENT-CONTRACT: downloads one owned Epic / GOG / Amazon game with its
// store client (ADR-0275 section 8) and finds the program it installed.
// Stages: Download (installRun; progress from the client's "= Progress:"
// lines through ProcessRunner::outputReceived) -> Locate (Epic: legendary
// list-installed; Amazon: nile's launch dry run; GOG: goggame-<id>.info)
// -> finished(). The located program must exist as a regular file, or the
// job fails with a sentence rather than registering a title that cannot
// start. It never touches a Wine prefix: the title's prefix and pinned
// build are the caller's (InstallController) business.
// AGENT-GUARD: install runs carry no secrets, so their output may be kept
// in the details log -- only the last kMaxTailLines lines, which is where
// clients report failures.
class StoreGameInstallJob final : public QObject {
  Q_OBJECT
public:
  explicit StoreGameInstallJob(ProcessRunner *runner, QObject *parent = nullptr);
  ~StoreGameInstallJob() override;

  void start(const StoreGameInstallRequest &request);
  void cancel();
  [[nodiscard]] bool isRunning() const { return m_stage != Stage::Idle; }
  [[nodiscard]] QString detailsText() const { return m_log.text(); }

  static constexpr int kMaxTailLines = 40;

Q_SIGNALS:
  // fraction in [0, 1], or -1 when the client has not reported one yet.
  void progressChanged(double fraction, const QString &stageText);
  void finished(const QindaQt::QindaLutris::StoreGameInstallResult &result);

private:
  enum class Stage { Idle, Download, Locate };

  void onOutput(const QByteArray &chunk);
  void onRunFinished(const ProcessRunResult &result);
  void locate(const QByteArray &clientOutput);
  void fail(const QString &message);
  void keepTail(const QByteArray &output);

  ProcessRunner *m_runner = nullptr; // not owned
  StoreGameInstallRequest m_request;
  Stage m_stage = Stage::Idle;
  QByteArray m_partialLine;
  QStringList m_tail;
  JobLog m_log;
};

} // namespace QindaQt::QindaLutris
