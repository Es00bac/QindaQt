// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compat_db.h"
#include "ge_proton_releases.h"
#include "install_preflight.h"

#include <QObject>
#include <QSet>
#include <QVariantList>

#include <memory>

namespace QindaQt::QindaLutris {

class LibraryController;
class NetworkDownloader;
class ProtonInstallJob;
class QProcessRunner;
struct ProtonJobResult;

// AGENT-CONTRACT: the Proton manager (ADR-0275 section 2), exposed to QML as
// `Protons`. Lists every discovered build with the database's status
// ("Tested by QindaQt" / known issues / not tested) and how many titles are
// pinned to it; downloads GE-Proton releases (the jobs package's verified,
// upstream-tied ProtonInstallJob) into the user root; removes user builds
// (refused while pinned); and stores the user's default build for NEW
// installs in preferences-v1.json. It never re-pins a title.
class ProtonManager final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList builds READ builds NOTIFY buildsChanged)
  Q_PROPERTY(QVariantList releases READ releases NOTIFY releasesChanged)
  Q_PROPERTY(QString defaultBuild READ defaultBuild NOTIFY buildsChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
  Q_PROPERTY(QString stageText READ stageText NOTIFY stateChanged)
  Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY stateChanged)
  Q_PROPERTY(QString details READ details NOTIFY stateChanged)
public:
  ProtonManager(LibraryController *library, const CompatDatabase *database,
                QObject *parent = nullptr);
  ~ProtonManager() override;

  [[nodiscard]] QVariantList builds() const;
  [[nodiscard]] QVariantList releases() const;
  [[nodiscard]] QString defaultBuild() const;
  [[nodiscard]] bool busy() const { return m_busy; }
  [[nodiscard]] double progress() const { return m_progress; }
  [[nodiscard]] QString stageText() const { return m_stageText; }
  [[nodiscard]] QString resultMessage() const { return m_resultMessage; }
  [[nodiscard]] QString details() const { return m_details; }

  // Loads preferences and applies the effective default to the library.
  void initialize();

  Q_INVOKABLE void checkForReleases();
  Q_INVOKABLE void installRelease(const QString &toolName);
  Q_INVOKABLE void removeBuild(const QString &name);
  Q_INVOKABLE bool setDefaultBuild(const QString &name); // "" = automatic
  Q_INVOKABLE void cancel();

  // Build names any title or hand-added entry is pinned to.
  [[nodiscard]] QSet<QString> pinnedBuildNames() const;

Q_SIGNALS:
  void buildsChanged();
  void releasesChanged();
  void stateChanged();

private:
  void finish(bool ok, const QString &message, const QString &details);
  void onReleasesDownloaded(bool ok, const QString &reason);
  void onInstallFinished(const ProtonJobResult &result);
  void applyEffectiveDefault();

  LibraryController *m_library = nullptr;
  const CompatDatabase *m_database = nullptr;
  HostSystemProbe m_probe;
  std::unique_ptr<NetworkDownloader> m_downloader;        // Proton archives (the job)
  std::unique_ptr<NetworkDownloader> m_releaseDownloader; // the GitHub release list
  std::unique_ptr<QProcessRunner> m_runner;
  ProtonInstallJob *m_installJob = nullptr; // owned
  QString m_userDefault;
  QString m_releaseFile;
  QVector<GeProtonRelease> m_releases;
  bool m_fetchingReleases = false;
  bool m_busy = false;
  double m_progress = 0.0;
  QString m_stageText;
  QString m_resultMessage;
  QString m_details;
};

} // namespace QindaQt::QindaLutris
