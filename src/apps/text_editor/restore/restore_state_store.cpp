// SPDX-License-Identifier: GPL-3.0-or-later
#include "restore_state_store.h"
#include "state_directory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::Apps::TextEditor {
namespace {

constexpr auto stateFileName = "open-documents-v1.json";

RestoreLoadResult loadFailure(const RestoreStateError error,
                              const QString &diagnostic) {
  RestoreLoadResult result;
  result.error = error;
  result.diagnostic = diagnostic.left(256);
  return result;
}

RestoreWriteResult writeFailure(const RestoreStateError error,
                                const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

} // namespace

RestoreStateStore::RestoreStateStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString RestoreStateStore::filePath() const {
  return QDir(m_stateDirectory).filePath(QString::fromLatin1(stateFileName));
}

bool RestoreStateStore::validate(const RestoreState &state,
                                 QString *diagnostic) {
  if (state.paths.size() > maximumPaths) {
    *diagnostic = QStringLiteral("Restore state contains too many paths");
    return false;
  }
  if ((state.paths.isEmpty() && state.activeIndex != -1) ||
      (!state.paths.isEmpty() &&
       (state.activeIndex < 0 || state.activeIndex >= state.paths.size()))) {
    *diagnostic = QStringLiteral("Restore state has an invalid active index");
    return false;
  }
  QSet<QString> unique;
  for (const QString &path : state.paths) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QString normalized = QDir::cleanPath(
        canonical.isEmpty() ? info.absoluteFilePath() : canonical);
    if (path.size() > maximumPathLength || !path.isValidUtf16() ||
        path.contains(QChar::Null) || !QFileInfo(path).isAbsolute() ||
        path != normalized || unique.contains(normalized)) {
      *diagnostic = QStringLiteral("Restore state contains an invalid path");
      return false;
    }
    unique.insert(normalized);
  }
  return true;
}

RestoreLoadResult RestoreStateStore::load() const {
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_stateDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return loadFailure(directoryError == ENOENT
                           ? RestoreStateError::Absent
                           : RestoreStateError::InvalidRoot,
                       directoryError == ENOENT
                           ? QString()
                           : QStringLiteral("Restore state root is unsafe"));
  }
  const int descriptor =
      ::openat(directoryDescriptor, stateFileName,
               O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  const int openError = errno;
  ::close(directoryDescriptor);
  if (descriptor < 0) {
    return loadFailure(
        openError == ENOENT ? RestoreStateError::Absent
                            : RestoreStateError::Malformed,
        openError == ENOENT
            ? QString()
            : QStringLiteral("Restore state is not a regular file"));
  }
  struct stat status{};
  if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
    ::close(descriptor);
    return loadFailure(RestoreStateError::Malformed,
                       QStringLiteral("Restore state is not a regular file"));
  }
  if (status.st_size > maximumBytes) {
    ::close(descriptor);
    return loadFailure(RestoreStateError::TooLarge,
                       QStringLiteral("Restore state exceeds 64 KiB"));
  }
  QFile file;
  if (!file.open(descriptor, QIODevice::ReadOnly,
                 QFileDevice::AutoCloseHandle)) {
    ::close(descriptor);
    return loadFailure(RestoreStateError::ReadFailed, file.errorString());
  }
  const QByteArray bytes = file.read(maximumBytes + 1);
  if (file.error() != QFileDevice::NoError) {
    return loadFailure(RestoreStateError::ReadFailed, file.errorString());
  }
  if (bytes.size() > maximumBytes) {
    return loadFailure(RestoreStateError::TooLarge,
                       QStringLiteral("Restore state exceeds 64 KiB"));
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(RestoreStateError::Malformed,
                       QStringLiteral("Restore state is malformed JSON"));
  }
  const QJsonObject object = document.object();
  const QSet<QString> expectedKeys{QStringLiteral("version"),
                                   QStringLiteral("paths"),
                                   QStringLiteral("activeIndex")};
  const QStringList objectKeys = object.keys();
  if (QSet<QString>(objectKeys.begin(), objectKeys.end()) != expectedKeys ||
      !object.value(QStringLiteral("version")).isDouble() ||
      object.value(QStringLiteral("version")).toDouble() != 1.0 ||
      !object.value(QStringLiteral("paths")).isArray() ||
      !object.value(QStringLiteral("activeIndex")).isDouble()) {
    return loadFailure(RestoreStateError::Malformed,
                       QStringLiteral("Restore state has an invalid schema"));
  }
  const double activeNumber =
      object.value(QStringLiteral("activeIndex")).toDouble();
  const int activeIndex = static_cast<int>(activeNumber);
  if (activeNumber != static_cast<double>(activeIndex)) {
    return loadFailure(
        RestoreStateError::Malformed,
        QStringLiteral("Restore active index is not an integer"));
  }
  RestoreState state;
  state.activeIndex = activeIndex;
  const QJsonArray paths = object.value(QStringLiteral("paths")).toArray();
  if (paths.size() > maximumPaths) {
    return loadFailure(RestoreStateError::Malformed,
                       QStringLiteral("Restore state contains too many paths"));
  }
  for (const QJsonValue &value : paths) {
    if (!value.isString()) {
      return loadFailure(RestoreStateError::Malformed,
                         QStringLiteral("Restore path is not a string"));
    }
    state.paths.append(value.toString());
  }
  QString diagnostic;
  if (!validate(state, &diagnostic)) {
    return loadFailure(RestoreStateError::Malformed, diagnostic);
  }
  RestoreLoadResult result;
  result.state = std::move(state);
  return result;
}

