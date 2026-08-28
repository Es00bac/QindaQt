// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_store.h"

namespace QindaQt::Apps::TextEditor {

class LocalDocumentStore final : public DocumentStore {
public:
  static constexpr qint64 maximumDocumentBytes = 32LL * 1024LL * 1024LL;

  [[nodiscard]] LoadResult load(const QString &absolutePath) const override;
  [[nodiscard]] RevisionResult
  revision(const QString &absolutePath) const override;
  [[nodiscard]] SaveResult
  saveAtomic(const SaveRequest &request) const override;

private:
  struct BytesResult final {
    QByteArray bytes;
    DocumentError error = DocumentError::None;
    QString diagnostic;

    [[nodiscard]] bool ok() const { return error == DocumentError::None; }
  };

  [[nodiscard]] static BytesResult readBytes(const QString &absolutePath);
  [[nodiscard]] static FileRevision revisionFor(const QByteArray &bytes);
};

} // namespace QindaQt::Apps::TextEditor
