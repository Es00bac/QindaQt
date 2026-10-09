// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/application_catalog.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/desktop_entry_parser.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace QindaQt::ShellLauncher {
namespace {

bool displayLessThan(const ApplicationEntry &left, const ApplicationEntry &right)
{
  const int nameCompare =
      left.name.compare(right.name, Qt::CaseInsensitive);
  if (nameCompare != 0)
    return nameCompare < 0;
  const int idCompare = left.id.compare(right.id, Qt::CaseInsensitive);
  if (idCompare != 0)
    return idCompare < 0;
  return left.id < right.id;
}

} // namespace

void ApplicationCatalog::addDiagnostic(const CatalogDiagnostic &diagnostic)
{
  if (m_diagnostics.size() >= Bounds::maxDiagnostics) {
    m_diagnosticsTruncated = true;
    return;
  }
  m_diagnostics.append(diagnostic);
}

ApplicationCatalog ApplicationCatalog::build(
    const QVector<SourceDocument> &documents)
{
  return build(documents, ApplicationVisibility::MenuEntries);
}

ApplicationCatalog ApplicationCatalog::build(
    const QVector<SourceDocument> &documents, ApplicationVisibility visibility)
{
  ApplicationCatalog catalog;
  catalog.m_entries.reserve(
      std::min(documents.size(), qsizetype(Bounds::maxVisibleEntries)));
  QSet<QString> claimedIds;

  for (int index = 0; index < documents.size(); ++index) {
    if (catalog.m_entries.size() >= Bounds::maxVisibleEntries) {
      catalog.addDiagnostic(
          { DiagnosticKind::EntryLimitReached, {},
            QStringLiteral("visible entry ceiling reached; remaining "
                           "documents were not parsed") });
      break;
    }
    if (index >= Bounds::maxSourceDocuments) {
      catalog.addDiagnostic(
          { DiagnosticKind::SourceLimitReached, {},
            QStringLiteral("source document ceiling reached") });
      break;
    }

    const SourceDocument &document = documents.at(index);
    if (!Bounds::isValidEntryId(document.sourceId)) {
      catalog.addDiagnostic(
          { DiagnosticKind::InvalidDocument,
            Bounds::boundedDiagnosticEntryId(document.sourceId),
            QStringLiteral("source id is empty or over the identity ceiling") });
      continue;
    }
    if (claimedIds.contains(document.sourceId)) {
      catalog.addDiagnostic(
          { DiagnosticKind::DuplicateEntryId, document.sourceId,
            QStringLiteral("later document for a known id was discarded") });
      continue;
    }
    // AGENT-GUARD: Identity precedence is claimed before parsing or applying
    // Hidden/NoDisplay. A broken or deletion-marker document from a higher
    // precedence source must never let a lower-precedence document resurrect
    // the same desktop-file id.
    claimedIds.insert(document.sourceId);

    const DesktopEntryParseResult parsed = DesktopEntryParser::parse(document.text);
    if (!parsed.ok()) {
      catalog.addDiagnostic(
          { DiagnosticKind::InvalidDocument, document.sourceId, parsed.error.message });
      continue;
    }
    if (parsed.entry->deleted
        || (parsed.entry->hidden && visibility != ApplicationVisibility::IncludeNoDisplay)) {
      // AGENT-GUARD: NoDisplay may be a valid MIME handler; Hidden always
      // deletes this identity, including when a lower root has a live copy.
      continue;
    }

    ApplicationEntry entry;
    entry.id = document.sourceId;
    entry.name = parsed.entry->name;
    entry.genericName = parsed.entry->genericName;
    entry.comment = parsed.entry->comment;
    entry.iconName = parsed.entry->iconName;
    entry.startupWmClass = parsed.entry->startupWmClass;
    entry.categories = parsed.entry->categories;
    entry.keywords = parsed.entry->keywords;
    entry.actions = parsed.entry->actions;
    catalog.m_entries.append(entry);
  }

  std::sort(catalog.m_entries.begin(), catalog.m_entries.end(), displayLessThan);
  return catalog;
}

