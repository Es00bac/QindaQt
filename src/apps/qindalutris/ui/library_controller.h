// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../core/desktop_source.h"
#include "../core/game_launcher.h"
#include "../core/game_library.h"
#include "../core/library_store.h"
#include "../core/proton_pin.h"
#include "../core/title_record.h"

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
  // Roots searched for Proton builds (ADR-0275), in precedence order.
  // Defaults to defaultProtonRoots(home, $XDG_DATA_HOME, Steam candidates).
  void setProtonRoots(const QVector<ProtonRoot> &roots);
  // Directories searched for umu-run, highest preference first. Defaults to
  // defaultUmuSearchPath(home, PATH): /usr/bin, PATH, ~/.local/bin.
  void setUmuSearchPath(const QStringList &directories);
  // The build name new hand-added entries pin when the caller names none
  // (the compatibility database's recommendation, once that lands).
  // chooseDefaultBuild falls back to the first System build.
  void setPreferredProtonBuild(const QString &name);
  // Directories searched for the Wine loader, highest preference first.
  // Defaults to PATH. An injection point rather than a way to set the binary
  // directly, so a test exercises the real discovery against its own fixture
  // instead of depending on whatever wine the host happens to have.
  void setWineLoaderSearchPath(const QStringList &directories);

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
  // Every catalog build for the add dialog, default first; row shape in
  // proton_choices.h. The dialog hands `path` back to addWineGame, which
  // records the build NAME and VERSION (ADR-0275 identity).
  Q_INVOKABLE QVariantList protonChoices() const;
  // The explicit re-pin after "<build> has changed since this game was set
  // up": records the selected hand-added entry's or installed title's pinned
  // build as it is NOW (confirmPinnedBuild) and persists it. False when the
  // selection has no pin that names an installed, pinnable build.
  Q_INVOKABLE bool confirmProtonBuildForSelected();

  // Read-only views for the install and Proton-manager controllers, which
  // share this root's discovery instead of scanning twice (ADR-0275).
  [[nodiscard]] const LaunchToolSet &toolSet() const { return m_tools; }
  [[nodiscard]] QString configRoot() const { return m_configRoot; }
  [[nodiscard]] QString preferredProtonBuild() const { return m_preferredProtonBuild; }
  [[nodiscard]] const QVector<TitleRecord> &titles() const { return m_titles; }
  [[nodiscard]] const QVector<WineEntryRecord> &wineRecords() const { return m_wineRecords; }

  // Cover/icon resolution for the image provider. Null when neither exists.
  [[nodiscard]] QImage imageForGame(const QString &gameId) const;

Q_SIGNALS:
  void refreshingChanged();
  void statusMessageChanged();
  void libraryChanged();
  void displaysChanged();
  void selectedGameChanged();
  void launchFailed(const QString &message);
  // A plan was handed to the process launcher successfully (Force quit and
  // the running-games view key their tracking on this).
  void gameLaunched(const QString &gameId);
  void storeError(const QString &message);

private:
  void loadPersistedState();
  void rebuildToolSet();
  void rebuildDisplays();
  void persistWineEntries();
  bool migrateWinePins(); // true when records changed (see wine_pin_migration.h)
  [[nodiscard]] const Game *findGame(const QString &gameId) const;
  [[nodiscard]] const TitleRecord *findTitle(const QString &titleId) const;
  [[nodiscard]] LaunchOptions optionsFor(const QString &gameId) const;
  [[nodiscard]] LaunchPlan planFor(const Game &game) const;

  QStringList m_steamCandidates;
  QString m_lutrisDbPath;
  QStringList m_desktopRoots;
  QString m_configRoot;
  QString m_coverCacheDir;
  QVector<ProtonRoot> m_protonRoots;
  QStringList m_umuSearchPath;
  QString m_preferredProtonBuild;

  GameListModel *m_model = nullptr;    // owned child
  GameFilterModel *m_filter = nullptr; // owned child
  GameLibrary m_library;
  DesktopDiscovery m_desktop;
  LaunchToolSet m_tools;
  QStringList m_wineSearchPath;
  QVector<DisplayTarget> m_displays;
  std::unique_ptr<QProcessGameLauncher> m_ownedLauncher;
  GameProcessLauncher *m_launcher = nullptr;
  QVector<WineEntryRecord> m_wineRecords;
  QVector<TitleRecord> m_titles; // titles-v1.json; written only on confirm
  QStringList m_pinNotes;         // migration notes for the next status line
  QHash<QString, LaunchOptions> m_options;
  QString m_selectedGameId;
  QString m_statusMessage;
  bool m_refreshing = false;
};

} // namespace QindaQt::QindaLutris
