// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/places_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopControls {

QList<PlaceEntry> standardPlaces(
    const std::function<bool(const QString &)> &directoryExists)
{
  const auto exists = directoryExists
      ? directoryExists
      : std::function<bool(const QString &)>([](const QString &path) {
          return QFileInfo(path).isDir();
        });
  struct Candidate {
    const char *id;
    const char *label;
    QStandardPaths::StandardLocation location;
    const char *icon;
  };
  static const Candidate candidates[] = {
      {"home", "Home", QStandardPaths::HomeLocation, "user-home"},
      {"desktop", "Desktop", QStandardPaths::DesktopLocation, "user-desktop"},
      {"documents", "Documents", QStandardPaths::DocumentsLocation, "folder-documents"},
      {"downloads", "Downloads", QStandardPaths::DownloadLocation, "folder-download"},
      {"music", "Music", QStandardPaths::MusicLocation, "folder-music"},
      {"pictures", "Pictures", QStandardPaths::PicturesLocation, "folder-pictures"},
      {"videos", "Videos", QStandardPaths::MoviesLocation, "folder-videos"},
  };
  QList<PlaceEntry> entries;
  QSet<QString> seenPaths;
  for (const Candidate &candidate : candidates) {
    const QString path =
        QDir::cleanPath(QStandardPaths::writableLocation(candidate.location));
    if (path.isEmpty() || !QFileInfo(path).isAbsolute() || seenPaths.contains(path)
        || !exists(path)) {
      continue;
    }
    seenPaths.insert(path);
    entries.append({QString::fromLatin1(candidate.id),
                    QString::fromLatin1(candidate.label), path,
                    QString::fromLatin1(candidate.icon)});
  }
  const QString root = QDir::rootPath();
  if (!seenPaths.contains(root) && exists(root)) {
    entries.append({QStringLiteral("computer"), QStringLiteral("Computer"), root,
                    QStringLiteral("drive-harddisk")});
  }
  return entries;
}

PlacesController::PlacesController(FolderOpener *opener,
                                   bool applicationsLaunchGranted,
                                   QList<PlaceEntry> entries, QObject *parent)
    : QObject(parent)
    , m_opener(opener)
    , m_launchGranted(applicationsLaunchGranted)
    , m_entries(std::move(entries))
{
  // AGENT-GUARD: the inventory is bounded at construction so a hostile or
  // misconfigured environment cannot grow the menu without limit.
  while (m_entries.size() > MaxPlaces) {
    m_entries.removeLast();
  }
}

QVariantList PlacesController::rows() const
{
  QVariantList rows;
  rows.reserve(m_entries.size());
  int index = 0;
  for (const PlaceEntry &entry : m_entries) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("label"), entry.label},
        {QStringLiteral("path"), entry.path},
        {QStringLiteral("iconName"), entry.iconName},
        {QStringLiteral("accessibleName"),
         QStringLiteral("%1, %2").arg(entry.label, entry.path)},
        {QStringLiteral("index"), index},
    });
    ++index;
  }
  return rows;
}

bool PlacesController::open(const QString &placeId)
{
  if (!m_launchGranted) {
    publishFeedback(QStringLiteral("Opening folders is not granted"));
    return false;
  }
  if (m_opener == nullptr) {
    publishFeedback(QStringLiteral("The file manager is unavailable"));
    return false;
  }
  for (const PlaceEntry &entry : m_entries) {
    if (entry.id != placeId) {
      continue;
    }
    const FolderOpener::Result result = m_opener->open(entry.path);
    if (!result.ok) {
      publishFeedback(QStringLiteral("Could not open %1: %2")
                          .arg(entry.label, result.diagnostic));
      return false;
    }
    clearFeedback();
    return true;
  }
  publishFeedback(QStringLiteral("That place is no longer available"));
  return false;
}

void PlacesController::clearFeedback()
{
  publishFeedback({});
}

void PlacesController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
