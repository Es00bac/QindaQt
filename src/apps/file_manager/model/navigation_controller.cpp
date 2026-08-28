// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"

#include <QDir>
#include <QVariantMap>

namespace QindaQt::Apps::FileManager {

namespace {

[[nodiscard]] NavigationStatus statusFor(const ListingResult &result) {
  if (result.ok()) {
    return result.entries.isEmpty() ? NavigationStatus::Empty
                                    : NavigationStatus::Ready;
  }
  switch (result.error) {
  case ListingError::NotFound:
    return NavigationStatus::Missing;
  case ListingError::PermissionDenied:
    return NavigationStatus::PermissionDenied;
  case ListingError::NotADirectory:
    return NavigationStatus::NotADirectory;
  case ListingError::Unknown:
  case ListingError::None:
    break;
  }
  return NavigationStatus::Error;
}

} // namespace

NavigationController::NavigationController(DirectoryListerPtr lister,
                                           FileLauncherPtr launcher,
                                           QObject *parent)
    : QObject(parent), m_lister(std::move(lister)),
      m_launcher(std::move(launcher)) {
  Q_ASSERT(m_lister);
  Q_ASSERT(m_launcher);
}

void NavigationController::navigateTo(const QString &path) {
  const QString normalized = QDir::cleanPath(path);
  if (!m_history.hasCurrent()) {
    m_history.reset(normalized);
    reload();
    emit navigationChanged();
    return;
  }
  if (!m_history.navigateTo(normalized)) {
    return; // already there: no reload, no history churn
  }
  reload();
  emit navigationChanged();
}

void NavigationController::goBack() {
  if (!m_history.goBack()) {
    return;
  }
  reload();
  emit navigationChanged();
}

void NavigationController::goForward() {
  if (!m_history.goForward()) {
    return;
  }
  reload();
  emit navigationChanged();
}

void NavigationController::goUp() {
  if (!m_history.hasCurrent()) {
    return;
  }
  const auto parent = NavigationHistory::parentOf(m_history.currentPath());
  if (!parent) {
    return;
  }
  navigateTo(*parent);
}

void NavigationController::refresh() { reload(); }

void NavigationController::activate(int index) {
  if (index < 0 || index >= m_entries.size()) {
    return;
  }
  const DirectoryEntry &entry = m_entries.at(index);
  if (entry.isDirectory) {
    navigateTo(entry.absolutePath);
    return;
  }
  const LaunchResult result = m_launcher->launch(entry.absolutePath);
  if (!result.ok()) {
    m_launchError = result.diagnostic;
    emit launchErrorChanged();
  }
}

void NavigationController::clearLaunchError() {
  if (m_launchError.isEmpty()) {
    return;
  }
  m_launchError.clear();
  emit launchErrorChanged();
}

int NavigationController::indexOfName(const QString &name) const {
  for (qsizetype i = 0; i < m_entries.size(); ++i) {
    if (m_entries.at(i).name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

QString NavigationController::currentPath() const {
  return m_history.currentPath();
}

bool NavigationController::canGoBack() const { return m_history.canGoBack(); }

bool NavigationController::canGoForward() const {
  return m_history.canGoForward();
}

bool NavigationController::canGoUp() const {
  return m_history.hasCurrent() &&
         NavigationHistory::parentOf(m_history.currentPath()).has_value();
}

QVariantList NavigationController::breadcrumb() const {
  QVariantList list;
  if (!m_history.hasCurrent()) {
    return list;
  }
  const auto segments = NavigationHistory::breadcrumbFor(m_history.currentPath());
  list.reserve(segments.size());
  for (const auto &segment : segments) {
    list.append(QVariantMap{{QStringLiteral("name"), segment.name},
                            {QStringLiteral("path"), segment.path}});
  }
  return list;
}

QString NavigationController::statusKey() const { return statusKeyFor(m_status); }

QString NavigationController::statusMessage() const { return m_statusMessage; }

QVariantList NavigationController::entries() const {
  QVariantList list;
  list.reserve(m_entries.size());
  for (const auto &entry : m_entries) {
    list.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("path"), entry.absolutePath},
        {QStringLiteral("isDirectory"), entry.isDirectory},
        {QStringLiteral("isSymlink"), entry.isSymlink},
        {QStringLiteral("isHidden"), entry.isHidden},
        {QStringLiteral("isReadable"), entry.isReadable},
        {QStringLiteral("size"), entry.size},
        {QStringLiteral("modified"), entry.lastModified},
    });
  }
  return list;
}

QString NavigationController::launchError() const { return m_launchError; }

int NavigationController::entryCount() const {
  return static_cast<int>(m_entries.size());
}

const DirectoryEntry *NavigationController::entryAt(int index) const {
  if (index < 0 || index >= m_entries.size()) {
    return nullptr;
  }
  return &m_entries.at(index);
}

NavigationStatus NavigationController::status() const { return m_status; }

void NavigationController::reload() {
  const ListingResult result = m_lister->list(m_history.currentPath());
  m_status = statusFor(result);
  m_entries = result.ok() ? result.entries : QVector<DirectoryEntry>{};
  if (result.ok()) {
    m_statusMessage = result.truncated
        ? QStringLiteral("Showing the first %1 entries").arg(m_entries.size())
        : QString();
  } else {
    m_statusMessage = result.diagnostic;
  }
  emit entriesChanged();
}

QString NavigationController::statusKeyFor(NavigationStatus status) {
  switch (status) {
  case NavigationStatus::Ready:
    return QStringLiteral("ready");
  case NavigationStatus::Empty:
    return QStringLiteral("empty");
  case NavigationStatus::PermissionDenied:
    return QStringLiteral("permission-denied");
  case NavigationStatus::Missing:
    return QStringLiteral("missing");
  case NavigationStatus::NotADirectory:
    return QStringLiteral("not-a-directory");
  case NavigationStatus::Error:
    return QStringLiteral("error");
  }
  return QStringLiteral("error");
}

} // namespace QindaQt::Apps::FileManager
