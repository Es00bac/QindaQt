// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <qindaqt/application_catalog/application_directory_scan.h>

#include <QtCore/QSet>
#include <QtCore/QStringList>

namespace QindaQt::Apps::SettingsDefaultApps {

QVector<QString> desktopEntryMimeTypes(const QString &documentText) {
  QVector<QString> mimeTypes;
  bool inDesktopEntryGroup = false;
  const QStringList lines = documentText.split(QLatin1Char('\n'));
  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();
    if (line.startsWith(QLatin1Char('['))) {
      inDesktopEntryGroup = (line == QStringLiteral("[Desktop Entry]"));
      continue;
    }
    if (!inDesktopEntryGroup) continue;
    if (!line.startsWith(QStringLiteral("MimeType="))) continue;
    const QString value = line.mid(9);
    for (const QString &field : value.split(QLatin1Char(';'))) {
      const QString trimmed = field.trimmed();
      if (!trimmed.isEmpty()) mimeTypes.append(trimmed);
    }
    // AGENT-GUARD: the Desktop Entry Spec permits at most one MimeType= key
    // per [Desktop Entry] group; stop at the first to avoid a later
    // duplicate silently overriding the real list.
    break;
  }
  return mimeTypes;
}

QVector<CandidateApplication> candidateApplicationsForCategory(
    const QindaQt::ApplicationCatalog::DirectoryScan &scan,
    const DefaultApplicationCategory category) {
  const QStringList categoryMimeTypes = defaultApplicationCategoryMimeTypes(category);
  const QSet<QString> wanted(categoryMimeTypes.cbegin(), categoryMimeTypes.cend());
  QVector<CandidateApplication> candidates;
  for (const auto &scanned : scan.applications) {
    bool matches = false;
    for (const QString &mimeType : desktopEntryMimeTypes(scanned.documentText)) {
      if (wanted.contains(mimeType)) {
        matches = true;
        break;
      }
    }
    if (!matches) continue;
    candidates.append(CandidateApplication{
        // AGENT-CONTRACT: ApplicationCatalog ids omit the .desktop suffix;
        // freedesktop mimeapps.list values and this route's choice IDs keep it.
        .id = scanned.entry.id + QStringLiteral(".desktop"),
        .name = scanned.entry.name,
        .iconName = scanned.entry.iconName,
    });
  }
  return candidates;
}

} // namespace QindaQt::Apps::SettingsDefaultApps
