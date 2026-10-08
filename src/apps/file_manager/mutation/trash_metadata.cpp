// SPDX-License-Identifier: GPL-3.0-or-later
#include "trash_metadata.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <optional>

namespace QindaQt::Apps::FileManager {
namespace {
MutationResult invalid() {
  MutationResult result;
  result.error = MutationError::InvalidRequest;
  result.diagnostic = QStringLiteral("Trash metadata has no safe original path or deletion date.");
  return result;
}
bool absolutePath(const QString &path) {
  return path.startsWith(QLatin1Char('/')) && !path.contains(QChar::Null) &&
      QDir::cleanPath(path) == path &&
      QFile::decodeName(QFile::encodeName(path)) == path;
}
bool relativePath(const QString &path) {
  if (path.isEmpty() || path.startsWith(QLatin1Char('/')) ||
      path.contains(QChar::Null)) return false;
  const auto components = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
  for (const auto &part : components)
    if (part.isEmpty() || part == QStringLiteral(".") ||
        part == QStringLiteral("..")) return false;
  return true;
}
QString pathBase(const TrashLocation &location) {
  return location.home ? QFileInfo(location.root).absolutePath()
                       : location.topDirectory;
}
int hex(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}
std::optional<QByteArray> unescape(const QByteArray &encoded) {
  QByteArray bytes;
  bytes.reserve(encoded.size());
  for (qsizetype i = 0; i < encoded.size(); ++i) {
    const unsigned char current = static_cast<unsigned char>(encoded[i]);
    if (current == '%') {
      if (i + 2 >= encoded.size()) return std::nullopt;
      const int high = hex(encoded[++i]);
      const int low = hex(encoded[++i]);
      if (high < 0 || low < 0) return std::nullopt;
      const char decoded = static_cast<char>((high << 4) | low);
      if (decoded == '\0') return std::nullopt;
      bytes.append(decoded);
    } else {
      if (current < 0x20 || current == 0x7f) return std::nullopt;
      bytes.append(static_cast<char>(current));
    }
  }
  return bytes;
}
QByteArray escape(const QByteArray &bytes) {
  QByteArray encoded;
  constexpr char digits[] = "0123456789ABCDEF";
  for (const char byte : bytes) {
    const auto value = static_cast<unsigned char>(byte);
    if ((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
        (value >= '0' && value <= '9') || value == '/' || value == '-' ||
        value == '_' || value == '.' || value == '~') {
      encoded.append(static_cast<char>(value));
    } else {
      encoded.append('%');
      encoded.append(digits[value >> 4]);
      encoded.append(digits[value & 15]);
    }
  }
  return encoded;
}
} // namespace

MutationResult TrashMetadataCodec::encode(
    const QString &originalPath, const TrashLocation &location,
    const QDateTime &deletionDate, QByteArray &destination) {
  if (!absolutePath(originalPath) || !absolutePath(location.root) ||
      !absolutePath(pathBase(location)) || !deletionDate.isValid())
    return invalid();
  QString recorded = originalPath;
  if (!location.home) {
    const QString prefix = location.topDirectory == QStringLiteral("/")
        ? QStringLiteral("/") : location.topDirectory + QLatin1Char('/');
    if (!originalPath.startsWith(prefix)) return invalid();
    recorded = originalPath.mid(prefix.size());
    if (!relativePath(recorded)) return invalid();
  }
  const QByteArray bytes = "[Trash Info]\nPath=" +
      escape(QFile::encodeName(recorded)) + "\nDeletionDate=" +
      deletionDate.toLocalTime().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss")).toLatin1() + '\n';
  if (bytes.size() > maximumBytes) return invalid();
  destination = bytes;
  return {};
}

MutationResult TrashMetadataCodec::decode(
    const QByteArray &bytes, const TrashLocation &location,
    TrashMetadata &destination) {
  if (bytes.size() > maximumBytes || !absolutePath(location.root) ||
      !absolutePath(pathBase(location)) || bytes.contains('\0')) return invalid();
  auto normalized = bytes;
  normalized.replace("\r\n", "\n");
  const auto lines = normalized.split('\n');
  if (lines.isEmpty() || lines.front() != QByteArray("[Trash Info]"))
    return invalid();
  std::optional<QByteArray> path;
  std::optional<QByteArray> date;
  for (const auto &line : lines) {
    // AGENT-CONTRACT: Trash1.0 uses the FIRST occurrence, including an invalid
    // empty first value; a later duplicate must not rescue hostile metadata.
    if (!path && line.startsWith("Path=")) path = line.mid(5);
    if (!date && line.startsWith("DeletionDate=")) date = line.mid(13);
  }
  if (!path || !date) return invalid();
  const auto decoded = unescape(*path);
  if (!decoded) return invalid();
  // AGENT-GUARD: an initial UTF-8 BOM is a filename character, not a
  // document marker. Prefix the decoder so it cannot silently consume U+FEFF.
  const QString recorded = QFile::decodeName(QByteArray("/") + *decoded).mid(1);
  if (QFile::encodeName(recorded) != *decoded) return invalid();
  QString original;
  if (recorded.startsWith(QLatin1Char('/'))) {
    if (!location.home || !absolutePath(recorded)) return invalid();
    original = recorded;
  } else {
    if (!relativePath(recorded)) return invalid();
    original = QDir(pathBase(location)).filePath(recorded);
    if (!absolutePath(original)) return invalid();
  }
  const QString dateText = QString::fromLatin1(*date);
  const auto deletion = QDateTime::fromString(dateText, QStringLiteral("yyyy-MM-dd'T'HH:mm:ss"));
  if (!deletion.isValid() ||
      deletion.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss")) != dateText)
    return invalid();
  destination = {original, deletion};
  return {};
}
} // namespace QindaQt::Apps::FileManager
