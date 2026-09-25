// SPDX-License-Identifier: GPL-3.0-or-later
#include "wine_source.h"

#include <qindaqt/compositor/peicon.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>

namespace QindaQt::QindaLutris {
namespace {

// The extracted cover stays within the PE parser's own pixel ceiling; the
// PNG we write is additionally capped so a cache write stays cheap.
constexpr qint64 kMaxCoverWriteBytes = qint64(4) * 1024 * 1024;

QString coverPathFor(const QString &cacheKey, const QString &cacheDir,
                     const QFileInfo &exeInfo) {
  if (cacheKey.isEmpty() || cacheKey.contains(QLatin1Char('/'))
      || cacheKey.startsWith(QLatin1Char('.'))) {
    return {};
  }
  const QString coverPath =
      cacheDir + QLatin1Char('/') + cacheKey + QStringLiteral(".png");
  const QFileInfo coverInfo(coverPath);
  if (coverInfo.isFile() && coverInfo.lastModified() >= exeInfo.lastModified()) {
    return coverPath; // Fresh cache hit: no decode at all.
  }
  QFile exe(exeInfo.absoluteFilePath());
  if (!exe.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QImage icon =
      Compositor::extractPeIcon(exe, exeInfo.size(), Compositor::kWineIconTargetSize * 2);
  if (icon.isNull()) {
    return {};
  }
  QDir().mkpath(cacheDir);
  if (!icon.save(coverPath, "PNG")) {
    return {};
  }
  if (QFileInfo(coverPath).size() > kMaxCoverWriteBytes) {
    QFile::remove(coverPath);
    return {};
  }
  return coverPath;
}

} // namespace

QString executableCoverPath(const QString &cacheKey,
                            const QString &executablePath,
                            const QString &cacheDir) {
  const QFileInfo exeInfo(executablePath);
  if (!exeInfo.isFile() || exeInfo.isSymLink()) {
    return {};
  }
  return coverPathFor(cacheKey, cacheDir, exeInfo);
}

QString wineSlugFor(const QString &title, const QString &executablePath) {
  QString base = normalizedTitleForMatch(title);
  base.replace(QLatin1Char(' '), QLatin1Char('-'));
  if (base.isEmpty()) {
    base = QStringLiteral("game");
  }
  const QByteArray digest = QCryptographicHash::hash(
      executablePath.toUtf8(), QCryptographicHash::Sha256).left(4).toHex();
  return base.left(48) + QLatin1Char('-') + QString::fromLatin1(digest);
}

QVector<Game> gamesFromWineEntries(const QVector<WineEntryRecord> &records,
                                   const QString &cacheDir) {
  QVector<Game> out;
  out.reserve(qMin(records.size(), kMaxWineEntries));
  for (const WineEntryRecord &record : records) {
    if (out.size() >= kMaxWineEntries || out.size() >= kMaxGames) {
      break;
    }
    Game game;
    game.id = QStringLiteral("wine/%1").arg(record.slug);
    game.title = record.title.left(kMaxGameTitleChars);
    game.source = GameSource::Wine;
    game.sourceRef = record.slug;
    game.installPath = record.executablePath;
    game.winePrefix = record.prefixPath;
    game.wineRunner = record.runner;
    game.protonPath = record.protonPath;
    game.protonVersion = record.protonVersion;
    const QFileInfo exeInfo(record.executablePath);
    if (exeInfo.isFile() && !exeInfo.isSymLink()) {
      game.installSizeBytes = quint64(exeInfo.size());
      game.coverPath = coverPathFor(record.slug, cacheDir, exeInfo);
    }
    out.append(game);
  }
  return out;
}

} // namespace QindaQt::QindaLutris
