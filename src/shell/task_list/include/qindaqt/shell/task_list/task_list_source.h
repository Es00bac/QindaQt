// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_intent.h"
#include "qindaqt/shell/task_list/task_list_types.h"

namespace QindaQt::ShellTaskList {

// Stateful, value-only owner of the latest accepted task generation. The
// source validates hostile facts batch-atomically, collapses containers to
// their primary identity, keeps one revision counter, and arbitrates request
// intents against the exact generation the caller saw.
//
// AGENT-CONTRACT: No event-loop attachment, timer, platform handle, or lock
// lives here; the source is affinity-free but not thread-safe, so the shell
// must use it from one thread (its composing thread). Intent results are pure
// decisions — performing activation/minimize/close remains a separate adapter
// boundary.
class TaskListSource final {
public:
  TaskListSource() = default;

  // Validates and installs a new generation. On success the revision
  // advances by exactly one. On failure the previous generation, revision,
  // and status are preserved untouched, and the returned error names the
  // first offending fact.
  [[nodiscard]] TaskListEvaluation
  publishGeneration(const QVector<TaskWindowFact> &facts);

  // Marks the facts producer unavailable after an accepted generation. The
  // last generation stays visible (degraded retention), and every intent is
  // refused until a fresh publish succeeds.
  void markDegraded();

  // Returns to the initial Loading state and drops the retained generation.
  void reset();

  [[nodiscard]] TaskListSourceStatus status() const { return m_status; }
  [[nodiscard]] quint64 revision() const { return m_generation.revision; }
  // Empty generation while Loading.
  [[nodiscard]] const TaskGeneration &generation() const {
    return m_generation;
  }

  // AGENT-GUARD: Stale-id rejection order is InvalidRequest, NoGeneration,
  // SourceDegraded, StaleRevision, UnknownTask, then acceptance. Checking
  // StaleRevision before UnknownTask guarantees an intent is never resolved
  // against a generation the caller was not looking at, which is the only
  // thing preventing activation of a window the list no longer shows.
  [[nodiscard]] TaskIntentOutcome
  requestIntent(const TaskIntentRequest &request) const;

private:
  TaskGeneration m_generation;
  TaskListSourceStatus m_status = TaskListSourceStatus::Loading;
};

} // namespace QindaQt::ShellTaskList
