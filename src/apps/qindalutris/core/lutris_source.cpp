// SPDX-License-Identifier: GPL-3.0-or-later
#include "lutris_source.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxWarnings = 32;
constexpr int kMaxSlugChars = 256;
constexpr int kMaxRunnerChars = 64;
constexpr int kMaxDirectoryChars = 1024;
constexpr qint64 kMaxCoverBytes = qint64(16) * 1024 * 1024;

void addWarning(QStringList *warnings, const QString &text) {
  if (warnings->size() < kMaxWarnings) {
    warnings->append(text);
  }
}

// One value from a row, kept only when it is valid text within `maxChars`;
// null, binary and oversized values come back empty so a hostile row can
// never reach presentation unchecked.
QString boundedText(const QSqlQuery &query, int column, int maxChars) {
  const QVariant value = query.value(column);
  if (!value.isValid() || value.isNull()
      || value.metaType().id() != QMetaType::QString) {
    return {};
  }
  const QString text = value.toString();
  if (text.size() > maxChars) {
    return {};
  }
  for (const QChar ch : text) {
    if (ch.isNull() || (ch.category() == QChar::Other_Control)) {
      return {};
    }
  }
  return text;
}

QString coverForSlug(const QString &dataDir, const QString &slug) {
  if (slug.isEmpty()) {
    return {};
  }
  for (const QLatin1String dirName :
       {QLatin1String("coverart"), QLatin1String("banners")}) {
    const QString candidate = dataDir + QLatin1Char('/') + dirName
                              + QLatin1Char('/') + slug + QStringLiteral(".jpg");
    const QFileInfo info(candidate);
    if (info.isFile() && !info.isSymLink() && info.size() <= kMaxCoverBytes) {
      return candidate;
    }
  }
  return {};
}

} // namespace

QStringList requiredLutrisGameColumns() {
  return {QStringLiteral("id"),   QStringLiteral("name"),
          QStringLiteral("slug"), QStringLiteral("runner"),
          QStringLiteral("directory"), QStringLiteral("installed")};
}

LutrisDiscovery scanLutrisDatabase(const QString &dbPath) {
  LutrisDiscovery out;
  const QFileInfo dbInfo(dbPath);
  if (dbPath.isEmpty() || !dbInfo.isFile() || dbInfo.isSymLink()
      || dbInfo.size() > qint64(512) * 1024 * 1024) {
    return out; // Absent or implausible: simply no Lutris games.
  }

  // AGENT-GUARD: immutable=1 is the no-write contract. SQLite will not take
  // locks, run recovery, or create -wal/-shm files, and any attempt to write
  // fails. The connection is uniquely named and always removed, so repeated
  // refreshes never leak connections or hold the file open.
  const QString connectionName =
      QStringLiteral("qindalutris-lutris-ro-%1").arg(quintptr(&out), 0, 16);
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                connectionName);
    const QString uri = QStringLiteral("file:%1?immutable=1")
                            .arg(QString::fromUtf8(
                                QUrl::toPercentEncoding(dbInfo.absoluteFilePath())));
    db.setDatabaseName(uri);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!db.open()) {
      addWarning(&out.warnings,
                 QStringLiteral("could not open Lutris database read-only"));
    } else {
      QSqlQuery query(db);
      // Schema gate first: every required column must exist, or the whole
      // source yields nothing rather than half-understood rows.
      QSet<QString> columns;
      if (query.exec(QStringLiteral("PRAGMA table_info(games)"))) {
        while (query.next()) {
          columns.insert(query.value(1).toString());
        }
      }
      bool schemaOk = !columns.isEmpty();
      for (const QString &required : requiredLutrisGameColumns()) {
        schemaOk = schemaOk && columns.contains(required);
      }
      if (!schemaOk) {
        addWarning(&out.warnings,
                   QStringLiteral("Lutris database schema is not recognised"));
      } else if (query.exec(QStringLiteral(
                     "SELECT id, name, slug, runner, directory FROM games "
                     "WHERE installed = 1 LIMIT %1")
                     .arg(kMaxGames))) {
        const QString dataDir = dbInfo.absolutePath();
        while (query.next() && out.games.size() < kMaxGames) {
          bool idOk = false;
          const qlonglong rowId = query.value(0).toLongLong(&idOk);
          const QString name = boundedText(query, 1, kMaxGameTitleChars);
          if (!idOk || rowId <= 0 || name.isEmpty()) {
            // A row with a null name is skipped, not fatal (ADR-0231).
            continue;
          }
          Game game;
          game.id = QStringLiteral("lutris/%1").arg(rowId);
          game.title = name;
          game.source = GameSource::Lutris;
          game.sourceRef = QString::number(rowId);
          game.installPath = boundedText(query, 4, kMaxDirectoryChars);
          game.coverPath = coverForSlug(dataDir,
                                        boundedText(query, 2, kMaxSlugChars));
          out.games.append(game);
        }
      } else {
        addWarning(&out.warnings,
                   QStringLiteral("Lutris database query failed"));
      }
      db.close();
    }
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

} // namespace QindaQt::QindaLutris
