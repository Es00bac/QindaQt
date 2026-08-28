// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_types.h"

#include <memory>

namespace QindaQt::Apps::TextEditor {

// AGENT-CONTRACT: The controller owns one store for its full lifetime and calls
// it only on the GUI thread. Implementations never display UI or mutate a
// DocumentState. Every failure is returned as a bounded value; no exception or
// partial document is published across this boundary.
class DocumentStore {
public:
  virtual ~DocumentStore() = default;

  [[nodiscard]] virtual LoadResult load(const QString &absolutePath) const = 0;
  [[nodiscard]] virtual RevisionResult
  revision(const QString &absolutePath) const = 0;
  [[nodiscard]] virtual SaveResult
  saveAtomic(const SaveRequest &request) const = 0;
};

using DocumentStorePtr = std::unique_ptr<DocumentStore>;

} // namespace QindaQt::Apps::TextEditor
