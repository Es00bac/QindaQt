// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_default_apps/default_applications_store.h>

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace QindaQt::ApplicationCatalog {
struct DirectoryScan;
}

namespace QindaQt::Apps::SettingsDefaultApps {

struct CandidateApplication final {
  // Freedesktop desktop-file ID, including the .desktop suffix.
  QString id;
  QString name;
  QString iconName;
  QStringList supportedMimeTypes;

  friend bool operator==(const CandidateApplication &,
                         const CandidateApplication &) = default;
};

// AGENT-CONTRACT: pure text work over an already-completed scan's retained
// raw document text; never touches the filesystem itself (the composition
// root owns the one scanApplicationDirectories() call, mirroring
// ApplicationsController's ADR-0164 split of resolution from scanning). A
// desktop entry is a candidate for a category when it effectively supports
// at least one MIME type. Optional associationProjection includes XDG Added
// and Removed Associations; absent it, raw MimeType= is used for pure tests.
[[nodiscard]] QVector<CandidateApplication> candidateApplicationsForCategory(
    const QindaQt::ApplicationCatalog::DirectoryScan &scan,
    DefaultApplicationCategory category,
    const QMap<QString, QStringList> *associationProjection = nullptr);

// The set of MimeType= values a single desktop-entry document declares, as
// literal strings (a semicolon-separated list per the Desktop Entry Spec,
// trailing empty fields dropped). Shared by candidate projection and the
// store’s effective-association check; neither caller reparses private state.
[[nodiscard]] QVector<QString> desktopEntryMimeTypes(const QString &documentText);

} // namespace QindaQt::Apps::SettingsDefaultApps
