// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/task_list_order.h"

#include <QHash>
#include <QSet>
#include <utility>

namespace QindaQt::ShellTaskList {

QStringList TaskListOrder::normalize(const QStringList &userOrder) {
  QStringList normalized;
  normalized.reserve(userOrder.size());
  QSet<QString> seen;
  seen.reserve(userOrder.size());
  for (const QString &id : userOrder) {
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty()
        || trimmed.size() > kMaxIdLength
        || seen.contains(trimmed)) {
      continue;
    }
    if (normalized.size() >= int(kMaxWindowFacts)) {
      break;
    }
    seen.insert(trimmed);
    normalized.append(trimmed);
  }
  return normalized;
}

void TaskListOrder::applyOverlay(const QStringList &userOrder,
                                 QVector<TaskEntry> &entries,
                                 QVector<TaskEntryIdentity> &identities) {
  if (userOrder.isEmpty() || entries.isEmpty()) {
    // Still re-number identities: callers rely on keyboardIndex matching the
    // displayed order even when no overlay is stored.
    for (int index = 0; index < identities.size(); ++index) {
      identities[index].keyboardIndex = index + 1;
    }
    return;
  }

  QHash<QString, int> positionById;
  positionById.reserve(entries.size());
  for (int index = 0; index < entries.size(); ++index) {
    positionById.insert(entries.at(index).taskId, index);
  }

  QVector<int> permutation;
  permutation.reserve(entries.size());
  QVector<bool> placed(entries.size(), false);
  for (const QString &id : userOrder) {
    const auto it = positionById.constFind(id);
    if (it == positionById.constEnd() || placed.at(it.value())) {
      continue;
    }
    placed[it.value()] = true;
    permutation.append(it.value());
  }
  for (int index = 0; index < entries.size(); ++index) {
    if (!placed.at(index)) {
      permutation.append(index);
    }
  }

  QVector<TaskEntry> orderedEntries;
  QVector<TaskEntryIdentity> orderedIdentities;
  orderedEntries.reserve(entries.size());
  orderedIdentities.reserve(identities.size());
  for (int rank = 0; rank < permutation.size(); ++rank) {
    const int source = permutation.at(rank);
    orderedEntries.append(entries.at(source));
    if (source < identities.size()) {
      TaskEntryIdentity identity = identities.at(source);
      identity.keyboardIndex = rank + 1;
      orderedIdentities.append(identity);
    }
  }
  // A malformed generation could present mismatched vectors; keep the
  // contract "parallel to entries" even then.
  while (orderedIdentities.size() < orderedEntries.size()) {
    orderedIdentities.append(TaskEntryIdentity{});
  }
  entries = std::move(orderedEntries);
  identities = std::move(orderedIdentities);
}

} // namespace QindaQt::ShellTaskList
