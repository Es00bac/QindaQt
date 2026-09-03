// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_controller.h"

#include "local_mutation_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>

#include <limits>
#include <type_traits>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] bool validName(const QString &name) {
  return !name.isEmpty() && QFile::encodeName(name).size() <= 255 &&
         name != QLatin1String(".") &&
         name != QLatin1String("..") && !name.contains(QLatin1Char('/')) &&
         !name.contains(QLatin1Char('\\')) && !name.contains(QChar::Null);
}

[[nodiscard]] QStringList rootsFor(const QString &source,
                                   const QString &destination = {}) {
  QStringList roots;
  if (!source.isEmpty()) {
    roots.append(QFileInfo(source).absolutePath());
  }
  if (!destination.isEmpty()) {
    roots.append(QFileInfo(destination).absolutePath());
  }
  roots.removeDuplicates();
  return roots;
}

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
                          : QStringLiteral("File operation completed");
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
