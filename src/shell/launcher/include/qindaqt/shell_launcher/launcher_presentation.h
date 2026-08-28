// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_category_model.h"
#include "qindaqt/shell_launcher/launcher_pinned_recent.h"
#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::ShellLauncher {

enum class SectionKind {
  Pinned,
  Recent,
  Categories,
  SearchResults,
};

// Stable translation identities. The pure Qt Core model never owns localized
// user-facing strings; the future presentation adapter translates these keys
// and LauncherCategory values at the UI boundary.
enum class SectionLabel {
  Pinned,
  Recent,
  SearchResults,
  Category,
};

enum class AccessibleRole {
  ListItem,
};

struct PresentationItem {
  QString entryId;
  QString displayText;
  QString iconName;
  QString accessibleDescription;
  // Every launcher row is an activatable list item; the role is explicit so
  // the future QML adapter cannot invent a divergent accessible taxonomy.
  AccessibleRole accessibleRole = AccessibleRole::ListItem;
  bool pinned = false;

  friend bool operator==(const PresentationItem &,
                         const PresentationItem &) = default;
};

struct PresentationSection {
  SectionKind kind = SectionKind::Categories;
  SectionLabel label = SectionLabel::Category;
  LauncherCategory category = LauncherCategory::Other;
  QVector<PresentationItem> items;

  friend bool operator==(const PresentationSection &,
                         const PresentationSection &) = default;
};

// A pure projection of the launcher surface: sections in stable order, items
// in stable order. Focus order is exactly section order then item order, so
// keyboard traversal needs no additional state. A QML adapter later renders
// these values; business rules stay here.
struct LauncherPresentation {
  LauncherStatus status = LauncherStatus::Loading;
  QVector<PresentationSection> sections;

  int itemCount() const;
  std::optional<PresentationItem> itemAt(int flatIndex) const;
};

// Builds the presentation from validated inputs. Keyboard and pointer
// activation both resolve through ApplicationCatalog::makeLaunchIntent; there
// is deliberately no divergent keyboard activation path.
class LauncherPresentationModel {
public:
  // An absent catalog means Loading. When a query is non-blank and within
  // the query bound the presentation collapses to one search-results section.
  // A query beyond the bound is a caller contract violation; the model keeps
  // the browse presentation rather than inventing a second degraded state
  // whose meaning would drift from catalog degradation.
  static LauncherPresentation build(const std::optional<ApplicationCatalog> &catalog,
                                   const PinnedApplications &pinned,
                                   const RecentApplications &recent,
                                   const QString &query = {});
};

} // namespace QindaQt::ShellLauncher
