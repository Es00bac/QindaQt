// SPDX-License-Identifier: GPL-3.0-or-later
#include "recents_place.h"

#include "local_directory_lister.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::FileManager {

namespace {

[[nodiscard]] ListingResult refused(ListingError error, const QString &diagnostic) {
  ListingResult result;
  result.path = RecentsLocation::location();
  result.error = error;
  result.diagnostic = diagnostic;
  return result;
}

// The local absolute path a bookmark's href names, or an empty string.
[[nodiscard]] QString localPathOf(QStringView href) {
  const QUrl url = QUrl::fromEncoded(href.toString().toUtf8());
  if (!url.isValid() || !url.isLocalFile() || url.hasQuery() || url.hasFragment()
      || !(url.host().isEmpty() || url.host() == QLatin1String("localhost"))) {
    return {};
  }
  const QString path = url.toLocalFile();
  return QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : QString();
}

// A bookmark's last use: the latest of its three stamps (desktop-bookmark
// spec; KRecentDocument::recentUrls() reads them the same way).
[[nodiscard]] QDateTime lastUsed(const QXmlStreamAttributes &attributes) {
  QDateTime latest;
  for (const char *name : {"added", "modified", "visited"}) {
    const QDateTime stamp = QDateTime::fromString(
        attributes.value(QLatin1String(name)).toString(), Qt::ISODateWithMs);
    if (stamp.isValid() && (!latest.isValid() || stamp > latest)) {
      latest = stamp;
    }
  }
  return latest;
}

} // namespace

QString recentlyUsedStorePath() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
      .filePath(QStringLiteral("recently-used.xbel"));
}

ListingResult readRecentFiles(const QString &storePath, qsizetype limit) {
  QFile store(storePath);
  if (!store.exists()) {
    ListingResult empty;
    empty.path = RecentsLocation::location();
    return empty;
  }
  if (store.size() > maximumRecentStoreBytes) {
    return refused(ListingError::Unknown,
                   QStringLiteral("The list of recently used files is too large to read"));
  }
  if (!store.open(QIODevice::ReadOnly)) {
    return refused(ListingError::PermissionDenied,
                   QStringLiteral("The list of recently used files cannot be read"));
  }

  QXmlStreamReader xml(&store);
  if (!xml.readNextStartElement() || xml.name() != QLatin1String("xbel")) {
    return refused(ListingError::Unknown,
                   QStringLiteral("The list of recently used files is not a bookmark file"));
  }
  QHash<QString, QDateTime> used;
  while (!xml.atEnd()) {
    if (xml.readNext() != QXmlStreamReader::StartElement
        || xml.name() != QLatin1String("bookmark")) {
      continue;
    }
    const QString path = localPathOf(xml.attributes().value(QLatin1String("href")));
    if (path.isEmpty()) {
      continue;
    }
    const QDateTime when = lastUsed(xml.attributes());
    const auto known = used.constFind(path);
    if (known == used.cend() || when > *known) {
      used.insert(path, when);
    }
  }
  if (xml.hasError()) {
    return refused(ListingError::Unknown,
                   QStringLiteral("The list of recently used files is damaged: %1")
                       .arg(xml.errorString()));
  }

  QList<std::pair<QDateTime, QString>> order;
  order.reserve(used.size());
  for (auto it = used.cbegin(); it != used.cend(); ++it) {
    order.append({it.value(), it.key()});
  }
  // Newest first; equal times fall back to the path so the order never
  // depends on hashing.
  std::sort(order.begin(), order.end(), [](const auto &left, const auto &right) {
    return left.first != right.first ? left.first > right.first : left.second < right.second;
  });

  ListingResult result;
  result.path = RecentsLocation::location();
  // AGENT-GUARD: every candidate costs a stat on the GUI thread, and a store
  // full of deleted files would otherwise stat all of them; four candidates
  // per row shown is the cap, and hitting it reports `truncated`.
  const qsizetype examined = std::min<qsizetype>(order.size(), limit * 4);
  for (qsizetype i = 0; i < examined; ++i) {
    if (result.entries.size() >= limit) {
      result.truncated = true;
      break;
    }
    const QFileInfo info(order.at(i).second);
    if (info.exists()) {
      result.entries.append(LocalDirectoryLister::entryFor(info));
    }
  }
  if (examined < order.size()) {
    result.truncated = true;
  }
  return result;
}

RecentsDirectoryLister::RecentsDirectoryLister(DirectoryListerPtr inner, QString storePath)
    : m_inner(std::move(inner)), m_storePath(std::move(storePath)) {
  Q_ASSERT(m_inner);
}

ListingResult RecentsDirectoryLister::list(const QString &absolutePath) const {
  if (RecentsLocation::isLocation(absolutePath)) {
    return readRecentFiles(m_storePath);
  }
  return m_inner->list(absolutePath);
}

} // namespace QindaQt::Apps::FileManager
