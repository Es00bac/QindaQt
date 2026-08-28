// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_document_store.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringConverter>

#include <utility>

namespace QindaQt::Apps::TextEditor {
namespace {

constexpr auto utf8Bom = "\xEF\xBB\xBF";

[[nodiscard]] bool isAbsoluteUsablePath(const QString &path) {
  return !path.isEmpty() && QFileInfo(path).isAbsolute();
}

[[nodiscard]] SaveResult saveError(const DocumentError error,
                                   QString diagnostic) {
  return {.revision = std::nullopt,
          .error = error,
          .diagnostic = std::move(diagnostic)};
}

[[nodiscard]] LineEnding detectLineEnding(const QString &text) {
  qsizetype lfCount = 0;
  qsizetype crlfCount = 0;
  qsizetype crCount = 0;
  for (qsizetype index = 0; index < text.size(); ++index) {
    if (text.at(index) == u'\r') {
      if (index + 1 < text.size() && text.at(index + 1) == u'\n') {
        ++crlfCount;
        ++index;
      } else {
        ++crCount;
      }
    } else if (text.at(index) == u'\n') {
      ++lfCount;
    }
  }
  if (crlfCount >= lfCount && crlfCount >= crCount && crlfCount > 0) {
    return LineEnding::CrLf;
  }
  if (crCount > lfCount && crCount > 0) {
    return LineEnding::Cr;
  }
  return LineEnding::Lf;
}

[[nodiscard]] QString normalizeLineEndings(QString text) {
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(u'\r', u'\n');
  return text;
}

[[nodiscard]] QString serializeLineEndings(QString text,
                                           const LineEnding lineEnding) {
  if (lineEnding == LineEnding::CrLf) {
    text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
  } else if (lineEnding == LineEnding::Cr) {
    text.replace(u'\n', u'\r');
  }
  return text;
}

} // namespace

LocalDocumentStore::BytesResult
LocalDocumentStore::readBytes(const QString &absolutePath) {
  if (!isAbsoluteUsablePath(absolutePath)) {
    return {{},
            DocumentError::InvalidPath,
            QStringLiteral("The path must be absolute")};
  }

  const QFileInfo info(absolutePath);
  if (!info.exists()) {
    return {
        {}, DocumentError::NotFound, QStringLiteral("The file does not exist")};
  }
  if (!info.isFile()) {
    return {{},
            DocumentError::NotRegularFile,
            QStringLiteral("The path is not a regular file")};
  }
  if (info.size() > maximumDocumentBytes) {
    return {{},
            DocumentError::TooLarge,
            QStringLiteral("The file exceeds the 32 MiB editor limit")};
  }

  QFile file(absolutePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return {{}, DocumentError::ReadFailed, file.errorString()};
  }
  QByteArray bytes = file.read(maximumDocumentBytes + 1);
  if (file.error() != QFileDevice::NoError) {
    return {{}, DocumentError::ReadFailed, file.errorString()};
  }
  if (bytes.size() > maximumDocumentBytes) {
    return {{},
            DocumentError::TooLarge,
            QStringLiteral("The file exceeds the 32 MiB editor limit")};
  }
  return {std::move(bytes), DocumentError::None, {}};
}

FileRevision LocalDocumentStore::revisionFor(const QByteArray &bytes) {
  return {.exists = true,
          .byteCount = bytes.size(),
          .sha256 =
              QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)};
}

LoadResult LocalDocumentStore::load(const QString &absolutePath) const {
  const BytesResult loaded = readBytes(absolutePath);
  if (!loaded.ok()) {
    return {.snapshot = std::nullopt,
            .error = loaded.error,
            .diagnostic = loaded.diagnostic};
  }

  QByteArray payload = loaded.bytes;
  const bool hasBom = payload.startsWith(utf8Bom);
  if (hasBom) {
    payload.remove(0, 3);
  }
  QStringDecoder decoder(QStringDecoder::Utf8);
  const QString decodedText = decoder.decode(payload);
  if (decoder.hasError()) {
    return {.snapshot = std::nullopt,
            .error = DocumentError::InvalidUtf8,
            .diagnostic = QStringLiteral("The file is not valid UTF-8")};
  }

  const LineEnding lineEnding = detectLineEnding(decodedText);
  return {.snapshot =
              DocumentSnapshot{.text = normalizeLineEndings(decodedText),
                               .hasUtf8Bom = hasBom,
                               .lineEnding = lineEnding,
                               .revision = revisionFor(loaded.bytes)},
          .error = DocumentError::None,
          .diagnostic = {}};
}

RevisionResult LocalDocumentStore::revision(const QString &absolutePath) const {
  const BytesResult loaded = readBytes(absolutePath);
  if (!loaded.ok()) {
    return {.revision = std::nullopt,
            .error = loaded.error,
            .diagnostic = loaded.diagnostic};
  }
  return {.revision = revisionFor(loaded.bytes),
          .error = DocumentError::None,
          .diagnostic = {}};
}

SaveResult LocalDocumentStore::saveAtomic(const SaveRequest &request) const {
  if (!isAbsoluteUsablePath(request.path)) {
    return saveError(DocumentError::InvalidPath,
                     QStringLiteral("The path must be absolute"));
  }
  if (QFileInfo(request.path).isSymLink()) {
    return saveError(
        DocumentError::NotRegularFile,
        QStringLiteral("A dangling symbolic link cannot be replaced"));
  }
  if (!request.text.isValidUtf16()) {
    return saveError(
        DocumentError::InvalidUtf8,
        QStringLiteral("The document contains an invalid Unicode sequence"));
  }

  QStringEncoder encoder(QStringEncoder::Utf8);
  QByteArray bytes =
      encoder.encode(serializeLineEndings(request.text, request.lineEnding));
  if (encoder.hasError()) {
    return saveError(DocumentError::InvalidUtf8,
                     QStringLiteral("The document cannot be encoded as UTF-8"));
  }
  if (request.includeUtf8Bom) {
    bytes.prepend(utf8Bom);
  }
  if (bytes.size() > maximumDocumentBytes) {
    return saveError(
        DocumentError::TooLarge,
        QStringLiteral("The document exceeds the 32 MiB editor limit"));
  }

  const RevisionResult current = revision(request.path);
  if (request.policy == SavePolicy::CreateOnly) {
    if (current.ok()) {
      return saveError(DocumentError::DestinationExists,
                       QStringLiteral("The destination already exists"));
    }
    if (current.error != DocumentError::NotFound) {
      return saveError(current.error, current.diagnostic);
    }
  } else if (request.policy == SavePolicy::MatchRevision) {
    // AGENT-GUARD: Never silently turn an unreadable or missing current
    // file into a successful save. The user's buffer is the only retained
    // copy after an external replacement, so the caller must choose Save As.
    if (!current.ok() || !request.expectedRevision.has_value() ||
        *current.revision != *request.expectedRevision) {
      return saveError(DocumentError::ExternalConflict,
                       QStringLiteral("The file changed outside the editor"));
    }
  } else if (!current.ok() && current.error != DocumentError::NotFound) {
    return saveError(current.error, current.diagnostic);
  }

  QSaveFile file(request.path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly)) {
    return saveError(DocumentError::WriteFailed, file.errorString());
  }
  if (file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    return saveError(DocumentError::WriteFailed, file.errorString());
  }
  if (!file.commit()) {
    return saveError(DocumentError::WriteFailed, file.errorString());
  }

  return {.revision = revisionFor(bytes),
          .error = DocumentError::None,
          .diagnostic = {}};
}

} // namespace QindaQt::Apps::TextEditor
