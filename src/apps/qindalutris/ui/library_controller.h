// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/desktop_source.h"
#include "../core/game_launcher.h"
#include "../core/game_library.h"
#include "../core/library_store.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

class QGuiApplication;

namespace QindaQt::QindaLutris {

class GameListModel;
class GameFilterModel;

// AGENT-CONTRACT: the composition root of QindaLutris (ADR-0231). It owns
// the injected roots/paths (test-injectable; production resolves XDG), runs
// the bounded source scans, keeps the retained desktop documents needed by
// launch planning, discovers host tools, enumerates displays through
// QScreen -- the public platform seam, never the compositor -- and mediates
// every mutation of the app-local store. Scans are synchronous and capped;
// the model contract keeps them cheap enough to run on the GUI thread.
class LibraryController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QAbstractItemModel *gameModel READ gameModel CONSTANT)
  Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
  Q_PROPERTY(int totalCount READ totalCount NOTIFY libraryChanged)
  Q_PROPERTY(QStringList sourcesPresent READ sourcesPresent NOTIFY libraryChanged)
  Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
  Q_PROPERTY(QString selectedGameId READ selectedGameId NOTIFY selectedGameChanged)
  Q_PROPERTY(QVariantMap selectedGame READ selectedGame NOTIFY selectedGameChanged)
  Q_PROPERTY(bool selectedPlayable READ selectedPlayable NOTIFY selectedGameChanged)
  Q_PROPERTY(QString selectedPlayReason READ selectedPlayReason NOTIFY selectedGameChanged)
public:
  explicit LibraryController(QObject *parent = nullptr);
  ~LibraryController() override;

  // Injectable seams (set before the first refresh; production main() uses
  // the XDG defaults resolved in the constructor).
  void setSteamCandidates(const QStringList &roots);
  void setLutrisDatabasePath(const QString &path);
  void setDesktopDataRoots(const QStringList &roots);
  void setConfigRoot(const QString &path);
  void setCoverCacheDir(const QString &path);
  void setProcessLauncher(GameProcessLauncher *launcher); // borrowed
  void setSteamRootsForProton(const QStringList &roots);

  QAbstractItemModel *gameModel();
  [[nodiscard]] bool refreshing() const { return m_refreshing; }
  [[nodiscard]] QString statusMessage() const { return m_statusMessage; }
  [[nodiscard]] int totalCount() const;
  [[nodiscard]] QStringList sourcesPresent() const;
  [[nodiscard]] QVariantList displays() const;
  [[nodiscard]] QString selectedGameId() const { return m_selectedGameId; }
  [[nodiscard]] QVariantMap selectedGame() const;
  [[nodiscard]] bool selectedPlayable() const;
  [[nodiscard]] QString selectedPlayReason() const;

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void selectGame(const QString &gameId);
  Q_INVOKABLE void playSelected();
  Q_INVOKABLE QVariantMap launchOptionsForSelected() const;
  Q_INVOKABLE void saveLaunchOptionsForSelected(const QVariantMap &options);
  Q_INVOKABLE bool addWineGame(const QString &title, const QString &executablePath,
                               const QString &prefixPath, const QString &runnerId,
                               const QString &protonPath);
  Q_INVOKABLE void removeWineGame(const QString &gameId);
  // Proton choices for the add dialog / options: [{name, path}] or empty.
  Q_INVOKABLE QVariantList protonChoices() const;

  // Cover/icon resolution for the image provider. Null when neither exists.
  [[nodiscard]] QImage imageForGame(const QString &gameId) const;

Q_SIGNALS:
  void refreshingChanged();
  void statusMessageChanged();
  void libraryChanged();
  void displaysChanged();
  void selectedGameChanged();
  void launchFailed(const QString &message);
  void storeError(const QString &message);

private:
  void loadPersistedState();
  void rebuildToolSet();
  void rebuildDisplays();
  void persistWineEntries();
  [[nodiscard]] const Game *findGame(const QString &gameId) const;
  [[nodiscard]] LaunchOptions optionsFor(const QString &gameId) const;
  [[nodiscard]] LaunchPlan planFor(const Game &game) const;

  QStringList m_steamCandidates;
  QString m_lutrisDbPath;
  QStringList m_desktopRoots;
  QString m_configRoot;
  QString m_coverCacheDir;
  QStringList m_protonRoots;

  GameListModel *m_model = nullptr;    // owned child
  GameFilterModel *m_filter = nullptr; // owned child
  GameLibrary m_library;
  DesktopDiscovery m_desktop;
  LaunchToolSet m_tools;
  QVector<DisplayTarget> m_displays;
  std::unique_ptr<QProcessGameLauncher> m_ownedLauncher;
  GameProcessLauncher *m_launcher = nullptr;
  QVector<WineEntryRecord> m_wineRecords;
  QHash<QString, LaunchOptions> m_options;
  QString m_selectedGameId;
  QString m_statusMessage;
  bool m_refreshing = false;
};

} // namespace QindaQt::QindaLutris
