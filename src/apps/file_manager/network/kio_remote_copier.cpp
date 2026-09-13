// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_remote_copier.h"
#include "network_location.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>

namespace QindaQt::Apps::FileManager {

namespace {

// Same-authority check, independent of NavigationController's own
// validation: both URLs must be supported schemes of one authority and
// carry no userinfo. A copy that would cross authorities never reaches
// KIO (cross-authority copies would need a credential decision this module
// never makes).
[[nodiscard]] bool isSameAuthorityCopy(const QUrl &source, const QUrl &destination) {
  if (!NetworkLocation::isSupportedScheme(source) ||
      !NetworkLocation::isSupportedScheme(destination)) {
    return false;
  }
  return source.scheme() == destination.scheme() &&
         source.host() == destination.host() && source.port() == destination.port() &&
         source.userName().isEmpty() && destination.userName().isEmpty() &&
         source.password().isEmpty() && destination.password().isEmpty();
}

} // namespace

KioRemoteCopier::KioRemoteCopier(QObject *parent) : RemoteCopier(parent) {}

KioRemoteCopier::~KioRemoteCopier() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->job != nullptr) {
      it->job->kill(KJob::Quietly);
    }
  }
}

KIO::CopyJob *KioRemoteCopier::createCopyJob(const QUrl &source,
                                             const QUrl &destination) const {
  // AGENT-NOTE: KIO::copy() takes no parent argument, so copy() parents
  // the returned job to this object: destruction kills any pending job
  // (and its prompt) with the copier; KIO jobs auto-delete after result.
  return KIO::copy(source, destination, KIO::HideProgressInfo);
}

void KioRemoteCopier::copy(quint64 generation, const QUrl &source,
                           const QUrl &destination) {
  // AGENT-GUARD: the policy boundary is enforced here, before any KIO job
  // exists -- mirroring KioNetworkDirectoryBackend's independent check.
  if (!isSameAuthorityCopy(source, destination)) {
    Q_EMIT copyFinished(generation, QStringLiteral("Unsupported network location"));
    return;
  }
  KIO::CopyJob *job = createCopyJob(source, destination);
  if (job == nullptr) {
    Q_EMIT copyFinished(generation, QStringLiteral("The network location could not be reached"));
    return;
  }
  job->setParent(this);
  // AGENT-CONTRACT (ADR-0151/0155): CopyJob is a KIO::Job, so it already
  // carries KIO's standard UI delegate and ordinary authentication prompts
  // work; failures still arrive here as a typed result. QindaQt installs no
  // custom delegate and never sees a credential.
  m_pending[generation].job = job;
  connect(job, &KJob::result, this, [this, generation](KJob *finished) {
    m_pending.remove(generation);
    Q_EMIT copyFinished(generation,
                        finished->error() == 0
                            ? QString()
                            : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

void KioRemoteCopier::cancel(quint64 generation) {
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
