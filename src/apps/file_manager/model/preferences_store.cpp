// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_store.h"

#include "preferences_json.h"
#include "state_file.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto preferencesFileName = "preferences-v2.json";
constexpr auto version1FileName = "preferences-v1.json";

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

[[nodiscard]] StateFile version1FileFor(const QString &directory) {
  return StateFile(directory, QByteArray(version1FileName),
                   PreferencesStore::maximumVersion1Bytes);
}

[[nodiscard]] QString tooLargeMessage(qint64 bound) {
  return QStringLiteral("Preference state exceeds %1 KiB").arg(bound / 1024);
}

[[nodiscard]] PreferencesLoadResult loadFailureFor(const StateFile::ReadResult &read,
                                                   qint64 bound) {
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
    return loadFailure(PreferencesError::TooLarge, tooLargeMessage(bound));
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
                        tooLargeMessage(PreferencesStore::maximumBytes));
  case StateFile::Error::WriteFailed:
  case StateFile::Error::ReadFailed:
  case StateFile::Error::Absent:
  case StateFile::Error::None:
    break;
  }
  return writeFailure(PreferencesError::WriteFailed, written.systemDiagnostic);
}

// Parses one document's bytes as `version`.
[[nodiscard]] PreferencesLoadResult parse(const QByteArray &bytes, int version) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(PreferencesError::Malformed,
                       QStringLiteral("Preference state is malformed JSON"));
  }
  QString diagnostic;
  const std::optional<Preferences> decoded =
      PreferencesJson::decode(document.object(), version, &diagnostic);
  if (!decoded) {
    return loadFailure(PreferencesError::Malformed, diagnostic);
  }
  PreferencesLoadResult result;
  result.preferences = *decoded;
  result.migrated = version == 1;
  return result;
}

} // namespace

PreferencesStore::PreferencesStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString PreferencesStore::filePath() const {
  return stateFileFor(m_stateDirectory).filePath();
}

PreferencesLoadResult PreferencesStore::load() const {
  const StateFile::ReadResult read = stateFileFor(m_stateDirectory).read();
  if (read.ok()) {
    return parse(read.bytes, 2);
  }
  if (read.error != StateFile::Error::Absent) {
    return loadFailureFor(read, maximumBytes);
  }
  // No v2 yet: the ADR-0198 document, if there is one, is migrated.
  const StateFile::ReadResult legacy = version1FileFor(m_stateDirectory).read();
  if (!legacy.ok()) {
    return loadFailureFor(legacy, maximumVersion1Bytes);
  }
  return parse(legacy.bytes, 1);
}

PreferencesWriteResult PreferencesStore::store(const Preferences &preferences) const {
  if (!preferences.isValid()) {
    return writeFailure(PreferencesError::Malformed,
                        QStringLiteral("Preference values are out of range"));
  }
  const QJsonDocument document(PreferencesJson::encode(preferences));
  const StateFile::WriteResult written =
      stateFileFor(m_stateDirectory).write(document.toJson(QJsonDocument::Compact));
  if (!written.ok()) {
    return writeFailureFor(written);
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
