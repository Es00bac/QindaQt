// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_locations_store.h"

#include "../model/state_file.h"
#include "network_location.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto locationsFileName = "network-locations-v1.json";

NetworkLocationsLoadResult loadFailure(const NetworkLocationsError error,
                                       const QString &diagnostic) {
  NetworkLocationsLoadResult result;
  result.error = error;
  result.diagnostic = diagnostic.left(256);
  return result;
}

NetworkLocationsWriteResult writeFailure(const NetworkLocationsError error,
                                         const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

[[nodiscard]] StateFile stateFileFor(const QString &directory) {
  return StateFile(directory, QByteArray(locationsFileName),
                   NetworkLocationsStore::maximumBytes);
}

[[nodiscard]] NetworkLocationsLoadResult
loadFailureFor(const StateFile::ReadResult &read) {
  switch (read.error) {
  case StateFile::Error::Absent:
    return loadFailure(NetworkLocationsError::Absent, QString());
  case StateFile::Error::InvalidRoot:
    return loadFailure(NetworkLocationsError::InvalidRoot,
                       QStringLiteral("Network location state root is unsafe"));
  case StateFile::Error::NotRegular:
    return loadFailure(
        NetworkLocationsError::Malformed,
        QStringLiteral("Network location state is not a regular file"));
  case StateFile::Error::TooLarge:
    return loadFailure(NetworkLocationsError::TooLarge,
                       QStringLiteral("Network location state exceeds 64 KiB"));
  case StateFile::Error::ReadFailed:
  case StateFile::Error::WriteFailed:
  case StateFile::Error::None:
    break;
  }
  return loadFailure(NetworkLocationsError::ReadFailed, read.systemDiagnostic);
}

[[nodiscard]] NetworkLocationsWriteResult
writeFailureFor(const StateFile::WriteResult &written) {
  switch (written.error) {
  case StateFile::Error::InvalidRoot:
    return writeFailure(
        NetworkLocationsError::InvalidRoot,
        QStringLiteral("Network location state directory is unavailable or unsafe"));
  case StateFile::Error::NotRegular:
    return writeFailure(NetworkLocationsError::InvalidRoot,
                        QStringLiteral("Network location state target is unsafe"));
  case StateFile::Error::TooLarge:
    return writeFailure(NetworkLocationsError::TooLarge,
                        QStringLiteral("Network location state exceeds 64 KiB"));
  case StateFile::Error::WriteFailed:
  case StateFile::Error::ReadFailed:
  case StateFile::Error::Absent:
  case StateFile::Error::None:
    break;
  }
  return writeFailure(NetworkLocationsError::WriteFailed, written.systemDiagnostic);
}

} // namespace

NetworkLocationsStore::NetworkLocationsStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString NetworkLocationsStore::filePath() const {
  return stateFileFor(m_stateDirectory).filePath();
}

QString NetworkLocationsStore::identityFor(const QUrl &canonicalUrl) {
  return canonicalUrl.toString();
}

bool NetworkLocationsStore::validate(
    const QVector<NetworkLocationRecord> &locations, QString *diagnostic) {
  if (locations.size() > maximumLocations) {
    *diagnostic = QStringLiteral("Network location inventory contains too many entries");
    return false;
  }
  QSet<QString> uniqueIdentities;
  for (const NetworkLocationRecord &location : locations) {
    const QString text = location.url.toString();
    // AGENT-GUARD: round-tripping through canonicalize() is what proves the
    // stored URL is a supported scheme, has a host, carries no userinfo, and
    // has no "." / ".." segment. Do not weaken this to a scheme check.
    const auto canonical = NetworkLocation::canonicalize(text);
    if (text.size() > maximumUrlLength || !canonical.has_value() ||
        *canonical != location.url) {
      *diagnostic =
          QStringLiteral("Network location inventory contains an invalid address");
      return false;
    }
    if (location.id != identityFor(location.url) ||
        uniqueIdentities.contains(location.id)) {
      *diagnostic =
          QStringLiteral("Network location inventory contains a duplicate address");
      return false;
    }
    if (location.name.isEmpty() || location.name.size() > maximumNameLength ||
        !location.name.isValidUtf16() || location.name.contains(QChar::Null)) {
      *diagnostic =
          QStringLiteral("Network location inventory contains an invalid name");
      return false;
    }
    uniqueIdentities.insert(location.id);
  }
  return true;
}

