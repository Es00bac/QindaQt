// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/launcher_pinned_recent.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

namespace QindaQt::ShellLauncher {

bool PinnedApplications::contains(const QString &entryId) const
{
  return m_ids.contains(entryId);
}

bool PinnedApplications::isFull() const
{
  return m_ids.size() >= Bounds::maxPinnedEntries;
}

PinError PinnedApplications::pin(const QString &entryId)
{
  if (!Bounds::isValidEntryId(entryId))
    return PinError::UnknownId;
  if (contains(entryId))
    return PinError::AlreadyPinned;
  if (isFull())
    return PinError::LimitReached;
  m_ids.append(entryId);
  return PinError::None;
}

PinError PinnedApplications::unpin(const QString &entryId)
{
  if (!m_ids.removeOne(entryId))
    return PinError::NotPinned;
  return PinError::None;
}

PinError PinnedApplications::moveUp(const QString &entryId)
{
  const qsizetype index = m_ids.indexOf(entryId);
  if (index < 0)
    return PinError::NotPinned;
  if (index == 0)
    return PinError::None;
  m_ids.swapItemsAt(index, index - 1);
  return PinError::None;
}

PinError PinnedApplications::moveDown(const QString &entryId)
{
  const qsizetype index = m_ids.indexOf(entryId);
  if (index < 0)
    return PinError::NotPinned;
  if (index == m_ids.size() - 1)
    return PinError::None;
  m_ids.swapItemsAt(index, index + 1);
  return PinError::None;
}

RecentError RecentApplications::record(const QString &entryId)
{
  if (!Bounds::isValidEntryId(entryId))
    return RecentError::InvalidId;
  m_ids.removeOne(entryId);
  m_ids.prepend(entryId);
  while (m_ids.size() > Bounds::maxRecentEntries)
    m_ids.removeLast();
  return RecentError::None;
}

void RecentApplications::clear()
{
  m_ids.clear();
}

} // namespace QindaQt::ShellLauncher
