// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.h"

#include <QList>
#include <QString>

namespace QindaQt::Apps::TextEditor {

enum class CloseChoice { SaveAll, DiscardAll, Cancel };

struct ClosePlan final {
  bool proceed = false;
  QList<int> saveIndexes;
  QList<int> discardIndexes;
};

[[nodiscard]] ClosePlan makeClosePlan(const QList<DocumentController *> &docs,
                                      CloseChoice choice);
[[nodiscard]] QString
boundedDirtyDocumentSummary(const QList<DocumentController *> &docs);

} // namespace QindaQt::Apps::TextEditor
