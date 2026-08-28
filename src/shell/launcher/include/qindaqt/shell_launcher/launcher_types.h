// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::ShellLauncher {

// Lifecycle of the launcher surface. Loading means no validated catalog has
// been published yet; Empty and Degraded are computed from a published
// catalog. Degraded always means at least one input document failed
// validation or a bound was hit, never that remaining entries are unusable.
enum class LauncherStatus {
  Loading,
  Ready,
  Empty,
  Degraded,
};

// One launchable sub-action declared by a desktop entry
// ([Desktop Action <id>] group referenced through Actions=).
struct DesktopEntryAction {
  QString id;
  QString name;
  QString iconName;

  friend bool operator==(const DesktopEntryAction &,
                         const DesktopEntryAction &) = default;
};

// A validated, visible installed-application value. The id is the desktop
// file identity supplied by the caller (for example "org.qindaqt.editor").
// This struct never carries a command line; launching is expressed as a
// LaunchIntent resolved against a catalog, never executed here.
struct ApplicationEntry {
  QString id;
  QString name;
  QString genericName;
  QString comment;
  QString iconName;
  QStringList categories;
  QStringList keywords;
  QVector<DesktopEntryAction> actions;

  friend bool operator==(const ApplicationEntry &,
                         const ApplicationEntry &) = default;
};

struct SourceDocument {
  // Desktop-file identity and raw keyfile text. The launcher never reads the
  // filesystem itself in L0; a later provider adapter scans installed entries
  // and feeds documents through this value.
  QString sourceId;
  QString text;
};

enum class DiagnosticKind {
  InvalidDocument,
  DuplicateEntryId,
  SourceLimitReached,
  EntryLimitReached,
};

struct CatalogDiagnostic {
  DiagnosticKind kind = DiagnosticKind::InvalidDocument;
  QString sourceId;
  QString message;

  friend bool operator==(const CatalogDiagnostic &,
                         const CatalogDiagnostic &) = default;
};

// A launch request value. actionId empty means the entry's primary action.
// AGENT-NOTE: The intent deliberately contains no Exec/command-line data and
// no execution path; turning an intent into a running process is a later
// milestone behind a separate adapter boundary (see
// docs/wiki/adr/0042-launcher-model-without-execution.md).
struct LaunchIntent {
  QString entryId;
  QString actionId;
  QString displayName;
  QString iconName;

  friend bool operator==(const LaunchIntent &, const LaunchIntent &) = default;
};

enum class LaunchIntentError {
  None,
  UnknownEntry,
  UnknownAction,
};

struct LaunchIntentResult {
  std::optional<LaunchIntent> intent;
  LaunchIntentError error = LaunchIntentError::None;

  bool ok() const { return intent.has_value(); }
};

} // namespace QindaQt::ShellLauncher
