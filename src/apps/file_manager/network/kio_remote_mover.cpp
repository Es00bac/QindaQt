// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_remote_mover.h"
#include "network_location.h"

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KJob>

namespace QindaQt::Apps::FileManager {

namespace {

// Same-authority check, independent of NavigationController's own
// validation: both URLs must be supported schemes of one authority and
// carry no userinfo. A move that would cross authorities never reaches
// KIO (cross-authority moves would need a credential decision this module
// never makes).
[[nodiscard]] bool isSameAuthorityMove(const QUrl &source, const QUrl &destination) {
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

KioRemoteMover::KioRemoteMover(QObject *parent) : RemoteMover(parent) {}

KioRemoteMover::~KioRemoteMover() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->job != nullptr) {
      it->job->kill(KJob::Quietly);
    }
  }
}

KIO::CopyJob *KioRemoteMover::createMoveJob(const QUrl &source,
                                            const QUrl &destination) const {
  // AGENT-NOTE: KIO::move() takes no parent argument, so move() parents
  // the returned job to this object: destruction kills any pending job
  // (and its prompt) with the mover; KIO jobs auto-delete after result.
  return KIO::move(source, destination, KIO::HideProgressInfo);
}

void KioRemoteMover::move(quint64 generation, const QUrl &source,
                          const QUrl &destination) {
  // AGENT-GUARD: the policy boundary is enforced here, before any KIO job
  // exists -- mirroring KioRemoteCopier's independent check. A move is
  // destructive (the server may delete the source), so a URL pair that
  // failed this check must never reach the facility.
  if (!isSameAuthorityMove(source, destination)) {
    Q_EMIT moveFinished(generation, QStringLiteral("Unsupported network location"));
    return;
  }
  KIO::CopyJob *job = createMoveJob(source, destination);
  if (job == nullptr) {
    Q_EMIT moveFinished(generation, QStringLiteral("The network location could not be reached"));
    return;
  }
  job->setParent(this);
  // AGENT-CONTRACT (ADR-0151/0156): KIO::move() returns a KIO::Job, so it
  // already carries KIO's standard UI delegate and ordinary authentication
  // prompts work; failures still arrive here as a typed result. QindaQt
  // installs no custom delegate and never sees a credential.
  m_pending[generation].job = job;
  connect(job, &KJob::result, this, [this, generation](KJob *finished) {
    m_pending.remove(generation);
    Q_EMIT moveFinished(generation,
                        finished->error() == 0
                            ? QString()
                            : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

void KioRemoteMover::cancel(quint64 generation) {
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
