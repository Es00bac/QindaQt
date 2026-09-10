// SPDX-License-Identifier: GPL-3.0-or-later
// Foreign clipboard/DnD batch entry points for MutationController, split from
// mutation_controller.cpp to keep both files under the source-shape review
// budget. Same class, same module: these methods only resolve fresh source
// identities and forward to the private submitBatch contract.
#include "mutation_controller.h"

#include "local_mutation_backend.h"

#include <QFileInfo>

namespace QindaQt::Apps::FileManager {

bool MutationController::copyForeignPathsTo(const QStringList &sourcePaths,
                                            const QString &destinationDirectory) {
  return submitForeignBatch(MutationKind::Copy, sourcePaths, destinationDirectory);
}

bool MutationController::moveForeignPathsTo(const QStringList &sourcePaths,
                                            const QString &destinationDirectory) {
  return submitForeignBatch(MutationKind::Move, sourcePaths, destinationDirectory);
}

bool MutationController::submitForeignBatch(MutationKind kind,
                                            const QStringList &sourcePaths,
                                            const QString &destinationDirectory) {
  if (sourcePaths.isEmpty()) {
    fail(MutationError::InvalidRequest, QStringLiteral("No items are selected"));
    return false;
  }
  if (sourcePaths.size() > maximumForeignPaths) {
    fail(MutationError::InvalidRequest,
         QStringLiteral("Too many dropped or pasted items (limit %1)")
             .arg(maximumForeignPaths));
    return false;
  }
  QVariantList items;
  items.reserve(sourcePaths.size());
  for (const QString &source : sourcePaths) {
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isAbsolute()) {
      fail(MutationError::InvalidRequest,
           QStringLiteral("Only absolute local paths can be pasted or dropped"));
      return false;
    }
    // The identity snapshot is taken at dispatch time on the GUI thread; the
    // backend re-verifies it on the worker before mutating, so a source that
    // changes in between fails closed like any other identity mismatch.
    const std::optional<FileIdentity> identity =
        LocalMutationBackend::identityForPath(sourceInfo.absoluteFilePath());
    if (!identity) {
      fail(MutationError::InvalidRequest,
           QStringLiteral("A pasted or dropped item is no longer available"));
      return false;
    }
    items.append(QVariantMap{
        {QStringLiteral("path"), sourceInfo.absoluteFilePath()},
        {QStringLiteral("device"), QString::number(identity->device)},
        {QStringLiteral("inode"), QString::number(identity->inode)},
        {QStringLiteral("identitySize"), QString::number(identity->size)},
        {QStringLiteral("modifiedNanoseconds"),
         QString::number(identity->modifiedNanoseconds)},
        {QStringLiteral("mode"), QString::number(identity->mode)},
    });
  }
  return submitBatch(kind, items, destinationDirectory);
}

} // namespace QindaQt::Apps::FileManager
