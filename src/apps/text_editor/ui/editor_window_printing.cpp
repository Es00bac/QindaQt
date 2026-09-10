// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"
#include "document_editor.h"

#include <QDialog>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTextDocument>

namespace QindaQt::Apps::TextEditor {

bool EditorWindow::renderDocumentForPrint(QPrinter *printer) {
  if (!printer || !m_document) {
    return false;
  }
  // AGENT-CONTRACT: Printing renders the authoritative plain text in the
  // document's base (unzoomed) font. Syntax colors are deliberately not
  // printed: palette-derived highlight ink is fitted to the on-screen canvas,
  // not to the printer's white page, and QTextDocument paginates the plain
  // content on its own.
  QTextDocument document;
  if (auto *documentEditor = static_cast<DocumentEditor *>(editor())) {
    document.setDefaultFont(documentEditor->baseFont());
  }
  document.setPlainText(m_document->state().text());
  document.print(printer);
  return true;
}

void EditorWindow::printDocument() {
  QPrinter printer(QPrinter::HighResolution);
  QPrintDialog dialog(&printer, this);
  dialog.setWindowTitle(tr("Print Document"));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  (void)renderDocumentForPrint(&printer);
}

void EditorWindow::printPreview() {
  QPrinter printer(QPrinter::HighResolution);
  QPrintPreviewDialog dialog(&printer, this);
  dialog.setWindowTitle(tr("Print Preview"));
  connect(&dialog, &QPrintPreviewDialog::paintRequested, this,
          [this](QPrinter *previewPrinter) {
            (void)renderDocumentForPrint(previewPrinter);
          });
  dialog.exec();
}

bool EditorWindow::printDocumentToPdfFile(const QString &outputPath) {
  if (outputPath.isEmpty()) {
    return false;
  }
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(outputPath);
  return renderDocumentForPrint(&printer);
}

} // namespace QindaQt::Apps::TextEditor
