// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_dialogs.h"
#include <QMessageBox>

namespace QindaQt::Apps::TextEditor {
NativeDocumentDialogs::NativeDocumentDialogs(QWidget *parent)
    : m_parent(parent) {}
DocumentCloseDecision NativeDocumentDialogs::confirmClose() {
  const auto choice = QMessageBox::warning(
      m_parent, QObject::tr("Unsaved changes"),
      QObject::tr("Save this document before closing it?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (choice == QMessageBox::Save)
    return DocumentCloseDecision::Save;
  if (choice == QMessageBox::Discard)
    return DocumentCloseDecision::Discard;
  return DocumentCloseDecision::Cancel;
}
bool NativeDocumentDialogs::confirmReplace() {
  return QMessageBox::question(
             m_parent, QObject::tr("Replace existing file?"),
             QObject::tr(
                 "A file with this name already exists. Replace its contents?"),
             QMessageBox::Yes | QMessageBox::No,
             QMessageBox::No) == QMessageBox::Yes;
}
void NativeDocumentDialogs::operationError(const QString &title,
                                           const QString &message) {
  QMessageBox::critical(m_parent, title, message);
}
} // namespace QindaQt::Apps::TextEditor
