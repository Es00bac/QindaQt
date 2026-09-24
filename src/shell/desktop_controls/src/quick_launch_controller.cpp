// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include "launcher_applet_controller.h"

#include "qindaqt/shell/desktop_controls/dock_path_port.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <qindaqt/services/dock_items/dock_items.h>

#include "file_manager_menu_catalog.h"

#include <QFileInfo>
#include <QMimeDatabase>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell::DesktopControls {
namespace {

using Services::DockItems::DockItem;
using Services::DockItems::DockItemKind;
using Services::DockItems::DockItems;

constexpr qsizetype kPreviewIcons = 4;

QString kindName(DockItemKind kind)
{
  switch (kind) {
  case DockItemKind::Application:
    return QStringLiteral("application");
  case DockItemKind::Folder:
    return QStringLiteral("folder");
  case DockItemKind::File:
    return QStringLiteral("file");
  case DockItemKind::Group:
    return QStringLiteral("group");
  case DockItemKind::Trash:
    return QStringLiteral("trash");
  }
  return QStringLiteral("application");
}

// Pure string work: a pinned path is shown by its last component and never
// stat'ed or read to build a row.
QString pathLabel(const QString &path)
{
  const QString name = QFileInfo(path).fileName();
  return name.isEmpty() ? path : name;
}

QString fileIconName(const QString &path)
{
  static const QMimeDatabase database;
  const QMimeType type = database.mimeTypeForFile(pathLabel(path), QMimeDatabase::MatchExtension);
  return type.isValid() ? type.iconName() : QStringLiteral("text-x-generic");
}

QVariantMap pathRow(const DockItem &item, int index)
{
  const bool folder = item.kind == DockItemKind::Folder;
  const QString label = item.kind == DockItemKind::Trash ? QStringLiteral("Trash")
                                                         : pathLabel(item.path);
  return QVariantMap{
      {QStringLiteral("index"), index},
      {QStringLiteral("kind"), kindName(item.kind)},
      {QStringLiteral("entryId"), QString{}},
      {QStringLiteral("path"), item.path},
      {QStringLiteral("displayText"), label},
      {QStringLiteral("iconName"),
       item.kind == DockItemKind::Trash ? QStringLiteral("user-trash")
       : folder                         ? QStringLiteral("folder")
                                        : fileIconName(item.path)},
      {QStringLiteral("accessibleName"), label},
      {QStringLiteral("accessibleDescription"), item.path},
      {QStringLiteral("categoryIdentity"), QString{}},
      {QStringLiteral("running"), false},
      {QStringLiteral("members"), QVariantList{}},
      {QStringLiteral("previewIcons"), QStringList{}},
  };
}

} // namespace

QuickLaunchController::QuickLaunchController(
    Launcher::LauncherAppletController *launcher, bool applicationsLaunchGranted,
    QObject *parent)
    : QObject(parent), m_launcher(launcher), m_launchGranted(applicationsLaunchGranted)
{
  if (m_launcher != nullptr) {
    connect(m_launcher, &Launcher::LauncherAppletController::stateChanged, this,
            &QuickLaunchController::rebuild);
  }
  rebuild();
}

void QuickLaunchController::setPathPort(DockPathPort *paths)
{
  m_paths = paths;
}

void QuickLaunchController::setWindowSource(
    ShellTaskListApplet::TaskListAppletController *taskList, DesktopEntryResolver resolver,
    WindowGrants grants)
{
  if (m_taskList != nullptr)
    disconnect(m_taskList, nullptr, this, nullptr);
  m_taskList = taskList;
  m_resolver = std::move(resolver);
  m_windowGrants = grants;
  if (m_taskList != nullptr && m_windowGrants.windowsRead) {
    connect(m_taskList, &ShellTaskListApplet::TaskListAppletController::stateReprojected,
            this, &QuickLaunchController::rebuild);
  }
  rebuild();
}

bool QuickLaunchController::available() const noexcept
{
  return m_launcher != nullptr && m_launchGranted;
}

QString QuickLaunchController::phaseText() const
{
  if (m_launcher == nullptr) {
    return QStringLiteral("unavailable");
  }
  const QString phase = m_launcher->phase();
  if (phase == QLatin1StringView("ready") && m_rows.isEmpty()) {
    return QStringLiteral("empty");
  }
  return phase;
}

