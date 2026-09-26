// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_controller.h"

#include "../core/steam_source.h"
#include "../core/lutris_source.h"
#include "../core/title_source.h"
#include "../core/title_store.h"
#include "../core/umu_launch.h"
#include "../core/wine_pin_migration.h"
#include "../core/wine_source.h"
#include "game_image_resolver.h"
#include "game_list_model.h"
#include "proton_choices.h"
#include "store_accounts.h"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>

namespace QindaQt::QindaLutris {
namespace {

QString xdgOr(const QStandardPaths::StandardLocation location,
              const QString &fallback) {
  const QStringList paths = QStandardPaths::standardLocations(location);
  return paths.isEmpty() ? fallback : paths.first();
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
  m_protonRoots = defaultProtonRoots(
      home, qEnvironmentVariable("XDG_DATA_HOME"), m_steamCandidates);
  m_umuSearchPath = defaultUmuSearchPath(home, m_wineSearchPath);

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
  // AGENT-NOTE: a refused titles-v1.json loads NO titles (ADR-0275 whole
  // refusal) and is never rewritten from here -- confirm finds no title to
  // change -- so the operator's document survives for repair.
  error = LibraryStore::Error::None;
  m_titles = TitleStore(m_configRoot).readTitles(&error);
  if (error == LibraryStore::Error::Refused && m_statusMessage.isEmpty()) {
    m_statusMessage = QStringLiteral(
        "The installed games file was not readable; those games are hidden");
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

void LibraryController::setProtonRoots(const QVector<ProtonRoot> &roots) {
  m_protonRoots = roots;
}

void LibraryController::setUmuSearchPath(const QStringList &directories) {
  m_umuSearchPath = directories;
}

void LibraryController::setPreferredProtonBuild(const QString &name) {
  m_preferredProtonBuild = name;
}

QAbstractItemModel *LibraryController::gameModel() {
  return m_filter;
}

int LibraryController::totalCount() const {
  return int(m_library.games.size());
}

QStringList LibraryController::sourcesPresent() const {
  QStringList out;
  for (const Game &game : m_library.games) {
    const QString id = gameSourceId(game.source);
    if (!out.contains(id)) {
      out.append(id);
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
    const QSize pixels = screen->size() * screen->devicePixelRatio();
    target.widthPx = pixels.width();
    target.heightPx = pixels.height();
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
  m_tools.gamescopeBinary = QStandardPaths::findExecutable(QStringLiteral("gamescope"));
  m_tools.umuRunBinary = discoverUmuRun(m_umuSearchPath);
  m_tools.protonBuilds = discoverProtonBuilds(m_protonRoots);
  m_tools.storeClients = discoverStoreClients(m_configRoot);
}

void LibraryController::refresh() {
  m_refreshing = true;
  Q_EMIT refreshingChanged();
  rebuildToolSet();
  migrateWinePins();
  const SteamDiscovery steam = scanSteamLibraries(m_steamCandidates);
  const LutrisDiscovery lutris = scanLutrisDatabase(m_lutrisDbPath);
  m_desktop = scanDesktopGames(m_desktopRoots);
  const QVector<Game> wine = gamesFromWineEntries(m_wineRecords, m_coverCacheDir);
  const QVector<Game> installed =
      gamesFromTitleRecords(m_titles, m_coverCacheDir);
  m_library = mergeGameSources(steam.games, lutris.games, m_desktop.games, wine,
                               installed,
                               steam.warnings + lutris.warnings
                                   + m_desktop.warnings);
  m_model->setGames(m_library.games);
  // The status line reports the refresh that just ran; a clean refresh
  // clears it. Store-refusal messages from construction stand until the
  // first refresh's own outcome replaces them.
  QString message =
      m_library.warnings.isEmpty() ? QString() : m_library.warnings.first();
  if (!m_pinNotes.isEmpty()) { // a one-time pin outranks a scan warning
    message = m_pinNotes.mid(0, 3).join(QStringLiteral("; "));
    m_pinNotes.clear();
  }
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

const TitleRecord *LibraryController::findTitle(const QString &titleId) const {
  for (const TitleRecord &title : m_titles) {
    if (title.id == titleId) {
      return &title;
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
  out.insert(QStringLiteral("sourceLabel"), gameSourceLabel(game->source));
  out.insert(QStringLiteral("installPath"), game->installPath);
  out.insert(QStringLiteral("sizeText"), sizeText(*game));
  out.insert(QStringLiteral("coverUrl"),
             QStringLiteral("image://gameicon/") + game->id);
  out.insert(QStringLiteral("winePrefix"), game->winePrefix);
  out.insert(QStringLiteral("wineRunner"), wineRunnerId(game->wineRunner));
  out.insert(QStringLiteral("protonBuild"), game->protonPath);
  return out;
}

LaunchOptions LibraryController::optionsFor(const QString &gameId) const {
  return m_options.value(gameId);
}

LaunchPlan LibraryController::planFor(const Game &game) const {
  // AGENT-CONTRACT: Installed titles plan from their TitleRecord (ADR-0275);
  // planGameLaunch refuses them because a Game does not carry the record.
  if (const TitleRecord *title = game.source == GameSource::Installed
                                    ? findTitle(game.id) : nullptr) {
    return planTitleLaunch(*title, optionsFor(game.id), m_tools, m_displays);
  }
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
  Q_EMIT gameLaunched(game->id);
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
  out.insert(QStringLiteral("ownScreen"), options.ownScreen);
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
  options.ownScreen = values.value(QStringLiteral("ownScreen")).toBool();
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
  // A runner override to Proton on an unpinned entry records a pin now.
  if (migrateWinePins()) {
    refresh();
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
  if (!runner.has_value()
      || (*runner == WineRunner::Proton && prefixPath.trimmed().isEmpty())) {
    return false; // umu needs a prefix to create or reuse
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
  // ADR-0275: a new entry records ONE concrete build by name -- the caller's
  // choice, else the default -- even for Wine, so a later switch to Proton
  // has a pin. An alias or uninstalled build refuses the add.
  const std::optional<ProtonPin> pin = pinForNewEntry(
      protonPath, m_tools.protonBuilds, m_preferredProtonBuild);
  if (!pin.has_value()
      && (*runner == WineRunner::Proton || !protonPath.trimmed().isEmpty())) {
    return false;
  }
  record.protonPath = pin.has_value() ? pin->name : QString();
  record.protonVersion = pin.has_value() ? pin->version : QString();
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

bool LibraryController::migrateWinePins() {
  const WinePinMigration migration = migrateWineEntryPins(
      m_wineRecords, m_options, m_tools.protonBuilds, m_preferredProtonBuild);
  if (!migration.changed) {
    return false;
  }
  m_wineRecords = migration.records;
  persistWineEntries();
  m_pinNotes += migration.notes;
  return true;
}

QVariantList LibraryController::protonChoices() const {
  return protonChoicesFor(
      m_tools.protonBuilds,
      chooseDefaultBuild(m_tools.protonBuilds, m_preferredProtonBuild));
}

bool LibraryController::confirmProtonBuildForSelected() {
  for (TitleRecord &title : m_titles) {
    if (title.id != m_selectedGameId) continue;
    const std::optional<ProtonPin> pin =
        confirmPinnedBuild(title.protonBuild, m_tools.protonBuilds);
    if (!pin.has_value()) return false;
    title.protonBuildVersion = pin->version;
    if (TitleStore(m_configRoot).writeTitles(m_titles) != TitleStore::Error::None) {
      Q_EMIT storeError(QStringLiteral("could not save the installed games"));
    }
    refresh();
    return true;
  }
  for (WineEntryRecord &record : m_wineRecords) {
    if (QStringLiteral("wine/") + record.slug != m_selectedGameId) continue;
    const std::optional<ProtonPin> pin =
        confirmPinnedBuild(record.protonPath, m_tools.protonBuilds);
    if (!pin.has_value()) return false;
    record.protonVersion = pin->version;
    persistWineEntries();
    refresh();
    return true;
  }
  return false;
}

QImage LibraryController::imageForGame(const QString &gameId) const {
  const Game *game = findGame(gameId);
  return game == nullptr ? QImage() : resolveGameImage(*game);
}

} // namespace QindaQt::QindaLutris
