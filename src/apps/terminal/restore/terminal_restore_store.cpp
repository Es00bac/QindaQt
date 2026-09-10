// SPDX-License-Identifier: GPL-3.0-or-later
#include "restore/terminal_restore_store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace QindaQt::Apps::Terminal {
namespace {

constexpr auto stateFileName = "terminal-restore-state-v1.json";

// Profile ids are builtin names, CLI safe ids, or brace-less UUIDs; all of
// those are lowercase ASCII within [a-z0-9-]. Anything wider is hostile.
bool profileIdIsSafe(const QString &profileId) {
  if (profileId.isEmpty() || profileId.size() > 64) {
    return false;
  }
  for (const QChar ch : profileId) {
    const ushort code = ch.unicode();
    const bool lower = code >= 'a' && code <= 'z';
    const bool digit = code >= '0' && code <= '9';
    if (!lower && !digit && ch != QLatin1Char('-')) {
      return false;
    }
  }
  return true;
}

// A restorable directory is an absolute POSIX path without NUL. We do not
// resolve symlinks or canonicalize here: the record is a hint for a future
// launch, and planTerminalRestore re-checks existence before use.
bool directoryIsSafe(const QString &workingDirectory) {
  return !workingDirectory.isEmpty() &&
         workingDirectory.size() <= kMaxTerminalRestorePathLength &&
         workingDirectory.startsWith(QLatin1Char('/')) &&
         workingDirectory.isValidUtf16() &&
         !workingDirectory.contains(QChar::Null);
}

bool entryIsSafe(const TerminalRestoreEntry &entry) {
  return profileIdIsSafe(entry.profileId) &&
         directoryIsSafe(entry.workingDirectory);
}

QJsonObject entryToJson(const TerminalRestoreEntry &entry) {
  return QJsonObject{
      {QStringLiteral("profileId"), entry.profileId},
      {QStringLiteral("workingDirectory"), entry.workingDirectory},
  };
}

} // namespace

QString encodeTerminalRestoreEntries(const QList<TerminalRestoreEntry> &entries,
                                     bool *ok) {
  *ok = false;
  if (entries.size() > kMaxTerminalRestoreEntries) {
    return QString();
  }
  QJsonArray array;
  for (const TerminalRestoreEntry &entry : entries) {
    if (!entryIsSafe(entry)) {
      return QString();
    }
    array.append(entryToJson(entry));
  }
  const QJsonDocument document(array);
  *ok = true;
  return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

TerminalRestoreEntriesResult decodeTerminalRestoreEntries(const QString &json) {
  TerminalRestoreEntriesResult result;
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    result.diagnostic = QStringLiteral("Terminal restore state is malformed");
    return result;
  }
  const QJsonArray array = document.array();
  if (array.size() > kMaxTerminalRestoreEntries) {
    result.diagnostic =
        QStringLiteral("Terminal restore state has too many entries");
    return result;
  }
  for (const QJsonValue &value : array) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("profileId")).isString() ||
        !object.value(QStringLiteral("workingDirectory")).isString()) {
      continue;
    }
    TerminalRestoreEntry entry;
    entry.profileId = object.value(QStringLiteral("profileId")).toString();
    entry.workingDirectory =
        object.value(QStringLiteral("workingDirectory")).toString();
    if (!entryIsSafe(entry)) {
      continue;
    }
    result.entries.append(entry);
  }
  result.ok = true;
  return result;
}

QList<TerminalRestoreEntry>
terminalRestoreWithExitAppended(QList<TerminalRestoreEntry> recorded,
                                const TerminalRestoreEntry &closed) {
  for (auto it = recorded.begin(); it != recorded.end();) {
    if (*it == closed) {
      it = recorded.erase(it);
    } else {
      ++it;
    }
  }
  recorded.append(closed);
  while (recorded.size() > kMaxTerminalRestoreEntries) {
    recorded.removeFirst();
  }
  return recorded;
}

TerminalRestorePlan planTerminalRestore(
    const QList<TerminalRestoreEntry> &recorded,
    const std::function<bool(const QString &)> &profileIdKnown,
    const std::function<bool(const QString &)> &directoryExists) {
  TerminalRestorePlan plan;
  for (const TerminalRestoreEntry &entry : recorded) {
    if (!profileIdKnown(entry.profileId) ||
        !directoryExists(entry.workingDirectory)) {
      continue;
    }
    if (!plan.hasPrimary) {
      plan.primary = entry;
      plan.hasPrimary = true;
    } else {
      plan.dispatched.append(entry);
    }
  }
  return plan;
}

TerminalRestoreStore::TerminalRestoreStore(QString stateDirectory)
    : m_stateDirectory(QDir::cleanPath(std::move(stateDirectory))) {}

QString TerminalRestoreStore::filePath() const {
  return QDir(m_stateDirectory).filePath(QString::fromLatin1(stateFileName));
}

TerminalRestoreEntriesResult TerminalRestoreStore::load() const {
  QFile file(filePath());
  if (!file.exists()) {
    TerminalRestoreEntriesResult result;
    result.ok = true;
    result.absent = true;
    return result;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    TerminalRestoreEntriesResult result;
    result.diagnostic = QStringLiteral("Terminal restore state is unreadable");
    return result;
  }
  const QByteArray bytes = file.read(maximumBytes + 1);
  if (bytes.size() > maximumBytes) {
    TerminalRestoreEntriesResult result;
    result.diagnostic = QStringLiteral("Terminal restore state exceeds 64 KiB");
    return result;
  }
  return decodeTerminalRestoreEntries(QString::fromUtf8(bytes));
}

TerminalRestoreWriteResult
TerminalRestoreStore::store(const QList<TerminalRestoreEntry> &entries) const {
  bool encoded = false;
  const QString json = encodeTerminalRestoreEntries(entries, &encoded);
  if (!encoded) {
    return {false, QStringLiteral("Terminal restore entries are invalid")};
  }
  // The state root is created lazily here so a first run that never enables
  // the policy leaves no empty directory behind.
  if (!QDir().mkpath(m_stateDirectory)) {
    return {false,
            QStringLiteral("Terminal restore state directory is unavailable")};
  }
  QSaveFile file(filePath());
  file.setDirectWriteFallback(false);
  const QByteArray bytes = json.toUtf8();
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    return {false, QStringLiteral("Terminal restore state write failed")};
  }
  if (!file.commit()) {
    return {false, QStringLiteral("Terminal restore state commit failed")};
  }
  return {true, QString()};
}

TerminalRestoreWriteResult TerminalRestoreStore::clear() const {
  QFile file(filePath());
  if (!file.exists()) {
    return {true, QString()};
  }
  if (!file.remove()) {
    return {false, QStringLiteral("Could not remove terminal restore state")};
  }
  return {true, QString()};
}

} // namespace QindaQt::Apps::Terminal
