// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// One visible Details column (ADR-0270). `width` is in logical pixels; 0
// means the view's own default width. Name is always the first column and
// always elastic, so its width is ignored.
struct DetailsColumn final {
  QString key;
  int width = 0;

  [[nodiscard]] bool operator==(const DetailsColumn &) const = default;
};

// What one folder looks like (ADR-0270): its view, order, grouping, zoom and
// Details columns. The defaults and every remembered folder share the shape,
// so "Use as Defaults" is a plain copy.
struct FolderView final {
  QString viewMode = QStringLiteral("grid");
  QString sortColumn = QStringLiteral("name");
  QString sortDirection = QStringLiteral("ascending");
  QString groupBy = QStringLiteral("none");
  int iconSize = 64;
  QList<DetailsColumn> columns = defaultColumns();

  [[nodiscard]] bool operator==(const FolderView &) const = default;
  [[nodiscard]] bool isValid() const;
  // Name, Size, Kind, Date Modified: the Details view the application always had.
  [[nodiscard]] static QList<DetailsColumn> defaultColumns();
};

// One folder's own view, remembered because the user changed it there.
struct RememberedFolderView final {
  QString location;
  FolderView view;

  [[nodiscard]] bool operator==(const RememberedFolderView &) const = default;
};

// Every durable File Manager preference, as one plain value (ADR-0198,
// preferences-v2 since ADR-0270).
//
// AGENT-GUARD: every field here must change something the user can see. A
// knob with no effect is worse than an absent one, so do not add a field
// without wiring it in the same change -- and the focused rows assert the
// wiring, not just the round trip.
//
// AGENT-CONTRACT: this supersedes the ADR-0090 deferral that left sort,
// hidden visibility, view mode, and zoom session-local pending a Settings1
// schema decision. They are app-local preferences, in the app's own state
// directory, exactly like bookmarks and saved network locations.
struct Preferences final {
  // Bounds that keep preferences-v2 small (PreferencesStore::maximumBytes).
  static constexpr int maximumFolderViews = 64;
  static constexpr int maximumLocationBytes = 1024;
  static constexpr int minimumColumnWidth = 40;
  static constexpr int maximumColumnWidth = 1000;

  // Presentation defaults, applied to every folder that has no view of its
  // own, when a window opens and whenever the user changes them.
  // AGENT-GUARD: every default here must be the value the application
  // already had before preferences existed, so a first run behaves exactly as
  // it used to. defaultViewMode mirrors NavigationController's own "grid",
  // and iconSize its default rung of the zoom ladder.
  QString defaultViewMode = QStringLiteral("grid");
  bool showHidden = false;
  bool directoriesFirst = true;
  QString sortColumn = QStringLiteral("name");
  QString sortDirection = QStringLiteral("ascending");
  int iconSize = 64;
  // ADR-0270 (preferences-v2): the rest of the default folder view, and the
  // Details view's window-wide presentation.
  QString groupBy = QStringLiteral("none");
  QList<DetailsColumn> detailsColumns = FolderView::defaultColumns();
  bool relativeDates = false;
  QString rowDensity = QStringLiteral("comfortable");
  bool showExtensions = true;
  // Folders whose view differs from the defaults, most recently changed
  // first, at most maximumFolderViews (the oldest is forgotten first).
  QList<RememberedFolderView> folderViews;

  // Network.
  bool discoverNearbyServers = false;
  QString defaultConnectScheme = QStringLiteral("sftp");

  // Trash. Only the recoverable home Trash may be skipped; Empty Trash is
  // permanent and always confirms.
  bool confirmTrash = true;

  [[nodiscard]] bool operator==(const Preferences &) const = default;

  [[nodiscard]] static QStringList viewModes();
  [[nodiscard]] static QStringList sortColumns();
  [[nodiscard]] static QStringList sortDirections();
  [[nodiscard]] static QList<int> iconSizes();
  [[nodiscard]] static QStringList groupKeys();
  [[nodiscard]] static QStringList columnKeys();
  [[nodiscard]] static QStringList rowDensities();
  // True when every field holds one of the accepted values above.
  [[nodiscard]] bool isValid() const;

  // The folder view every folder without its own starts from.
  [[nodiscard]] FolderView defaultFolderView() const;
  void setDefaultFolderView(const FolderView &view);
  // The view `location` shows: its own when remembered, else the defaults.
  [[nodiscard]] FolderView folderViewFor(const QString &location) const;
  [[nodiscard]] bool remembers(const QString &location) const;
  // Remembers `view` for `location` as the most recent; a view equal to the
  // defaults is forgotten instead, so that folder follows later defaults.
  void rememberFolderView(const QString &location, const FolderView &view);
};

} // namespace QindaQt::Apps::FileManager
