// SPDX-License-Identifier: GPL-3.0-or-later
#include "applications_place.h"

#include "applications_location.h"

#include <utility>

namespace QindaQt::Apps::FileManager {

ApplicationsDirectoryLister::ApplicationsDirectoryLister(DirectoryListerPtr inner,
                                                         Source applications)
    : m_inner(std::move(inner)), m_applications(std::move(applications)) {
  Q_ASSERT(m_inner);
  Q_ASSERT(m_applications);
}

ListingResult ApplicationsDirectoryLister::list(const QString &absolutePath) const {
  if (ApplicationsLocation::isLocation(absolutePath)) {
    return m_applications();
  }
  return m_inner->list(absolutePath);
}

ApplicationsFileLauncher::ApplicationsFileLauncher(FileLauncherPtr inner, Opener open)
    : m_inner(std::move(inner)), m_open(std::move(open)) {
  Q_ASSERT(m_inner);
  Q_ASSERT(m_open);
}

LaunchResult ApplicationsFileLauncher::launch(const QString &absolutePath) const {
  const QString prefix = ApplicationsLocation::location();
  // AGENT-GUARD: only a row path with a non-empty id reaches the opener; the
  // bare location (or anything merely resembling it) is never "launched".
  if (absolutePath.size() > prefix.size() && absolutePath.startsWith(prefix)) {
    const QString failure = m_open(absolutePath.mid(prefix.size()));
    if (failure.isEmpty()) {
      return {};
    }
    return {LaunchError::LaunchFailed, failure};
  }
  return m_inner->launch(absolutePath);
}

} // namespace QindaQt::Apps::FileManager
