// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/task_list/task_list_types.h"

namespace QindaQt::ShellTaskList::Grouping {

// Builds canonical, deterministically ordered entries from one batch that
// validateBatch() has already accepted; pairing and uniqueness assumptions
// are not re-checked here.
[[nodiscard]] QVector<TaskEntry>
canonicalEntries(const QVector<TaskWindowFact> &facts);

} // namespace QindaQt::ShellTaskList::Grouping
