// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam_source.h"

#include <qindaqt/compositor/steamappidentity.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxWarnings = 32;

void addWarning(QStringList *warnings, const QString &text) {
  if (warnings->size() < kMaxWarnings) {
    warnings->append(text);
  }
}

// Reads at most kMaxSteamFileBytes of one regular, non-symlink file. Steam's
// own files are user-owned content, but this adapter runs in whatever
// session hosts QindaLutris, so it refuses to follow links into elsewhere.
QByteArray readBoundedRegularFile(const QString &path, qint64 maxBytes,
                                  bool *ok) {
  *ok = false;
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink() || info.size() > maxBytes) {
    return {};
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  *ok = true;
  return file.read(maxBytes);
}

// The digits between "appmanifest_" and ".acf", or nullopt for anything
// else (a stray backup, a truncated name, an overflowing id).
std::optional<quint64> appIdFromManifestName(const QString &fileName) {
  static const QRegularExpression pattern(
      QStringLiteral("^appmanifest_(\\d{1,19})\\.acf$"));
  const QRegularExpressionMatch match = pattern.match(fileName);
  if (!match.hasMatch()) {
    return std::nullopt;
  }
  bool ok = false;
  const quint64 id = match.captured(1).toULongLong(&ok);
  if (!ok) {
    return std::nullopt;
  }
  return id;
}

// Cover art is cached per install root, keyed by appid. Only the two
// fixed portrait/header names Steam writes are considered; no walk.
QString coverForApp(const QString &installRoot, quint64 appId) {
  const QString base = installRoot + QStringLiteral("/appcache/librarycache/")
                       + QString::number(appId);
  for (const QLatin1String name :
       {QLatin1String("library_600x900.jpg"), QLatin1String("header.jpg")}) {
    const QString candidate = base + QLatin1Char('/') + name;
    const QFileInfo info(candidate);
    if (info.isFile() && !info.isSymLink()
        && info.size() <= kMaxSteamFileBytes * 8) {
      return candidate;
    }
  }
  return {};
}

void scanManifestsInto(const QString &libraryRoot, const QString &installRoot,
                       SteamDiscovery *out) {
  const QString steamapps = libraryRoot + QStringLiteral("/steamapps");
  const QDir dir(steamapps);
  const QStringList entries = dir.entryList(QDir::Files, QDir::Name);
  int seen = 0;
  for (const QString &name : entries) {
    if (out->games.size() >= kMaxGames) {
      return;
    }
    if (!name.startsWith(QLatin1String("appmanifest_"))) {
      continue;
    }
    if (++seen > kMaxSteamManifestsPerRoot) {
      addWarning(&out->warnings,
                 QStringLiteral("too many appmanifests in %1").arg(steamapps));
      return;
    }
    const std::optional<quint64> appId = appIdFromManifestName(name);
    if (!appId.has_value()) {
      continue;
    }
    bool ok = false;
    const QByteArray bytes =
        readBoundedRegularFile(dir.filePath(name), kMaxSteamFileBytes, &ok);
    if (!ok) {
      addWarning(&out->warnings,
                 QStringLiteral("unreadable manifest %1").arg(name));
      continue;
    }
    const QString title = Compositor::parseSteamAppManifestName(bytes);
    if (title.isEmpty()) {
      addWarning(&out->warnings,
                 QStringLiteral("malformed manifest %1").arg(name));
      continue;
    }
    Game game;
    game.id = QStringLiteral("steam/%1").arg(*appId);
    game.title = title;
    game.source = GameSource::Steam;
    game.sourceRef = QString::number(*appId);
    game.appId = *appId;
    // The exact install subdirectory lives behind the manifest's
    // "installdir" key, which no shared parser exposes yet (ADR-0231 flags
    // this). The library folder is reported honestly instead.
    game.installPath = steamapps + QStringLiteral("/common");
    game.coverPath = coverForApp(installRoot, *appId);
    out->games.append(game);
  }
}

} // namespace

SteamDiscovery scanSteamLibraries(const QStringList &candidateRoots) {
  SteamDiscovery out;
  QStringList roots;          // library roots across all candidates
  QStringList rootOwners;     // parallel: install root that yielded each
  int candidates = 0;
  for (const QString &candidate : candidateRoots) {
    if (++candidates > kMaxSteamCandidateRoots) {
      addWarning(&out.warnings, QStringLiteral("too many Steam candidates"));
      break;
    }
    if (candidate.isEmpty() || !QDir(candidate).exists()) {
      continue;
    }
    // libraryfolders.vdf is the declared root list. A candidate whose index
    // is absent or unreadable still counts itself when it has a steamapps
    // tree -- the manifests are the real data, the index is a hint.
    QStringList declared;
    bool declaredOk = false;
    bool readOk = false;
    const QByteArray vdf = readBoundedRegularFile(
        candidate + QStringLiteral("/steamapps/libraryfolders.vdf"),
        kMaxSteamFileBytes, &readOk);
    if (readOk) {
      declaredOk = Compositor::parseSteamLibraryFolders(vdf, &declared);
      if (!declaredOk) {
        addWarning(&out.warnings,
                   QStringLiteral("malformed libraryfolders.vdf in %1")
                       .arg(candidate));
      }
    }
    if (declaredOk && !declared.isEmpty()) {
      for (const QString &root : declared) {
        roots.append(root);
        rootOwners.append(candidate);
      }
    } else if (QDir(candidate + QStringLiteral("/steamapps")).exists()) {
      roots.append(candidate);
      rootOwners.append(candidate);
    }
  }

  QStringList seen;
  for (int i = 0; i < roots.size(); ++i) {
    const QString canonical = QDir(roots.at(i)).canonicalPath();
    const QString key = canonical.isEmpty() ? roots.at(i) : canonical;
    if (seen.contains(key)) {
      continue;
    }
    seen.append(key);
    if (seen.size() > Compositor::kMaxSteamLibraryRoots) {
      addWarning(&out.warnings, QStringLiteral("too many Steam roots"));
      break;
    }
    scanManifestsInto(roots.at(i), rootOwners.at(i), &out);
  }
  return out;
}

} // namespace QindaQt::QindaLutris
