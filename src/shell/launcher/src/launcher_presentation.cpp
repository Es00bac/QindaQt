// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/launcher_presentation.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_search_ranker.h"

namespace QindaQt::ShellLauncher {
namespace {

PresentationItem makeItem(const ApplicationEntry &entry, bool pinned)
{
  PresentationItem item;
  item.entryId = entry.id;
  item.displayText = entry.name;
  item.iconName = entry.iconName;
  if (!entry.comment.trimmed().isEmpty())
    item.accessibleDescription = entry.comment;
  else if (!entry.genericName.trimmed().isEmpty())
    item.accessibleDescription = entry.genericName;
  else
    item.accessibleDescription = entry.name;
  item.pinned = pinned;
  return item;
}

LauncherStatus statusFor(const ApplicationCatalog &catalog)
{
  if (!catalog.diagnostics().isEmpty() || catalog.diagnosticsTruncated())
    return LauncherStatus::Degraded;
  if (catalog.entries().isEmpty())
    return LauncherStatus::Empty;
  return LauncherStatus::Ready;
}

QVector<PresentationItem> itemsForIds(const QVector<ApplicationEntry> &entries,
                                      const QVector<QString> &ids)
{
  QVector<PresentationItem> items;
  // Ids arrive in pinned/recent model order; the catalog provides the values
  // and silently drops ids that are not visible entries.
  for (const QString &id : ids) {
    for (const ApplicationEntry &entry : entries) {
      if (entry.id == id) {
        items.append(makeItem(entry, false));
        break;
      }
    }
  }
  return items;
}

LauncherPresentation buildSearchPresentation(const ApplicationCatalog &catalog,
                                             const PinnedApplications &pinned,
                                             const QString &query)
{
  LauncherPresentation presentation;
  presentation.status = statusFor(catalog);

  const SearchOutcome outcome =
      LauncherSearchRanker::search(catalog.entries(), query);
  if (!outcome.ok())
    return presentation;

  PresentationSection section;
  section.kind = SectionKind::SearchResults;
  section.label = SectionLabel::SearchResults;
  for (const RankedEntry &ranked : outcome.results) {
    section.items.append(
        makeItem(ranked.entry, pinned.contains(ranked.entry.id)));
  }
  presentation.sections.append(section);
  return presentation;
}

LauncherPresentation buildBrowsePresentation(const ApplicationCatalog &catalog,
                                             const PinnedApplications &pinned,
                                             const RecentApplications &recent)
{
  LauncherPresentation presentation;
  presentation.status = statusFor(catalog);

  auto pinnedItems = itemsForIds(catalog.entries(), pinned.ids());
  for (PresentationItem &item : pinnedItems)
    item.pinned = true;
  if (!pinnedItems.isEmpty()) {
    presentation.sections.append(
        PresentationSection { SectionKind::Pinned, SectionLabel::Pinned,
                              LauncherCategory::Other, pinnedItems });
  }

  auto recentItems = itemsForIds(catalog.entries(), recent.ids());
  if (!recentItems.isEmpty()) {
    // The pinned flag stays truthful in every section an entry appears in.
    for (PresentationItem &item : recentItems)
      item.pinned = pinned.contains(item.entryId);
    presentation.sections.append(
        PresentationSection { SectionKind::Recent, SectionLabel::Recent,
                              LauncherCategory::Other, recentItems });
  }

  const QVector<CategoryGroup> groups =
      LauncherCategoryModel::group(catalog.entries());
  for (const CategoryGroup &group : groups) {
    PresentationSection section;
    section.kind = SectionKind::Categories;
    section.label = SectionLabel::Category;
    section.category = group.category;
    for (const ApplicationEntry &entry : group.entries)
      section.items.append(makeItem(entry, pinned.contains(entry.id)));
    presentation.sections.append(section);
  }
  return presentation;
}

} // namespace

int LauncherPresentation::itemCount() const
{
  qsizetype count = 0;
  for (const PresentationSection &section : sections)
    count += section.items.size();
  // The catalog and the two identity collections are independently bounded;
  // their combined projection remains far below int's range.
  return static_cast<int>(count);
}

std::optional<PresentationItem> LauncherPresentation::itemAt(int flatIndex) const
{
  if (flatIndex < 0)
    return std::nullopt;
  const qsizetype requested = flatIndex;
  qsizetype consumed = 0;
  for (const PresentationSection &section : sections) {
    if (requested < consumed + section.items.size()) {
      return section.items.at(requested - consumed);
    }
    consumed += section.items.size();
  }
  return std::nullopt;
}

LauncherPresentation LauncherPresentationModel::build(
    const std::optional<ApplicationCatalog> &catalog,
    const PinnedApplications &pinned, const RecentApplications &recent,
    const QString &query)
{
  if (!catalog.has_value())
    return LauncherPresentation { LauncherStatus::Loading, {} };

  if (!query.trimmed().isEmpty()
      && query.size() <= Bounds::maxQueryLength)
    return buildSearchPresentation(*catalog, pinned, query);

  return buildBrowsePresentation(*catalog, pinned, recent);
}

} // namespace QindaQt::ShellLauncher
