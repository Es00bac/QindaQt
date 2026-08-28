// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace QindaQt::Apps::TextEditor {

enum class DocumentError {
  None,
  InvalidPath,
  NotFound,
  NotRegularFile,
  TooLarge,
  InvalidUtf8,
  ReadFailed,
  WriteFailed,
  ExternalConflict,
  DestinationExists,
};

enum class LineEnding {
  Lf,
  CrLf,
  Cr,
};

struct FileRevision final {
  bool exists = false;
  qint64 byteCount = 0;
  QByteArray sha256;

  [[nodiscard]] bool operator==(const FileRevision &) const = default;
};

struct DocumentSnapshot final {
  QString text;
  bool hasUtf8Bom = false;
  LineEnding lineEnding = LineEnding::Lf;
  FileRevision revision;
};

struct LoadResult final {
  std::optional<DocumentSnapshot> snapshot;
  DocumentError error = DocumentError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return snapshot.has_value(); }
};

struct RevisionResult final {
  std::optional<FileRevision> revision;
  DocumentError error = DocumentError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return revision.has_value(); }
};

enum class SavePolicy {
  CreateOnly,
  MatchRevision,
  ReplaceExisting,
};

struct SaveRequest final {
  QString path;
  QString text;
  bool includeUtf8Bom = false;
  LineEnding lineEnding = LineEnding::Lf;
  SavePolicy policy = SavePolicy::CreateOnly;
  std::optional<FileRevision> expectedRevision;
};

struct SaveResult final {
  std::optional<FileRevision> revision;
  DocumentError error = DocumentError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return revision.has_value(); }
};

enum class ExternalState {
  InSync,
  Changed,
  Missing,
  Unreadable,
};

} // namespace QindaQt::Apps::TextEditor
