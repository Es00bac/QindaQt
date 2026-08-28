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
    if (parsed.entry->hidden) {
      // Normal producer hint, not degradation: keep the catalog quiet.
      continue;
    }

    ApplicationEntry entry;
    entry.id = document.sourceId;
    entry.name = parsed.entry->name;
    entry.genericName = parsed.entry->genericName;
    entry.comment = parsed.entry->comment;
    entry.iconName = parsed.entry->iconName;
    entry.categories = parsed.entry->categories;
    entry.keywords = parsed.entry->keywords;
    entry.actions = parsed.entry->actions;
    catalog.m_entries.append(entry);
  }

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
