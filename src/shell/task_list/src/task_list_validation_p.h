// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

#include <QSet>

namespace QindaQt::ShellTaskList::Validation {

// Batch validation for one candidate generation. Returns the first error in
// a fixed severity-independent scan order so hostile inputs produce stable
// diagnostics. Grouping context (primary availability, identity collisions)
// requires the whole batch, so per-fact and cross-fact checks share this one
// pass instead of being spread over callers.
[[nodiscard]] TaskListError
validateBatch(const QVector<TaskWindowFact> &facts);

// Shared identity helpers used by grouping as well.
[[nodiscard]] bool isValidIdentifier(const QString &value);

} // namespace QindaQt::ShellTaskList::Validation
