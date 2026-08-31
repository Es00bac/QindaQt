// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_journal/file_journal_store.h>

#include <memory>
#include <optional>

namespace QindaQt::DisplayJournal::Private {

// Test-only timing/failure seam. Production instances use the default no-op
// hooks; the installed API cannot inject or observe filesystem operations.
class FileJournalHooks {
public:
  virtual ~FileJournalHooks() = default;
  virtual void beforeOpenJournal() {}
  [[nodiscard]] virtual std::optional<bool> directorySyncResult() {
    return std::nullopt;
  }
};

class FileJournalStoreTestAccess {
public:
  [[nodiscard]] static std::unique_ptr<FileJournalStore>
  create(QString userStateRoot, std::shared_ptr<FileJournalHooks> hooks) {
    return std::unique_ptr<FileJournalStore>(
        new FileJournalStore(std::move(userStateRoot), std::move(hooks)));
  }
};

} // namespace QindaQt::DisplayJournal::Private
