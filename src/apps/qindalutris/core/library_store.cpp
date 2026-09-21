// SPDX-License-Identifier: GPL-3.0-or-later
#include "library_store.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxOptionEntries = kMaxGames;

// AGENT-GUARD: every read path refuses symlinks and oversized documents
// before a byte is parsed, and every write commits through QSaveFile in the
// same directory. A partially written store must never masquerade as state.
QByteArray readBoundedJson(const QString &path, bool *ok) {
  *ok = false;
  const QFileInfo info(path);
  if (!info.exists()) {
    return {}; // Absent: caller maps to Error::Absent.
  }
  if (!info.isFile() || info.isSymLink() || info.size() > kMaxStoreBytes) {
    *ok = false;
    return QByteArray(1, '\0'); // distinguish Refused from Absent
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QByteArray(1, '\0');
  }
  *ok = true;
  return file.read(kMaxStoreBytes + 1);
}

bool writeAtomicJson(const QString &path, const QJsonDocument &document) {
  const QFileInfo info(path);
  QDir dir(info.absolutePath());
  if (!dir.mkpath(QStringLiteral("."))) {
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(document.toJson(QJsonDocument::Compact));
  return file.commit();
}

QString boundedString(const QJsonValue &value, int maxChars, bool *ok) {
  if (!value.isString()) {
    *ok = false;
    return {};
  }
  const QString text = value.toString();
  if (text.size() > maxChars) {
    *ok = false;
    return {};
  }
  for (const QChar ch : text) {
    if (ch.category() == QChar::Other_Control && ch != QLatin1Char('\n')) {
      *ok = false;
      return {};
    }
  }
  *ok = true;
  return text;
}

QJsonObject wineRecordToJson(const WineEntryRecord &record) {
  QJsonObject object;
  object.insert(QStringLiteral("title"), record.title);
  object.insert(QStringLiteral("slug"), record.slug);
  object.insert(QStringLiteral("executable"), record.executablePath);
  object.insert(QStringLiteral("prefix"), record.prefixPath);
  object.insert(QStringLiteral("runner"), wineRunnerId(record.runner));
  object.insert(QStringLiteral("proton"), record.protonPath);
  return object;
}

bool wineRecordFromJson(const QJsonValue &value, WineEntryRecord *record) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  bool ok = false;
  WineEntryRecord out;
  out.title = boundedString(object.value(QStringLiteral("title")),
                            kMaxGameTitleChars, &ok);
  if (!ok || out.title.isEmpty()) return false;
  out.slug = boundedString(object.value(QStringLiteral("slug")), 128, &ok);
  if (!ok || out.slug.isEmpty()) return false;
  out.executablePath = boundedString(object.value(QStringLiteral("executable")),
                                     4096, &ok);
  if (!ok || out.executablePath.isEmpty()) return false;
  out.prefixPath = boundedString(object.value(QStringLiteral("prefix")),
                                 4096, &ok);
  if (!ok) return false;
  const QString runner = boundedString(object.value(QStringLiteral("runner")),
                                       16, &ok);
  if (!ok) return false;
  const std::optional<WineRunner> parsed = wineRunnerForId(runner);
  if (!parsed.has_value()) return false;
  out.runner = *parsed;
  out.protonPath = boundedString(object.value(QStringLiteral("proton")),
                                 4096, &ok);
  if (!ok) return false;
  *record = out;
  return true;
}

QJsonObject optionsToJson(const LaunchOptions &options) {
  QJsonObject object;
  object.insert(QStringLiteral("gamemode"), options.gamemode);
  object.insert(QStringLiteral("mangohud"), options.mangohud);
  object.insert(QStringLiteral("display"), options.targetDisplay);
  object.insert(QStringLiteral("environment"),
                QJsonArray::fromStringList(options.extraEnvironment));
  object.insert(QStringLiteral("runner"),
                options.runnerOverride.has_value()
                    ? wineRunnerId(*options.runnerOverride) : QString());
  object.insert(QStringLiteral("prefixOverride"), options.prefixOverride);
  return object;
}

bool optionsFromJson(const QJsonValue &value, LaunchOptions *options) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  // Exact value discipline (ADR-0198): a key present with the wrong type
  // refuses the document, never snaps to a default.
  const auto boolField = [&object](const char *key, bool *out) -> bool {
    const QJsonValue v = object.value(QLatin1String(key));
    if (v.isUndefined()) { *out = false; return true; }
    if (!v.isBool()) return false;
    *out = v.toBool();
    return true;
  };
  LaunchOptions out;
  if (!boolField("gamemode", &out.gamemode)) return false;
  if (!boolField("mangohud", &out.mangohud)) return false;
  bool ok = false;
  const QJsonValue display = object.value(QStringLiteral("display"));
  if (!display.isUndefined()) {
    out.targetDisplay = boundedString(display, 256, &ok);
    if (!ok) return false;
  }
  const QJsonValue env = object.value(QStringLiteral("environment"));
  if (!env.isUndefined()) {
    if (!env.isArray() || env.toArray().size() > kMaxExtraEnvironmentEntries) {
      return false;
    }
    for (const QJsonValue &line : env.toArray()) {
      const QString text =
          boundedString(line, kMaxEnvironmentValueChars + 65, &ok);
      if (!ok || !isValidEnvironmentAssignment(text)) return false;
      out.extraEnvironment.append(text);
    }
  }
  const QJsonValue runner = object.value(QStringLiteral("runner"));
  if (!runner.isUndefined()) {
    const QString text = boundedString(runner, 16, &ok);
    if (!ok) return false;
    if (!text.isEmpty()) {
      const std::optional<WineRunner> parsed = wineRunnerForId(text);
      if (!parsed.has_value()) return false;
      out.runnerOverride = *parsed;
    }
  }
  const QJsonValue prefix = object.value(QStringLiteral("prefixOverride"));
  if (!prefix.isUndefined()) {
    out.prefixOverride = boundedString(prefix, 4096, &ok);
    if (!ok) return false;
  }
  *options = out;
  return true;
}

} // namespace

