// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_store.h"

#include "proton_catalog.h"
#include "proton_pin.h"
#include "store_io.h"

#include <QDir>
#include <QJsonObject>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kVersion = 1;
constexpr qint64 kMaxBytes = 16 * 1024;
constexpr int kMaxBuildNameChars = 128;

} // namespace

PreferencesStore::PreferencesStore(QString configRoot) : m_root(std::move(configRoot)) {}

QString PreferencesStore::path() const {
  return QDir(m_root).filePath(QStringLiteral("preferences-v1.json"));
}

Preferences PreferencesStore::read(Error *error) const {
  const auto set = [error](Error value) {
    if (error != nullptr) {
      *error = value;
    }
  };
  StoreIo::ReadStatus status = StoreIo::ReadStatus::Ok;
  const QByteArray bytes = StoreIo::readBoundedFile(path(), kMaxBytes, &status);
  if (status == StoreIo::ReadStatus::Absent) {
    set(Error::Absent);
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(bytes);
  if (status != StoreIo::ReadStatus::Ok || !document.isObject()) {
    set(Error::Refused);
    return {};
  }
  const QJsonObject root = document.object();
  const QStringList allowed{QStringLiteral("version"), QStringLiteral("defaultProtonBuild")};
  for (const QString &key : root.keys()) {
    if (!allowed.contains(key)) {
      set(Error::Refused);
      return {};
    }
  }
  if (root.value(QStringLiteral("version")).toInt(-1) != kVersion) {
    set(Error::Refused); // absent, malformed, or a newer document
    return {};
  }
  Preferences preferences;
  if (root.contains(QStringLiteral("defaultProtonBuild"))) {
    bool ok = false;
    const QString name = StoreIo::boundedLine(root.value(QStringLiteral("defaultProtonBuild")),
                                              kMaxBuildNameChars, &ok);
    if (!ok || (!name.isEmpty() && (!isValidProtonBuildName(name) || isProtonAliasName(name)))) {
      set(Error::Refused);
      return {};
    }
    preferences.defaultProtonBuild = name;
  }
  set(Error::None);
  return preferences;
}

PreferencesStore::Error PreferencesStore::write(const Preferences &preferences) const {
  // AGENT-GUARD: a write refuses anything a read would refuse.
  if (!preferences.defaultProtonBuild.isEmpty() &&
      (!isValidProtonBuildName(preferences.defaultProtonBuild) ||
       isProtonAliasName(preferences.defaultProtonBuild) ||
       !StoreIo::isCleanLine(preferences.defaultProtonBuild, kMaxBuildNameChars))) {
    return Error::Refused;
  }
  QJsonObject root{{QStringLiteral("version"), kVersion}};
  if (!preferences.defaultProtonBuild.isEmpty()) {
    root.insert(QStringLiteral("defaultProtonBuild"), preferences.defaultProtonBuild);
  }
  if (!QDir().mkpath(m_root)) {
    return Error::WriteFailed;
  }
  return StoreIo::writeAtomicJson(path(), QJsonDocument(root)) ? Error::None
                                                               : Error::WriteFailed;
}

} // namespace QindaQt::QindaLutris
