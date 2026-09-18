// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_store.h"

#include "state_file.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto preferencesFileName = "preferences-v1.json";

// AGENT-GUARD: this list is the schema. Adding a name here without bumping to
// preferences-v2 makes every older document Malformed on the next launch.
[[nodiscard]] QStringList preferenceKeys() {
  return {QStringLiteral("defaultViewMode"),       QStringLiteral("showHidden"),
          QStringLiteral("directoriesFirst"),      QStringLiteral("sortColumn"),
          QStringLiteral("sortDirection"),         QStringLiteral("iconSize"),
          QStringLiteral("discoverNearbyServers"), QStringLiteral("defaultConnectScheme"),
          QStringLiteral("confirmTrash")};
}

PreferencesLoadResult loadFailure(const PreferencesError error,
                                  const QString &diagnostic) {
  PreferencesLoadResult result;
  result.error = error;
  result.diagnostic = diagnostic.left(256);
  return result;
}

PreferencesWriteResult writeFailure(const PreferencesError error,
                                    const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

[[nodiscard]] StateFile stateFileFor(const QString &directory) {
  return StateFile(directory, QByteArray(preferencesFileName),
                   PreferencesStore::maximumBytes);
}

[[nodiscard]] PreferencesLoadResult loadFailureFor(const StateFile::ReadResult &read) {
  switch (read.error) {
  case StateFile::Error::Absent:
    return loadFailure(PreferencesError::Absent, QString());
  case StateFile::Error::InvalidRoot:
    return loadFailure(PreferencesError::InvalidRoot,
                       QStringLiteral("Preference state root is unsafe"));
  case StateFile::Error::NotRegular:
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state is not a regular file"));
  case StateFile::Error::TooLarge:
    return loadFailure(PreferencesError::TooLarge,
                       QStringLiteral("Preference state exceeds 16 KiB"));
  case StateFile::Error::ReadFailed:
  case StateFile::Error::WriteFailed:
  case StateFile::Error::None:
    break;
  }
  return loadFailure(PreferencesError::ReadFailed, read.systemDiagnostic);
}

[[nodiscard]] PreferencesWriteResult
writeFailureFor(const StateFile::WriteResult &written) {
  switch (written.error) {
  case StateFile::Error::InvalidRoot:
    return writeFailure(
        PreferencesError::InvalidRoot,
        QStringLiteral("Preference state directory is unavailable or unsafe"));
  case StateFile::Error::NotRegular:
    return writeFailure(PreferencesError::InvalidRoot,
                        QStringLiteral("Preference state target is unsafe"));
  case StateFile::Error::TooLarge:
    return writeFailure(PreferencesError::TooLarge,
                        QStringLiteral("Preference state exceeds 16 KiB"));
  case StateFile::Error::WriteFailed:
  case StateFile::Error::ReadFailed:
  case StateFile::Error::Absent:
  case StateFile::Error::None:
    break;
  }
  return writeFailure(PreferencesError::WriteFailed, written.systemDiagnostic);
}

} // namespace

PreferencesStore::PreferencesStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString PreferencesStore::filePath() const {
  return stateFileFor(m_stateDirectory).filePath();
}

PreferencesLoadResult PreferencesStore::load() const {
  const StateFile::ReadResult read = stateFileFor(m_stateDirectory).read();
  if (!read.ok()) {
    return loadFailureFor(read);
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(read.bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state is malformed JSON"));
  }
  const QJsonObject object = document.object();
  const QSet<QString> expectedKeys{QStringLiteral("version"),
                                   QStringLiteral("preferences")};
  const QStringList objectKeys = object.keys();
  if (QSet<QString>(objectKeys.begin(), objectKeys.end()) != expectedKeys ||
      object.value(QStringLiteral("version")).toDouble() != 1.0 ||
      !object.value(QStringLiteral("preferences")).isObject()) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state has an invalid schema"));
  }
  const QJsonObject values = object.value(QStringLiteral("preferences")).toObject();
  const QStringList expected = preferenceKeys();
  const QStringList present = values.keys();
  if (QSet<QString>(present.begin(), present.end()) !=
      QSet<QString>(expected.begin(), expected.end())) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state has an invalid schema"));
  }
  const auto stringAt = [&values](const QString &key, bool *ok) {
    const QJsonValue value = values.value(key);
    *ok = *ok && value.isString();
    return value.toString();
  };
  const auto boolAt = [&values](const QString &key, bool *ok) {
    const QJsonValue value = values.value(key);
    *ok = *ok && value.isBool();
    return value.toBool();
  };
  bool shaped = true;
  Preferences loaded;
  loaded.defaultViewMode = stringAt(QStringLiteral("defaultViewMode"), &shaped);
  loaded.showHidden = boolAt(QStringLiteral("showHidden"), &shaped);
  loaded.directoriesFirst = boolAt(QStringLiteral("directoriesFirst"), &shaped);
  loaded.sortColumn = stringAt(QStringLiteral("sortColumn"), &shaped);
  loaded.sortDirection = stringAt(QStringLiteral("sortDirection"), &shaped);
  const QJsonValue iconSize = values.value(QStringLiteral("iconSize"));
  shaped = shaped && iconSize.isDouble();
  loaded.iconSize = iconSize.toInt();
  loaded.discoverNearbyServers =
      boolAt(QStringLiteral("discoverNearbyServers"), &shaped);
  loaded.defaultConnectScheme =
      stringAt(QStringLiteral("defaultConnectScheme"), &shaped);
  loaded.confirmTrash = boolAt(QStringLiteral("confirmTrash"), &shaped);
  if (!shaped) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state has an invalid shape"));
  }
  if (!loaded.isValid()) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state contains an unknown value"));
  }
  PreferencesLoadResult result;
  result.preferences = loaded;
  return result;
}

PreferencesWriteResult PreferencesStore::store(const Preferences &preferences) const {
  if (!preferences.isValid()) {
    return writeFailure(PreferencesError::Malformed,
                        QStringLiteral("Preference values are out of range"));
  }
  const QJsonObject values{
      {QStringLiteral("defaultViewMode"), preferences.defaultViewMode},
      {QStringLiteral("showHidden"), preferences.showHidden},
      {QStringLiteral("directoriesFirst"), preferences.directoriesFirst},
      {QStringLiteral("sortColumn"), preferences.sortColumn},
      {QStringLiteral("sortDirection"), preferences.sortDirection},
      {QStringLiteral("iconSize"), preferences.iconSize},
      {QStringLiteral("discoverNearbyServers"), preferences.discoverNearbyServers},
      {QStringLiteral("defaultConnectScheme"), preferences.defaultConnectScheme},
      {QStringLiteral("confirmTrash"), preferences.confirmTrash},
  };
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("preferences"), values},
  });
  const StateFile::WriteResult written =
      stateFileFor(m_stateDirectory).write(document.toJson(QJsonDocument::Compact));
  if (!written.ok()) {
    return writeFailureFor(written);
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
