// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_controller.h"

#include "local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QSet>

#include <limits>
#include <type_traits>

namespace QindaQt::Apps::FileManager {
namespace {

template <typename Integer>
[[nodiscard]] bool parseDecimalIdentityField(const QVariantMap &identity,
                                             const QString &key,
                                             Integer *output) {
  const QVariant field = identity.value(key);
  if (!field.isValid() || field.metaType().id() != QMetaType::QString) {
    return false;
  }
  bool ok = false;
  if constexpr (std::is_signed_v<Integer>) {
    const qlonglong parsed = field.toString().toLongLong(&ok, 10);
    if (ok) {
      *output = static_cast<Integer>(parsed);
    }
  } else {
    const qulonglong parsed = field.toString().toULongLong(&ok, 10);
    if (ok && parsed <= std::numeric_limits<Integer>::max()) {
      *output = static_cast<Integer>(parsed);
    } else {
      ok = false;
    }
  }
  return ok;
}

// AGENT-GUARD: an earlier item of a batch changes the time stamp of the
// folder it writes into, so the parent identity taken when the batch was
// requested would fail every later item bound for the same folder. Accept
// exactly that change -- the same device and inode, stat'ed afresh -- and
// never a different folder.
void acceptOwnWrites(MutationRequest &request, const QSet<QString> &written) {
  if (!request.expectedParent || request.destinationPath.isEmpty()) {
    return;
  }
  const QString folder = QDir::cleanPath(QFileInfo(request.destinationPath).absolutePath());
  const auto fresh = written.contains(folder)
      ? LocalMutationBackend::identityForPath(folder) : std::nullopt;
  if (fresh && fresh->device == request.expectedParent->device &&
      fresh->inode == request.expectedParent->inode) {
    request.expectedParent = fresh;
  }
}

void recordWrites(const MutationRequest &request, QSet<QString> &written) {
  for (const QString &path : {request.destinationPath, request.sourcePath}) {
    if (!path.isEmpty()) {
      written.insert(QDir::cleanPath(QFileInfo(path).absolutePath()));
    }
  }
}

} // namespace

MutationController::MutationController(MutationBackendPtr backend, QObject *parent)
    : QObject(parent), m_backend(std::move(backend)) {
  Q_ASSERT(m_backend);
  m_workerContext = new QObject;
  m_workerContext->moveToThread(&m_workerThread);
  connect(&m_workerThread, &QThread::finished, m_workerContext,
          &QObject::deleteLater);
  m_workerThread.setObjectName(QStringLiteral("qindaqt-file-mutation"));
  m_workerThread.start();
}

MutationController::~MutationController() {
  if (m_cancellation) {
    m_cancellation->store(true, std::memory_order_relaxed);
  }
  if (m_workerThread.isRunning()) {
    QMetaObject::invokeMethod(m_workerContext, []() {},
                              Qt::BlockingQueuedConnection);
    m_workerThread.quit();
    m_workerThread.wait();
  }
}

bool MutationController::busy() const { return m_busy; }
int MutationController::progressValue() const { return m_progressValue; }
QString MutationController::progressText() const { return m_progressText; }
QString MutationController::failureCode() const { return mutationErrorKey(m_failure); }
QString MutationController::failureMessage() const { return m_failureMessage; }
QString MutationController::resultText() const { return m_resultText; }
bool MutationController::canUndo() const { return !busy() && m_undoRequest != nullptr; }
bool MutationController::canRestore() const {
  return !busy() && !m_lastTrashToken.isEmpty() && m_lastTrashIdentity.has_value();
}

bool MutationController::createFolder(const QString &parentPath,
                                      const QString &name) {
  if (!validName(name)) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose a valid folder name"));
    return false;
  }
  MutationRequest request;
  request.kind = MutationKind::CreateFolder;
  request.destinationPath = QDir(parentPath).filePath(name);
  request.declaredRoots = {QFileInfo(parentPath).absoluteFilePath()};
  request.expectedParent = LocalMutationBackend::identityForPath(parentPath);
  return submit(std::move(request));
}

bool MutationController::renameItem(const QString &sourcePath,
                                    const QString &newName,
                                    const QVariantMap &identity) {
  if (!validName(newName)) {
    fail(MutationError::InvalidRequest, QStringLiteral("Choose a valid item name"));
    return false;
  }
  // AGENT-NOTE: P3-1 requires accepting the dialog's unchanged default name as
  // a true no-op. Do not dispatch it to the no-replace backend, where source
  // and destination necessarily describe the same existing item.
  if (newName == QFileInfo(sourcePath).fileName()) {
    return true;
  }
  MutationRequest request;
  request.kind = MutationKind::Rename;
  request.sourcePath = sourcePath;
  request.destinationPath = QDir(QFileInfo(sourcePath).absolutePath()).filePath(newName);
  request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
  request.expectedSource = identityFromMap(identity);
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(request.destinationPath).absolutePath());
  return submit(std::move(request));
}

