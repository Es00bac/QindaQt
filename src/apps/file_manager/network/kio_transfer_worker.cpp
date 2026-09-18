// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_transfer_worker.h"

#include "network_location.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>

#include <QDir>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] bool isUsableLocalFolderOrFile(const QUrl &url) {
  return url.isLocalFile() && url.userName().isEmpty() &&
         url.password().isEmpty() && url.host().isEmpty() &&
         QDir::isAbsolutePath(url.toLocalFile());
}

[[nodiscard]] bool isUsableRemote(const QUrl &url) {
  return NetworkLocation::isSupportedScheme(url) && !url.host().isEmpty() &&
         url.userName().isEmpty() && url.password().isEmpty();
}

} // namespace

KioTransferWorker::KioTransferWorker(QObject *parent) : TransferWorker(parent) {}

KioTransferWorker::~KioTransferWorker() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it.value() != nullptr) {
      it.value()->kill(KJob::Quietly);
    }
  }
}

bool KioTransferWorker::isTransferableAcrossRealms(const QUrl &source,
                                                   const QUrl &destinationFolder) {
  const bool sourceLocal = isUsableLocalFolderOrFile(source);
  const bool destinationLocal = isUsableLocalFolderOrFile(destinationFolder);
  const bool sourceRemote = isUsableRemote(source);
  const bool destinationRemote = isUsableRemote(destinationFolder);
  if (!(sourceLocal || sourceRemote) || !(destinationLocal || destinationRemote)) {
    return false;
  }
  // AGENT-GUARD: at least one endpoint must be on the network. A purely
  // local transfer must never reach KIO from here -- MutationController owns
  // local copies and moves, with the identity checks this worker has not.
  return sourceRemote || destinationRemote;
}

KIO::CopyJob *KioTransferWorker::createJob(const QUrl &source,
                                           const QUrl &destinationFolder,
                                           const TransferOperation operation) const {
  // AGENT-NOTE: KIO::copy()/move() take no parent argument, so start()
  // parents the returned job to this object: destruction kills any pending
  // job (and its prompt) with the worker; KIO jobs auto-delete after result.
  // KIO::copy into a *folder* URL places the source beneath it under its own
  // name, which is what the destination dialog asks for.
  return operation == TransferOperation::Move
             ? KIO::move(source, destinationFolder, KIO::HideProgressInfo)
             : KIO::copy(source, destinationFolder, KIO::HideProgressInfo);
}

void KioTransferWorker::start(const quint64 id, const QUrl &source,
                              const QUrl &destinationFolder,
                              const TransferOperation operation) {
  if (!isTransferableAcrossRealms(source, destinationFolder)) {
    Q_EMIT finished(id, QStringLiteral("Unsupported transfer location"));
    return;
  }
  KIO::CopyJob *job = createJob(source, destinationFolder, operation);
  if (job == nullptr) {
    Q_EMIT finished(id, QStringLiteral("The location could not be reached"));
    return;
  }
  job->setParent(this);
  // AGENT-CONTRACT (ADR-0151/0195): CopyJob is a KIO::Job, so it already
  // carries KIO's standard UI delegate; authentication and overwrite prompts
  // work, and failures still arrive here as a typed result.
  m_pending.insert(id, job);
  connect(job, &KJob::percentChanged, this,
          [this, id](KJob *, unsigned long percent) {
            Q_EMIT progressChanged(id, static_cast<int>(percent));
          });
  connect(job, &KJob::result, this, [this, id](KJob *finishedJob) {
    m_pending.remove(id);
    Q_EMIT finished(id, finishedJob->error() == 0
                            ? QString()
                            : NetworkLocation::boundedDiagnostic(
                                  finishedJob->errorString()));
  });
  job->start();
}

void KioTransferWorker::pause(const quint64 id) {
  KIO::CopyJob *job = m_pending.value(id, nullptr);
  if (job != nullptr) {
    job->suspend();
  }
}

void KioTransferWorker::resume(const quint64 id) {
  KIO::CopyJob *job = m_pending.value(id, nullptr);
  if (job != nullptr) {
    job->resume();
  }
}

void KioTransferWorker::cancel(const quint64 id) {
  const auto it = m_pending.find(id);
  if (it == m_pending.end()) {
    return;
  }
  KIO::CopyJob *job = it.value();
  // The result lambda removes the entry when the kill delivers; drop it now
  // so a later cancel of the same id stays a no-op.
  m_pending.erase(it);
  if (job != nullptr) {
    job->kill(KJob::Quietly);
  }
}

} // namespace QindaQt::Apps::FileManager
