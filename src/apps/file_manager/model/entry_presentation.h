// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// Presentation text is produced C++-side so QML delegates stay dumb and the
// formatting rules are unit-testable through the public entries() snapshot.
// Kept out of NavigationController so the controller stays under the
// project's source-size invariant; pure functions, no state.
namespace EntryPresentation {

[[nodiscard]] inline QString sizeTextFor(const DirectoryEntry &entry) {
  if (entry.isDirectory) {
    return QStringLiteral("—");
  }
  return QLocale().formattedDataSize(entry.size);
}

[[nodiscard]] inline QString modifiedTextFor(const DirectoryEntry &entry) {
  if (!entry.lastModified.isValid()) {
    return QString();
  }
  return QLocale().toString(entry.lastModified, QLocale::ShortFormat);
}

[[nodiscard]] inline QString kindTextFor(const DirectoryEntry &entry) {
  if (entry.isDirectory) {
    return QStringLiteral("Folder");
  }
  if (entry.isSymlink) {
    return QStringLiteral("Link");
  }
  const QString suffix = QFileInfo(entry.name).suffix();
  if (suffix.isEmpty()) {
    return QStringLiteral("File");
  }
  return QStringLiteral("%1 File").arg(suffix.toUpper());
}

// Marshals the visible listing for QML. entryIconName/previewUrl stay in
// their owning modules; this helper owns only the plain QVariant mapping.
// AGENT-GUARD: The identity fields cross QVariant -> JavaScript -> QVariant
// before mutation dispatch. Decimal strings preserve all 64 bits; JS Number
// would round current-epoch nanoseconds and make every UI mutation fail its
// optimistic identity check.
template <typename IconNameFn, typename PreviewUrlFn>
[[nodiscard]] QVariantList
entryListToVariants(const QVector<DirectoryEntry> &entries, quint64 generation,
                    IconNameFn entryIconName, PreviewUrlFn previewUrl) {
  QVariantList list;
  list.reserve(entries.size());
  for (const DirectoryEntry &entry : entries) {
    list.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("path"), entry.absolutePath},
        {QStringLiteral("isDirectory"), entry.isDirectory},
        {QStringLiteral("isSymlink"), entry.isSymlink},
        {QStringLiteral("isHidden"), entry.isHidden},
        {QStringLiteral("isReadable"), entry.isReadable},
        {QStringLiteral("size"), entry.size},
        {QStringLiteral("modified"), entry.lastModified},
        {QStringLiteral("sizeText"), sizeTextFor(entry)},
        {QStringLiteral("modifiedText"), modifiedTextFor(entry)},
        {QStringLiteral("kindText"), kindTextFor(entry)},
        {QStringLiteral("iconName"), entryIconName(entry)},
        {QStringLiteral("previewUrl"), previewUrl(entry, generation)},
        {QStringLiteral("device"), QString::number(entry.device)},
        {QStringLiteral("inode"), QString::number(entry.inode)},
        {QStringLiteral("identitySize"), QString::number(entry.identitySize)},
        {QStringLiteral("modifiedNanoseconds"),
         QString::number(entry.modifiedNanoseconds)},
        {QStringLiteral("mode"), QString::number(entry.mode)},
    });
  }
  return list;
}

} // namespace EntryPresentation

} // namespace QindaQt::Apps::FileManager
