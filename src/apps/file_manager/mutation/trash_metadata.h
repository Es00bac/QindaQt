// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_types.h"
#include <QByteArray>
#include <QDateTime>

namespace QindaQt::Apps::FileManager {

// Pure worker-thread values, not mount/storage authority. The admitting
// collaborator independently pins and verifies these directories per action.
struct TrashLocation final {
  QString root;
  QString topDirectory;
  bool home = false;
};
struct TrashMetadata final {
  QString originalPath;
  QDateTime deletionDate;
};

// Freedesktop Trash1.0, ADR0363. Value-owned bounded parser/encoder: no I/O,
// no QObject affinity, no exceptions. Invalid/unrepresentable native filenames
// refuse rather than replacing bytes; callers retain payload/source on failure.
class TrashMetadataCodec final {
public:
  static constexpr qsizetype maximumBytes = 4096;
  [[nodiscard]] static MutationResult encode(
      const QString &originalPath, const TrashLocation &location,
      const QDateTime &deletionDate, QByteArray &destination);
  [[nodiscard]] static MutationResult decode(
      const QByteArray &bytes, const TrashLocation &location,
      TrashMetadata &destination);
};

} // namespace QindaQt::Apps::FileManager
