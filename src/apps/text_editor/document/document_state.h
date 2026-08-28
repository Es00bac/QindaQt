// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_types.h"

namespace QindaQt::Apps::TextEditor {

// This value owns only document policy. It performs no I/O, starts no watcher,
// and displays no UI, so tests and later front ends can reason about dirty and
// external-change truth without constructing a window.
class DocumentState final {
public:
  [[nodiscard]] const QString &path() const;
  [[nodiscard]] const QString &text() const;
  [[nodiscard]] bool hasUtf8Bom() const;
  [[nodiscard]] LineEnding lineEnding() const;
  [[nodiscard]] bool isDirty() const;
  [[nodiscard]] bool isUntitled() const;
  [[nodiscard]] const std::optional<FileRevision> &baselineRevision() const;
  [[nodiscard]] ExternalState externalState() const;

  void reset();
  void load(QString absolutePath, DocumentSnapshot snapshot);
  [[nodiscard]] bool setText(QString text);
  [[nodiscard]] bool applyTextEdit(qsizetype position, qsizetype charsRemoved,
                                   QString insertedText);
  void markSaved(QString absolutePath, FileRevision revision);
  [[nodiscard]] bool setExternalState(ExternalState state);

private:
  void refreshDirty();

  QString m_path;
  QString m_text;
  QString m_savedText;
  bool m_dirty = false;
  bool m_hasUtf8Bom = false;
  LineEnding m_lineEnding = LineEnding::Lf;
  std::optional<FileRevision> m_baselineRevision;
  ExternalState m_externalState = ExternalState::InSync;
};

} // namespace QindaQt::Apps::TextEditor
