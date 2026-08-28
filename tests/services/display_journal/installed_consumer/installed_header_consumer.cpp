// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_journal/file_journal_store.h>

#include <memory>
#include <type_traits>

using namespace QindaQt::DisplayJournal;

static_assert(
    std::is_base_of_v<QindaQt::DisplayWriter::JournalStore, FileJournalStore>);

std::unique_ptr<QindaQt::DisplayWriter::JournalStore> installedJournalStore() {
  return std::make_unique<FileJournalStore>(
      QStringLiteral("/injected/state/root"));
}
