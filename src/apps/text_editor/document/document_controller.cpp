// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

namespace QindaQt::Apps::TextEditor {

DocumentController::DocumentController(DocumentStorePtr store, QObject *parent)
    : QObject(parent), m_store(std::move(store)) {
  // AGENT-GUARD: A null store is a composition bug, not a recoverable user
  // error. All runtime I/O failures must instead cross DocumentStore values.
  Q_ASSERT(m_store);
  m_externalRefreshTimer.setSingleShot(true);
  m_externalRefreshTimer.setInterval(120);
  connect(&m_externalRefreshTimer, &QTimer::timeout, this,
          &DocumentController::refreshExternalState);
  const auto scheduleRefresh = [this] { m_externalRefreshTimer.start(); };
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, scheduleRefresh);
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
          scheduleRefresh);
}

const DocumentState &DocumentController::state() const { return m_state; }

QString DocumentController::normalizePath(const QString &path) {
  if (path.trimmed().isEmpty()) {
    return {};
  }
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath()
                                             : canonical);
}

void DocumentController::newDocument() {
  const ExternalState previousExternalState = m_state.externalState();
  m_externalRefreshTimer.stop();
  m_watcher.removePaths(m_watcher.files());
  m_watcher.removePaths(m_watcher.directories());
  m_state.reset();
  emit contentsReplacementRequested({});
  emit stateChanged();
  if (previousExternalState != ExternalState::InSync) {
    emit externalStateChanged(ExternalState::InSync);
  }
}

DocumentOperation DocumentController::openPath(const QString &path) {
  const QString absolutePath = normalizePath(path);
  if (absolutePath.isEmpty()) {
    return {.error = DocumentError::InvalidPath,
            .diagnostic = QStringLiteral("Choose a local file")};
  }
  const LoadResult loaded = m_store->load(absolutePath);
  if (!loaded.ok()) {
    return {.error = loaded.error, .diagnostic = loaded.diagnostic};
  }

  const ExternalState previousExternalState = m_state.externalState();
  m_state.load(absolutePath, *loaded.snapshot);
  rebuildWatch();
  emit contentsReplacementRequested(m_state.text());
  emit stateChanged();
  if (previousExternalState != ExternalState::InSync) {
    emit externalStateChanged(ExternalState::InSync);
  }
  return {};
}

void DocumentController::applyTextEdit(const qsizetype position,
                                       const qsizetype charsRemoved,
                                       QString insertedText) {
  const bool oldDirty = m_state.isDirty();
  if (!m_state.applyTextEdit(position, charsRemoved, std::move(insertedText))) {
    return;
  }
  if (oldDirty != m_state.isDirty()) {
    emit stateChanged();
  }
}

void DocumentController::setText(const QString &text) {
  const bool oldDirty = m_state.isDirty();
  if (!m_state.setText(text)) {
    return;
  }
  if (oldDirty != m_state.isDirty()) {
    emit stateChanged();
  }
}

DocumentOperation DocumentController::fromSaveResult(const SaveResult &result) {
  return {.error = result.error, .diagnostic = result.diagnostic};
}

DocumentOperation DocumentController::save() {
  if (m_state.isUntitled() || !m_state.baselineRevision().has_value()) {
    return {.error = DocumentError::InvalidPath,
            .diagnostic = QStringLiteral("Choose a file name with Save As")};
  }
  const SaveResult result = m_store->saveAtomic({
      .path = m_state.path(),
      .text = m_state.text(),
      .includeUtf8Bom = m_state.hasUtf8Bom(),
      .lineEnding = m_state.lineEnding(),
      .policy = SavePolicy::MatchRevision,
      .expectedRevision = m_state.baselineRevision(),
  });
  if (!result.ok()) {
    if (result.error == DocumentError::ExternalConflict) {
      refreshExternalState();
    }
    return fromSaveResult(result);
  }
  adoptSuccessfulSave(m_state.path(), *result.revision);
  return {};
}

DocumentOperation DocumentController::saveAs(const QString &path,
                                             const bool replaceExisting) {
  const QString absolutePath = normalizePath(path);
  if (absolutePath.isEmpty()) {
    return {.error = DocumentError::InvalidPath,
            .diagnostic = QStringLiteral("Choose a local file")};
  }
  const SaveResult result = m_store->saveAtomic({
      .path = absolutePath,
      .text = m_state.text(),
      .includeUtf8Bom = m_state.hasUtf8Bom(),
      .lineEnding = m_state.lineEnding(),
      .policy = replaceExisting ? SavePolicy::ReplaceExisting
                                : SavePolicy::CreateOnly,
      .expectedRevision = std::nullopt,
  });
  if (!result.ok()) {
    return fromSaveResult(result);
  }
  adoptSuccessfulSave(absolutePath, *result.revision);
  return {};
}

void DocumentController::adoptSuccessfulSave(const QString &path,
                                             const FileRevision &revision) {
  const ExternalState previousExternalState = m_state.externalState();
  m_state.markSaved(path, revision);
  rebuildWatch();
  emit stateChanged();
  if (previousExternalState != ExternalState::InSync) {
    emit externalStateChanged(ExternalState::InSync);
  }
}

void DocumentController::refreshExternalState() {
  if (m_state.isUntitled() || !m_state.baselineRevision().has_value()) {
    publishExternalState(ExternalState::InSync);
    return;
  }
  const RevisionResult current = m_store->revision(m_state.path());
  if (!current.ok()) {
    publishExternalState(current.error == DocumentError::NotFound
                             ? ExternalState::Missing
                             : ExternalState::Unreadable);
    rebuildWatch();
    return;
  }
  publishExternalState(*current.revision == *m_state.baselineRevision()
                           ? ExternalState::InSync
                           : ExternalState::Changed);
  rebuildWatch();
}

void DocumentController::rebuildWatch() {
  const QStringList files = m_watcher.files();
  if (!files.isEmpty()) {
    m_watcher.removePaths(files);
  }
  const QStringList directories = m_watcher.directories();
  if (!directories.isEmpty()) {
    m_watcher.removePaths(directories);
  }
  if (m_state.isUntitled()) {
    return;
  }

  const QFileInfo info(m_state.path());
  if (info.exists()) {
    m_watcher.addPath(m_state.path());
  }
  if (info.dir().exists()) {
    m_watcher.addPath(info.dir().absolutePath());
  }
}

void DocumentController::publishExternalState(const ExternalState state) {
  if (!m_state.setExternalState(state)) {
    return;
  }
  emit stateChanged();
  emit externalStateChanged(state);
}

} // namespace QindaQt::Apps::TextEditor
