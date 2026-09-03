// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <functional>

namespace QindaQt::Apps::TextEditor {

using DocumentStoreFactory = std::function<DocumentStorePtr()>;

struct AddDocumentResult final {
  DocumentController *controller = nullptr;
  int index = -1;
  bool focusedExisting = false;
  DocumentOperation operation;

  [[nodiscard]] bool ok() const { return controller != nullptr; }
};

// AGENT-CONTRACT: The collection owns every controller in one window and is
// the only authority allowed to add, remove, or rename an open document. This
// keeps canonical paths unique while every controller retains its independent
// state, byte revision, watcher, and store. The injected factory is called once
// per new document and must return a fresh non-null store.
class DocumentCollection final : public QObject {
  Q_OBJECT

public:
  static constexpr int maximumDocuments = 32;

  explicit DocumentCollection(DocumentStoreFactory storeFactory,
                              QObject *parent = nullptr);

  [[nodiscard]] AddDocumentResult addUntitled();
  [[nodiscard]] AddDocumentResult openPath(const QString &path);
  [[nodiscard]] DocumentOperation saveAs(int index, const QString &path,
                                         bool replaceExisting);
  [[nodiscard]] bool removeAt(int index);

  [[nodiscard]] int count() const;
  [[nodiscard]] DocumentController *at(int index) const;
  [[nodiscard]] int indexOf(const DocumentController *controller) const;
  [[nodiscard]] int indexOfPath(const QString &path) const;
  [[nodiscard]] QStringList openPaths() const;

signals:
  void documentAdded(QindaQt::Apps::TextEditor::DocumentController *controller,
                     int index);
  void documentAboutToRemove(
      QindaQt::Apps::TextEditor::DocumentController *controller, int index);
  void documentsChanged();

private:
  [[nodiscard]] AddDocumentResult capacityFailure() const;
  [[nodiscard]] DocumentStorePtr createStore() const;

  DocumentStoreFactory m_storeFactory;
  QList<DocumentController *> m_documents;
};

} // namespace QindaQt::Apps::TextEditor