int QuickLaunchController::itemCount() const
{
  const DockItems *items = dock();
  return items != nullptr ? static_cast<int>(items->size()) : 0;
}

bool QuickLaunchController::editable() const
{
  return m_launcher != nullptr && m_launcher->dockEditable();
}

const DockItems *QuickLaunchController::dock() const
{
  return m_launcher != nullptr ? &m_launcher->dock() : nullptr;
}

QString QuickLaunchController::desktopEntryForWindow(const QString &windowApplicationId) const
{
  const QString resolved = m_resolver ? m_resolver(windowApplicationId) : QString{};
  // Without a resolver answer a window whose app id IS a desktop-entry id
  // still matches exactly; nothing fuzzier is guessed here.
  return resolved.isEmpty() ? windowApplicationId : resolved;
}

QVariantMap QuickLaunchController::applicationRow(const QString &entryId, int index,
                                                  const QVariantMap &presentation) const
{
  const bool running = m_running.contains(entryId);
  const QString displayText = presentation.value(QStringLiteral("displayText")).toString();
  return QVariantMap{
      {QStringLiteral("index"), index},
      {QStringLiteral("kind"), QStringLiteral("application")},
      {QStringLiteral("entryId"), entryId},
      {QStringLiteral("path"), QString{}},
      {QStringLiteral("displayText"), displayText},
      {QStringLiteral("iconName"), presentation.value(QStringLiteral("iconName"))},
      {QStringLiteral("accessibleName"), displayText},
      {QStringLiteral("accessibleDescription"),
       presentation.value(QStringLiteral("accessibleDescription"))},
      {QStringLiteral("categoryIdentity"), presentation.value(QStringLiteral("categoryIdentity"))},
      {QStringLiteral("running"), running},
      {QStringLiteral("members"), QVariantList{}},
      {QStringLiteral("previewIcons"), QStringList{}},
  };
}

