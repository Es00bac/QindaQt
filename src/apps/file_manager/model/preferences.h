// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

// Every durable File Manager preference, as one plain value (ADR-0198).
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
  // Presentation defaults, applied to a window when it opens and whenever the
  // user changes them.
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
  // True when every field holds one of the accepted values above.
  [[nodiscard]] bool isValid() const;
};

} // namespace QindaQt::Apps::FileManager