ApplicationCatalog ApplicationCatalog::build(
    const QVector<SourceDocument> &documents,
    const QVector<ManagedApplicationRegistration> &registrations,
    ApplicationVisibility visibility)
{
  ApplicationCatalog catalog = build(documents, visibility);
  if (registrations.isEmpty())
    return catalog;
  QString error;
  if (!validateManagedApplications(registrations, &error)) {
    catalog.addDiagnostic({DiagnosticKind::InvalidManagedSnapshot, {}, error});
    return catalog;
  }
  // AGENT-GUARD: Incomplete XDG inspection cannot establish that no user
  // override masks a managed ID. Never resurrect an unseen lower source.
  if (documents.size() > Bounds::maxSourceDocuments
      || std::any_of(catalog.m_diagnostics.cbegin(), catalog.m_diagnostics.cend(),
                     [](const CatalogDiagnostic &diagnostic) {
                       return diagnostic.kind == DiagnosticKind::EntryLimitReached
                           || diagnostic.kind == DiagnosticKind::SourceLimitReached;
                     })) {
    catalog.addDiagnostic({DiagnosticKind::InvalidManagedSnapshot, {},
                           QStringLiteral("managed rows withheld after incomplete XDG inspection")});
    return catalog;
  }
  QSet<QString> claimedIds;
  for (const auto &document : documents) {
    if (Bounds::isValidEntryId(document.sourceId))
      claimedIds.insert(document.sourceId);
  }
  QVector<ApplicationEntry> managedEntries;
  for (const auto &registration : registrations) {
    const QString id = managedApplicationEntryId(registration.binding);
    if (claimedIds.contains(id))
      continue;
    ApplicationEntry entry;
    entry.id = id;
    entry.name = registration.name;
    entry.genericName = registration.genericName;
    entry.comment = registration.comment;
    entry.iconName = registration.iconName;
    entry.categories = registration.categories;
    entry.keywords = registration.keywords;
    entry.managedBinding = registration.binding;
    managedEntries.append(entry);
  }
  if (catalog.m_entries.size() + managedEntries.size() > Bounds::maxVisibleEntries) {
    catalog.addDiagnostic({DiagnosticKind::InvalidManagedSnapshot, {},
                           QStringLiteral("combined catalog exceeds its entry ceiling")});
    return catalog;
  }
  catalog.m_entries += managedEntries;
  std::sort(catalog.m_entries.begin(), catalog.m_entries.end(), displayLessThan);
  return catalog;
}

std::optional<ApplicationEntry> ApplicationCatalog::entry(
    const QString &entryId) const
{
  for (const ApplicationEntry &candidate : m_entries) {
    if (candidate.id == entryId)
      return candidate;
  }
  return std::nullopt;
}

LaunchIntentResult ApplicationCatalog::makeLaunchIntent(
    const QString &entryId, const QString &actionId) const
{
  const std::optional<ApplicationEntry> source = entry(entryId);
  if (!source)
    return { std::nullopt, LaunchIntentError::UnknownEntry };

  LaunchIntent intent;
  intent.entryId = source->id;
  intent.displayName = source->name;
  intent.iconName = source->iconName;
  intent.managedBinding = source->managedBinding;
  if (!actionId.isEmpty()) {
    const auto action = std::find_if(source->actions.cbegin(), source->actions.cend(),
                                     [&actionId](const DesktopEntryAction &candidate) {
                                       return candidate.id == actionId;
                                     });
    if (action == source->actions.cend())
      return { std::nullopt, LaunchIntentError::UnknownAction };
    intent.actionId = action->id;
    intent.displayName = action->name;
    intent.iconName = action->iconName.isEmpty() ? source->iconName : action->iconName;
  }
  return { intent, LaunchIntentError::None };
}

} // namespace QindaQt::ShellLauncher
