// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/producer/task_list_wire.h"
#include "qindaqt/shell/task_list/task_list_types.h"

#include <optional>

namespace QindaQt::ShellTaskList::Producer {

enum class TaskListJoinError {
  None,
  // A published window has no entry in the scope snapshot; the producer
  // refuses to invent an output or workspace scope (fail-closed).
  ScopeUnavailable,
  // A window names a container the Containers() read does not know.
  UnknownContainerOwner,
  // Containers() lists a container no published window belongs to.
  EmptyContainerOnWire,
  // No container member carries the collapsed native identity
  // (skipTaskbar == false); the primary cannot be classified.
  TaskIdentityMissing,
  // Two or more members of one container claim the native identity.
  TaskIdentityAmbiguous,
};

struct TaskListJoinFailure {
  TaskListJoinError code = TaskListJoinError::None;
  QString windowId;
  QString containerId;
  QString message;

  [[nodiscard]] bool hasError() const noexcept {
    return code != TaskListJoinError::None;
  }
};

// Container lineage of one accepted join, retained by the producer so the
// operation adapter can fence Submit transactions and reject authorities the
// public protocol does not mutate.
struct TaskListContainerLineage {
  QString containerId;
  quint64 revision = 0;
  TaskListContainerAuthority authority = TaskListContainerAuthority::HybridProcess;

  friend bool operator==(const TaskListContainerLineage &,
                         const TaskListContainerLineage &) = default;
};

struct TaskListJoinResult {
  QVector<TaskWindowFact> facts;
  QVector<TaskListContainerLineage> containers;
  TaskListJoinFailure error;

  [[nodiscard]] bool ok() const noexcept { return !error.hasError(); }
};

// Joins one owner-coherent set of wire reads into the T0 injected-facts
// contract (docs/wiki/shell/task-list.md). Pure value policy: no transport,
// no clocks, no event-loop attachment.
//
// AGENT-CONTRACT: Classification reuses the compositor's collapsed native
// identity instead of re-deriving page state from per-container Snapshot
// reads; a Snapshot fan-out could never be coherent with the Windows() read
// it supplements. Member facts copy the primary's output/workspace scope
// because the primary's placement describes the whole container, and Compositor1
// 1.1 exposes no urgent state, so every fact's urgent flag is false (the
// extension request is recorded in the wiki page).
class TaskListFactJoiner final {
public:
  [[nodiscard]] static TaskListJoinResult
  join(const QVector<TaskListWireWindow> &windows,
       const QVector<TaskListWireContainer> &containers,
       const TaskListWireScopeSnapshot &scope);
};

} // namespace QindaQt::ShellTaskList::Producer
