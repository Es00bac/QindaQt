// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/wallpaper_catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QUrl>

#include <iterator>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

// AGENT-CONTRACT: Bundled image formats, in priority order. The shell's
// resolveWallpaperSource() (src/shell/runtime/shellpreferencevalues.cpp) must
// try exactly these extensions in this order, or one qindaqt:<name> identity
// would preview one file here and paint another (or none) on the desktop.
constexpr QLatin1StringView BundledWallpaperExtensions[] = {
    QLatin1StringView("png"), QLatin1StringView("jpg"), QLatin1StringView("jpeg"),
    QLatin1StringView("webp"), QLatin1StringView("bmp")};

int extensionPriority(const QString &suffix) {
  const QString folded = suffix.toCaseFolded();
  for (int index = 0; index < static_cast<int>(std::size(BundledWallpaperExtensions));
       ++index) {
    if (folded == BundledWallpaperExtensions[index])
      return index;
  }
  return -1;
}

} // namespace

QVariantList discoverBundledWallpapers(const QStringList &roots) {
  QVariantList result;
  QSet<QString> names;
  for (const QString &root : roots) {
    const QDir directory(root);
    QStringList filters;
    for (const QLatin1StringView extension : BundledWallpaperExtensions)
      filters.append(QStringLiteral("*.") + extension);
    const QFileInfoList files = directory.entryInfoList(
        filters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    // One qindaqt:<name> identity per basename: when several formats of the
    // same name sit side by side, the contract's priority format wins.
    QMap<QString, QFileInfo> bestByBaseName;
    for (const QFileInfo &file : files) {
      const QString key = file.completeBaseName().toCaseFolded();
      const auto existing = bestByBaseName.constFind(key);
      if (existing == bestByBaseName.constEnd() ||
          extensionPriority(file.suffix()) <
              extensionPriority(existing->suffix()))
        bestByBaseName.insert(key, file);
    }
    for (const QFileInfo &file : bestByBaseName) {
      const QString key = file.completeBaseName().toCaseFolded();
      if (names.contains(key))
        continue;
      names.insert(key);
      QString label = file.completeBaseName();
      label.replace(QLatin1Char('-'), QLatin1Char(' '));
      if (!label.isEmpty())
        label[0] = label.at(0).toUpper();
      result.append(
          QVariantMap{{QStringLiteral("name"), label},
                      {QStringLiteral("path"), file.absoluteFilePath()},
                      {QStringLiteral("previewUrl"),
                       QUrl::fromLocalFile(file.absoluteFilePath())},
                      {QStringLiteral("value"),
                       QStringLiteral("qindaqt:") + file.completeBaseName()}});
    }
  }
  return result;
}

} // namespace QindaQt::Apps::SettingsAppearance
