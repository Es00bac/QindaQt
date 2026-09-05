// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QtTypes>

namespace QindaQt::Shell::DesktopControls {

// Where a command candidate comes from. Each kind maps to exactly one borrowed
// facade and one activation path; the search never invents a fifth path.
enum class CommandSourceKind {
  Applications, // launcher facade: installed application entries
  MenuActions,  // global-menu facade: the active window's exported actions
  Windows,      // task-list facade: presented task rows
  Workspaces,   // workspace facade: desktops plus the show-desktop toggle
};

[[nodiscard]] QString commandSourceKindText(CommandSourceKind kind);

// AGENT-CONTRACT: bounded search input/output. The controller truncates the
// query, caps the per-source candidate count, and caps the ranked result list
// so an unbounded menu tree or window inventory cannot grow the popup.
namespace CommandSearchBounds {
inline constexpr int maxQueryLength = 128;
inline constexpr int maxCandidatesPerSource = 512;
inline constexpr int maxResults = 48;
inline constexpr int maxTextLength = 256;
} // namespace CommandSearchBounds

// One searchable, activatable candidate. `id` is unique across sources
// ("<kind>:<targetId>"); `targetId` and `revision` are what the owning facade
// needs to act (entry id, action id, task id + generation, desktop id +
// revision). `order` preserves the source's own ordering for stable ranking.
struct CommandCandidate {
  QString id;
  CommandSourceKind kind = CommandSourceKind::Applications;
  QString text;
  QString detail;
  QString iconName;
  QString accessibleName;
  bool enabled = true;
  QString targetId;
  quint64 revision = 0;
  int order = 0;

  friend bool operator==(const CommandCandidate &, const CommandCandidate &) =
      default;
};

// Match strength, strongest first. The enum order is the ranking order.
enum class CommandMatch {
  TextPrefix,
  WordStart,
  TextSubstring,
  DetailSubstring,
  None,
};

// Trims, collapses internal whitespace, and bounds the query.
[[nodiscard]] QString normalizeCommandQuery(const QString &query);

// Case-insensitive, locale-independent match classification.
[[nodiscard]] CommandMatch matchCommand(const QString &normalizedQuery,
                                        const CommandCandidate &candidate);

// Deterministic ranking: an empty normalized query returns the candidates in
// input order (bounded); otherwise matching candidates sorted by match
// strength, then input order. Non-matching candidates are dropped.
[[nodiscard]] QList<CommandCandidate> rankCommands(
    const QString &query, const QList<CommandCandidate> &candidates,
    int maxResults = CommandSearchBounds::maxResults);

} // namespace QindaQt::Shell::DesktopControls

Q_DECLARE_METATYPE(QindaQt::Shell::DesktopControls::CommandCandidate)
