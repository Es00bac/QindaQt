// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_recipes.h"

#include "download_allowlist.h"
#include "prefix_paths.h"

#include <QRegularExpression>

namespace QindaQt::QindaLutris {

namespace {

constexpr qint64 kGiB = qint64(1024) * 1024 * 1024;

QString str(const char *text) { return QString::fromUtf8(text); }

// AGENT-NOTE: sources for every field below, checked 2026-09-25.
//  - Installer URLs: verified live by the operator on 2026-09-25 (redirects:
//    Battle.net getInstaller -> same host; Epic's API host -> the
//    epicgames-download1.akamaized.net CDN).
//  - Arguments, launcher paths and notes: Lutris install scripts
//    (https://lutris.net/api/installers/<slug>): battlenet-standard,
//    ea-app-standard ("EAX_LAUNCH_CLIENT=0", return code 768 accepted,
//    versioned "EA Desktop/<version>/EA Desktop/EALauncher.exe" layout),
//    ubisoft-connect-latest ("/S"), epic-games-store-standard ("msiexec /i
//    <msi> /q", Win64 under "Program Files"), gog-galaxy-windows (no flags,
//    "Program Files/GOG Galaxy"), amazon-games-app-latest (no flags).
//    Battle.net, GOG Galaxy and Amazon Games have no documented silent flag,
//    so their installer windows are shown.
//  - Additional candidates: umu-launcher's README (Epic under
//    "Program Files (x86)/.../Win32"); the vendors' default Windows install
//    folders ("Program Files (x86)" for 32-bit Battle.net/GOG Galaxy/Epic).
//    UNVERIFIED under Proton: Amazon Games' own exe path (Lutris registers a
//    Desktop shortcut instead) and EADesktop.exe as a fallback to EALauncher.
//  - umuStore: umu-protonfixes has gamefixes-{battlenet,ea,ubisoft,egs,gog,
//    amazon}; umu-database has no entry for any launcher itself, so every
//    launcher runs as umu-0.
QVector<StoreRecipe> buildTable() {
  QVector<StoreRecipe> table;

  StoreRecipe battlenet;
  battlenet.id = str("battlenet");
  battlenet.displayName = str("Battle.net");
  battlenet.installerUrl = QUrl(str("https://downloader.battle.net/download/"
                                    "getInstaller?os=win&installer=Battle.net-Setup.exe"));
  battlenet.installerFileName = str("Battle.net-Setup.exe");
  battlenet.defaultPrefixDirName = str("battlenet");
  battlenet.umuId = str("umu-0");
  battlenet.umuStore = str("battlenet");
  battlenet.launcherExecutableCandidates = {
      str("C:\\Program Files (x86)\\Battle.net\\Battle.net Launcher.exe"),
      str("C:\\Program Files (x86)\\Battle.net\\Battle.net.exe")};
  battlenet.plainNotes = str(
      "The Battle.net installer opens its own window. If it asks you to sign in "
      "before it finishes, you can close that window: you will sign in the first "
      "time you start Battle.net.");
  battlenet.minimumFreeBytes = 2 * kGiB;
  table.append(battlenet);

  StoreRecipe ea;
  ea.id = str("ea");
  ea.displayName = str("EA app");
  ea.installerUrl = QUrl(str("https://origin-a.akamaihd.net/EA-Desktop-Client-Download/"
                             "installer-releases/EAappInstaller.exe"));
  ea.installerFileName = str("EAappInstaller.exe");
  ea.installerArguments = {str("EAX_LAUNCH_CLIENT=0")};
  ea.defaultPrefixDirName = str("ea-app");
  ea.umuId = str("umu-0");
  ea.umuStore = str("ea");
  ea.launcherExecutableCandidates = {
      str("C:\\Program Files\\Electronic Arts\\EA Desktop\\EA Desktop\\EALauncher.exe"),
      str("C:\\Program Files\\Electronic Arts\\EA Desktop\\EA Desktop\\EADesktop.exe"),
      str("C:\\Program Files\\Electronic Arts\\EA Desktop\\*\\EA Desktop\\EALauncher.exe"),
      str("C:\\Program Files\\Electronic Arts\\EA Desktop\\*\\EA Desktop\\EADesktop.exe")};
  ea.plainNotes = str(
      "The EA app installer sometimes reports an error even when it worked. "
      "QindaLutris checks for the EA app itself afterwards.");
  ea.minimumFreeBytes = 2 * kGiB;
  table.append(ea);

  StoreRecipe ubisoft;
  ubisoft.id = str("ubisoft");
  ubisoft.displayName = str("Ubisoft Connect");
  ubisoft.installerUrl = QUrl(
      str("https://static3.cdn.ubi.com/orbit/launcher_installer/UbisoftConnectInstaller.exe"));
  ubisoft.installerFileName = str("UbisoftConnectInstaller.exe");
  ubisoft.installerArguments = {str("/S")};
  ubisoft.defaultPrefixDirName = str("ubisoft-connect");
  ubisoft.umuId = str("umu-0");
  ubisoft.umuStore = str("ubisoft");
  ubisoft.launcherExecutableCandidates = {
      str("C:\\Program Files (x86)\\Ubisoft\\Ubisoft Game Launcher\\UbisoftConnect.exe")};
  ubisoft.plainNotes = str("Ubisoft Connect installs quietly; no window appears "
                           "until it is ready.");
  ubisoft.minimumFreeBytes = 2 * kGiB;
  table.append(ubisoft);

  StoreRecipe epic;
  epic.id = str("egs-launcher");
  epic.displayName = str("Epic Games Launcher");
  epic.installerUrl = QUrl(str("https://launcher-public-service-prod06.ol.epicgames.com/"
                               "launcher/api/installer/download/"
                               "EpicGamesLauncherInstaller.msi"));
  epic.installerFileName = str("EpicGamesLauncherInstaller.msi");
  epic.installerKind = InstallerKind::Msi;
  epic.installerArguments = {str("/q")};
  epic.defaultPrefixDirName = str("epic-games-store");
  epic.umuId = str("umu-0");
  epic.umuStore = str("egs");
  epic.launcherExecutableCandidates = {
      str("C:\\Program Files (x86)\\Epic Games\\Launcher\\Portal\\Binaries\\Win64\\"
          "EpicGamesLauncher.exe"),
      str("C:\\Program Files\\Epic Games\\Launcher\\Portal\\Binaries\\Win64\\"
          "EpicGamesLauncher.exe"),
      str("C:\\Program Files (x86)\\Epic Games\\Launcher\\Portal\\Binaries\\Win32\\"
          "EpicGamesLauncher.exe")};
  epic.plainNotes = str("Epic games can also be installed with the built-in "
                        "sign-in, which is usually simpler than the launcher.");
  epic.minimumFreeBytes = 2 * kGiB;
  table.append(epic);

  StoreRecipe gog;
  gog.id = str("gog-galaxy");
  gog.displayName = str("GOG Galaxy");
  gog.installerUrl = QUrl(str("https://webinstallers.gog-statics.com/download/GOG_Galaxy_2.0.exe"));
  gog.installerFileName = str("GOG_Galaxy_2.0.exe");
  gog.defaultPrefixDirName = str("gog-galaxy");
  gog.umuId = str("umu-0");
  gog.umuStore = str("gog");
  gog.launcherExecutableCandidates = {
      str("C:\\Program Files (x86)\\GOG Galaxy\\GalaxyClient.exe"),
      str("C:\\Program Files\\GOG Galaxy\\GalaxyClient.exe")};
  gog.plainNotes = str("The GOG Galaxy installer opens its own window; follow it "
                       "to the end. GOG games can also be installed with the "
                       "built-in sign-in.");
  gog.minimumFreeBytes = 2 * kGiB;
  table.append(gog);

  StoreRecipe amazon;
  amazon.id = str("amazon-games");
  amazon.displayName = str("Amazon Games");
  amazon.installerUrl = QUrl(str("https://download.amazongames.com/AmazonGamesSetup.exe"));
  amazon.installerFileName = str("AmazonGamesSetup.exe");
  amazon.defaultPrefixDirName = str("amazon-games");
  amazon.umuId = str("umu-0");
  amazon.umuStore = str("amazon");
  amazon.launcherExecutableCandidates = {
      str("C:\\users\\steamuser\\AppData\\Local\\Amazon Games\\App\\Amazon Games.exe"),
      str("C:\\users\\*\\AppData\\Local\\Amazon Games\\App\\Amazon Games.exe")};
  amazon.plainNotes = str("The Amazon Games installer opens its own window and may "
                          "start the app when it finishes; you can close it.");
  amazon.minimumFreeBytes = 2 * kGiB;
  table.append(amazon);

  return table;
}

bool isSafeFileName(const QString &name) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 ._+()-]{0,127}$"));
  return pattern.match(name).hasMatch() && !name.contains(QLatin1String(".."));
}

} // namespace

