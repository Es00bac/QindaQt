// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QVector>

namespace QindaQt::ShellLauncher {

// Fixed QindaQt launcher groups. The enum order is the presentation order and
// must not be reordered: the category model, presentation builder, and tests
// all rely on it. Other is always the terminal bucket for entries whose XDG
// categories map to nothing else.
enum class LauncherCategory {
  Utilities,
  Development,
  Education,
  Games,
  Graphics,
  AudioVideo,
  Network,
  Office,
  Science,
  Settings,
  System,
  Other,
};

struct CategoryGroup {
  LauncherCategory category = LauncherCategory::Other;
  QVector<ApplicationEntry> entries;

  friend bool operator==(const CategoryGroup &, const CategoryGroup &) = default;
};

// Deterministic classification of validated entries into fixed groups.
// AGENT-NOTE: The XDG-category mapping is locale-independent on purpose; a
// translated desktop entry must land in the same group as its English
// original so category order never depends on the user's locale.
class LauncherCategoryModel {
public:
  static LauncherCategory categoryFor(const QStringList &xdgCategories);

  // Returns only non-empty groups, in fixed enum order, each with its entries
  // in the catalog's deterministic display order.
  static QVector<CategoryGroup> group(const QVector<ApplicationEntry> &entries);
};

} // namespace QindaQt::ShellLauncher
