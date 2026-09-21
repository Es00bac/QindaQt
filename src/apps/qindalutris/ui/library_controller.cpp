// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_controller.h"

#include "../core/steam_source.h"
#include "../core/lutris_source.h"
#include "../core/wine_source.h"
#include "game_list_model.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QImageReader>
#include <QScreen>
#include <QStandardPaths>

namespace QindaQt::QindaLutris {
namespace {

QString xdgOr(const QStandardPaths::StandardLocation location,
              const QString &fallback) {
  const QStringList paths = QStandardPaths::standardLocations(location);
  return paths.isEmpty() ? fallback : paths.first();
}

QString sourceLabelFor(GameSource source) {
  switch (source) {
  case GameSource::Steam: return QStringLiteral("Steam");
  case GameSource::Lutris: return QStringLiteral("Lutris");
  case GameSource::Desktop: return QStringLiteral("Native");
  case GameSource::Wine: return QStringLiteral("Wine");
  }
  Q_UNREACHABLE();
}

QString sizeText(const Game &game) {
  if (!game.installSizeBytes.has_value()) {
    return QStringLiteral("Not tracked");
  }
  const double mib = double(*game.installSizeBytes) / (1024.0 * 1024.0);
  if (mib >= 1024.0) {
    return QStringLiteral("%1 GiB").arg(mib / 1024.0, 0, 'f', 1);
  }
  if (mib >= 1.0) {
    return QStringLiteral("%1 MiB").arg(mib, 0, 'f', 1);
  }
  return QStringLiteral("%1 KiB")
      .arg(double(*game.installSizeBytes) / 1024.0, 0, 'f', 1);
}

} // namespace

LibraryController::LibraryController(QObject *parent) : QObject(parent) {
  m_model = new GameListModel(this);
  m_filter = new GameFilterModel(this);
  m_filter->setSourceModel(m_model);
  m_ownedLauncher = std::make_unique<QProcessGameLauncher>();
  m_launcher = m_ownedLauncher.get();

  const QString home = QDir::homePath();
  m_steamCandidates = {
      home + QStringLiteral("/.steam/root"),
      home + QStringLiteral("/.local/share/Steam"),
      home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
  };
  m_wineSearchPath = QString::fromLocal8Bit(qgetenv("PATH"))
                         .split(QLatin1Char(':'), Qt::SkipEmptyParts);
  m_lutrisDbPath = xdgOr(QStandardPaths::GenericDataLocation,
                         home + QStringLiteral("/.local/share"))
                       + QStringLiteral("/lutris/pga.db");
  // The scanner wants data roots (it appends applications/), not the
  // applications dirs themselves.
  m_desktopRoots = QStandardPaths::standardLocations(
      QStandardPaths::GenericDataLocation);
  m_configRoot = xdgOr(QStandardPaths::GenericConfigLocation,
                       home + QStringLiteral("/.config"))
                     + QStringLiteral("/qindaqt/qindalutris");
  m_coverCacheDir = xdgOr(QStandardPaths::GenericCacheLocation,
                          home + QStringLiteral("/.cache"))
                        + QStringLiteral("/qindaqt/qindalutris/covers");
  m_protonRoots = m_steamCandidates;

  rebuildDisplays();
  connect(qApp, &QGuiApplication::screenAdded, this, [this] {
    rebuildDisplays();
    Q_EMIT displaysChanged();
  });
  connect(qApp, &QGuiApplication::screenRemoved, this, [this] {
    rebuildDisplays();
    Q_EMIT displaysChanged();
  });

  loadPersistedState();
}

// Durable state loads once per injected config root; a refused document
// leaves defaults standing and says so once, in the status bar (ADR-0231).
void LibraryController::loadPersistedState() {
  LibraryStore store(m_configRoot);
  LibraryStore::Error error = LibraryStore::Error::None;
  m_wineRecords = store.readWineEntries(&error);
  if (error == LibraryStore::Error::Refused) {
    m_statusMessage = QStringLiteral(
        "The saved Wine games file was not readable; starting without it");
  }
  error = LibraryStore::Error::None;
  m_options = store.readLaunchOptions(&error);
  if (error == LibraryStore::Error::Refused && m_statusMessage.isEmpty()) {
    m_statusMessage = QStringLiteral(
        "The saved launch options file was not readable; using defaults");
  }
}

LibraryController::~LibraryController() = default;

void LibraryController::setSteamCandidates(const QStringList &roots) {
  m_steamCandidates = roots;
}

void LibraryController::setLutrisDatabasePath(const QString &path) {
  m_lutrisDbPath = path;
}

void LibraryController::setDesktopDataRoots(const QStringList &roots) {
  m_desktopRoots = roots;
}

void LibraryController::setWineLoaderSearchPath(const QStringList &directories) {
  m_wineSearchPath = directories;
}

void LibraryController::setConfigRoot(const QString &path) {
  m_configRoot = path;
  // An injection point (tests, --config-root): the state that belongs to
  // this root must be the state the controller holds.
  loadPersistedState();
}

void LibraryController::setCoverCacheDir(const QString &path) {
  m_coverCacheDir = path;
}

void LibraryController::setProcessLauncher(GameProcessLauncher *launcher) {
  m_launcher = launcher != nullptr ? launcher : m_ownedLauncher.get();
}

void LibraryController::setSteamRootsForProton(const QStringList &roots) {
  m_protonRoots = roots;
}

QAbstractItemModel *LibraryController::gameModel() {
  return m_filter;
}

int LibraryController::totalCount() const {
  return int(m_library.games.size());
}

QStringList LibraryController::sourcesPresent() const {
  QStringList out;
  bool seen[4] = {false, false, false, false};
  for (const Game &game : m_library.games) {
    const int index = int(game.source);
    if (!seen[index]) {
      seen[index] = true;
      out.append(gameSourceId(game.source));
    }
  }
  return out;
}

QVariantList LibraryController::displays() const {
  QVariantList out;
  for (const DisplayTarget &display : m_displays) {
    QVariantMap entry;
    entry.insert(QStringLiteral("key"), display.key);
    entry.insert(QStringLiteral("label"), display.label);
    out.append(entry);
  }
  return out;
}

void LibraryController::rebuildDisplays() {
  m_displays.clear();
  const QList<QScreen *> screens = QGuiApplication::screens();
  for (int i = 0; i < screens.size(); ++i) {
    QScreen *screen = screens.at(i);
    DisplayTarget target;
    target.key = screen->name();
    target.label = screen->manufacturer().isEmpty()
                       ? screen->name()
                       : QStringLiteral("%1 (%2 %3)")
                             .arg(screen->name(), screen->manufacturer(),
                                  screen->model())
                             .trimmed();
    target.sdlDisplayIndex = i;
    m_displays.append(target);
  }
}

void LibraryController::rebuildToolSet() {
  m_tools.steamBinary = QStandardPaths::findExecutable(QStringLiteral("steam"));
  m_tools.lutrisBinary = QStandardPaths::findExecutable(QStringLiteral("lutris"));
  // Not findExecutable("wine"): Gentoo's wine-proton ships only versioned
  // loaders, so that call returned empty on a machine with a complete Wine
  // stack and every Wine game refused to launch with "Wine is not installed".
  m_tools.wineBinary = discoverWineLoader(m_wineSearchPath);
  m_tools.gamemodeRunBinary =
      QStandardPaths::findExecutable(QStringLiteral("gamemoderun"));
  m_tools.mangohudBinary =
      QStandardPaths::findExecutable(QStringLiteral("mangohud"));
  m_tools.protons = discoverProtonInstalls(m_protonRoots);
}

void LibraryController::refresh() {
  m_refreshing = true;
  Q_EMIT refreshingChanged();
  rebuildToolSet();
  const SteamDiscovery steam = scanSteamLibraries(m_steamCandidates);
  const LutrisDiscovery lutris = scanLutrisDatabase(m_lutrisDbPath);
  m_desktop = scanDesktopGames(m_desktopRoots);
  const QVector<Game> wine = gamesFromWineEntries(m_wineRecords, m_coverCacheDir);
  m_library = mergeGameSources(steam.games, lutris.games, m_desktop.games, wine,
                               steam.warnings + lutris.warnings
                                   + m_desktop.warnings);
  m_model->setGames(m_library.games);
  // The status line reports the refresh that just ran; a clean refresh
  // clears it. Store-refusal messages from construction stand until the
  // first refresh's own outcome replaces them.
  const QString message =
      m_library.warnings.isEmpty() ? QString() : m_library.warnings.first();
  const bool messageChanged = message != m_statusMessage;
  m_statusMessage = message;
  if (!m_selectedGameId.isEmpty() && findGame(m_selectedGameId) == nullptr) {
    m_selectedGameId.clear();
    Q_EMIT selectedGameChanged();
  }
  m_refreshing = false;
  Q_EMIT refreshingChanged();
  Q_EMIT libraryChanged();
  if (messageChanged) {
    Q_EMIT statusMessageChanged();
  }
}

const Game *LibraryController::findGame(const QString &gameId) const {
  for (const Game &game : m_library.games) {
    if (game.id == gameId) {
      return &game;
    }
  }
  return nullptr;
}

void LibraryController::selectGame(const QString &gameId) {
  if (m_selectedGameId == gameId) {
    return;
  }
  m_selectedGameId = gameId;
  Q_EMIT selectedGameChanged();
}

QVariantMap LibraryController::selectedGame() const {
  QVariantMap out;
  const Game *game = findGame(m_selectedGameId);
  if (game == nullptr) {
    return out;
  }
  out.insert(QStringLiteral("id"), game->id);
  out.insert(QStringLiteral("title"), game->title);
  out.insert(QStringLiteral("sourceId"), gameSourceId(game->source));
  out.insert(QStringLiteral("sourceLabel"), sourceLabelFor(game->source));
  out.insert(QStringLiteral("installPath"), game->installPath);
  out.insert(QStringLiteral("sizeText"), sizeText(*game));
  out.insert(QStringLiteral("coverUrl"),
             QStringLiteral("image://gameicon/") + game->id);
  out.insert(QStringLiteral("winePrefix"), game->winePrefix);
  out.insert(QStringLiteral("wineRunner"), wineRunnerId(game->wineRunner));
  return out;
}

LaunchOptions LibraryController::optionsFor(const QString &gameId) const {
  return m_options.value(gameId);
}

LaunchPlan LibraryController::planFor(const Game &game) const {
  return planGameLaunch(game, optionsFor(game.id), m_tools, m_displays,
                        &m_desktop);
}

bool LibraryController::selectedPlayable() const {
  const Game *game = findGame(m_selectedGameId);
  return game != nullptr && planFor(*game).ok;
}

QString LibraryController::selectedPlayReason() const {
  const Game *game = findGame(m_selectedGameId);
  if (game == nullptr) {
    return {};
  }
  return planFor(*game).reason;
}

void LibraryController::playSelected() {
  const Game *game = findGame(m_selectedGameId);
  if (game == nullptr) {
    return;
  }
  const LaunchPlan plan = planFor(*game);
  if (!plan.ok) {
    Q_EMIT launchFailed(plan.reason);
    return;
  }
  const LaunchOutcome outcome = m_launcher->launch(plan);
  if (!outcome.ok) {
    Q_EMIT launchFailed(outcome.diagnostic.isEmpty()
                            ? QStringLiteral("the game did not start")
                            : outcome.diagnostic);
    return;
  }
  if (!plan.notes.isEmpty()) {
    m_statusMessage = plan.notes.first();
    Q_EMIT statusMessageChanged();
  }
}

QVariantMap LibraryController::launchOptionsForSelected() const {
  QVariantMap out;
  const LaunchOptions options = optionsFor(m_selectedGameId);
  out.insert(QStringLiteral("gamemode"), options.gamemode);
  out.insert(QStringLiteral("mangohud"), options.mangohud);
  out.insert(QStringLiteral("display"), options.targetDisplay);
  out.insert(QStringLiteral("environment"),
             options.extraEnvironment.join(QLatin1Char('\n')));
  out.insert(QStringLiteral("runner"),
             options.runnerOverride.has_value()
                 ? wineRunnerId(*options.runnerOverride) : QString());
  out.insert(QStringLiteral("prefixOverride"), options.prefixOverride);
  return out;
}

void LibraryController::saveLaunchOptionsForSelected(const QVariantMap &values) {
  if (m_selectedGameId.isEmpty()) {
    return;
  }
  LaunchOptions options;
  options.gamemode = values.value(QStringLiteral("gamemode")).toBool();
  options.mangohud = values.value(QStringLiteral("mangohud")).toBool();
  options.targetDisplay =
      values.value(QStringLiteral("display")).toString().left(256);
  const QString environment =
      values.value(QStringLiteral("environment")).toString();
  const QStringList lines = environment.split(QLatin1Char('\n'));
  for (const QString &raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    if (options.extraEnvironment.size() >= kMaxExtraEnvironmentEntries) {
      break;
    }
    if (isValidEnvironmentAssignment(line)) {
      options.extraEnvironment.append(line);
    }
  }
  const QString runner = values.value(QStringLiteral("runner")).toString();
  if (const std::optional<WineRunner> parsed = wineRunnerForId(runner)) {
    options.runnerOverride = *parsed;
  }
  options.prefixOverride =
      values.value(QStringLiteral("prefixOverride")).toString().left(4096);
  m_options.insert(m_selectedGameId, options);
  LibraryStore store(m_configRoot);
  if (store.writeLaunchOptions(m_options) != LibraryStore::Error::None) {
    Q_EMIT storeError(QStringLiteral("could not save the launch options"));
  }
  Q_EMIT selectedGameChanged();
}

bool LibraryController::addWineGame(const QString &title,
                                    const QString &executablePath,
                                    const QString &prefixPath,
                                    const QString &runnerId,
                                    const QString &protonPath) {
  if (m_wineRecords.size() >= kMaxWineEntries) {
    return false;
  }
  const QString cleanTitle = title.trimmed().left(kMaxGameTitleChars);
  if (cleanTitle.isEmpty() || executablePath.isEmpty()) {
    return false;
  }
  const std::optional<WineRunner> runner = wineRunnerForId(runnerId);
  if (!runner.has_value()) {
    return false;
  }
  WineEntryRecord record;
  record.title = cleanTitle;
  record.slug = wineSlugFor(cleanTitle, executablePath);
  for (const WineEntryRecord &existing : m_wineRecords) {
    if (existing.slug == record.slug) {
      return false; // already added
    }
  }
  record.executablePath = executablePath;
  record.prefixPath = prefixPath;
  record.runner = *runner;
  record.protonPath = protonPath;
  m_wineRecords.append(record);
  persistWineEntries();
  refresh();
  selectGame(QStringLiteral("wine/%1").arg(record.slug));
  return true;
}

void LibraryController::removeWineGame(const QString &gameId) {
  const QString slug = gameId.startsWith(QLatin1String("wine/"))
                           ? gameId.mid(5) : QString();
  for (int i = 0; i < m_wineRecords.size(); ++i) {
    if (m_wineRecords.at(i).slug == slug) {
      m_wineRecords.removeAt(i);
      persistWineEntries();
      refresh();
      return;
    }
  }
}

void LibraryController::persistWineEntries() {
  LibraryStore store(m_configRoot);
  if (store.writeWineEntries(m_wineRecords) != LibraryStore::Error::None) {
    Q_EMIT storeError(QStringLiteral("could not save the Wine games"));
  }
}

QVariantList LibraryController::protonChoices() const {
  QVariantList out;
  for (const ProtonInstall &proton : m_tools.protons) {
    QVariantMap entry;
    entry.insert(QStringLiteral("name"), proton.name);
    entry.insert(QStringLiteral("path"), proton.protonScript);
    out.append(entry);
  }
  return out;
}

QImage LibraryController::imageForGame(const QString &gameId) const {
  const Game *game = findGame(gameId);
  if (game == nullptr) {
    return {};
  }
  if (!game->coverPath.isEmpty()) {
    QImageReader reader(game->coverPath);
    reader.setAllocationLimit(64); // MiB; hostile art fails, not the session
    reader.setScaledSize(QSize(512, 512));
    return reader.read();
  }
  if (!game->iconName.isEmpty()) {
    const QIcon icon = QIcon::fromTheme(game->iconName);
    if (!icon.isNull()) {
      return icon.pixmap(256, 256).toImage();
    }
    // AGENT-NOTE: a session with no platform icon theme (bare offscreen
    // runs, minimal sessions) leaves QIcon::fromTheme with nowhere to look.
    // The bounded fallback below checks fixed hicolor/pixmaps paths by name;
    // it never walks a directory and never follows a symlink.
    for (const QString &base : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
      for (const QLatin1String size :
           {QLatin1String("256x256"), QLatin1String("128x128"),
            QLatin1String("64x64"), QLatin1String("48x48"),
            QLatin1String("scalable")}) {
        for (const QLatin1String ext :
             {QLatin1String("png"), QLatin1String("svg")}) {
          const QString candidate = base
              + QStringLiteral("/icons/hicolor/") + size
              + QStringLiteral("/apps/") + game->iconName + QLatin1Char('.') + ext;
          const QFileInfo info(candidate);
          if (info.isFile() && !info.isSymLink()
              && info.size() < qint64(16) * 1024 * 1024) {
            QImageReader reader(candidate);
            reader.setAllocationLimit(64);
            reader.setScaledSize(QSize(512, 512));
            const QImage image = reader.read();
            if (!image.isNull()) {
              return image;
            }
          }
        }
      }
    }
  }
  return {};
}

} // namespace QindaQt::QindaLutris
