// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <QtCore/QString>

namespace QindaQt::DisplayJournal {

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
  explicit FileJournalStore(QString userStateRoot);

  [[nodiscard]] LoadResult load() const;
  [[nodiscard]] bool store(const DisplayTransaction::Journal &journal) override;
  [[nodiscard]] bool clear() override;

  [[nodiscard]] const QString &userStateRoot() const noexcept;

private:
  QString m_userStateRoot;
};

} // namespace QindaQt::DisplayJournal