bool MutationController::copyItem(const QString &sourcePath,
                                  const QString &destinationPath,
                                  const QVariantMap &identity) {
  if (!QFileInfo(destinationPath).isAbsolute()) {
    fail(MutationError::InvalidRequest,
         QStringLiteral("Choose an absolute local destination path"));
    return false;
  }
  MutationRequest request;
  request.kind = MutationKind::Copy;
  request.sourcePath = sourcePath;
  request.destinationPath = QFileInfo(destinationPath).absoluteFilePath();
  request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
  request.expectedSource = identityFromMap(identity);
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(request.destinationPath).absolutePath());
  return submit(std::move(request));
}

bool MutationController::moveItem(const QString &sourcePath,
                                  const QString &destinationPath,
                                  const QVariantMap &identity) {
  if (!QFileInfo(destinationPath).isAbsolute()) {
    fail(MutationError::InvalidRequest,
         QStringLiteral("Choose an absolute local destination path"));
    return false;
  }
  MutationRequest request;
  request.kind = MutationKind::Move;
  request.sourcePath = sourcePath;
  request.destinationPath = QFileInfo(destinationPath).absoluteFilePath();
  request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
  request.expectedSource = identityFromMap(identity);
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(request.destinationPath).absolutePath());
  return submit(std::move(request));
}

bool MutationController::trashItem(const QString &sourcePath,
                                   const QVariantMap &identity) {
  MutationRequest request;
  request.kind = MutationKind::Trash;
  request.sourcePath = sourcePath;
  request.declaredRoots = rootsFor(sourcePath);
  request.expectedSource = identityFromMap(identity);
  return submit(std::move(request));
}

bool MutationController::trashItems(const QVariantList &items) {
  return submitBatch(MutationKind::Trash, items);
}

bool MutationController::copyItemsTo(const QVariantList &items,
                                     const QString &destinationDirectory) {
  return submitBatch(MutationKind::Copy, items, destinationDirectory);
}

bool MutationController::moveItemsTo(const QVariantList &items,
                                     const QString &destinationDirectory) {
  return submitBatch(MutationKind::Move, items, destinationDirectory);
}

bool MutationController::restoreLast() {
  if (!canRestore()) {
    fail(MutationError::InvalidRequest, QStringLiteral("There is no recoverable Trash item"));
    return false;
  }
  MutationRequest request;
  request.kind = MutationKind::Restore;
  request.trashToken = m_lastTrashToken;
  request.destinationPath = m_lastTrashOriginalPath;
  request.declaredRoots = {QFileInfo(m_lastTrashOriginalPath).absolutePath()};
  request.expectedSource = m_lastTrashIdentity;
  request.expectedParent = LocalMutationBackend::identityForPath(
      QFileInfo(m_lastTrashOriginalPath).absolutePath());
  return submit(std::move(request));
}

bool MutationController::emptyTrash() {
  MutationRequest request;
  request.kind = MutationKind::EmptyTrash;
  request.declaredRoots = {QStringLiteral("/")};
  return submit(std::move(request));
}

bool MutationController::undo() {
  if (!canUndo()) {
    fail(MutationError::InvalidRequest, QStringLiteral("There is no recoverable operation"));
    return false;
  }
  const MutationRequest request = *m_undoRequest;
  m_undoRequest.reset();
  return submit(request, true);
}

void MutationController::cancel() {
  if (m_cancellation) {
    m_cancellation->store(true, std::memory_order_relaxed);
    m_progressText = QStringLiteral("Cancelling operation");
    emit stateChanged();
  }
}

void MutationController::clearFailure() {
  if (m_failure == MutationError::None) {
    return;
  }
  m_failure = MutationError::None;
  m_failureMessage.clear();
  emit stateChanged();
}

