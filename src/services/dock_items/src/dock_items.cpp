// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/dock_items/dock_items.h"

#include <QDir>

#include <utility>

namespace QindaQt::Services::DockItems {
namespace {

bool hasControlCharacter(const QString &text)
{
  for (const QChar character : text) {
    if (character.unicode() < 0x20 || character.unicode() == 0x7f)
      return true;
  }
  return false;
}

bool isApplication(const DockItem &item)
{
  return item.kind == DockItemKind::Application;
}

bool isGroup(const DockItem &item)
{
  return item.kind == DockItemKind::Group;
}

} // namespace

DockItem DockItem::application(const QString &id)
{
  DockItem item;
  item.kind = DockItemKind::Application;
  item.applicationId = id;
  return item;
}

DockItem DockItem::folder(const QString &absolutePath)
{
  DockItem item;
  item.kind = DockItemKind::Folder;
  item.path = absolutePath;
  return item;
}

DockItem DockItem::file(const QString &absolutePath)
{
  DockItem item;
  item.kind = DockItemKind::File;
  item.path = absolutePath;
  return item;
}

DockItem DockItem::group(const QString &groupName, const QStringList &members)
{
  DockItem item;
  item.kind = DockItemKind::Group;
  item.name = groupName;
  item.applications = members;
  return item;
}

DockItem DockItem::trash()
{
  DockItem item;
  item.kind = DockItemKind::Trash;
  return item;
}

bool DockItems::isValidPath(const QString &path)
{
  // AGENT-GUARD: only the string shape is checked. The dock never resolves,
  // stats, or opens a stored path here; opening happens later through the
  // File Manager boundary, which revalidates the target at that moment.
  return !path.isEmpty() && path.size() <= Bounds::maxPathLength
      && path.startsWith(QLatin1Char('/')) && QDir::cleanPath(path) == path
      && !hasControlCharacter(path);
}

bool DockItems::isValidGroupName(const QString &name)
{
  return !name.isEmpty() && name.size() <= Bounds::maxNameLength
      && name.trimmed() == name && !hasControlCharacter(name);
}

bool DockItems::isValidItem(const DockItem &item)
{
  switch (item.kind) {
  case DockItemKind::Application:
    return ShellLauncher::Bounds::isValidEntryId(item.applicationId)
        && item.path.isEmpty() && item.name.isEmpty() && item.applications.isEmpty();
  case DockItemKind::Folder:
  case DockItemKind::File:
    return isValidPath(item.path) && item.applicationId.isEmpty()
        && item.name.isEmpty() && item.applications.isEmpty();
  case DockItemKind::Group: {
    if (!isValidGroupName(item.name) || !item.applicationId.isEmpty()
        || !item.path.isEmpty() || item.applications.isEmpty()
        || item.applications.size() > Bounds::maxGroupApplications) {
      return false;
    }
    for (qsizetype index = 0; index < item.applications.size(); ++index) {
      const QString &member = item.applications.at(index);
      if (!ShellLauncher::Bounds::isValidEntryId(member)
          || item.applications.indexOf(member) != index) {
        return false;
      }
    }
    return true;
  }
  case DockItemKind::Trash:
    return item.applicationId.isEmpty() && item.path.isEmpty() && item.name.isEmpty()
        && item.applications.isEmpty();
  }
  return false;
}

std::optional<DockItems> DockItems::fromItems(const QVector<DockItem> &items)
{
  if (items.size() > Bounds::maxItems)
    return std::nullopt;
  DockItems result;
  for (const DockItem &item : items) {
    if (result.admissionError(item) != DockEditError::None)
      return std::nullopt;
    result.m_items.append(item);
  }
  return result;
}

QStringList DockItems::applicationIds() const
{
  QStringList ids;
  for (const DockItem &item : m_items) {
    if (isApplication(item))
      ids.append(item.applicationId);
    else if (isGroup(item))
      ids.append(item.applications);
  }
  return ids;
}

qsizetype DockItems::applicationCount() const
{
  qsizetype count = 0;
  for (const DockItem &item : m_items) {
    if (isApplication(item))
      ++count;
    else if (isGroup(item))
      count += item.applications.size();
  }
  return count;
}

qsizetype DockItems::indexOfApplication(const QString &applicationId) const
{
  for (qsizetype index = 0; index < m_items.size(); ++index) {
    const DockItem &item = m_items.at(index);
    if ((isApplication(item) && item.applicationId == applicationId)
        || (isGroup(item) && item.applications.contains(applicationId))) {
      return index;
    }
  }
  return -1;
}

qsizetype DockItems::indexOfPath(const QString &path) const
{
  for (qsizetype index = 0; index < m_items.size(); ++index) {
    const DockItem &item = m_items.at(index);
    if ((item.kind == DockItemKind::Folder || item.kind == DockItemKind::File)
        && item.path == path) {
      return index;
    }
  }
  return -1;
}

qsizetype DockItems::indexOfTrash() const
{
  for (qsizetype index = 0; index < m_items.size(); ++index) {
    if (m_items.at(index).kind == DockItemKind::Trash)
      return index;
  }
  return -1;
}

DockEditError DockItems::admissionError(const DockItem &item) const
{
  if (!isValidItem(item))
    return DockEditError::InvalidItem;
  switch (item.kind) {
  case DockItemKind::Application:
    if (indexOfApplication(item.applicationId) >= 0)
      return DockEditError::AlreadyInDock;
    break;
  case DockItemKind::Group:
    for (const QString &member : item.applications) {
      if (indexOfApplication(member) >= 0)
        return DockEditError::AlreadyInDock;
    }
    break;
  case DockItemKind::Folder:
  case DockItemKind::File:
    if (indexOfPath(item.path) >= 0)
      return DockEditError::AlreadyInDock;
    break;
  case DockItemKind::Trash:
    if (indexOfTrash() >= 0)
      return DockEditError::AlreadyInDock;
    break;
  }
  if (m_items.size() >= Bounds::maxItems)
    return DockEditError::DockFull;
  const qsizetype added = isApplication(item) ? 1
      : isGroup(item) ? item.applications.size() : 0;
  if (applicationCount() + added > Bounds::maxApplications)
    return DockEditError::DockFull;
  return DockEditError::None;
}

DockEditError DockItems::insert(qsizetype gap, const DockItem &item)
{
  if (gap < 0 || gap > m_items.size())
    return DockEditError::OutOfRange;
  const DockEditError error = admissionError(item);
  if (error != DockEditError::None)
    return error;
  m_items.insert(gap, item);
  return DockEditError::None;
}

DockEditError DockItems::insertAll(qsizetype gap, const QVector<DockItem> &items)
{
  if (gap < 0 || gap > m_items.size())
    return DockEditError::OutOfRange;
  DockItems next = *this;
  qsizetype at = gap;
  for (const DockItem &item : items) {
    const DockEditError error = next.insert(at, item);
    if (error == DockEditError::AlreadyInDock)
      continue;
    if (error != DockEditError::None)
      return error;
    ++at;
  }
  if (at == gap)
    return DockEditError::AlreadyInDock;
  *this = std::move(next);
  return DockEditError::None;
}

DockEditError DockItems::moveToGap(qsizetype from, qsizetype gap)
{
  if (from < 0 || from >= m_items.size() || gap < 0 || gap > m_items.size())
    return DockEditError::OutOfRange;
  // Dropping an item into its own slot, or the gap right behind it, keeps
  // the order; the caller still gets success because nothing was refused.
  if (gap == from || gap == from + 1)
    return DockEditError::None;
  const DockItem moved = m_items.takeAt(from);
  m_items.insert(gap > from ? gap - 1 : gap, moved);
  return DockEditError::None;
}

DockEditError DockItems::removeAt(qsizetype index)
{
  if (index < 0 || index >= m_items.size())
    return DockEditError::OutOfRange;
  m_items.removeAt(index);
  return DockEditError::None;
}

DockEditError DockItems::removeApplication(const QString &applicationId)
{
  const qsizetype index = indexOfApplication(applicationId);
  if (index < 0)
    return DockEditError::NotInDock;
  DockItem &item = m_items[index];
  if (isApplication(item)) {
    m_items.removeAt(index);
    return DockEditError::None;
  }
  item.applications.removeAll(applicationId);
  if (item.applications.isEmpty())
    m_items.removeAt(index);
  return DockEditError::None;
}

DockEditError DockItems::combine(qsizetype targetIndex, qsizetype sourceIndex,
                                 const QString &groupName)
{
  if (targetIndex < 0 || targetIndex >= m_items.size() || sourceIndex < 0
      || sourceIndex >= m_items.size() || targetIndex == sourceIndex) {
    return DockEditError::OutOfRange;
  }
  const DockItem source = m_items.at(sourceIndex);
  const DockItem &target = m_items.at(targetIndex);
  if (!isApplication(source) || (!isApplication(target) && !isGroup(target)))
    return DockEditError::WrongKind;
  DockItem combined;
  if (isGroup(target)) {
    if (target.applications.size() >= Bounds::maxGroupApplications)
      return DockEditError::GroupFull;
    combined = target;
    combined.applications.append(source.applicationId);
  } else {
    if (!isValidGroupName(groupName))
      return DockEditError::InvalidItem;
    combined = DockItem::group(groupName, {target.applicationId, source.applicationId});
  }
  // The application count is unchanged: the source leaves the top level.
  m_items[targetIndex] = combined;
  m_items.removeAt(sourceIndex);
  return DockEditError::None;
}

DockEditError DockItems::combineWith(qsizetype targetIndex, const QString &applicationId,
                                     const QString &groupName)
{
  if (targetIndex < 0 || targetIndex >= m_items.size())
    return DockEditError::OutOfRange;
  if (!ShellLauncher::Bounds::isValidEntryId(applicationId))
    return DockEditError::InvalidItem;
  if (indexOfApplication(applicationId) >= 0)
    return DockEditError::AlreadyInDock;
  if (applicationCount() >= Bounds::maxApplications)
    return DockEditError::DockFull;
  DockItem &target = m_items[targetIndex];
  if (isGroup(target)) {
    if (target.applications.size() >= Bounds::maxGroupApplications)
      return DockEditError::GroupFull;
    target.applications.append(applicationId);
    return DockEditError::None;
  }
  if (!isApplication(target))
    return DockEditError::WrongKind;
  if (!isValidGroupName(groupName))
    return DockEditError::InvalidItem;
  target = DockItem::group(groupName, {target.applicationId, applicationId});
  return DockEditError::None;
}

DockEditError DockItems::makeGroup(qsizetype index, const QString &groupName)
{
  if (index < 0 || index >= m_items.size())
    return DockEditError::OutOfRange;
  if (!isApplication(m_items.at(index)))
    return DockEditError::WrongKind;
  if (!isValidGroupName(groupName))
    return DockEditError::InvalidItem;
  m_items[index] = DockItem::group(groupName, {m_items.at(index).applicationId});
  return DockEditError::None;
}

DockEditError DockItems::renameGroup(qsizetype index, const QString &groupName)
{
  if (index < 0 || index >= m_items.size())
    return DockEditError::OutOfRange;
  if (!isGroup(m_items.at(index)))
    return DockEditError::WrongKind;
  if (!isValidGroupName(groupName))
    return DockEditError::InvalidItem;
  m_items[index].name = groupName;
  return DockEditError::None;
}

DockEditError DockItems::ungroup(qsizetype index)
{
  if (index < 0 || index >= m_items.size())
    return DockEditError::OutOfRange;
  if (!isGroup(m_items.at(index)))
    return DockEditError::WrongKind;
  const QStringList members = m_items.at(index).applications;
  if (m_items.size() - 1 + members.size() > Bounds::maxItems)
    return DockEditError::DockFull;
  m_items.removeAt(index);
  for (qsizetype offset = 0; offset < members.size(); ++offset)
    m_items.insert(index + offset, DockItem::application(members.at(offset)));
  return DockEditError::None;
}

DockEditError DockItems::moveOutOfGroup(qsizetype groupIndex, const QString &applicationId,
                                        qsizetype gap)
{
  if (groupIndex < 0 || groupIndex >= m_items.size() || gap < 0 || gap > m_items.size())
    return DockEditError::OutOfRange;
  if (!isGroup(m_items.at(groupIndex)))
    return DockEditError::WrongKind;
  if (!m_items.at(groupIndex).applications.contains(applicationId))
    return DockEditError::NotInDock;
  const bool groupEmpties = m_items.at(groupIndex).applications.size() == 1;
  if (!groupEmpties && m_items.size() >= Bounds::maxItems)
    return DockEditError::DockFull;
  qsizetype insertAt = gap;
  if (groupEmpties) {
    m_items.removeAt(groupIndex);
    if (insertAt > groupIndex)
      --insertAt;
  } else {
    m_items[groupIndex].applications.removeAll(applicationId);
  }
  m_items.insert(insertAt, DockItem::application(applicationId));
  return DockEditError::None;
}

} // namespace QindaQt::Services::DockItems
