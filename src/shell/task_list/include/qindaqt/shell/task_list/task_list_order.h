// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_presentation.h"
#include "qindaqt/shell/task_list/task_list_types.h"

#include <QStringList>

namespace QindaQt::ShellTaskList {

// AGENT-CONTRACT: the user-order overlay. The module's canonical deterministic
// order (application id, then kind, then task id) remains the default and the
// fallback; a persisted user order — a bounded, deduplicated list of task ids
// — takes precedence for the displayed (and therefore keyboard traversal)
// order. Entries whose id is not named in the overlay keep canonical order
// behind the overlayed ones, so a restored window or a never-moved window
// always appears at a deterministic position. The overlay stores only ids;
// it never touches window facts, generation revisions, or intent arbitration.
class TaskListOrder {
public:
  TaskListOrder() = delete;

  // Total normalization of a persisted candidate: drops blank, oversized, and
  // duplicate ids, and bounds the list at kMaxWindowFacts. The result is
  // safe to retain and compare; hostile settings values can never smuggle
  // unbounded or duplicated ids into presentation state.
  [[nodiscard]] static QStringList normalize(const QStringList &userOrder);

  // Applies one stable overlay pass: entries named in userOrder come first
  // in userOrder order (ids without a matching entry are ignored), and every
  // other entry keeps its canonical relative order behind them. identities is
  // permuted in lockstep with entries and keyboardIndex is re-assigned 1..n
  // along the displayed order, so traversal order stays the displayed order.
  static void applyOverlay(const QStringList &userOrder,
                           QVector<TaskEntry> &entries,
                           QVector<TaskEntryIdentity> &identities);
};

} // namespace QindaQt::ShellTaskList