std::optional<FileIdentity>
MutationController::identityFromMap(const QVariantMap &identity) {
  FileIdentity value;
  const bool parsed =
      parseDecimalIdentityField(identity, QStringLiteral("device"),
                                &value.device) &&
      parseDecimalIdentityField(identity, QStringLiteral("inode"),
                                &value.inode) &&
      parseDecimalIdentityField(identity, QStringLiteral("identitySize"),
                                &value.size) &&
      parseDecimalIdentityField(identity,
                                QStringLiteral("modifiedNanoseconds"),
                                &value.modifiedNanoseconds) &&
      parseDecimalIdentityField(identity, QStringLiteral("mode"), &value.mode);
  return parsed && value.valid() ? std::optional<FileIdentity>{value}
                                 : std::nullopt;
}

bool MutationController::submit(MutationRequest request, bool isUndo) {
  if (busy()) {
    fail(MutationError::Busy, QStringLiteral("Another file operation is still running"));
    return false;
  }
  m_failure = MutationError::None;
  m_failureMessage.clear();
  m_resultText.clear();
  m_progressValue = 0;
  m_progressText = QStringLiteral("Starting file operation");
  m_isUndo = isUndo;
  m_runningKind = request.kind;
  m_cancellation = std::make_shared<std::atomic_bool>(false);
  MutationBackend *backend = m_backend.get();
  const MutationCancellation cancellation = m_cancellation;
  const QPointer<MutationController> guard(this);
  auto progress = [guard](const MutationProgress &update) {
    if (!guard) {
      return;
    }
    QMetaObject::invokeMethod(guard, [guard, update]() {
      if (!guard || !guard->busy()) {
        return;
      }
      guard->m_progressValue = update.totalItems > 0
          ? qBound(0, update.completedItems * 100 / update.totalItems, 100)
          : qMin(99, update.completedItems);
      guard->m_progressText = boundedMutationDiagnostic(update.accessibleText);
      emit guard->stateChanged();
    }, Qt::QueuedConnection);
  };
  m_busy = true;
  QMetaObject::invokeMethod(
      m_workerContext,
      [guard, backend, request = std::move(request), cancellation,
       progress = std::move(progress)]() mutable {
        const MutationResult result = backend->execute(request, cancellation, progress);
        if (guard) {
          QMetaObject::invokeMethod(guard, [guard, result]() {
            if (guard) {
              guard->finish(result);
            }
          }, Qt::QueuedConnection);
        }
      },
      Qt::QueuedConnection);
  emit stateChanged();
  return true;
}

bool MutationController::submitBatch(MutationKind kind,
                                     const QVariantList &items,
                                     const QString &destinationDirectory) {
  if (busy()) {
    fail(MutationError::Busy, QStringLiteral("Another file operation is still running"));
    return false;
  }
  if (items.isEmpty()) {
    fail(MutationError::InvalidRequest, QStringLiteral("No items are selected"));
    return false;
  }
  const bool needsDestination =
      kind == MutationKind::Copy || kind == MutationKind::Move;
  const QFileInfo destinationInfo(destinationDirectory);
  if (needsDestination && !destinationInfo.isAbsolute()) {
    fail(MutationError::InvalidRequest,
         QStringLiteral("Choose an absolute local destination path"));
    return false;
  }
  const QString destinationRoot =
      needsDestination ? destinationInfo.absoluteFilePath() : QString();

  QVector<MutationRequest> requests;
  requests.reserve(items.size());
  for (const QVariant &item : items) {
    QString source;
    FileIdentity identity;
    if (!parseItem(item, &source, &identity)) {
      return false;
    }
    MutationRequest request;
    request.kind = kind;
    request.sourcePath = source;
    if (needsDestination) {
      request.destinationPath =
          QDir(destinationRoot).filePath(QFileInfo(source).fileName());
    }
    request.declaredRoots = rootsFor(request.sourcePath, request.destinationPath);
    if (needsDestination) {
      request.declaredRoots.append(destinationRoot);
      request.declaredRoots.removeDuplicates();
    }
    request.expectedSource = identity;
    if (needsDestination) {
      request.expectedParent = LocalMutationBackend::identityForPath(destinationRoot);
    }
    requests.append(std::move(request));
  }
  return submitRequests(kind, std::move(requests));
}

