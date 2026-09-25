// SPDX-License-Identifier: GPL-3.0-or-later
#include "archive_listing.h"

#include "link_resolution.h"

#include <QHash>

#include <algorithm>

namespace QindaQt::QindaLutris {

namespace {

// Decodes one C-quoted string starting at text[*pos] == '"'; advances *pos
// past the closing quote. nullopt on malformed input.
std::optional<QString> takeQuoted(const QByteArray &text, qsizetype *pos) {
  if (*pos >= text.size() || text.at(*pos) != '"') {
    return std::nullopt;
  }
  QByteArray bytes;
  qsizetype i = *pos + 1;
  while (i < text.size()) {
    const char c = text.at(i);
    if (c == '"') {
      *pos = i + 1;
      return QString::fromUtf8(bytes);
    }
    if (c != '\\') {
      bytes.append(c);
      ++i;
      continue;
    }
    if (++i >= text.size()) {
      return std::nullopt;
    }
    const char e = text.at(i);
    static const QHash<char, char> simple{{'\\', '\\'}, {'"', '"'}, {'n', '\n'}, {'t', '\t'},
                                          {'r', '\r'},  {'a', '\a'}, {'b', '\b'}, {'f', '\f'},
                                          {'v', '\v'},  {'?', '?'}};
    if (simple.contains(e)) {
      bytes.append(simple.value(e));
      ++i;
    } else if (e >= '0' && e <= '7') {
      int value = 0;
      int digits = 0;
      while (digits < 3 && i < text.size() && text.at(i) >= '0' && text.at(i) <= '7') {
        value = value * 8 + (text.at(i) - '0');
        ++i;
        ++digits;
      }
      bytes.append(static_cast<char>(value & 0xff));
    } else {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

QString normalizedName(QString name) {
  while (name.startsWith(QLatin1String("./"))) {
    name.remove(0, 2);
  }
  while (name.endsWith(QLatin1Char('/'))) {
    name.chop(1);
  }
  return name;
}

ArchiveListingVerdict refuse(const QString &reason) {
  ArchiveListingVerdict verdict;
  verdict.reason = reason;
  return verdict;
}

} // namespace

QStringList tarListArguments(const QString &archivePath) {
  return {QStringLiteral("--list"),         QStringLiteral("--verbose"),
          QStringLiteral("--numeric-owner"), QStringLiteral("--quoting-style=c"),
          QStringLiteral("--gzip"),          QStringLiteral("--file=") + archivePath};
}

std::optional<ArchiveEntry> parseArchiveListingLine(const QByteArray &line) {
  if (line.size() < 11) {
    return std::nullopt;
  }
  ArchiveEntry entry;
  entry.type = QLatin1Char(line.at(0));
  entry.mode = QString::fromLatin1(line.mid(1, 9));
  qsizetype pos = line.indexOf('"');
  if (pos < 0) {
    return std::nullopt;
  }
  const auto name = takeQuoted(line, &pos);
  if (!name) {
    return std::nullopt;
  }
  entry.name = *name;
  const QByteArray rest = line.mid(pos);
  if (rest.isEmpty()) {
    return entry;
  }
  qsizetype targetPos = 0;
  if (rest.startsWith(" -> \"")) {
    targetPos = pos + 4;
  } else if (rest.startsWith(" link to \"")) {
    targetPos = pos + 9;
  } else {
    return std::nullopt;
  }
  const auto target = takeQuoted(line, &targetPos);
  if (!target || targetPos != line.size()) {
    return std::nullopt;
  }
  entry.linkTarget = *target;
  return entry;
}

ArchiveListingVerdict validateArchiveListing(const QByteArray &verboseListing,
                                             const QByteArray &standardError,
                                             qsizetype maxEntries) {
  if (standardError.contains("Removing leading")) {
    return refuse(QStringLiteral("tar had to rewrite names in the archive: %1")
                      .arg(QString::fromUtf8(standardError.left(300))));
  }
  QHash<QString, QChar> types;
  QList<QPair<QStringList, ArchiveEntry>> entries;
  QString top;
  for (const QByteArray &line : verboseListing.split('\n')) {
    if (line.isEmpty()) {
      continue;
    }
    if (entries.size() >= maxEntries) {
      return refuse(QStringLiteral("The archive has too many files."));
    }
    const auto entry = parseArchiveListingLine(line);
    if (!entry) {
      return refuse(QStringLiteral("Unreadable listing line: %1").arg(QString::fromUtf8(line.left(200))));
    }
    if (entry->name.startsWith(QLatin1Char('/'))) {
      return refuse(QStringLiteral("The archive contains an absolute path: %1").arg(entry->name));
    }
    const QString name = normalizedName(entry->name);
    if (name.isEmpty() || name == QLatin1String(".")) {
      continue;
    }
    const QStringList segments = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.contains(QStringLiteral("..")) || segments.contains(QStringLiteral("."))) {
      return refuse(QStringLiteral("The archive tries to write outside its folder: %1").arg(name));
    }
    if (top.isEmpty()) {
      top = segments.first();
    } else if (segments.first() != top) {
      return refuse(QStringLiteral("The archive has more than one top-level item (%1 and %2).")
                        .arg(top, segments.first()));
    }
    const QString type = QString(entry->type);
    if (!QStringLiteral("-dlh").contains(entry->type)) {
      return refuse(QStringLiteral("The archive contains a special file (type %1): %2").arg(type, name));
    }
    if (entry->mode.size() == 9 &&
        (entry->mode.at(2).toLower() == QLatin1Char('s') || entry->mode.at(5).toLower() == QLatin1Char('s'))) {
      return refuse(QStringLiteral("The archive contains a setuid/setgid file: %1").arg(name));
    }
    if (types.contains(name)) {
      return refuse(QStringLiteral("The archive lists %1 more than once.").arg(name));
    }
    if (segments.size() == 1 && entry->type != QLatin1Char('d')) {
      return refuse(QStringLiteral("The archive's top-level item is not a folder."));
    }
    if (entry->type == QLatin1Char('l') &&
        (entry->linkTarget.isEmpty() || entry->linkTarget.startsWith(QLatin1Char('/')))) {
      return refuse(QStringLiteral("The archive has a link pointing outside its folder: %1 -> %2")
                        .arg(name, entry->linkTarget));
    }
    if (entry->type == QLatin1Char('h')) {
      const QString target = normalizedName(entry->linkTarget);
      const QStringList parts = target.split(QLatin1Char('/'), Qt::SkipEmptyParts);
      if (target.startsWith(QLatin1Char('/')) || parts.isEmpty() || parts.first() != top ||
          parts.contains(QStringLiteral(".."))) {
        return refuse(QStringLiteral("The archive has a hard link to outside its folder: %1 -> %2")
                          .arg(name, entry->linkTarget));
      }
    }
    types.insert(name, entry->type);
    entries.append({segments, *entry});
  }
  const bool nested = std::any_of(entries.cbegin(), entries.cend(),
                                  [](const auto &item) { return item.first.size() > 1; });
  if (top.isEmpty() || !nested) {
    return refuse(QStringLiteral("The archive does not contain a single folder."));
  }
  // Links are resolved physically against the listing's own link table, so a
  // target that climbs out THROUGH another link is caught (see
  // link_resolution.h). The staged-tree check repeats this on disk.
  QHash<QString, QString> links;
  for (const auto &[segments, entry] : std::as_const(entries)) {
    if (entry.type == QLatin1Char('l')) {
      links.insert(segments.join(QLatin1Char('/')), entry.linkTarget);
    }
  }
  const LinkReader readLink = [&links](const QStringList &path) -> std::optional<QString> {
    const auto it = links.constFind(path.join(QLatin1Char('/')));
    return it == links.constEnd() ? std::nullopt : std::optional<QString>(it.value());
  };
  for (const auto &[segments, entry] : std::as_const(entries)) {
    const QString name = segments.join(QLatin1Char('/'));
    if (entry.type == QLatin1Char('l') &&
        !resolveConfined(segments.mid(0, segments.size() - 1), entry.linkTarget, readLink)) {
      return refuse(QStringLiteral("The archive has a link pointing outside its folder: %1 -> %2")
                        .arg(name, entry.linkTarget));
    }
    if (entry.type == QLatin1Char('h')) {
      const QStringList target =
          normalizedName(entry.linkTarget).split(QLatin1Char('/'), Qt::SkipEmptyParts);
      for (qsizetype depth = 1; depth < target.size(); ++depth) {
        if (links.contains(target.mid(0, depth).join(QLatin1Char('/')))) {
          return refuse(QStringLiteral("The archive has a hard link through a link: %1 -> %2")
                            .arg(name, entry.linkTarget));
        }
      }
    }
    for (qsizetype depth = 1; depth < segments.size(); ++depth) {
      const QString ancestor = segments.mid(0, depth).join(QLatin1Char('/'));
      const auto it = types.constFind(ancestor);
      if (it != types.constEnd() && it.value() != QLatin1Char('d')) {
        return refuse(QStringLiteral("The archive writes through a link: %1")
                          .arg(segments.join(QLatin1Char('/'))));
      }
    }
  }
  ArchiveListingVerdict verdict;
  verdict.ok = true;
  verdict.topLevel = top;
  return verdict;
}

} // namespace QindaQt::QindaLutris
