// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QString>
#include <QtCore/QVector>

namespace QindaQt::ApplicationCatalog {
struct DirectoryScan;
}

namespace QindaQt::Apps::SettingsDefaultApps {

struct CandidateApplication final {
  QString id;
  QString name;
  QString iconName;

  friend bool operator==(const CandidateApplication &,
                         const CandidateApplication &) = default;
};

// AGENT-CONTRACT: pure text work over an already-completed scan's retained
// raw document text; never touches the filesystem itself (the composition
// root owns the one scanApplicationDirectories() call, mirroring
// ApplicationsController's ADR-0164 split of resolution from scanning). A
// desktop entry is a candidate for a category when its raw MimeType= line
// lists any one of that category's mimetypes.
[[nodiscard]] QVector<CandidateApplication> candidateApplicationsForCategory(
    const QindaQt::ApplicationCatalog::DirectoryScan &scan,
    DefaultApplicationCategory category);

// The set of MimeType= values a single desktop-entry document declares, as
// literal strings (a semicolon-separated list per the Desktop Entry Spec,
// trailing empty fields dropped). Exposed for tests; the composition uses it
// only indirectly through candidateApplicationsForCategory().
[[nodiscard]] QVector<QString> desktopEntryMimeTypes(const QString &documentText);

} // namespace QindaQt::Apps::SettingsDefaultApps