RestoreWriteResult RestoreStateStore::store(const RestoreState &state) const {
  QString diagnostic;
  if (!validate(state, &diagnostic)) {
    return writeFailure(RestoreStateError::Malformed, diagnostic);
  }
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_stateDirectory, true, &directoryError);
  if (directoryDescriptor < 0) {
    return writeFailure(
        RestoreStateError::InvalidRoot,
        QStringLiteral("Restore state directory is unavailable or unsafe"));
  }

  QJsonArray paths;
  for (const QString &path : state.paths) {
    paths.append(path);
  }
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("paths"), paths},
      {QStringLiteral("activeIndex"), state.activeIndex},
  });
  const QByteArray bytes = document.toJson(QJsonDocument::Compact);
  if (bytes.size() > maximumBytes) {
    ::close(directoryDescriptor);
    return writeFailure(RestoreStateError::TooLarge,
                        QStringLiteral("Restore state exceeds 64 KiB"));
  }
  // AGENT-GUARD: The directory descriptor is reached component-by-component
  // with O_NOFOLLOW. Keep it open through commit so a symlinked ancestor can
  // never redirect the paths-only inventory outside the selected state root.
  if (!StateDirectory::finalEntryIsRegularOrAbsent(directoryDescriptor, stateFileName)) {
    ::close(directoryDescriptor);
    return writeFailure(RestoreStateError::InvalidRoot,
                        QStringLiteral("Restore state target is unsafe"));
  }
  QSaveFile file(StateDirectory::descriptorFilePath(directoryDescriptor, stateFileName));
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    ::close(directoryDescriptor);
    return writeFailure(RestoreStateError::WriteFailed, file.errorString());
  }
  if (!file.commit()) {
    ::close(directoryDescriptor);
    return writeFailure(RestoreStateError::WriteFailed, file.errorString());
  }
  ::close(directoryDescriptor);
  return {};
}

RestoreWriteResult RestoreStateStore::clear() const {
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_stateDirectory, false, &directoryError);
  if (directoryDescriptor < 0 && directoryError == ENOENT) {
    return {};
  }
  if (directoryDescriptor < 0 ||
      !StateDirectory::finalEntryIsRegularOrAbsent(directoryDescriptor, stateFileName)) {
    if (directoryDescriptor >= 0) {
      ::close(directoryDescriptor);
    }
    return writeFailure(RestoreStateError::WriteFailed,
                        QStringLiteral("Could not remove restore state"));
  }
  if (::unlinkat(directoryDescriptor, stateFileName, 0) != 0 &&
      errno != ENOENT) {
    ::close(directoryDescriptor);
    return writeFailure(RestoreStateError::WriteFailed,
                        QStringLiteral("Could not remove restore state"));
  }
  ::close(directoryDescriptor);
  return {};
}

} // namespace QindaQt::Apps::TextEditor
