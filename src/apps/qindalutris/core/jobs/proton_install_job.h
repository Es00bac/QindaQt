// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ge_proton_releases.h"
#include "job_log.h"
#include "process_runner.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QTimer>

#include <memory>

class QTemporaryDir;

namespace QindaQt::QindaLutris {

class Downloader;

struct ProtonJobResult final {
  bool ok = false;
  bool cancelled = false;
  QString message;       // ONE plain sentence for the user
  QString toolName;      // "GE-Proton11-6-x86_64"
  QString installedPath; // <root>/<toolName> on success
};

// $XDG_DATA_HOME/Steam/compatibilitytools.d -- the user root of ADR-0275
// section 2 (the same directory as ~/.local/share/Steam/compatibilitytools.d).
[[nodiscard]] QString defaultUserCompatToolsRoot();

// AGENT-CONTRACT: the "download a GE-Proton release" job of ADR-0275
// section 2. Stages: refuse an existing <root>/<toolName> -> download the
// tarball and its .sha512sum into a hidden staging directory INSIDE the
// target root (so the final rename is atomic on one filesystem) -> verify
// SHA-512, streamed in slices so the UI stays live -> `tar --list` and refuse
// any listing that escapes one top-level directory named toolName -> `tar
// --extract` into staging -> renameNoReplace(staging/<toolName>,
// <root>/<toolName>). Every failure and cancel removes the staging directory;
// nothing half-extracted is ever visible under the root.
// AGENT-CONTRACT (with the Proton catalog): staging directories are named
// `.qindalutris-staging-*` and the removal trash `.qindalutris-trash`;
// catalog scanners must ignore dot-directories.
// Seams are borrowed, not owned, and must be dedicated to this job while it
// runs. Threading: owner thread only.
class ProtonInstallJob final : public QObject {
  Q_OBJECT
public:
  ProtonInstallJob(QString targetRoot, Downloader *downloader, ProcessRunner *runner,
                   QObject *parent = nullptr);
  ~ProtonInstallJob() override;

  // Absolute tar program; default resolves `tar` on PATH at start().
  void setTarProgram(const QString &program) { m_tarProgram = program; }

  void start(const GeProtonRelease &release);
  // Stops the running stage, removes staging, and emits finished(cancelled)
  // before returning.
  void cancel();

  [[nodiscard]] bool isRunning() const { return m_stage != Stage::Idle; }
  [[nodiscard]] QString detailsText() const { return m_log.text(); }

Q_SIGNALS:
  void progress(double fraction, const QString &stageText);
  void finished(const QindaQt::QindaLutris::ProtonJobResult &result);

private:
  enum class Stage { Idle, DownloadingArchive, DownloadingChecksum, Hashing, Listing, Extracting };

  void onDownloadProgress(qint64 received, qint64 total);
  void onDownloadFinished(bool ok, const QString &reason);
  void onProcessFinished(const ProcessRunResult &result);
  void beginHashing();
  void hashSlice();
  void beginListing();
  void beginExtracting();
  void commit();
  void fail(const QString &plain, const QString &detail);
  void conclude(ProtonJobResult result);
  void report(double fraction, const QString &stageText);

  QString m_root;
  Downloader *m_downloader = nullptr;
  ProcessRunner *m_runner = nullptr;
  QString m_tarProgram;
  QString m_resolvedTar;
  GeProtonRelease m_release;
  Stage m_stage = Stage::Idle;
  std::unique_ptr<QTemporaryDir> m_staging;
  QFile m_hashFile;
  QCryptographicHash m_hash{QCryptographicHash::Sha512};
  QByteArray m_expectedHash;
  QTimer m_hashTimer;
  double m_reported = 0.0;
  JobLog m_log;
};

} // namespace QindaQt::QindaLutris
