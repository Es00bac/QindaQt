// SPDX-License-Identifier: GPL-3.0-or-later
// Dock edits and path opening for QuickLaunchController (ADR-0265). Every
// edit is one DockItems operation applied through the launcher facade, which
// owns the confirmed Settings1 write; every open goes through DockPathPort.
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include "launcher_applet_controller.h"

#include "qindaqt/shell/desktop_controls/dock_path_port.h"

#include <qindaqt/services/dock_items/dock_items.h>

#include <QDir>
#include <QPointer>
#include <QUrl>

namespace QindaQt::Shell::DesktopControls {
namespace {

using Services::DockItems::DockEditError;
using Services::DockItems::DockItem;
using Services::DockItems::DockItemKind;
using Services::DockItems::DockItems;

// A pinned folder's stack shows at most this many children; the File
// Manager is one click away for the rest.
constexpr int kStackEntries = 48;

// User-typed or suggested group names are normalized to one line inside the
// stored bound; an empty result falls back to a neutral name.
QString boundedGroupName(const QString &requested)
{
  const QString name = requested.simplified()
                           .left(Services::DockItems::Bounds::maxNameLength)
                           .trimmed();
  return name.isEmpty() ? QStringLiteral("Group") : name;
}

} // namespace

bool QuickLaunchController::edit(
    const std::function<DockEditError(DockItems &)> &change)
{
  if (m_launcher == nullptr) {
    publishFeedback(QStringLiteral("The Dock is unavailable"));
    return false;
  }
  QString refusal;
  if (!m_launcher->editDock(change, &refusal)) {
    publishFeedback(refusal);
    return false;
  }
  clearFeedback();
  return true;
}

bool QuickLaunchController::moveItem(int index, int gap)
{
  return edit([index, gap](DockItems &items) { return items.moveToGap(index, gap); });
}

bool QuickLaunchController::removeItem(int index)
{
  return edit([index](DockItems &items) { return items.removeAt(index); });
}

bool QuickLaunchController::insertApplication(int gap, const QString &entryId)
{
  if (m_launcher == nullptr || m_launcher->applicationPresentation(entryId).isEmpty()) {
    publishFeedback(QStringLiteral("That application is not installed"));
    return false;
  }
  return edit([gap, &entryId](DockItems &items) {
    return items.insert(gap, DockItem::application(entryId));
  });
}

QString QuickLaunchController::applicationForDesktopFile(const QString &path) const
{
  // AGENT-GUARD: a dropped desktop entry is used only as a NAME for an
  // installed application; its contents are never read or executed. The
  // candidates are the id the launcher scanner would give the file under an
  // applications/ root ("kde/foo.desktop" -> "kde-foo") and its base name.
  if (m_launcher == nullptr || !path.endsWith(QLatin1StringView(".desktop")))
    return {};
  QStringList candidates;
  const qsizetype root = path.lastIndexOf(QLatin1StringView("/applications/"));
  if (root >= 0) {
    QString relative = path.mid(root + qsizetype(sizeof("/applications/") - 1));
    relative.chop(qsizetype(sizeof(".desktop") - 1));
    candidates.append(relative.replace(QLatin1Char('/'), QLatin1Char('-')));
  }
  QString base = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
  base.chop(qsizetype(sizeof(".desktop") - 1));
  candidates.append(base);
  for (const QString &candidate : candidates) {
    if (!m_launcher->applicationPresentation(candidate).isEmpty())
      return candidate;
  }
  return {};
}

bool QuickLaunchController::insertUrls(int gap, const QVariantList &urls)
{
  QVector<DockItem> items;
  const QString trash = m_paths != nullptr ? QDir::cleanPath(m_paths->trashFilesDirectory())
                                           : QString{};
  for (const QVariant &value : urls) {
    const QUrl url = value.toUrl();
    // Remote and non-file URLs are not dock items; nothing is fetched.
    if (!url.isLocalFile() || m_paths == nullptr)
      continue;
    const QString path = QDir::cleanPath(url.toLocalFile());
    if (!DockItems::isValidPath(path))
      continue;
    if (!trash.isEmpty() && path == trash) {
      items.append(DockItem::trash());
      continue;
    }
    switch (m_paths->classify(path)) {
    case DockPathPort::PathKind::Directory:
      items.append(DockItem::folder(path));
      break;
    case DockPathPort::PathKind::File: {
      const QString application = applicationForDesktopFile(path);
      items.append(application.isEmpty() ? DockItem::file(path)
                                         : DockItem::application(application));
      break;
    }
    case DockPathPort::PathKind::Missing:
      break;
    }
  }
  if (items.isEmpty()) {
    publishFeedback(QStringLiteral("Only local files, folders, and applications can be kept in the Dock"));
    return false;
  }
  return edit([gap, &items](DockItems &dock) { return dock.insertAll(gap, items); });
}

bool QuickLaunchController::addPaths(const QStringList &absolutePaths)
{
  QVariantList urls;
  for (const QString &path : absolutePaths)
    urls.append(QUrl::fromLocalFile(path));
  return insertUrls(itemCount(), urls);
}

bool QuickLaunchController::combineItems(int targetIndex, int sourceIndex,
                                         const QString &groupName)
{
  const QString name = boundedGroupName(groupName);
  return edit([targetIndex, sourceIndex, &name](DockItems &items) {
    return items.combine(targetIndex, sourceIndex, name);
  });
}

bool QuickLaunchController::combineWithApplication(int targetIndex, const QString &entryId,
                                                   const QString &groupName)
{
  if (m_launcher == nullptr || m_launcher->applicationPresentation(entryId).isEmpty()) {
    publishFeedback(QStringLiteral("That application is not installed"));
    return false;
  }
  const DockItems *items = dock();
  const qsizetype existing = items != nullptr ? items->indexOfApplication(entryId) : -1;
  // Dragging a pinned top-level application in from elsewhere (the
  // launcher) onto a tile is the same gesture as dragging its own tile.
  if (existing >= 0 && existing != targetIndex
      && items->items().at(existing).kind == DockItemKind::Application) {
    return combineItems(targetIndex, static_cast<int>(existing), groupName);
  }
  const QString name = boundedGroupName(groupName);
  return edit([targetIndex, &entryId, &name](DockItems &dock) {
    return dock.combineWith(targetIndex, entryId, name);
  });
}

bool QuickLaunchController::newGroup(int index, const QString &groupName)
{
  const QString name = boundedGroupName(groupName);
  return edit([index, &name](DockItems &items) { return items.makeGroup(index, name); });
}

bool QuickLaunchController::renameGroup(int index, const QString &groupName)
{
  const QString name = boundedGroupName(groupName);
  return edit([index, &name](DockItems &items) { return items.renameGroup(index, name); });
}

bool QuickLaunchController::ungroup(int index)
{
  return edit([index](DockItems &items) { return items.ungroup(index); });
}

bool QuickLaunchController::moveOutOfGroup(int groupIndex, const QString &entryId, int gap)
{
  return edit([groupIndex, &entryId, gap](DockItems &items) {
    return items.moveOutOfGroup(groupIndex, entryId, gap);
  });
}

bool QuickLaunchController::showTrash()
{
  return edit([](DockItems &items) { return items.insert(items.size(), DockItem::trash()); });
}

bool QuickLaunchController::activateItem(int index)
{
  const DockItems *items = dock();
  if (items == nullptr || index < 0 || index >= items->size()) {
    publishFeedback(QStringLiteral("That item is no longer in the Dock"));
    return false;
  }
  const DockItem item = items->items().at(index);
  switch (item.kind) {
  case DockItemKind::Application:
    return activate(item.applicationId);
  case DockItemKind::Folder:
  case DockItemKind::Trash:
    return openInFileManager(index);
  case DockItemKind::File: {
    if (m_paths == nullptr) {
      publishFeedback(QStringLiteral("Opening files is unavailable"));
      return false;
    }
    const FolderOpener::Result opened = m_paths->openFile(item.path);
    if (!opened.ok) {
      publishFeedback(QStringLiteral("Could not open %1: %2")
                          .arg(QDir(item.path).dirName(), opened.diagnostic));
      return false;
    }
    clearFeedback();
    return true;
  }
  case DockItemKind::Group:
    break;
  }
  return false;
}

bool QuickLaunchController::openInFileManager(int index)
{
  const DockItems *items = dock();
  if (items == nullptr || index < 0 || index >= items->size() || m_paths == nullptr) {
    publishFeedback(QStringLiteral("Opening folders is unavailable"));
    return false;
  }
  const DockItem item = items->items().at(index);
  if (item.kind != DockItemKind::Folder && item.kind != DockItemKind::Trash) {
    publishFeedback(QStringLiteral("That item is not a folder"));
    return false;
  }
  const QString path = item.kind == DockItemKind::Trash ? m_paths->trashFilesDirectory()
                                                        : item.path;
  if (item.kind == DockItemKind::Trash
      && m_paths->classify(path) != DockPathPort::PathKind::Directory) {
    // The Trash folder appears on first use; before that it is simply empty.
    publishFeedback(QStringLiteral("The Trash is empty"));
    return false;
  }
  const FolderOpener::Result opened = m_paths->openFolder(path);
  if (!opened.ok) {
    publishFeedback(QStringLiteral("Could not open the folder: %1").arg(opened.diagnostic));
    return false;
  }
  clearFeedback();
  return true;
}

QVariantList QuickLaunchController::folderEntries(int index)
{
  const DockItems *items = dock();
  if (items == nullptr || index < 0 || index >= items->size() || m_paths == nullptr
      || items->items().at(index).kind != DockItemKind::Folder) {
    return {};
  }
  QString diagnostic;
  const QVariantList entries =
      m_paths->listFolder(items->items().at(index).path, kStackEntries, &diagnostic);
  if (!diagnostic.isEmpty())
    publishFeedback(diagnostic);
  return entries;
}

bool QuickLaunchController::openFolderEntry(const QVariantMap &entry)
{
  if (m_paths == nullptr) {
    publishFeedback(QStringLiteral("Opening files is unavailable"));
    return false;
  }
  const FolderOpener::Result opened = m_paths->openListedEntry(entry);
  if (!opened.ok) {
    publishFeedback(opened.diagnostic.isEmpty() ? QStringLiteral("Could not open that item")
                                                : opened.diagnostic);
    return false;
  }
  clearFeedback();
  return true;
}

bool QuickLaunchController::emptyTrash()
{
  if (m_paths == nullptr) {
    publishFeedback(QStringLiteral("Emptying the Trash is unavailable"));
    return false;
  }
  const QPointer<QuickLaunchController> self(this);
  QString diagnostic;
  const bool started = m_paths->emptyTrash(
      [self](bool ok, const QString &message) {
        if (self.isNull())
          return;
        if (ok)
          self->clearFeedback();
        else
          self->publishFeedback(message.isEmpty()
                                    ? QStringLiteral("The Trash could not be emptied")
                                    : message);
      },
      &diagnostic);
  if (!started) {
    publishFeedback(diagnostic.isEmpty() ? QStringLiteral("The Trash could not be emptied")
                                         : diagnostic);
    return false;
  }
  clearFeedback();
  return true;
}

} // namespace QindaQt::Shell::DesktopControls
