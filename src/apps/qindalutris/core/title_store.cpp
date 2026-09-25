// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_store.h"

#include "proton_catalog.h"
#include "store_io.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace QindaQt::QindaLutris {
namespace {

// AGENT-GUARD: the exact key set of one titles-v1 record. Adding a key is a
// schema change: it needs titles-v2 and a migration (ADR-0275 "Revisit
// when"), never a silent addition that an older reader would refuse.
const QStringList &recordKeys() {
  static const QStringList keys{
      QStringLiteral("id"),          QStringLiteral("title"),
      QStringLiteral("kind"),        QStringLiteral("store"),
      QStringLiteral("storeGameId"), QStringLiteral("prefixPath"),
      QStringLiteral("protonBuild"), QStringLiteral("protonBuildVersion"),
      QStringLiteral("umuId"),
      QStringLiteral("umuStore"),    QStringLiteral("executable"),
      QStringLiteral("arguments"),   QStringLiteral("environment"),
      QStringLiteral("launcherTitleId"),
      QStringLiteral("winetricksApplied"),
      QStringLiteral("installedAt"),
  };
  return keys;
}

bool hasExactKeys(const QJsonObject &object, const QStringList &keys) {
  if (object.size() != keys.size()) {
    return false;
  }
  for (const QString &key : keys) {
    if (!object.contains(key)) {
      return false;
    }
  }
  return true;
}

QJsonObject toJson(const TitleRecord &record) {
  QJsonObject object;
  object.insert(QStringLiteral("id"), record.id);
  object.insert(QStringLiteral("title"), record.title);
  object.insert(QStringLiteral("kind"), titleKindId(record.kind));
  object.insert(QStringLiteral("store"), gameStoreId(record.store));
  object.insert(QStringLiteral("storeGameId"), record.storeGameId);
  object.insert(QStringLiteral("prefixPath"), record.prefixPath);
  object.insert(QStringLiteral("protonBuild"), record.protonBuild);
  object.insert(QStringLiteral("protonBuildVersion"), record.protonBuildVersion);
  object.insert(QStringLiteral("umuId"), record.umuId);
  object.insert(QStringLiteral("umuStore"), record.umuStore);
  object.insert(QStringLiteral("executable"), record.executable);
  object.insert(QStringLiteral("arguments"),
                QJsonArray::fromStringList(record.arguments));
  object.insert(QStringLiteral("environment"),
                QJsonArray::fromStringList(record.environment));
  object.insert(QStringLiteral("launcherTitleId"), record.launcherTitleId);
  object.insert(QStringLiteral("winetricksApplied"),
                QJsonArray::fromStringList(record.winetricksApplied));
  object.insert(QStringLiteral("installedAt"), record.installedAt);
  return object;
}

// Types first, then the shared semantic validator: a record the writer
// would refuse is a record the reader refuses.
bool fromJson(const QJsonValue &value, TitleRecord *record) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!hasExactKeys(object, recordKeys())) {
    return false;
  }
  bool ok = true;
  const auto line = [&object, &ok](const char *key, int maxChars) {
    bool fieldOk = false;
    const QString text = StoreIo::boundedLine(object.value(QLatin1String(key)),
                                              maxChars, &fieldOk);
    ok = ok && fieldOk;
    return text;
  };
  const auto list = [&object, &ok](const char *key, int maxEntries) {
    bool fieldOk = false;
    const QStringList out = StoreIo::boundedLineList(
        object.value(QLatin1String(key)), maxEntries, kMaxTitlePathChars,
        &fieldOk);
    ok = ok && fieldOk;
    return out;
  };
  TitleRecord out;
  out.id = line("id", 128);
  out.title = line("title", kMaxGameTitleChars);
  const QString kind = line("kind", 32);
  const QString store = line("store", 32);
  out.storeGameId = line("storeGameId", 256);
  out.prefixPath = line("prefixPath", kMaxTitlePathChars);
  out.protonBuild = line("protonBuild", kMaxProtonBuildNameChars);
  out.protonBuildVersion = line("protonBuildVersion", 256);
  out.umuId = line("umuId", 64);
  out.umuStore = line("umuStore", 32);
  out.executable = line("executable", kMaxTitlePathChars);
  out.arguments = list("arguments", kMaxTitleArguments);
  out.environment = list("environment", kMaxExtraEnvironmentEntries);
  out.launcherTitleId = line("launcherTitleId", 128);
  out.winetricksApplied = list("winetricksApplied", kMaxWinetricksVerbs);
  out.installedAt = line("installedAt", 32);
  if (!ok) {
    return false;
  }
  const std::optional<TitleKind> parsedKind = titleKindForId(kind);
  const std::optional<GameStore> parsedStore = gameStoreForId(store);
  if (!parsedKind.has_value() || !parsedStore.has_value()) {
    return false;
  }
  out.kind = *parsedKind;
  out.store = *parsedStore;
  if (!validateTitleRecord(out, nullptr)) {
    return false;
  }
  *record = out;
  return true;
}

} // namespace

TitleStore::TitleStore(QString configRoot) : m_root(std::move(configRoot)) {}

QString TitleStore::titlesPath() const {
  return m_root + QStringLiteral("/titles-v1.json");
}

QVector<TitleRecord> TitleStore::readTitles(Error *error) const {
  StoreIo::ReadStatus status = StoreIo::ReadStatus::Refused;
  const QByteArray bytes =
      StoreIo::readBoundedFile(titlesPath(), kMaxStoreBytes, &status);
  if (status == StoreIo::ReadStatus::Absent) {
    *error = Error::Absent;
    return {};
  }
  *error = Error::Refused; // until the whole document has been accepted
  if (status != StoreIo::ReadStatus::Ok) {
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(bytes);
  if (!document.isObject()) {
    return {};
  }
  const QJsonObject root = document.object();
  if (!hasExactKeys(root, {QStringLiteral("version"), QStringLiteral("titles")})
      || !root.value(QStringLiteral("version")).isDouble()
      || root.value(QStringLiteral("version")).toDouble() != 1.0
      || !root.value(QStringLiteral("titles")).isArray()) {
    return {};
  }
  const QJsonArray titles = root.value(QStringLiteral("titles")).toArray();
  if (titles.size() > kMaxTitles) {
    return {};
  }
  QVector<TitleRecord> out;
  out.reserve(titles.size());
  QSet<QString> ids;
  for (const QJsonValue &value : titles) {
    TitleRecord record;
    if (!fromJson(value, &record) || ids.contains(record.id)) {
      return {};
    }
    ids.insert(record.id);
    out.append(record);
  }
  *error = Error::None;
  return out;
}

TitleStore::Error TitleStore::writeTitles(
    const QVector<TitleRecord> &records) const {
  if (records.size() > kMaxTitles) {
    return Error::WriteFailed;
  }
  QJsonArray titles;
  QSet<QString> ids;
  for (const TitleRecord &record : records) {
    if (!validateTitleRecord(record, nullptr) || ids.contains(record.id)) {
      return Error::WriteFailed;
    }
    ids.insert(record.id);
    titles.append(toJson(record));
  }
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("titles"), titles);
  const QJsonDocument document(root);
  // A document the reader would refuse for size must never be committed.
  if (document.toJson(QJsonDocument::Compact).size() > kMaxStoreBytes) {
    return Error::WriteFailed;
  }
  return StoreIo::writeAtomicJson(titlesPath(), document) ? Error::None
                                                          : Error::WriteFailed;
}

} // namespace QindaQt::QindaLutris
