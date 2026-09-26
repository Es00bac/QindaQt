// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compat_db.h"
#include "downloader.h"

#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::QindaLutris {

class CompatRefreshJob;
struct CompatRefreshResult;

// AGENT-CONTRACT: "Check for newer information" (ADR-0275 section 3),
// exposed to QML as `CompatInfo`. Downloads the published document
// (CompatRefreshJob), loads it with loadCompatDatabase -- the same
// refusals as the shipped copy -- and keeps it only when chooseNewer
// prefers it over what is in use. Kept documents are written atomically to
// the refreshed path and replace *database in place, so every holder of the
// pointer (Installs, Protons) sees the new information at once. Single
// threaded (the UI thread); never on a timer.
class CompatRefresher final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString dateText READ dateText NOTIFY changed)
  Q_PROPERTY(int gameCount READ gameCount NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
public:
  // database: the effective database main() loaded; not owned, must
  // outlive this object. refreshedPath: where a kept copy is written.
  // downloader: null for the production NetworkDownloader.
  CompatRefresher(CompatDatabase *database, QString refreshedPath,
                  std::unique_ptr<Downloader> downloader = {}, QObject *parent = nullptr);
  ~CompatRefresher() override;

  [[nodiscard]] QString dateText() const; // "2026-09-25", empty when none loaded
  [[nodiscard]] int gameCount() const;
  [[nodiscard]] bool busy() const;
  [[nodiscard]] QString message() const { return m_message; }

  Q_INVOKABLE void checkForNewer();

Q_SIGNALS:
  void changed();

private:
  void onFinished(const CompatRefreshResult &result);

  CompatDatabase *m_database = nullptr;
  QString m_refreshedPath;
  std::unique_ptr<Downloader> m_downloader;
  CompatRefreshJob *m_job = nullptr; // owned child
  QString m_message;
};

} // namespace QindaQt::QindaLutris
