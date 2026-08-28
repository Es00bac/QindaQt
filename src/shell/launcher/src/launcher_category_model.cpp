// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/launcher_category_model.h"

#include <QHash>

#include <initializer_list>

namespace QindaQt::ShellLauncher {
namespace {

struct CategoryMapping {
  const char *xdgCategory;
  LauncherCategory category;
};

// AGENT-GUARD: The outer scan follows the producer's category order, so the
// first recognized input category wins. Table order only maps one exact name;
// changing the loop nesting would silently change precedence.
constexpr CategoryMapping kCategoryMappings[] = {
  { "Development", LauncherCategory::Development },
  { "IDE", LauncherCategory::Development },
  { "Game", LauncherCategory::Games },
  { "Graphics", LauncherCategory::Graphics },
  { "AudioVideo", LauncherCategory::AudioVideo },
  { "Audio", LauncherCategory::AudioVideo },
  { "Video", LauncherCategory::AudioVideo },
  { "Music", LauncherCategory::AudioVideo },
  { "Network", LauncherCategory::Network },
  { "InstantMessaging", LauncherCategory::Network },
  { "Email", LauncherCategory::Network },
  { "Office", LauncherCategory::Office },
  { "WordProcessor", LauncherCategory::Office },
  { "Spreadsheet", LauncherCategory::Office },
  { "Presentation", LauncherCategory::Office },
  { "Database", LauncherCategory::Office },
  { "Calendar", LauncherCategory::Office },
  { "Science", LauncherCategory::Science },
  { "Education", LauncherCategory::Education },
  { "Settings", LauncherCategory::Settings },
  { "System", LauncherCategory::System },
  { "Utility", LauncherCategory::Utilities },
  { "Utilities", LauncherCategory::Utilities },
  { "Accessibility", LauncherCategory::Utilities },
};

} // namespace

LauncherCategory LauncherCategoryModel::categoryFor(
    const QStringList &xdgCategories)
{
  for (const QString &candidate : xdgCategories) {
    for (const CategoryMapping &mapping : kCategoryMappings) {
      if (candidate == QLatin1String(mapping.xdgCategory))
        return mapping.category;
    }
  }
  return LauncherCategory::Other;
}

QVector<CategoryGroup> LauncherCategoryModel::group(
    const QVector<ApplicationEntry> &entries)
{
  // Bucket first, then emit groups in enum order; entries arrive in the
  // catalog's deterministic order, so per-group order needs no extra sort.
  QHash<LauncherCategory, QVector<ApplicationEntry>> buckets;
  for (const ApplicationEntry &entry : entries)
    buckets[categoryFor(entry.categories)].append(entry);

  QVector<CategoryGroup> groups;
  const auto allCategories = {
    LauncherCategory::Utilities, LauncherCategory::Development,
    LauncherCategory::Education, LauncherCategory::Games,
    LauncherCategory::Graphics,  LauncherCategory::AudioVideo,
    LauncherCategory::Network,   LauncherCategory::Office,
    LauncherCategory::Science,   LauncherCategory::Settings,
    LauncherCategory::System,    LauncherCategory::Other,
  };
  for (const LauncherCategory category : allCategories) {
    const auto bucket = buckets.constFind(category);
    if (bucket == buckets.constEnd() || bucket->isEmpty())
      continue;
    groups.append(CategoryGroup { category, *bucket });
  }
  return groups;
}

} // namespace QindaQt::ShellLauncher