void QuickLaunchController::rebuild()
{
  // Running windows by desktop-entry id. Container rows are QindaQt window
  // groups the user built; they stay task-list tiles and never count here.
  m_running.clear();
  if (m_taskList != nullptr && m_windowGrants.windowsRead) {
    for (const auto &row : m_taskList->projection().rows) {
      if (row.kind != ShellTaskList::TaskEntryKind::Window)
        continue;
      m_running[desktopEntryForWindow(row.applicationId)].append(
          RunningWindow{row.taskId, row.generationRevision, row.active});
    }
  }

  QVariantList rows;
  QVariantList applicationRows;
  QStringList claimed;
  bool trash = false;
  const DockItems *items = dock();
  // AGENT-NOTE: without a published catalog no application can be named,
  // so application tiles wait (the launcher's Loading truth) while folder,
  // file, and Trash tiles, which need no catalog, show at once.
  const bool catalog = m_launcher != nullptr && m_launcher->hasCatalog();
  // ADR-0268: the permanent File Manager tile, whether or not the dock value
  // also stores the application.
  const QString fileManagerEntry = QindaQt::Apps::FileManager::MenuCatalog::desktopEntryId();
  QVariantMap fileManagerRow;
  if (catalog) {
    const QVariantMap presentation = m_launcher->applicationPresentation(fileManagerEntry);
    if (!presentation.isEmpty()) {
      fileManagerRow = applicationRow(fileManagerEntry, -1, presentation);
      fileManagerRow.insert(QStringLiteral("fixed"), true);
    }
  }
  const auto appendApplication = [&](const QString &entryId, int index) {
    const QVariantMap presentation = m_launcher->applicationPresentation(entryId);
    if (presentation.isEmpty())
      return QVariantMap{};
    QVariantMap row = applicationRow(entryId, index, presentation);
    QVariantMap historical{
        {QStringLiteral("entryId"), entryId},
        {QStringLiteral("displayText"), row.value(QStringLiteral("displayText"))},
        {QStringLiteral("iconName"), row.value(QStringLiteral("iconName"))},
        {QStringLiteral("accessibleName"), row.value(QStringLiteral("accessibleName"))},
        {QStringLiteral("accessibleDescription"),
         row.value(QStringLiteral("accessibleDescription"))},
        {QStringLiteral("index"), static_cast<int>(applicationRows.size())},
    };
    applicationRows.append(historical);
    return row;
  };
  for (qsizetype position = 0; items != nullptr && position < items->size(); ++position) {
    const DockItem &item = items->items().at(position);
    const int index = static_cast<int>(position);
    switch (item.kind) {
    case DockItemKind::Application: {
      if (!catalog)
        break;
      const QVariantMap row = appendApplication(item.applicationId, index);
      if (row.isEmpty())
        break;
      rows.append(row);
      for (const RunningWindow &window : m_running.value(item.applicationId))
        claimed.append(window.taskId);
      break;
    }
    case DockItemKind::Group: {
      if (!catalog)
        break;
      QVariantList members;
      QStringList previewIcons;
      bool running = false;
      for (const QString &member : item.applications) {
        const QVariantMap row = appendApplication(member, index);
        if (row.isEmpty())
          continue;
        members.append(row);
        running = running || row.value(QStringLiteral("running")).toBool();
        if (previewIcons.size() < kPreviewIcons)
          previewIcons.append(row.value(QStringLiteral("iconName")).toString());
      }
      // A group whose members are all uninstalled keeps its stored place
      // but shows nothing, like a stale pinned application.
      if (members.isEmpty())
        break;
      rows.append(QVariantMap{
          {QStringLiteral("index"), index},
          {QStringLiteral("kind"), QStringLiteral("group")},
          {QStringLiteral("entryId"), QString{}},
          {QStringLiteral("path"), QString{}},
          {QStringLiteral("displayText"), item.name},
          {QStringLiteral("iconName"), QStringLiteral("folder")},
          {QStringLiteral("accessibleName"), item.name},
          {QStringLiteral("accessibleDescription"), QString{}},
          {QStringLiteral("categoryIdentity"), QString{}},
          {QStringLiteral("running"), running},
          {QStringLiteral("members"), members},
          {QStringLiteral("previewIcons"), previewIcons},
      });
      break;
    }
    case DockItemKind::Trash:
      trash = true;
      rows.append(pathRow(item, index));
      break;
    case DockItemKind::Folder:
    case DockItemKind::File:
      rows.append(pathRow(item, index));
      break;
    }
  }
  // While a permanent File Manager tile is shown it stands for the File
  // Manager's windows, as a pinned tile does for its application's.
  if (m_fileManagerEnds > 0 && !fileManagerRow.isEmpty()) {
    for (const RunningWindow &window : m_running.value(fileManagerEntry)) {
      if (!claimed.contains(window.taskId))
        claimed.append(window.taskId);
    }
  }
  // AGENT-GUARD: QML repeats rows as a plain list, so every published change
  // rebuilds each tile (focus, hover, anchored popups). The task list
  // reprojects on every focus or title change; publish only real changes.
  // Rows therefore carry running, never which window is focused.
  // Editability, phase, and the stored item count come from the launcher
  // and are published through the same signal, so they count as changes.
  const bool editableNow = editable();
  const int itemCountNow = itemCount();
  const QString launcherPhase = m_launcher != nullptr ? m_launcher->phase() : QString{};
  if (rows == m_rows && applicationRows == m_applicationRows && claimed == m_claimedTaskIds
      && fileManagerRow == m_fileManagerRow
      && trash == m_trashInDock && editableNow == m_publishedEditable
      && itemCountNow == m_publishedItemCount && launcherPhase == m_publishedPhase) {
    return;
  }
  m_publishedEditable = editableNow;
  m_publishedItemCount = itemCountNow;
  m_publishedPhase = launcherPhase;
  m_rows = rows;
  m_applicationRows = applicationRows;
  m_claimedTaskIds = claimed;
  m_fileManagerRow = fileManagerRow;
  m_trashInDock = trash;
  Q_EMIT stateChanged();
}

void QuickLaunchController::holdFileManagerEnd(bool shown)
{
  m_fileManagerEnds = std::max(0, m_fileManagerEnds + (shown ? 1 : -1));
  rebuild();
}