bool isValidEnvironmentAssignment(const QString &line) {
  const qsizetype equals = line.indexOf(QLatin1Char('='));
  if (equals <= 0 || line.size() > kMaxEnvironmentValueChars + 65) {
    return false;
  }
  const QStringView key = QStringView(line).first(equals);
  for (qsizetype i = 0; i < key.size(); ++i) {
    const QChar ch = key.at(i);
    const bool okChar = ch.isLetterOrNumber() || ch == QLatin1Char('_');
    if (!okChar || (i == 0 && ch.isNumber())) {
      return false;
    }
  }
  for (const QChar ch : line) {
    if (ch.category() == QChar::Other_Control) {
      return false;
    }
  }
  return true;
}

LibraryStore::LibraryStore(QString configRoot) : m_root(std::move(configRoot)) {}

QString LibraryStore::wineEntriesPath() const {
  return m_root + QStringLiteral("/wine-entries-v1.json");
}

QString LibraryStore::launchOptionsPath() const {
  return m_root + QStringLiteral("/launch-options-v1.json");
}

QVector<WineEntryRecord> LibraryStore::readWineEntries(Error *error) const {
  bool ok = false;
  const QByteArray bytes = readBoundedJson(wineEntriesPath(), &ok);
  if (bytes.isEmpty()) {
    *error = Error::Absent;
    return {};
  }
  if (!ok) {
    *error = Error::Refused;
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(bytes);
  if (!document.isObject()
      || document.object().value(QStringLiteral("version")) != 1
      || !document.object().value(QStringLiteral("entries")).isArray()) {
    *error = Error::Refused;
    return {};
  }
  const QJsonArray entries =
      document.object().value(QStringLiteral("entries")).toArray();
  if (entries.size() > kMaxWineEntries) {
    *error = Error::Refused;
    return {};
  }
  QVector<WineEntryRecord> out;
  out.reserve(entries.size());
  for (const QJsonValue &value : entries) {
    WineEntryRecord record;
    if (!wineRecordFromJson(value, &record)) {
      *error = Error::Refused;
      return {};
    }
    out.append(record);
  }
  *error = Error::None;
  return out;
}

LibraryStore::Error LibraryStore::writeWineEntries(
    const QVector<WineEntryRecord> &records) const {
  if (records.size() > kMaxWineEntries) {
    return Error::WriteFailed;
  }
  QJsonArray entries;
  for (const WineEntryRecord &record : records) {
    entries.append(wineRecordToJson(record));
  }
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("entries"), entries);
  return writeAtomicJson(wineEntriesPath(), QJsonDocument(root))
             ? Error::None : Error::WriteFailed;
}

QHash<QString, LaunchOptions> LibraryStore::readLaunchOptions(Error *error) const {
  bool ok = false;
  const QByteArray bytes = readBoundedJson(launchOptionsPath(), &ok);
  if (bytes.isEmpty()) {
    *error = Error::Absent;
    return {};
  }
  if (!ok) {
    *error = Error::Refused;
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(bytes);
  if (!document.isObject()
      || document.object().value(QStringLiteral("version")) != 1
      || !document.object().value(QStringLiteral("games")).isObject()) {
    *error = Error::Refused;
    return {};
  }
  const QJsonObject games =
      document.object().value(QStringLiteral("games")).toObject();
  if (games.size() > kMaxOptionEntries) {
    *error = Error::Refused;
    return {};
  }
  QHash<QString, LaunchOptions> out;
  for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
    if (it.key().size() > 256) {
      *error = Error::Refused;
      return {};
    }
    LaunchOptions options;
    if (!optionsFromJson(it.value(), &options)) {
      *error = Error::Refused;
      return {};
    }
    out.insert(it.key(), options);
  }
  *error = Error::None;
  return out;
}

LibraryStore::Error LibraryStore::writeLaunchOptions(
    const QHash<QString, LaunchOptions> &options) const {
  if (options.size() > kMaxOptionEntries) {
    return Error::WriteFailed;
  }
  QJsonObject games;
  for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
    games.insert(it.key(), optionsToJson(it.value()));
  }
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("games"), games);
  return writeAtomicJson(launchOptionsPath(), QJsonDocument(root))
             ? Error::None : Error::WriteFailed;
}

} // namespace QindaQt::QindaLutris
