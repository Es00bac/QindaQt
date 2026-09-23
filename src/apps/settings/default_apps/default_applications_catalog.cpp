// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <qindaqt/application_catalog/application_directory_scan.h>

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
    const DefaultApplicationCategory category,
    const QMap<QString, QStringList> *associationProjection) {
  const QStringList categoryMimeTypes = defaultApplicationCategoryMimeTypes(category);
  QVector<CandidateApplication> candidates;
  for (const auto &scanned : scan.applications) {
    // AGENT-CONTRACT: the store and chooser use the same effective association
    // projection. Raw MimeType= alone misses user Added/Removed Associations.
    const QString desktopId = scanned.entry.id + QStringLiteral(".desktop");
    QStringList associated;
    if (associationProjection) {
      associated = associationProjection->value(desktopId);
    } else {
      for (const QString &mimeType : desktopEntryMimeTypes(scanned.documentText))
        associated.append(mimeType);
    }
    QStringList supported;
    for (const QString &mimeType : categoryMimeTypes)
      if (associated.contains(mimeType)) supported.append(mimeType);
    if (supported.isEmpty()) continue;
    candidates.append(CandidateApplication{
        // AGENT-CONTRACT: ApplicationCatalog ids omit the .desktop suffix;
        // freedesktop mimeapps.list values and this route's choice IDs keep it.
        .id = desktopId,
        .name = scanned.entry.name,
        .iconName = scanned.entry.iconName,
        .supportedMimeTypes = supported,
    });
  }
  return candidates;
}

} // namespace QindaQt::Apps::SettingsDefaultApps
