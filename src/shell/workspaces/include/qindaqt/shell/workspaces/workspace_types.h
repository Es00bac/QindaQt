// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QtTypes>

namespace QindaQt::Shell::Workspaces {

// AGENT-CONTRACT: hostile-input ceilings for the compositor workspace
// inventory. The D-Bus adapter and every consumer rely on these; duplicating a
// limit elsewhere creates a second authority that can drift.
namespace Bounds {
inline constexpr int maxWorkspaces = 64;
inline constexpr int maxIdLength = 128;
inline constexpr int maxNameLength = 128;
inline constexpr int maxReasonLength = 256;
} // namespace Bounds

// One virtual desktop as published by the compositor. `id` is the compositor's
// stable desktop identifier (KWin publishes UUID strings); `position` is the
// zero-based ordering index. Presentation never invents an id from a position.
struct WorkspaceRow {
  int position = 0;
  QString id;
  QString name;

  friend bool operator==(const WorkspaceRow &, const WorkspaceRow &) = default;
};

// One complete generation of workspace truth from one exact compositor owner.
struct WorkspaceSnapshot {
  QList<WorkspaceRow> desktops;
  QString currentId;
  quint32 rows = 1;
  bool showingDesktop = false;

  friend bool operator==(const WorkspaceSnapshot &,
                         const WorkspaceSnapshot &) = default;
};

struct WorkspaceValidation {
  bool ok = false;
  QString reasonCode;
  // Normalized copy: desktops sorted by position (ties by id) and names bounded.
  WorkspaceSnapshot snapshot;
};

// Total validation of one snapshot. Rejects an empty or over-long desktop
// list, empty/duplicate/over-long ids, a current id absent from the list, and
// zero rows. Over-long names are truncated rather than rejected because the
// compositor owner is already authenticated and a name is presentation only.
[[nodiscard]] WorkspaceValidation validateWorkspaceSnapshot(
    const WorkspaceSnapshot &snapshot);

// Least-authority grants evaluated by shell composition once. Read gates
// observation entirely; manage gates every switch/show-desktop intent.
struct WorkspaceGrants {
  bool windowsRead = false;
  bool windowsManage = false;

  friend bool operator==(const WorkspaceGrants &, const WorkspaceGrants &) =
      default;
};

enum class WorkspacePhase {
  Loading,
  Ready,
  Degraded,
  Unavailable,
};

[[nodiscard]] QString workspacePhaseText(WorkspacePhase phase);

} // namespace QindaQt::Shell::Workspaces

Q_DECLARE_METATYPE(QindaQt::Shell::Workspaces::WorkspaceRow)
Q_DECLARE_METATYPE(QindaQt::Shell::Workspaces::WorkspaceSnapshot)