const QVector<StoreRecipe> &storeRecipes() {
  static const QVector<StoreRecipe> table = buildTable();
  return table;
}

std::optional<StoreRecipe> findStoreRecipe(const QString &id) {
  for (const StoreRecipe &recipe : storeRecipes()) {
    if (recipe.id == id) {
      return recipe;
    }
  }
  return std::nullopt;
}

const QStringList &knownUmuStores() {
  static const QStringList stores{
      QStringLiteral("none"),   QStringLiteral("amazon"),    QStringLiteral("battlenet"),
      QStringLiteral("ea"),     QStringLiteral("egs"),       QStringLiteral("gog"),
      QStringLiteral("humble"), QStringLiteral("itchio"),    QStringLiteral("steam"),
      QStringLiteral("ubisoft"), QStringLiteral("zoomplatform")};
  return stores;
}

bool isSafePrefixDirName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9._-]{0,63}$"));
  return name != QLatin1String(".") && name != QLatin1String("..") &&
         pattern.match(name).hasMatch();
}

QStringList validateStoreRecipe(const StoreRecipe &recipe) {
  QStringList problems;
  const auto require = [&problems](bool ok, const QString &problem) {
    if (!ok) {
      problems.append(problem);
    }
  };
  static const QRegularExpression umuIdPattern(QStringLiteral("^umu-[A-Za-z0-9._-]{1,64}$"));
  require(isSafePrefixDirName(recipe.id), QStringLiteral("unsafe id"));
  require(!recipe.displayName.trimmed().isEmpty(), QStringLiteral("empty display name"));
  require(isAllowedDownloadUrl(recipe.installerUrl),
          QStringLiteral("installer URL is not on the download allowlist"));
  require(isSafeFileName(recipe.installerFileName), QStringLiteral("unsafe installer file name"));
  const QString wantedSuffix = recipe.installerKind == InstallerKind::Msi
                                   ? QStringLiteral(".msi")
                                   : QStringLiteral(".exe");
  require(recipe.installerFileName.endsWith(wantedSuffix, Qt::CaseInsensitive),
          QStringLiteral("installer file name does not match its kind"));
  for (const QString &argument : recipe.installerArguments) {
    bool clean = !argument.isEmpty();
    for (const QChar c : argument) {
      clean = clean && c.category() != QChar::Other_Control;
    }
    require(clean, QStringLiteral("empty or control-character installer argument"));
  }
  require(isSafePrefixDirName(recipe.defaultPrefixDirName), QStringLiteral("unsafe prefix name"));
  require(umuIdPattern.match(recipe.umuId).hasMatch(), QStringLiteral("malformed umu id"));
  require(knownUmuStores().contains(recipe.umuStore), QStringLiteral("unknown umu store"));
  require(!recipe.launcherExecutableCandidates.isEmpty(),
          QStringLiteral("no launcher executable candidates"));
  for (const QString &candidate : recipe.launcherExecutableCandidates) {
    require(!windowsPathToPrefixPath(QStringLiteral("/prefix"), candidate).isEmpty() &&
                candidate.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive),
            QStringLiteral("unmappable launcher candidate: %1").arg(candidate));
  }
  require(!recipe.plainNotes.trimmed().isEmpty(), QStringLiteral("no plain notes"));
  require(recipe.minimumFreeBytes > 0, QStringLiteral("no free-space floor"));
  return problems;
}

QStringList installerCommand(InstallerKind kind, const QString &installerPath,
                             const QStringList &arguments) {
  QStringList command;
  if (kind == InstallerKind::Msi) {
    command << QStringLiteral("msiexec") << QStringLiteral("/i");
  }
  command << installerPath << arguments;
  return command;
}

} // namespace QindaQt::QindaLutris
