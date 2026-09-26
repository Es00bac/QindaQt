// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "job_log.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace QindaQt::QindaLutris {

class Downloader;

// AGENT-CONTRACT: the manual "check for newer compatibility information" of
// ADR-0275 section 3. The refreshed document is a file on ONE fixed GitHub
// release of the QindaQt project, replaced in place when curators publish:
//   https://github.com/Es00bac/QindaQt/releases/download/compat-db/compat-db-v1.json
//   https://github.com/Es00bac/QindaQt/releases/download/compat-db/compat-db-v1.json.sha512sum
// (github.com redirects to release-assets.githubusercontent.com; both hosts
// are on the download allowlist). The job downloads the checksum, then the
// document, into stagingDir, and succeeds only when the document's SHA-512
// matches. It does NOT parse or install the document: the caller loads the
// staged file with loadCompatDatabase (the same refusals as the shipped
// copy) and keeps it only when chooseNewer prefers it. Nothing here runs on
// a timer; the user asks.
inline constexpr char kCompatDbReleaseBase[] =
    "https://github.com/Es00bac/QindaQt/releases/download/compat-db/";
inline constexpr char kCompatDbFileName[] = "compat-db-v1.json";

[[nodiscard]] QUrl compatDbRefreshUrl();
[[nodiscard]] QUrl compatDbChecksumUrl();

struct CompatRefreshResult final {
  bool ok = false;
  QString message;     // one plain sentence when !ok
  QString stagedPath;  // the verified document when ok
};

class CompatRefreshJob final : public QObject {
  Q_OBJECT
public:
  explicit CompatRefreshJob(Downloader *downloader, QObject *parent = nullptr);
  ~CompatRefreshJob() override;

  // stagingDir must be absolute; it is created when missing and its two
  // files are replaced.
  void start(const QString &stagingDir);
  void cancel();
  [[nodiscard]] bool isRunning() const { return m_stage != Stage::Idle; }
  [[nodiscard]] QString detailsText() const { return m_log.text(); }

  // The document is a few MB today; refuse anything absurd.
  static constexpr qint64 kMaxDocumentBytes = 64 * 1024 * 1024;

Q_SIGNALS:
  void finished(const QindaQt::QindaLutris::CompatRefreshResult &result);

private:
  enum class Stage { Idle, Checksum, Document };

  void onDownloadFinished(bool ok, const QString &reason);
  void conclude(bool ok, const QString &message);

  Downloader *m_downloader = nullptr; // not owned
  Stage m_stage = Stage::Idle;
  QString m_stagingDir;
  JobLog m_log;
};

} // namespace QindaQt::QindaLutris
