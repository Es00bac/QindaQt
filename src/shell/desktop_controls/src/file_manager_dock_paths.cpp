// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/file_manager_dock_paths.h"

#include "public/desktop_file_boundary.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell::DesktopControls {
namespace {

using Apps::FileManager::DirectoryEntry;
using Apps::FileManager::MutationController;
using Apps::FileManager::Desktop::FileBoundary;

// Extension-only matching: the stack never reads a child's contents just to
// pick its icon.
QString iconNameFor(const DirectoryEntry &entry)
{
  if (entry.isDirectory)
    return QStringLiteral("folder");
  static const QMimeDatabase database;
  const QMimeType type = database.mimeTypeForFile(entry.name, QMimeDatabase::MatchExtension);
  return type.isValid() ? type.iconName() : QStringLiteral("text-x-generic");
}

bool parseIdentity(const QVariant &value, quint64 *number)
{
  bool ok = false;
  *number = value.toString().toULongLong(&ok);
  return ok;
}

} // namespace

FileManagerDockPaths::FileManagerDockPaths(FolderOpener &opener)
    : m_opener(opener)
{
}

FileManagerDockPaths::~FileManagerDockPaths() = default;

DockPathPort::PathKind FileManagerDockPaths::classify(const QString &absolutePath) const
{
  const QFileInfo info(absolutePath);
  if (!info.isAbsolute() || !info.exists())
    return PathKind::Missing;
  if (info.isDir())
    return PathKind::Directory;
  return info.isFile() ? PathKind::File : PathKind::Missing;
}

FolderOpener::Result FileManagerDockPaths::openFolder(const QString &absolutePath)
{
  return m_opener.open(absolutePath);
}

FolderOpener::Result FileManagerDockPaths::openFile(const QString &absolutePath)
{
  const auto launched = FileBoundary::launchLocalFile(absolutePath);
  return {launched.ok(), launched.diagnostic};
}

QVariantList FileManagerDockPaths::listFolder(const QString &absolutePath, int limit,
                                              QString *diagnostic) const
{
  const auto listing = FileBoundary::listLocalFolder(absolutePath);
  if (!listing.ok()) {
    if (diagnostic != nullptr)
      *diagnostic = listing.diagnostic.isEmpty()
          ? QStringLiteral("The folder could not be read")
          : listing.diagnostic.left(512);
    return {};
  }
  QVector<DirectoryEntry> entries;
  for (const DirectoryEntry &entry : listing.entries) {
    if (!entry.isHidden)
      entries.append(entry);
  }
  std::sort(entries.begin(), entries.end(), [](const DirectoryEntry &left,
                                               const DirectoryEntry &right) {
    if (left.isDirectory != right.isDirectory)
      return left.isDirectory;
    return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
  });
  QVariantList rows;
  for (const DirectoryEntry &entry : entries) {
    if (rows.size() >= limit)
      break;
    rows.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("path"), entry.absolutePath},
        {QStringLiteral("isDirectory"), entry.isDirectory},
        {QStringLiteral("iconName"), iconNameFor(entry)},
        {QStringLiteral("device"), QString::number(entry.device)},
        {QStringLiteral("inode"), QString::number(entry.inode)},
    });
  }
  if (diagnostic != nullptr)
    diagnostic->clear();
  return rows;
}

FolderOpener::Result FileManagerDockPaths::openListedEntry(const QVariantMap &entry)
{
  const QString path = entry.value(QStringLiteral("path")).toString();
  if (!entry.value(QStringLiteral("isDirectory")).toBool())
    return openFile(path);
  quint64 device = 0;
  quint64 inode = 0;
  if (!parseIdentity(entry.value(QStringLiteral("device")), &device)
      || !parseIdentity(entry.value(QStringLiteral("inode")), &inode)) {
    return {false, QStringLiteral("the folder is not one the stack listed")};
  }
  const auto opened = FileBoundary::openLocalFolder(path, {device, inode});
  return {opened.ok(), opened.diagnostic};
}

QString FileManagerDockPaths::trashFilesDirectory() const
{
  // AGENT-NOTE: the File Manager's Trash place (model/places_controller.cpp)
  // and its mutation backend use exactly this home Trash (ADR-0064).
  return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
      .filePath(QStringLiteral("Trash/files"));
}

bool FileManagerDockPaths::emptyTrash(std::function<void(bool, const QString &)> finished,
                                      QString *diagnostic)
{
  if (!m_mutation) {
    m_mutation = FileBoundary::createLocalMutationController();
    QObject::connect(m_mutation.get(), &MutationController::stateChanged, m_mutation.get(),
                     [this] { settleTrash(); });
  }
  if (m_trashFinished || m_mutation->busy()) {
    if (diagnostic != nullptr)
      *diagnostic = QStringLiteral("The Trash is already being emptied");
    return false;
  }
  if (!m_mutation->emptyTrash()) {
    if (diagnostic != nullptr)
      *diagnostic = m_mutation->failureMessage().isEmpty()
          ? QStringLiteral("The Trash could not be emptied")
          : m_mutation->failureMessage();
    return false;
  }
  m_trashFinished = std::move(finished);
  return true;
}

void FileManagerDockPaths::settleTrash()
{
  // The controller also signals progress while busy; only the settled state
  // (busy cleared) completes the request, exactly once.
  if (!m_trashFinished || m_mutation->busy())
    return;
  auto finished = std::move(m_trashFinished);
  m_trashFinished = nullptr;
  const bool ok = m_mutation->failureCode() == QLatin1StringView("none");
  finished(ok, ok ? QString{} : m_mutation->failureMessage());
}

} // namespace QindaQt::Shell::DesktopControls