NetworkLocationsLoadResult NetworkLocationsStore::load() const {
  const StateFile::ReadResult read = stateFileFor(m_stateDirectory).read();
  if (!read.ok()) {
    return loadFailureFor(read);
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(read.bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(NetworkLocationsError::Malformed,
                       QStringLiteral("Network location state is malformed JSON"));
  }
  const QJsonObject object = document.object();
  const QSet<QString> expectedKeys{QStringLiteral("version"),
                                   QStringLiteral("locations")};
  const QStringList objectKeys = object.keys();
  if (QSet<QString>(objectKeys.begin(), objectKeys.end()) != expectedKeys ||
      object.value(QStringLiteral("version")).toDouble() != 1.0 ||
      !object.value(QStringLiteral("locations")).isArray()) {
    return loadFailure(NetworkLocationsError::Malformed,
                       QStringLiteral("Network location state has an invalid schema"));
  }
  const QJsonArray entries = object.value(QStringLiteral("locations")).toArray();
  if (entries.size() > maximumLocations) {
    return loadFailure(
        NetworkLocationsError::Malformed,
        QStringLiteral("Network location state contains too many entries"));
  }
  const QSet<QString> expectedEntryKeys{QStringLiteral("name"),
                                        QStringLiteral("url"),
                                        QStringLiteral("showInPlaces")};
  QVector<NetworkLocationRecord> locations;
  for (const QJsonValue &value : entries) {
    if (!value.isObject()) {
      return loadFailure(NetworkLocationsError::Malformed,
                         QStringLiteral("Network location entry is not an object"));
    }
    const QJsonObject entry = value.toObject();
    const QStringList entryKeys = entry.keys();
    if (QSet<QString>(entryKeys.begin(), entryKeys.end()) != expectedEntryKeys ||
        !entry.value(QStringLiteral("name")).isString() ||
        !entry.value(QStringLiteral("url")).isString() ||
        !entry.value(QStringLiteral("showInPlaces")).isBool()) {
      return loadFailure(NetworkLocationsError::Malformed,
                         QStringLiteral("Network location entry has an invalid shape"));
    }
    NetworkLocationRecord record;
    record.name = entry.value(QStringLiteral("name")).toString();
    record.url = QUrl(entry.value(QStringLiteral("url")).toString(), QUrl::StrictMode);
    record.showInPlaces = entry.value(QStringLiteral("showInPlaces")).toBool();
    record.id = identityFor(record.url);
    locations.append(std::move(record));
  }
  QString diagnostic;
  if (!validate(locations, &diagnostic)) {
    return loadFailure(NetworkLocationsError::Malformed, diagnostic);
  }
  NetworkLocationsLoadResult result;
  result.locations = std::move(locations);
  return result;
}

NetworkLocationsWriteResult NetworkLocationsStore::store(
    const QVector<NetworkLocationRecord> &locations) const {
  QString diagnostic;
  if (!validate(locations, &diagnostic)) {
    return writeFailure(NetworkLocationsError::Malformed, diagnostic);
  }
  QJsonArray entries;
  for (const NetworkLocationRecord &location : locations) {
    entries.append(QJsonObject{
        {QStringLiteral("name"), location.name},
        {QStringLiteral("url"), location.url.toString()},
        {QStringLiteral("showInPlaces"), location.showInPlaces},
    });
  }
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("locations"), entries},
  });
  const StateFile::WriteResult written =
      stateFileFor(m_stateDirectory).write(document.toJson(QJsonDocument::Compact));
  if (!written.ok()) {
    return writeFailureFor(written);
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
