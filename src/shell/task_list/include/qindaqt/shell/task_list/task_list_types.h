// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace QindaQt::ShellTaskList {

// AGENT-CONTRACT: Every value in this header is an immutable snapshot supplied
// by shell composition from public compositor/hybrid state. This module never
// dereferences compositor objects, never mutates windows, and never issues
// platform requests; it only classifies, orders, filters, and records request
// intents (the boundary check in tests/shell/task_list enforces this).
// Producers copy one coherent generation before calling publishGeneration().
inline constexpr quint32 kMaxWindowFacts = 4096;
inline constexpr int kMaxWorkspacesPerWindow = 64;
inline constexpr qsizetype kMaxIdLength = 512;

// How the compositor/hybrid classifies one client toplevel for task lists.
// Grouped containers collapse to exactly one entry represented by the
// primary active-page member; other members are suppressed (see
// window-containers.md) but remain listed so the container entry can report
// its window count and demand-attention state.
enum class TaskWindowRole {
  Standalone,
  ContainerPrimary,
  ContainerMember,
};

enum class TaskEntryKind {
  Window,
  Container,
};

struct TaskWindowFact {
  // windowId is unique across the batch. containerId is required for
  // ContainerPrimary/ContainerMember roles and must be empty for Standalone.
  // Exactly one fact per accepted generation may set active, and an active
  // window is never minimized. ContainerMember facts carry state only through
  // urgent; active/minimized belong to the representative primary fact.
  QString windowId;
  QString applicationId;
  QString applicationName;
  QString title;
  // Compositor-assigned output and virtual-desktop scope of the window (for
  // members: the primary's placement describes the whole container).
  QString outputId;
  QStringList workspaceIds;
  bool onAllWorkspaces = false;
  TaskWindowRole role = TaskWindowRole::Standalone;
  QString containerId;
  bool active = false;
  bool minimized = false;
  bool urgent = false;

  friend bool operator==(const TaskWindowFact &,
                         const TaskWindowFact &) = default;
};

// One user-visible task-list row: a standalone window, or a whole QindaQt
// container collapsed to its primary identity. windowCount counts the
// representative plus every suppressed member; memberWindowIds is sorted so
// downstream adapters target windows deterministically.
struct TaskEntry {
  // Stable within one generation: windowId for Window entries, containerId
  // for Container entries. Producers must keep these identifier spaces
  // disjoint; validation rejects collisions.
  QString taskId;
  TaskEntryKind kind = TaskEntryKind::Window;
  QString applicationId;
  QString applicationName;
  QString title;
  QString primaryWindowId;
  QStringList memberWindowIds;
  quint32 windowCount = 1;
  QString outputId;
  QStringList workspaceIds;
  bool onAllWorkspaces = false;
  bool active = false;
  bool minimized = false;
  bool urgent = false;

  friend bool operator==(const TaskEntry &, const TaskEntry &) = default;
};

// AGENT-NOTE: Entries are stored in the module's canonical deterministic
// order (grouped by applicationId, then kind, then taskId). That order is
// also the keyboard traversal order, so it must never become iteration-order
// dependent; see task_list_grouping.cpp before changing the comparator.
struct TaskGeneration {
  quint64 revision = 0;
  QVector<TaskEntry> entries;

  friend bool operator==(const TaskGeneration &,
                         const TaskGeneration &) = default;
};

enum class TaskListErrorCode {
  None,
  LimitExceeded,
  InvalidIdentity,
  InvalidWorkspaceScope,
  DuplicateWindowId,
  ConflictingContainerRole,
  OrphanContainerMember,
  DuplicateContainerPrimary,
  InvalidMemberState,
  MultipleActiveWindows,
  ActiveMinimizedConflict,
  EntryIdentityCollision,
};

struct TaskListError {
  TaskListErrorCode code = TaskListErrorCode::None;
  // Context of the first offending fact, when applicable.
  QString windowId;
  QString containerId;
  QString message;

  [[nodiscard]] bool hasError() const noexcept {
    return code != TaskListErrorCode::None;
  }

  friend bool operator==(const TaskListError &, const TaskListError &) = default;
};

// AGENT-GUARD: A failed publish must leave the source's previous generation
// and revision untouched. Shell surfaces cache the last accepted generation
// and cannot recover from a partially applied or corrupt intermediate state.
struct TaskListEvaluation {
  // Valid only when ok() is true; otherwise it is a default generation.
  TaskGeneration generation;
  TaskListError error;

  [[nodiscard]] bool ok() const noexcept { return !error.hasError(); }

  friend bool operator==(const TaskListEvaluation &,
                         const TaskListEvaluation &) = default;
};

// Source availability. Degraded means the facts producer is gone or failed
// validation: the last accepted generation is retained so the list does not
// flicker empty, but every intent is refused until a fresh publish succeeds.
enum class TaskListSourceStatus {
  Loading,
  Ready,
  Degraded,
};

} // namespace QindaQt::ShellTaskList
