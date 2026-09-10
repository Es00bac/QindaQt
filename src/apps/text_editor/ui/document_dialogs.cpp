// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_dialogs.h"
#include <QMessageBox>
#include <QPushButton>

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
RecoveryDecision NativeDocumentDialogs::recoverUnsaved(const QString &displayName,
                                                       bool untitled) {
  QMessageBox box(QMessageBox::Question, QObject::tr("Recover unsaved changes?"),
                  untitled
                      ? QObject::tr("QindaQt Text Editor kept unsaved text from an "
                                    "untitled document that did not close cleanly. "
                                    "Restore it or discard it?")
                      : QObject::tr("QindaQt Text Editor kept unsaved changes to "
                                    "\"%1\" from a session that did not close "
                                    "cleanly. Restore them or discard them?")
                            .arg(displayName),
                  QMessageBox::NoButton, m_parent);
  auto *restore = box.addButton(QObject::tr("Restore"), QMessageBox::AcceptRole);
  box.addButton(QObject::tr("Discard"), QMessageBox::DestructiveRole);
  // Restore is non-destructive (disk stays untouched); it is the default so
  // Enter never discards recovery data.
  box.setDefaultButton(qobject_cast<QPushButton *>(restore));
  box.exec();
  return box.clickedButton() == restore ? RecoveryDecision::Restore
                                        : RecoveryDecision::Discard;
}
} // namespace QindaQt::Apps::TextEditor
