// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_login_screen/sddm_config_store.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>

namespace QindaQt::Apps::SettingsLoginScreen {
namespace {

struct KeyHit {
  QString tag;
  QString value;
};

void applyFile(const QString &filePath, SddmConfigReadResult *result,
               QSet<QString> *keysOwnedFileSets, bool isOwnedFile,
               bool afterOwnedFile) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    result->unreadableFiles.append(filePath);
    return;
  }
  QString section;
  const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n');
  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';')) {
      continue;
    }
    if (line.startsWith(u'[') && line.endsWith(u']')) {
      section = line.mid(1, line.size() - 2).trimmed();
      continue;
    }
    const qsizetype equals = line.indexOf(u'=');
    if (equals <= 0) {
      continue;
    }
    const QString key = line.left(equals).trimmed();
    const QString value = line.mid(equals + 1).trimmed();
    // Only the owned keys are tracked; everything else is another tool's
    // business (or SDDM's), and this route neither shows nor overrides it.
    QString tag;
    if (section == QLatin1String("Theme") && key == QLatin1String("Current")) {
      tag = QStringLiteral("theme");
      result->values.theme = value;
    } else if (section == QLatin1String("General") &&
               key == QLatin1String("Numlock")) {
      tag = QStringLiteral("numlock");
      result->values.numlock = value;
    } else if (section == QLatin1String("Theme") &&
               key == QLatin1String("CursorTheme")) {
      tag = QStringLiteral("cursorTheme");
      result->values.cursorTheme = value;
    } else if (section == QLatin1String("Autologin") &&
               key == QLatin1String("User")) {
      tag = QStringLiteral("autologinUser");
      result->values.autologinUser = value;
    } else if (section == QLatin1String("Autologin") &&
               key == QLatin1String("Session")) {
      tag = QStringLiteral("autologinSession");
      result->values.autologinSession = value;
    }
    if (tag.isEmpty()) {
      continue;
    }
    result->winnerFileByKey.insert(tag, filePath);
    if (isOwnedFile && keysOwnedFileSets != nullptr) {
      keysOwnedFileSets->insert(tag);
    } else if (afterOwnedFile && keysOwnedFileSets != nullptr &&
               keysOwnedFileSets->contains(tag) &&
               !result->shadowedKeys.contains(tag)) {
      result->shadowedKeys.append(tag);
      if (!result->shadowingFiles.contains(filePath)) {
        result->shadowingFiles.append(filePath);
      }
    }
  }
}

} // namespace

SddmConfigReadResult readSddmConfig(const QStringList &scanDirectoriesInOrder,
                                    const QString &legacyMainFile,
                                    const QString &ownedDropInFile) {
  SddmConfigReadResult result;
  QSet<QString> keysOwnedFileSets;
  QStringList orderedFiles;
  for (const QString &directoryPath : scanDirectoriesInOrder) {
    const QDir directory(directoryPath);
    // No QDir::Readable filter: an unreadable file must reach applyFile so
    // its open failure is reported in unreadableFiles instead of silently
    // dropping the value SDDM would still read (it runs as root).
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.conf")}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
      orderedFiles.append(file.absoluteFilePath());
    }
  }
  if (!legacyMainFile.isEmpty() && QFile::exists(legacyMainFile)) {
    orderedFiles.append(legacyMainFile);
  }
  const bool checkShadowing = !ownedDropInFile.isEmpty();
  bool seenOwnedFile = false;
  for (const QString &filePath : orderedFiles) {
    const bool isOwnedFile =
        checkShadowing && QFileInfo(filePath) == QFileInfo(ownedDropInFile);
    applyFile(filePath, &result, checkShadowing ? &keysOwnedFileSets : nullptr,
              isOwnedFile, seenOwnedFile);
    if (isOwnedFile) {
      seenOwnedFile = true;
    }
  }
  return result;
}

} // namespace QindaQt::Apps::SettingsLoginScreen
