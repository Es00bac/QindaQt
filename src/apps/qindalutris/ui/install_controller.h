// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compat_db.h"
#include "install_preflight.h"
#include "setup_file_install_job.h"
#include "store_game_install_job.h"
#include "title_factory.h"

#include <QObject>
#include <QVariantList>

#include <memory>
#include <optional>

namespace QindaQt::QindaLutris {

class FixApplier;
class LibraryController;
class LauncherInstallJob;
class NetworkDownloader;
class QProcessRunner;
struct LauncherInstallResult;

// AGENT-CONTRACT: the "Get games" side of QindaLutris (ADR-0275 sections
// 4-6), exposed to QML as `Installs`. One job at a time: a store launcher
// from a recipe, a game from the user's own setup file, or adopting a prefix
// that already exists (e.g. ~/Games/battlenet). Every flow ends in ONE plain
// sentence (`resultMessage`) plus `details` for "Copy details"; a finished
// install is appended to titles-v1.json (title_factory) and the library is
// refreshed. The pin is chosen once, at install time, and never moved here.
class InstallController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList stores READ stores NOTIFY storesChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
  Q_PROPERTY(QString stageText READ stageText NOTIFY stateChanged)
  Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY stateChanged)
  Q_PROPERTY(QString resultNote READ resultNote NOTIFY stateChanged)
  Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY stateChanged)
  Q_PROPERTY(QString details READ details NOTIFY stateChanged)
  Q_PROPERTY(QVariantList setupCandidates READ setupCandidates NOTIFY stateChanged)
public:
  InstallController(LibraryController *library, const CompatDatabase *database,
                    QObject *parent = nullptr);
  ~InstallController() override;

  [[nodiscard]] QVariantList stores() const;
  [[nodiscard]] bool busy() const { return m_busy; }
  [[nodiscard]] double progress() const { return m_progress; }
  [[nodiscard]] QString stageText() const { return m_stageText; }
  [[nodiscard]] QString resultMessage() const { return m_resultMessage; }
  [[nodiscard]] QString resultNote() const { return m_resultNote; }
  [[nodiscard]] bool lastSucceeded() const { return m_lastSucceeded; }
  [[nodiscard]] QString details() const { return m_details; }
  [[nodiscard]] QVariantList setupCandidates() const { return m_candidateRows; }

  Q_INVOKABLE void installStore(const QString &recipeId);
  Q_INVOKABLE void installSetupFile(const QString &title, const QString &installerPath);
  // After a setup-file install: the program the user confirms is the game.
  Q_INVOKABLE bool confirmSetupCandidate(const QString &executablePath);
  // Registers an existing prefix that already contains the recipe's launcher,
  // pinned to the build its own `version` file names when that is installed.
  Q_INVOKABLE bool adoptExistingLauncher(const QString &recipeId, const QString &prefixPath);
  // Downloads a game the user owns on Epic, GOG or Amazon (Accounts rows)
  // with the store's client, then registers it like any other install:
  // a new prefix, the build chosen now, the database's fixes.
  Q_INVOKABLE void installOwnedGame(const QString &storeId, const QString &gameId,
                                    const QString &title);
  Q_INVOKABLE void cancel();
  // "Copy details" (ADR-0275 section 5): puts a job's log on the clipboard.
  Q_INVOKABLE void copyText(const QString &text) const;
  // The verdict card (compat_advice_view.h) for a library game map
  // (Library.selectedGame) or for a store launcher recipe.
  Q_INVOKABLE QVariantMap verdictForGame(const QVariantMap &game) const;
  Q_INVOKABLE QVariantMap verdictForStore(const QString &recipeId) const;

Q_SIGNALS:
  void storesChanged();
  void stateChanged();

private:
  void begin(const QString &stageText);
  void finish(bool ok, const QString &message, const QString &note, const QString &details);
  void onLauncherFinished(const LauncherInstallResult &result);
  void onSetupFinished(const SetupFileInstallResult &result);
  void onStoreGameFinished(const StoreGameInstallResult &result);
  [[nodiscard]] bool registerTitle(const NewTitle &facts, const QString &buildName,
                                   const QString &buildVersion, QString *error);
  [[nodiscard]] QStringList takenTitleIds() const;
  // After a successful install: applies the database's one-time fixes when
  // there are any (FixApplier), then registers the title.
  void completeInstall(NewTitle facts, const QString &buildName, const QString &buildVersion,
                       const std::optional<CompatAdvice> &advice, const QString &message,
                       const QString &note);
  void onFixesFinished(bool ok, const QString &details);

  LibraryController *m_library = nullptr;
  const CompatDatabase *m_database = nullptr;
  HostSystemProbe m_probe;
  std::unique_ptr<NetworkDownloader> m_downloader;
  std::unique_ptr<QProcessRunner> m_runner;
  LauncherInstallJob *m_launcherJob = nullptr; // owned child
  SetupFileInstallJob *m_setupJob = nullptr;   // owned child
  StoreGameInstallJob *m_storeJob = nullptr;   // owned child
  struct PendingStoreGame {
    NewTitle facts;
    QString buildName;
    QString buildVersion;
    std::optional<CompatAdvice> advice;
  };
  std::optional<PendingStoreGame> m_storeGame;
  FixApplier *m_fixes = nullptr;                // owned child
  struct PendingTitle {
    NewTitle facts;
    QString buildName;
    QString buildVersion;
    QStringList verbs;
    QString message;
    QString note;
  };
  std::optional<PendingTitle> m_pending; // waiting for fixes to finish
  QString m_pendingRecipe;
  SetupFileInstallResult m_setupResult;
  QVariantList m_candidateRows;
  bool m_busy = false;
  double m_progress = 0.0;
  QString m_stageText;
  QString m_resultMessage;
  QString m_resultNote;
  QString m_details;
  bool m_lastSucceeded = false;
};

} // namespace QindaQt::QindaLutris
