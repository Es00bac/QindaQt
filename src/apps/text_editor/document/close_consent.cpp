// SPDX-License-Identifier: GPL-3.0-or-later
#include "close_consent.h"

#include "ui/document_title.h"

#include <QFileInfo>

namespace QindaQt::Apps::TextEditor {

ClosePlan makeClosePlan(const QList<DocumentController *> &docs,
                        const CloseChoice choice) {
  ClosePlan plan;
  if (choice == CloseChoice::Cancel) {
    return plan;
  }
  plan.proceed = true;
  for (int index = 0; index < docs.size(); ++index) {
    if (!docs.at(index) || !docs.at(index)->state().isDirty()) {
      continue;
    }
    if (choice == CloseChoice::SaveAll) {
      plan.saveIndexes.append(index);
    } else {
      plan.discardIndexes.append(index);
    }
  }
  return plan;
}

QString boundedDirtyDocumentSummary(const QList<DocumentController *> &docs) {
  constexpr int maximumListed = 8;
  constexpr int maximumLength = 512;
  QStringList names;
  int dirtyCount = 0;
  for (const DocumentController *controller : docs) {
    if (!controller || !controller->state().isDirty()) {
      continue;
    }
    ++dirtyCount;
    if (names.size() < maximumListed) {
      const QString raw =
          controller->state().isUntitled()
              ? QStringLiteral("Untitled")
              : QFileInfo(controller->state().path()).fileName();
      const QString title = sanitizeDocumentTitle(raw);
      names.append(title.isEmpty() ? QStringLiteral("Untitled") : title);
    }
  }
  if (dirtyCount > maximumListed) {
    names.append(QStringLiteral("and %1 more").arg(dirtyCount - maximumListed));
  }
  return names.join(QStringLiteral(", ")).left(maximumLength);
}

} // namespace QindaQt::Apps::TextEditor
