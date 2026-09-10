// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_journal_store.h"
#include "state_directory.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::Apps::TextEditor {
namespace {

RecoveryJournalLoadResult loadFailure(const RecoveryJournalError error,
                                      const QString &diagnostic) {
  RecoveryJournalLoadResult result;
  result.error = error;
  result.diagnostic = diagnostic.left(256);
  return result;
}

RecoveryJournalWriteResult writeFailure(const RecoveryJournalError error,
                                        const QString &diagnostic) {
  return {.error = error, .diagnostic = diagnostic.left(256)};
}

QString journalFileName(const QString &key) {
  return key + QStringLiteral(".json");
}

} // namespace

RecoveryJournalStore::RecoveryJournalStore(QString rootDirectory)
    : m_rootDirectory(QDir::cleanPath(std::move(rootDirectory))) {}

QString RecoveryJournalStore::keyForPath(const QString &canonicalPath) {
  return QStringLiteral("file-%1")
      .arg(QString::fromLatin1(
          QCryptographicHash::hash(canonicalPath.toUtf8(),
                                   QCryptographicHash::Sha256)
              .toHex()));
}

QString RecoveryJournalStore::keyForUntitled(const int sequence) {
  return QStringLiteral("untitled-%1-%2")
      .arg(QCoreApplication::applicationPid())
      .arg(sequence);
}

bool RecoveryJournalStore::isValidKey(const QString &key) {
  static const QRegularExpression shape(
      QStringLiteral("^(file-[0-9a-f]{64}|untitled-[0-9]+-[0-9]+)$"));
  return shape.match(key).hasMatch();
}

bool RecoveryJournalStore::contains(const QString &key) const {
  if (!isValidKey(key)) {
    return false;
  }
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_rootDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return false;
  }
  struct stat status {};
  const bool present =
      ::fstatat(directoryDescriptor,
                QFile::encodeName(journalFileName(key)).constData(), &status,
                AT_SYMLINK_NOFOLLOW) == 0 &&
      S_ISREG(status.st_mode);
  ::close(directoryDescriptor);
  return present;
}

RecoveryJournalLoadResult
RecoveryJournalStore::load(const QString &key) const {
  if (!isValidKey(key)) {
    return loadFailure(RecoveryJournalError::Malformed,
                       QStringLiteral("Journal key is invalid"));
  }
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_rootDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return loadFailure(directoryError == ENOENT
                           ? RecoveryJournalError::Absent
                           : RecoveryJournalError::InvalidRoot,
                       directoryError == ENOENT
                           ? QString()
                           : QStringLiteral("Recovery journal root is unsafe"));
  }
  auto result = loadAt(directoryDescriptor, key);
  ::close(directoryDescriptor);
  return result;
}

RecoveryJournalLoadResult
RecoveryJournalStore::loadAt(const int directoryDescriptor,
                             const QString &key) const {
  const QByteArray name = QFile::encodeName(journalFileName(key));
  const int descriptor =
      ::openat(directoryDescriptor, name.constData(),
               O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (descriptor < 0) {
    return loadFailure(errno == ENOENT ? RecoveryJournalError::Absent
                                       : RecoveryJournalError::Malformed,
                       errno == ENOENT
                           ? QString()
                           : QStringLiteral("Journal is not a regular file"));
  }
  struct stat status {};
  if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
    ::close(descriptor);
    return loadFailure(RecoveryJournalError::Malformed,
                       QStringLiteral("Journal is not a regular file"));
  }
  if (status.st_size > maximumJournalBytes) {
    ::close(descriptor);
    return loadFailure(RecoveryJournalError::TooLarge,
                       QStringLiteral("Journal exceeds the size cap"));
  }
  QFile file;
  if (!file.open(descriptor, QIODevice::ReadOnly,
                 QFileDevice::AutoCloseHandle)) {
    ::close(descriptor);
    return loadFailure(RecoveryJournalError::ReadFailed, file.errorString());
  }
  const QByteArray bytes = file.read(maximumJournalBytes + 1);
  if (file.error() != QFileDevice::NoError) {
    return loadFailure(RecoveryJournalError::ReadFailed, file.errorString());
  }
  if (bytes.size() > maximumJournalBytes) {
    return loadFailure(RecoveryJournalError::TooLarge,
                       QStringLiteral("Journal exceeds the size cap"));
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return loadFailure(RecoveryJournalError::Malformed,
                       QStringLiteral("Journal is malformed JSON"));
  }
  const QJsonObject object = document.object();
  const QSet<QString> expectedKeys{QStringLiteral("version"),
                                   QStringLiteral("path"),
                                   QStringLiteral("text")};
  const QStringList objectKeys = object.keys();
  if (QSet<QString>(objectKeys.begin(), objectKeys.end()) != expectedKeys ||
      object.value(QStringLiteral("version")).toDouble() != 1.0 ||
      !object.value(QStringLiteral("text")).isString() ||
      !(object.value(QStringLiteral("path")).isString() ||
        object.value(QStringLiteral("path")).isNull())) {
    return loadFailure(RecoveryJournalError::Malformed,
                       QStringLiteral("Journal has an invalid schema"));
  }
  RecoveryJournalEntry entry;
  entry.key = key;
  entry.text = object.value(QStringLiteral("text")).toString();
  const QJsonValue path = object.value(QStringLiteral("path"));
  if (path.isString()) {
    const QString value = path.toString();
    if (!QFileInfo(value).isAbsolute() || value.size() > 4096) {
      return loadFailure(RecoveryJournalError::Malformed,
                         QStringLiteral("Journal path is not absolute"));
    }
    entry.path = value;
  }
  RecoveryJournalLoadResult result;
  result.entry = std::move(entry);
  return result;
}

