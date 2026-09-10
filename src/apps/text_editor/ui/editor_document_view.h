// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document/document_controller.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace QindaQt::Apps::TextEditor {

// One document's presentation. The view owns one QTextDocument/undo stack and
// projects exactly one controller; it never chooses close/save consent or
// touches storage. Ordinary windows cannot exchange edit history or external
// change truth through this view. Appearance comes from the widget palette and
// style (ADR-0116); there is no per-application token or palette authority.
class EditorDocumentView final : public QWidget {
  Q_OBJECT

public:
  EditorDocumentView(DocumentController *controller, QWidget *parent = nullptr);

  [[nodiscard]] DocumentController *controller() const { return m_controller; }
  [[nodiscard]] QPlainTextEdit *editor() const { return m_editor; }
  [[nodiscard]] QString statusText() const;

signals:
  void presentationChanged();
  void reloadRequested();
  void saveAsRequested();

protected:
  void changeEvent(QEvent *event) override;

private:
  void connectState();
  void refreshExternalIcon();
  void updateExternalBanner(ExternalState state);
  void announceExternalState(ExternalState state);

  DocumentController *m_controller = nullptr;
  QPlainTextEdit *m_editor = nullptr;
  QWidget *m_externalBanner = nullptr;
  QLabel *m_externalIcon = nullptr;
  QLabel *m_externalLabel = nullptr;
  QPushButton *m_reloadButton = nullptr;
  QPushButton *m_saveAsButton = nullptr;
  ExternalState m_renderedExternalState = ExternalState::InSync;
  bool m_replacingContents = false;
};

} // namespace QindaQt::Apps::TextEditor
