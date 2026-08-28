// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_state.h"

#include <utility>

namespace QindaQt::Apps::TextEditor {

const QString &DocumentState::path() const { return m_path; }
const QString &DocumentState::text() const { return m_text; }
bool DocumentState::hasUtf8Bom() const { return m_hasUtf8Bom; }
LineEnding DocumentState::lineEnding() const { return m_lineEnding; }
bool DocumentState::isDirty() const { return m_dirty; }
bool DocumentState::isUntitled() const { return m_path.isEmpty(); }
const std::optional<FileRevision> &DocumentState::baselineRevision() const {
  return m_baselineRevision;
}
ExternalState DocumentState::externalState() const { return m_externalState; }

void DocumentState::reset() {
  m_path.clear();
  m_text.clear();
  m_savedText.clear();
  m_dirty = false;
  m_hasUtf8Bom = false;
  m_lineEnding = LineEnding::Lf;
  m_baselineRevision.reset();
  m_externalState = ExternalState::InSync;
}

void DocumentState::load(QString absolutePath, DocumentSnapshot snapshot) {
  m_path = std::move(absolutePath);
  m_text = std::move(snapshot.text);
  m_savedText = m_text;
  m_dirty = false;
  m_hasUtf8Bom = snapshot.hasUtf8Bom;
  m_lineEnding = snapshot.lineEnding;
  m_baselineRevision = std::move(snapshot.revision);
  m_externalState = ExternalState::InSync;
}

bool DocumentState::setText(QString text) {
  if (m_text == text) {
    return false;
  }
  m_text = std::move(text);
  refreshDirty();
  return true;
}

bool DocumentState::applyTextEdit(const qsizetype position,
                                  const qsizetype charsRemoved,
                                  QString insertedText) {
  if (position < 0 || charsRemoved < 0 || position > m_text.size() ||
      charsRemoved > m_text.size() - position ||
      (charsRemoved == 0 && insertedText.isEmpty())) {
    return false;
  }

  // AGENT-GUARD: QTextDocument change offsets and QString indexes are both
  // UTF-16 code-unit offsets. Keep this incremental path so a keystroke in a
  // large document does not materialize and compare the complete widget text.
  m_text.replace(position, charsRemoved, insertedText);
  refreshDirty();
  return true;
}

void DocumentState::markSaved(QString absolutePath, FileRevision revision) {
  m_path = std::move(absolutePath);
  m_savedText = m_text;
  m_dirty = false;
  m_baselineRevision = std::move(revision);
  m_externalState = ExternalState::InSync;
}

bool DocumentState::setExternalState(const ExternalState state) {
  if (m_externalState == state) {
    return false;
  }
  m_externalState = state;
  return true;
}

void DocumentState::refreshDirty() {
  // A length mismatch proves inequality without scanning the full saved
  // baseline. Equal-length edits still receive exact dirty truth.
  m_dirty = m_text.size() != m_savedText.size() || m_text != m_savedText;
}

} // namespace QindaQt::Apps::TextEditor
