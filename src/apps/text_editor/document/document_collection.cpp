// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_collection.h"

#include <utility>

namespace QindaQt::Apps::TextEditor {

DocumentCollection::DocumentCollection(DocumentStoreFactory storeFactory,
                                       QObject *parent)
    : QObject(parent), m_storeFactory(std::move(storeFactory)) {
  Q_ASSERT(m_storeFactory);
}

DocumentStorePtr DocumentCollection::createStore() const {
  return m_storeFactory ? m_storeFactory() : nullptr;
}

AddDocumentResult DocumentCollection::capacityFailure() const {
  return {.operation = {
              .error = DocumentError::CapacityExceeded,
              .diagnostic = QStringLiteral("Document limit reached (%1 documents)")
                                .arg(maximumDocuments),
          }};
}

AddDocumentResult DocumentCollection::addUntitled() {
  if (m_documents.size() >= maximumDocuments) {
    return capacityFailure();
  }
  DocumentStorePtr store = createStore();
  if (!store) {
    return {.operation = {
                .error = DocumentError::ReadFailed,
                .diagnostic = QStringLiteral("No document store is available"),
            }};
  }
  auto *controller = new DocumentController(std::move(store), this);
  m_documents.append(controller);
  const int index = static_cast<int>(m_documents.size()) - 1;
  emit documentAdded(controller, index);
  emit documentsChanged();
  return {.controller = controller, .index = index, .operation = {}};
}

AddDocumentResult DocumentCollection::openPath(const QString &path) {
  const QString normalized = DocumentController::normalizePath(path);
  if (normalized.isEmpty()) {
    return {.operation = {
                .error = DocumentError::InvalidPath,
                .diagnostic = QStringLiteral("Choose a local file"),
            }};
  }
  const int existing = indexOfPath(normalized);
  if (existing >= 0) {
    return {.controller = m_documents.at(existing),
            .index = existing,
            .focusedExisting = true,
            .operation = {}};
  }
  if (m_documents.size() >= maximumDocuments) {
    return capacityFailure();
  }
  DocumentStorePtr store = createStore();
  if (!store) {
    return {.operation = {
                .error = DocumentError::ReadFailed,
                .diagnostic = QStringLiteral("No document store is available"),
            }};
  }
  auto *controller = new DocumentController(std::move(store), this);
  const DocumentOperation opened = controller->openPath(normalized);
  if (!opened.ok()) {
    delete controller;
    return {.operation = opened};
  }
  m_documents.append(controller);
  const int index = static_cast<int>(m_documents.size()) - 1;
  emit documentAdded(controller, index);
  emit documentsChanged();
  return {.controller = controller, .index = index, .operation = {}};
}

DocumentOperation DocumentCollection::saveAs(const int index,
                                             const QString &path,
                                             const bool replaceExisting) {
  DocumentController *controller = at(index);
  const QString normalized = DocumentController::normalizePath(path);
  if (!controller || normalized.isEmpty()) {
    return {.error = DocumentError::InvalidPath,
            .diagnostic = QStringLiteral("Choose a local file")};
  }
  const int existing = indexOfPath(normalized);
  if (existing >= 0 && existing != index) {
    return {.error = DocumentError::AlreadyOpen,
            .diagnostic =
                QStringLiteral("That file is already open in another window")};
  }
  const DocumentOperation result =
      controller->saveAs(normalized, replaceExisting);
  if (result.ok()) {
    emit documentsChanged();
  }
  return result;
}

bool DocumentCollection::removeAt(const int index) {
  if (index < 0 || index >= static_cast<int>(m_documents.size())) {
    return false;
  }
  DocumentController *controller = m_documents.at(index);
  emit documentAboutToRemove(controller, index);
  m_documents.removeAt(index);
  controller->deleteLater();
  emit documentsChanged();
  return true;
}

int DocumentCollection::count() const {
  return static_cast<int>(m_documents.size());
}

DocumentController *DocumentCollection::at(const int index) const {
  return index >= 0 && index < static_cast<int>(m_documents.size())
             ? m_documents.at(index)
             : nullptr;
}

int DocumentCollection::indexOf(const DocumentController *controller) const {
  return static_cast<int>(
      m_documents.indexOf(const_cast<DocumentController *>(controller)));
}

int DocumentCollection::indexOfPath(const QString &path) const {
  const QString normalized = DocumentController::normalizePath(path);
  if (normalized.isEmpty()) {
    return -1;
  }
  for (int index = 0; index < static_cast<int>(m_documents.size()); ++index) {
    const DocumentState &state = m_documents.at(index)->state();
    if (!state.isUntitled() && state.path() == normalized) {
      return index;
    }
  }
  return -1;
}

QStringList DocumentCollection::openPaths() const {
  QStringList paths;
  for (const DocumentController *controller : m_documents) {
    if (!controller->state().isUntitled()) {
      paths.append(controller->state().path());
    }
  }
  return paths;
}

} // namespace QindaQt::Apps::TextEditor
