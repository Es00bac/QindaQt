// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "editor_application.h"
#include "editor_document_view.h"

#include <QFileInfo>
#include <QPlainTextEdit>

namespace QindaQt::Apps::TextEditor {
namespace {

// Process-local untitled journal identity: one sequence number per window for
// the process lifetime; the store scopes the file by pid as well.
int nextUntitledSequence() {
  static int sequence = 0;
  return ++sequence;
}

} // namespace

EditorWindow::~EditorWindow() {
  // AGENT-GUARD: QSyntaxHighlighter detaches from the QTextDocument while the
  // view subtree is destroyed from ~QWidget, which flushes a pending
  // textChanged. The connection below names an EditorWindow member function,
  // and by then the dynamic type is already the base class, so delivery is a
  // fatal assert. Sever it here, while the body still runs and m_view lives.
  if (m_journalStore && m_view && m_view->editor()) {
    disconnect(m_view->editor(), &QPlainTextEdit::textChanged, this,
               &EditorWindow::scheduleRecoveryWrite);
  }
}

void EditorWindow::connectRecoveryJournal() {
  if (!m_journalStore) {
    return;
  }
  m_untitledJournalKey =
      RecoveryJournalStore::keyForUntitled(nextUntitledSequence());
  m_lastDirtyState = m_document->state().isDirty();
  m_recoveryDebounce.setSingleShot(true);
  m_recoveryDebounce.setInterval(2000);
  connect(&m_recoveryDebounce, &QTimer::timeout, this,
          &EditorWindow::writeRecoveryJournal);
  // AGENT-NOTE: writes follow the editor's textChanged, not the controller's
  // stateChanged: edits only emit stateChanged on dirty flips, while openPath
  // emits it unconditionally. Tracking a dirty transition here is what keeps
  // "clear on clean" from erasing a stale journal before consent is offered.
  connect(m_view->editor(), &QPlainTextEdit::textChanged, this,
          &EditorWindow::scheduleRecoveryWrite);
  connect(m_document, &DocumentController::stateChanged, this, [this] {
    const bool dirty = m_document->state().isDirty();
    if (m_lastDirtyState && !dirty) {
      // A real dirty-to-clean transition (successful save, reload, or
      // undo-to-clean): disk and memory agree again, so the journal retires.
      clearRecoveryJournal();
    }
    m_lastDirtyState = dirty;
  });
}

QString EditorWindow::currentRecoveryKey() const {
  if (m_document->state().isUntitled()) {
    return m_untitledJournalKey;
  }
  return RecoveryJournalStore::keyForPath(m_document->state().path());
}

void EditorWindow::scheduleRecoveryWrite() {
  if (!m_journalStore || m_recoveryDenied ||
      !m_document->state().isDirty()) {
    return;
  }
  if (!m_journalWritten) {
    // The first dirty transition journals immediately so a crash right after
    // an edit is still covered; subsequent churn is debounced.
    writeRecoveryJournal();
    return;
  }
  m_recoveryDebounce.start();
}

void EditorWindow::writeRecoveryJournal() {
  if (!m_journalStore || m_recoveryDenied || m_journalOversize ||
      !m_document->state().isDirty()) {
    return;
  }
  const auto &state = m_document->state();
  const auto result = m_journalStore->store(
      currentRecoveryKey(), state.isUntitled() ? QString() : state.path(),
      state.text());
  if (result.ok()) {
    m_journalWritten = true;
  } else if (result.error == RecoveryJournalError::TooLarge) {
    // Do not re-encode an oversized document on every keystroke; the stale
    // earlier journal (if any) still offers the last bounded state.
    m_journalOversize = true;
  } else {
    announceStatus(tr("Could not update the recovery journal: %1")
                       .arg(result.diagnostic));
  }
}

void EditorWindow::clearRecoveryJournal() {
  if (!m_journalStore) {
    return;
  }
  m_recoveryDebounce.stop();
  // Both identities are cleared: a Save As moves an untitled journal onto the
  // path key, and a stale untitled file must not outlive the transition.
  if (!m_untitledJournalKey.isEmpty()) {
    (void)m_journalStore->clear(m_untitledJournalKey);
  }
  if (!m_document->state().isUntitled()) {
    (void)m_journalStore->clear(
        RecoveryJournalStore::keyForPath(m_document->state().path()));
  }
  m_journalWritten = false;
  m_journalOversize = false;
}

void EditorWindow::offerRecoveryIfPresent() {
  if (!m_journalStore || m_recoveryDenied) {
    return;
  }
  const auto loaded = m_journalStore->load(currentRecoveryKey());
  if (!loaded.ok()) {
    return;
  }
  if (loaded.entry->text == m_document->state().text()) {
    // The journal merely duplicates the disk content (for example a crash
    // between save and clear); no consent is needed to retire it.
    clearRecoveryJournal();
    return;
  }
  recoveryConsentFor(*loaded.entry);
}

void EditorWindow::recoveryConsentFor(const RecoveryJournalEntry &entry) {
  const auto &state = m_document->state();
  const QString displayName =
      state.isUntitled() ? tr("Untitled")
                         : QFileInfo(state.path()).fileName();
  // AGENT-GUARD: Recovery is never silent. The user must explicitly choose
  // Restore or Discard; a Discard also ends journaling for this window so the
  // same content is not re-offered on the next launch.
  if (m_dialogs->recoverUnsaved(displayName, state.isUntitled()) ==
      RecoveryDecision::Restore) {
    // The journal already holds exactly this content; the debounce path
    // rewrites it only if the user keeps editing.
    m_journalWritten = true;
    m_document->restoreRecoveredText(entry.text);
  } else {
    m_recoveryDenied = true;
    clearRecoveryJournal();
  }
  updateDocumentPresentation();
}

bool EditorWindow::offerUntitledRecovery(const RecoveryJournalEntry &entry) {
  if (!m_journalStore || entry.path.has_value()) {
    return false;
  }
  if (m_dialogs->recoverUnsaved(tr("Untitled"), true) ==
      RecoveryDecision::Restore) {
    // The dirty transition re-journals immediately under this window's own
    // key before the orphan file is retired, so no crash window exists where
    // the content lives in neither journal.
    m_journalWritten = false;
    m_document->restoreRecoveredText(entry.text);
    (void)m_journalStore->clear(entry.key);
    updateDocumentPresentation();
    return true;
  }
  (void)m_journalStore->clear(entry.key);
  return false;
}

} // namespace QindaQt::Apps::TextEditor
