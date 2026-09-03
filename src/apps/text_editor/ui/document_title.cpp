// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_title.h"

namespace QindaQt::Apps::TextEditor {

QString sanitizeDocumentTitle(const QString &rawTitle) {
  QString sanitized;
  sanitized.reserve(qMin(rawTitle.size(), maximumDocumentTitleLength + 1));
  bool pendingSpace = false;
  for (qsizetype index = 0; index < rawTitle.size(); ++index) {
    const QChar character = rawTitle.at(index);
    if (character.isSpace()) {
      pendingSpace = !sanitized.isEmpty();
      continue;
    }
    if (character.isHighSurrogate()) {
      if (index + 1 >= rawTitle.size() ||
          !rawTitle.at(index + 1).isLowSurrogate()) {
        continue;
      }
      if (pendingSpace && !sanitized.isEmpty()) {
        sanitized.append(QLatin1Char(' '));
      }
      pendingSpace = false;
      if (sanitized.size() + 2 > maximumDocumentTitleLength) {
        break;
      }
      sanitized.append(character);
      sanitized.append(rawTitle.at(++index));
      continue;
    }
    if (character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format ||
        character.isLowSurrogate()) {
      continue;
    }
    if (pendingSpace) {
      sanitized.append(QLatin1Char(' '));
      pendingSpace = false;
    }
    sanitized.append(character);
    if (sanitized.size() >= maximumDocumentTitleLength) {
      break;
    }
  }
  return sanitized.trimmed();
}

} // namespace QindaQt::Apps::TextEditor
