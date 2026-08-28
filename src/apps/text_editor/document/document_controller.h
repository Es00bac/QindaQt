// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_state.h"
#include "document_store.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace QindaQt::Apps::TextEditor {

struct DocumentOperation final {
  DocumentError error = DocumentError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == DocumentError::None; }
};

// AGENT-CONTRACT: This GUI-thread QObject owns the store and watcher. It
// publishes complete state transitions and never prompts the user; overwrite,
// discard, and reload consent remain presentation decisions.
class DocumentController final : public QObject {
  Q_OBJECT

public:
  explicit DocumentController(DocumentStorePtr store,
                              QObject *parent = nullptr);

  [[nodiscard]] const DocumentState &state() const;
  void newDocument();
  [[nodiscard]] DocumentOperation openPath(const QString &path);
  void setText(const QString &text);
  void applyTextEdit(qsizetype position, qsizetype charsRemoved,
                     QString insertedText);
  [[nodiscard]] DocumentOperation save();
  [[nodiscard]] DocumentOperation saveAs(const QString &path,
                                         bool replaceExisting);
  void refreshExternalState();

signals:
  void contentsReplacementRequested(const QString &text);
  void stateChanged();
  // Emitted only when the published external state actually transitions.
  void externalStateChanged(QindaQt::Apps::TextEditor::ExternalState state);

private:
  [[nodiscard]] static QString normalizePath(const QString &path);
  [[nodiscard]] static DocumentOperation
  fromSaveResult(const SaveResult &result);
  void adoptSuccessfulSave(const QString &path, const FileRevision &revision);
  void rebuildWatch();
  void publishExternalState(ExternalState state);

  DocumentStorePtr m_store;
  DocumentState m_state;
  QFileSystemWatcher m_watcher;
  QTimer m_externalRefreshTimer;
};

} // namespace QindaQt::Apps::TextEditor
