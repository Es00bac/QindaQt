// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"
#include "navigation_history.h"
#include "../network/network_location.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QString>
#include <QStringList>
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
  // An application row has no meaningful size: unknown reads as a dash.
  if (entry.isDirectory || !entry.applicationId.isEmpty()) {
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
  if (!entry.kindText.isEmpty()) {
    return entry.kindText;
  }
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

// The listing's status notice ("3 matching items; 2 hidden"), kept here
// with the other presentation text so NavigationController stays within its
// source-size budget.
[[nodiscard]] inline QString listingNotices(bool filterActive, qsizetype visibleCount,
                                            bool truncated, qsizetype listedCount,
                                            int hiddenCount) {
  QStringList notices;
  if (filterActive) {
    notices.append(visibleCount == 0 ? QStringLiteral("No matching items")
                   : visibleCount == 1
                       ? QStringLiteral("1 matching item")
                       : QStringLiteral("%1 matching items").arg(visibleCount));
  }
  if (truncated) {
    notices.append(QStringLiteral("Showing the first %1 entries").arg(listedCount));
  }
  if (hiddenCount > 0) {
    notices.append(QStringLiteral("%1 hidden").arg(hiddenCount));
  }
  return notices.join(QStringLiteral("; "));
}

// Marshals the visible listing for QML. entryIconName/previewUrl stay in
// their owning modules; this helper owns only the plain QVariant mapping.
// `groupLabel` names the entry's Group By heading (ADR-0270), empty when the
// listing is not grouped.
// AGENT-GUARD: The identity fields cross QVariant -> JavaScript -> QVariant
// before mutation dispatch. Decimal strings preserve all 64 bits; JS Number
// would round current-epoch nanoseconds and make every UI mutation fail its
// optimistic identity check.
template <typename IconNameFn, typename PreviewUrlFn, typename GroupLabelFn>
[[nodiscard]] QVariantList
entryListToVariants(const QVector<DirectoryEntry> &entries, quint64 generation,
                    IconNameFn entryIconName, PreviewUrlFn previewUrl,
                    GroupLabelFn groupLabel) {
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
        // ADR-0270: the Details view's listing-time columns; an invalid date
        // or a -1 id is unknown and shows as a dash.
        {QStringLiteral("created"), entry.created},
        {QStringLiteral("accessed"), entry.accessed},
        {QStringLiteral("ownerId"), entry.ownerId},
        {QStringLiteral("groupId"), entry.groupId},
        {QStringLiteral("group"), groupLabel(entry)},
        // ADR-0262: application rows open through ApplicationsController;
        // "launchable" is false only for a row whose note explains why not.
        {QStringLiteral("applicationId"), entry.applicationId},
        {QStringLiteral("launchable"), entry.note.isEmpty()},
        {QStringLiteral("note"), entry.note},
    });
  }
  return list;
}

} // namespace EntryPresentation

// Breadcrumb marshalling for QML, kept out of NavigationController with the
// other presentation helpers so the controller stays under the project's
// source-size invariant. The remote branch mirrors NavigationHistory's
// shape with network URL segments instead of plain paths.
namespace NavigationPresentation {

[[nodiscard]] inline QVariantList
breadcrumbVariants(bool remoteActive, const QUrl &remoteUrl, bool hasCurrent,
                   const QString &currentPath) {
  QVariantList list;
  if (!hasCurrent) {
    return list;
  }
  if (remoteActive) {
    const auto segments = NetworkLocation::breadcrumbFor(remoteUrl);
    list.reserve(segments.size());
    for (const auto &segment : segments) {
      list.append(QVariantMap{{QStringLiteral("name"), segment.name},
                              {QStringLiteral("path"), segment.url.toString()}});
    }
    return list;
  }
  const auto segments = NavigationHistory::breadcrumbFor(currentPath);
  list.reserve(segments.size());
  for (const auto &segment : segments) {
    list.append(QVariantMap{{QStringLiteral("name"), segment.name},
                            {QStringLiteral("path"), segment.path}});
  }
  return list;
}

} // namespace NavigationPresentation

} // namespace QindaQt::Apps::FileManager
