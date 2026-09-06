// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/wallpaper_catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace QindaQt::Apps::SettingsAppearance {

QVariantList discoverBundledWallpapers(const QStringList &roots) {
  QVariantList result;
  QSet<QString> names;
  for (const QString &root : roots) {
    const QDir directory(root);
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.png")}, QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files) {
      const QString key = file.fileName().toCaseFolded();
      if (names.contains(key))
        continue;
      names.insert(key);
      QString label = file.completeBaseName();
      label.replace(QLatin1Char('-'), QLatin1Char(' '));
      if (!label.isEmpty())
        label[0] = label.at(0).toUpper();
      result.append(
          QVariantMap{{QStringLiteral("name"), label},
                      {QStringLiteral("path"), file.absoluteFilePath()}});
    }
  }
  return result;
}

} // namespace QindaQt::Apps::SettingsAppearance