QVariantMap QuickLaunchController::trashRow() const
{
  QVariantMap row = pathRow(DockItem::trash(), -1);
  row.insert(QStringLiteral("fixed"), true);
  return row;
}

bool QuickLaunchController::knownEntry(const QString &entryId) const
{
  // The permanent File Manager tile opens like a pinned one (ADR-0268).
  if (!m_fileManagerRow.isEmpty()
      && m_fileManagerRow.value(QStringLiteral("entryId")).toString() == entryId) {
    return true;
  }
  for (const QVariant &value : m_applicationRows) {
    if (value.toMap().value(QStringLiteral("entryId")).toString() == entryId) {
      return true;
    }
  }
  return false;
}

bool QuickLaunchController::launch(const QString &entryId)
{
  if (!m_launchGranted) {
    publishFeedback(QStringLiteral("Application launching is not granted"));
    return false;
  }
  if (!m_launcher->activate(entryId)) {
    const QString detail = m_launcher->feedback();
    publishFeedback(detail.isEmpty() ? QStringLiteral("Could not start the application")
                                     : detail);
    return false;
  }
  clearFeedback();
  return true;
}

bool QuickLaunchController::activate(const QString &entryId)
{
  if (m_launcher == nullptr) {
    publishFeedback(m_launchGranted ? QStringLiteral("Application launching is unavailable")
                                    : QStringLiteral("Application launching is not granted"));
    return false;
  }
  if (!knownEntry(entryId)) {
    publishFeedback(QStringLiteral("That application is no longer pinned"));
    return false;
  }
  // One icon per application: a running pinned application comes forward
  // instead of starting a second time. Several windows cycle, starting after
  // the active one.
  const QList<RunningWindow> windows = m_running.value(entryId);
  if (!windows.isEmpty() && m_taskList != nullptr && m_windowGrants.windowsActivate) {
    qsizetype next = 0;
    for (qsizetype position = 0; position < windows.size(); ++position) {
      if (windows.at(position).active) {
        next = (position + 1) % windows.size();
        break;
      }
    }
    const RunningWindow &window = windows.at(next);
    if (m_taskList->activateTask(window.taskId, window.revision)) {
      clearFeedback();
      return true;
    }
    publishFeedback(QStringLiteral("Could not switch to the application"));
    return false;
  }
  return launch(entryId);
}

bool QuickLaunchController::openNewWindow(const QString &entryId)
{
  if (m_launcher == nullptr || !knownEntry(entryId)) {
    publishFeedback(QStringLiteral("That application is no longer pinned"));
    return false;
  }
  return launch(entryId);
}

bool QuickLaunchController::canKeepInDock(const QString &windowApplicationId) const
{
  if (m_launcher == nullptr || windowApplicationId.isEmpty())
    return false;
  const QString entryId = desktopEntryForWindow(windowApplicationId);
  const DockItems *items = dock();
  return !m_launcher->applicationPresentation(entryId).isEmpty() && items != nullptr
      && items->indexOfApplication(entryId) < 0;
}

bool QuickLaunchController::keepInDock(const QString &windowApplicationId)
{
  if (!canKeepInDock(windowApplicationId)) {
    publishFeedback(QStringLiteral("That window has no installed application to keep in the Dock"));
    return false;
  }
  const QString entryId = desktopEntryForWindow(windowApplicationId);
  return edit([&entryId](DockItems &items) {
    return items.insert(items.size(), DockItem::application(entryId));
  });
}

QString QuickLaunchController::applicationCategory(const QString &entryId) const
{
  if (m_launcher == nullptr)
    return {};
  return m_launcher->applicationPresentation(entryId)
      .value(QStringLiteral("categoryIdentity"))
      .toString();
}

bool QuickLaunchController::unpin(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId) && m_launcher->unpin(entryId);
}

bool QuickLaunchController::moveUp(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId)
      && m_launcher->movePinnedUp(entryId);
}

bool QuickLaunchController::moveDown(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId)
      && m_launcher->movePinnedDown(entryId);
}

void QuickLaunchController::clearFeedback()
{
  publishFeedback({});
}

void QuickLaunchController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
