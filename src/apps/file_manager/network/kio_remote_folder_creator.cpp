// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_remote_folder_creator.h"
#include "network_location.h"

#include <KIO/Job>
#include <KIO/MkdirJob>
#include <KJob>

namespace QindaQt::Apps::FileManager {

KioRemoteFolderCreator::KioRemoteFolderCreator(QObject *parent)
    : RemoteFolderCreator(parent) {}

KioRemoteFolderCreator::~KioRemoteFolderCreator() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->job != nullptr) {
      it->job->kill(KJob::Quietly);
    }
  }
}

KIO::MkdirJob *KioRemoteFolderCreator::createMkdirJob(const QUrl &url) const {
  // AGENT-NOTE: KIO::mkdir() takes no parent argument, so createFolder()
  // parents the returned job to this object: destruction kills any pending
  // job (and its prompt) with the creator; KIO jobs auto-delete after
  // result.
  return KIO::mkdir(url);
}

void KioRemoteFolderCreator::createFolder(quint64 generation, const QUrl &url) {
  // AGENT-GUARD: the policy boundary is enforced here, before any KIO job
  // exists -- mirroring KioNetworkDirectoryBackend's independent check.
  if (!NetworkLocation::isSupportedScheme(url) || !url.userName().isEmpty() ||
      !url.password().isEmpty()) {
    Q_EMIT createFinished(generation, QStringLiteral("Unsupported network location"));
    return;
  }
  KIO::MkdirJob *job = createMkdirJob(url);
  if (job == nullptr) {
    Q_EMIT createFinished(generation, QStringLiteral("The network location could not be reached"));
    return;
  }
  job->setParent(this);
  // AGENT-CONTRACT (ADR-0151/0154): MkdirJob is a KIO::Job, so it already
  // carries KIO's standard UI delegate and ordinary authentication prompts
  // work; failures still arrive here as a typed result. QindaQt installs no
  // custom delegate and never sees a credential.
  m_pending[generation].job = job;
  connect(job, &KJob::result, this, [this, generation](KJob *finished) {
    m_pending.remove(generation);
    Q_EMIT createFinished(generation,
                          finished->error() == 0
                              ? QString()
                              : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

void KioRemoteFolderCreator::cancel(quint64 generation) {
  const auto it = m_pending.find(generation);
  if (it == m_pending.end()) {
    return;
  }
  if (it->job != nullptr) {
    it->job->kill(KJob::Quietly);
  }
  // The result lambda removes the entry when the kill delivers; drop it now
  // so a later cancel of the same generation stays a no-op.
  m_pending.erase(it);
}

} // namespace QindaQt::Apps::FileManager