RecoveryJournalWriteResult
RecoveryJournalStore::store(const QString &key, const QString &path,
                            const QString &text) const {
  if (!isValidKey(key)) {
    return writeFailure(RecoveryJournalError::Malformed,
                        QStringLiteral("Journal key is invalid"));
  }
  if (!path.isEmpty() && !QFileInfo(path).isAbsolute()) {
    return writeFailure(RecoveryJournalError::Malformed,
                        QStringLiteral("Journal path is not absolute"));
  }
  const QJsonDocument document(QJsonObject{
      {QStringLiteral("version"), 1},
      {QStringLiteral("path"), path.isEmpty() ? QJsonValue()
                                              : QJsonValue(path)},
      {QStringLiteral("text"), text},
  });
  const QByteArray bytes = document.toJson(QJsonDocument::Compact);
  if (bytes.size() > maximumJournalBytes) {
    // Keep any earlier journal: a truncated replacement would silently lose
    // the last bounded recoverable state.
    return writeFailure(RecoveryJournalError::TooLarge,
                        QStringLiteral("Document exceeds the journal cap"));
  }
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_rootDirectory, true, &directoryError);
  if (directoryDescriptor < 0) {
    return writeFailure(
        RecoveryJournalError::InvalidRoot,
        QStringLiteral("Recovery journal directory is unavailable or unsafe"));
  }
  const QByteArray name = QFile::encodeName(journalFileName(key));
  if (!StateDirectory::finalEntryIsRegularOrAbsent(directoryDescriptor,
                                                   name.constData())) {
    ::close(directoryDescriptor);
    return writeFailure(RecoveryJournalError::InvalidRoot,
                        QStringLiteral("Journal target is unsafe"));
  }
  QSaveFile file(StateDirectory::descriptorFilePath(directoryDescriptor,
                                                    name.constData()));
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    ::close(directoryDescriptor);
    return writeFailure(RecoveryJournalError::WriteFailed, file.errorString());
  }
  if (!file.commit()) {
    ::close(directoryDescriptor);
    return writeFailure(RecoveryJournalError::WriteFailed, file.errorString());
  }
  ::close(directoryDescriptor);
  return {};
}

RecoveryJournalWriteResult RecoveryJournalStore::clear(const QString &key) const {
  if (!isValidKey(key)) {
    return writeFailure(RecoveryJournalError::Malformed,
                        QStringLiteral("Journal key is invalid"));
  }
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_rootDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return directoryError == ENOENT
               ? RecoveryJournalWriteResult{}
               : writeFailure(RecoveryJournalError::InvalidRoot,
                              QStringLiteral("Recovery journal root is unsafe"));
  }
  const QByteArray name = QFile::encodeName(journalFileName(key));
  if (::unlinkat(directoryDescriptor, name.constData(), 0) != 0 &&
      errno != ENOENT) {
    ::close(directoryDescriptor);
    return writeFailure(RecoveryJournalError::WriteFailed,
                        QStringLiteral("Could not remove recovery journal"));
  }
  ::close(directoryDescriptor);
  return {};
}

QList<RecoveryJournalEntry> RecoveryJournalStore::entries() const {
  QList<RecoveryJournalEntry> result;
  int directoryError = 0;
  const int directoryDescriptor =
      StateDirectory::open(m_rootDirectory, false, &directoryError);
  if (directoryDescriptor < 0) {
    return result;
  }
  const QString root =
      QStringLiteral("/proc/self/fd/%1").arg(directoryDescriptor);
  const QStringList names =
      QDir(root).entryList({QStringLiteral("*.json")},
                           QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &name : names) {
    if (result.size() >= maximumJournals) {
      break;
    }
    const QString key = name.left(name.size() - 5);
    if (!isValidKey(key)) {
      continue;
    }
    auto loaded = loadAt(directoryDescriptor, key);
    if (loaded.ok()) {
      result.append(std::move(*loaded.entry));
    }
  }
  ::close(directoryDescriptor);
  return result;
}

} // namespace QindaQt::Apps::TextEditor