bool MutationController::submitRequests(MutationKind kind,
                                        QVector<MutationRequest> requests) {
  if (busy()) {
    fail(MutationError::Busy, QStringLiteral("Another file operation is still running"));
    return false;
  }
  if (requests.isEmpty()) {
    fail(MutationError::InvalidRequest, QStringLiteral("No items are selected"));
    return false;
  }
  m_failure = MutationError::None;
  m_failureMessage.clear();
  m_resultText.clear();
  m_progressValue = 0;
  m_progressText = QStringLiteral("Starting file operation");
  m_isUndo = false;
  m_runningKind = kind;
  m_cancellation = std::make_shared<std::atomic_bool>(false);
  MutationBackend *backend = m_backend.get();
  const MutationCancellation cancellation = m_cancellation;
  const QPointer<MutationController> guard(this);
  const int total = static_cast<int>(requests.size());
  auto progress = [guard, total](const MutationProgress &update) {
    if (!guard) {
      return;
    }
    QMetaObject::invokeMethod(guard, [guard, update, total]() {
      if (!guard || !guard->busy()) {
        return;
      }
      guard->m_progressValue = qBound(0, update.completedItems * 100 / total, 100);
      guard->m_progressText = boundedMutationDiagnostic(update.accessibleText);
      emit guard->stateChanged();
    }, Qt::QueuedConnection);
  };
  m_busy = true;
  QMetaObject::invokeMethod(
      m_workerContext,
      [guard, backend, requests = std::move(requests), cancellation,
       progress = std::move(progress), total]() mutable {
        int completed = 0;
        MutationResult outcome;
        QSet<QString> written;
        for (MutationRequest &request : requests) {
          acceptOwnWrites(request, written);
          if (cancellation->load(std::memory_order_relaxed)) {
            outcome.error = MutationError::Cancelled;
            outcome.diagnostic =
                QStringLiteral("Cancelled after %1 of %2 items")
                    .arg(completed)
                    .arg(total);
            break;
          }
          const int itemIndex = completed;
          auto itemProgress = [&progress, itemIndex, total,
                               &request](const MutationProgress &update) {
            MutationProgress forwarded;
            forwarded.completedItems = itemIndex;
            forwarded.totalItems = total;
            forwarded.accessibleText =
                QStringLiteral("Item %1 of %2 (%3): %4")
                    .arg(itemIndex + 1)
                    .arg(total)
                    .arg(QFileInfo(request.sourcePath).fileName())
                    .arg(update.accessibleText);
            progress(forwarded);
          };
          outcome = backend->execute(request, cancellation, itemProgress);
          if (!outcome.ok()) {
            outcome.diagnostic =
                QStringLiteral("Completed %1 of %2 items; %3: %4")
                    .arg(completed)
                    .arg(total)
                    .arg(QFileInfo(request.sourcePath).fileName())
                    .arg(outcome.diagnostic);
            break;
          }
          recordWrites(request, written);
          ++completed;
        }
        if (outcome.ok()) {
          outcome.trashToken.clear();
          outcome.undoRequest.reset();
          outcome.outputIdentity.reset();
          outcome.diagnostic =
              QStringLiteral("Finished %1 items").arg(completed);
        }
        if (guard) {
          QMetaObject::invokeMethod(guard, [guard, outcome]() {
            if (guard) {
              guard->finish(outcome);
            }
          }, Qt::QueuedConnection);
        }
      },
      Qt::QueuedConnection);
  emit stateChanged();
  return true;
}

void MutationController::finish(const MutationResult &result) {
  m_busy = false;
  m_progressValue = result.ok() ? 100 : 0;
  m_progressText.clear();
  m_cancellation.reset();
  if (!result.ok()) {
    m_failure = result.error;
    m_failureMessage = boundedMutationDiagnostic(result.diagnostic);
    m_resultText.clear();
    m_isUndo = false;
    emit stateChanged();
    return;
  }
  m_failure = MutationError::None;
  m_failureMessage.clear();
  m_resultText = m_isUndo ? QStringLiteral("Operation undone")
                 : result.diagnostic.isEmpty()
                     ? QStringLiteral("File operation completed")
                     : boundedMutationDiagnostic(result.diagnostic);
  m_undoRequest = m_isUndo ? nullptr : result.undoRequest;
  m_isUndo = false;
  if (!result.trashToken.isEmpty() && result.outputIdentity) {
    m_lastTrashToken = result.trashToken;
    m_lastTrashOriginalPath = result.originalPath;
    m_lastTrashIdentity = result.outputIdentity;
  } else if (m_runningKind == MutationKind::Restore ||
             m_runningKind == MutationKind::EmptyTrash) {
    m_lastTrashToken.clear();
    m_lastTrashOriginalPath.clear();
    m_lastTrashIdentity.reset();
  }
  emit stateChanged();
  emit mutationCommitted();
}

void MutationController::fail(MutationError error, const QString &message) {
  m_failure = error;
  m_failureMessage = boundedMutationDiagnostic(message);
  emit stateChanged();
}

} // namespace QindaQt::Apps::FileManager
