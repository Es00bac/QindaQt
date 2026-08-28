// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

namespace QindaQt::ShellLauncher {

enum class PinError {
  None,
  UnknownId,
  AlreadyPinned,
  NotPinned,
  LimitReached,
};

// Ordered pinned-entry identities. AGENT-GUARD: This model is session memory
// only. It must never grow persistence, defaults, or Settings access; a
// durable pinned list is a later Settings1-backed boundary that will consume
// this model, not live inside it. Keeping it pure is what makes the hostile
// reorder/bounds tests exhaustive.
class PinnedApplications {
public:
  const QVector<QString> &ids() const { return m_ids; }
  bool contains(const QString &entryId) const;
  bool isFull() const;

  PinError pin(const QString &entryId);
  PinError unpin(const QString &entryId);
  PinError moveUp(const QString &entryId);
  PinError moveDown(const QString &entryId);

private:
  QVector<QString> m_ids;
};

enum class RecentError {
  None,
  InvalidId,
};

// Bounded most-recently-used entry identities. Recording an existing id moves
// it to the front; the oldest id is evicted beyond the bound, so recording
// never fails for capacity reasons.
class RecentApplications {
public:
  const QVector<QString> &ids() const { return m_ids; }

  RecentError record(const QString &entryId);
  void clear();

private:
  QVector<QString> m_ids;
};

} // namespace QindaQt::ShellLauncher
