// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <QtCore/QString>

#include <memory>

namespace QindaQt::DisplayJournal {

namespace Private {
class FileJournalHooks;
class FileJournalStoreTestAccess;
} // namespace Private

enum class LoadStatus {
  Absent,
  Loaded,
  Rejected,
};

struct LoadResult {
  LoadStatus status = LoadStatus::Rejected;
  DisplayTransaction::Journal journal;
  QString reasonCode;

  [[nodiscard]] bool loaded() const noexcept {
    return status == LoadStatus::Loaded;
  }
};

class FileJournalStore final : public DisplayWriter::JournalStore {
public:
  // The caller supplies an existing, user-owned state directory dedicated to
  // QindaQt. Construction performs no I/O and never consults HOME/XDG state.
  // Calls are synchronous and confined to the caller's thread. Failure never
  // follows symlinks or accepts a partial/non-canonical journal.
  // AGENT-CONTRACT: Unchanged is available only before rename/unlink;
  // DurabilityUncertain reports a committed pathname followed by a failed
  // directory barrier. Callers must never treat that third state as permission
  // for a forward compositor mutation.
  explicit FileJournalStore(QString userStateRoot);
  ~FileJournalStore() override;

  [[nodiscard]] LoadResult load() const;
  [[nodiscard]] DisplayTransaction::JournalMutationOutcome
  store(const DisplayTransaction::Journal &journal) override;
  [[nodiscard]] DisplayTransaction::JournalMutationOutcome clear() override;

  [[nodiscard]] const QString &userStateRoot() const noexcept;

private:
  friend class Private::FileJournalStoreTestAccess;
  FileJournalStore(QString userStateRoot,
                   std::shared_ptr<Private::FileJournalHooks> hooks);

  QString m_userStateRoot;
  std::shared_ptr<Private::FileJournalHooks> m_hooks;
};

} // namespace QindaQt::DisplayJournal
