// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_remote_renamer.h"
#include "network_location.h"

#include <KIO/SimpleJob>
#include <KIO/Job>
#include <KJob>

namespace QindaQt::Apps::FileManager {

namespace {

// Same-folder sibling check, independent of NavigationController's own
// validation: scheme and authority preserved, and both URLs' parent paths
// identical. A rename that would cross directories never reaches KIO.
[[nodiscard]] bool isSameFolderRename(const QUrl &source, const QUrl &destination) {
  if (!NetworkLocation::isSupportedScheme(source) ||
      !NetworkLocation::isSupportedScheme(destination)) {
    return false;
  }
  if (source.scheme() != destination.scheme() ||
      source.host() != destination.host() || source.port() != destination.port() ||
      !source.userName().isEmpty() || !destination.userName().isEmpty()) {
    return false;
  }
  const QString sourceParent = source.path().section(QLatin1Char('/'), 0, -2);
  const QString destinationParent = destination.path().section(QLatin1Char('/'), 0, -2);
  return sourceParent == destinationParent;
}

} // namespace

KioRemoteRenamer::KioRemoteRenamer(QObject *parent) : RemoteRenamer(parent) {}

KioRemoteRenamer::~KioRemoteRenamer() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->job != nullptr) {
      it->job->kill(KJob::Quietly);
    }
  }
}

KIO::SimpleJob *KioRemoteRenamer::createRenameJob(const QUrl &source,
                                                    const QUrl &destination) const {
  // AGENT-NOTE: KIO::rename() takes no parent argument, so rename() parents
  // the returned job to this object: destruction kills any pending job (and
  // its prompt) with the renamer; KIO jobs auto-delete after result.
  return KIO::rename(source, destination, KIO::HideProgressInfo);
}

void KioRemoteRenamer::rename(quint64 generation, const QUrl &source,
                              const QUrl &destination) {
  // AGENT-GUARD: the policy boundary is enforced here, before any KIO job
  // exists -- mirroring KioNetworkDirectoryBackend's independent check.
  if (!isSameFolderRename(source, destination)) {
    Q_EMIT renameFinished(generation, QStringLiteral("Rename must stay in the current folder"));
    return;
  }
  KIO::SimpleJob *job = createRenameJob(source, destination);
  if (job == nullptr) {
    Q_EMIT renameFinished(generation, QStringLiteral("The network location could not be reached"));
    return;
  }
  job->setParent(this);
  // AGENT-CONTRACT (ADR-0151/0153): KIO::SimpleJob is a KIO::Job, so it
  // already carries KIO's standard UI delegate and ordinary authentication
  // prompts work; failures still arrive here as a typed result. QindaQt
  // installs no custom delegate and never sees a credential.
  m_pending[generation].job = job;
  connect(job, &KJob::result, this, [this, generation](KJob *finished) {
    m_pending.remove(generation);
    Q_EMIT renameFinished(generation,
                          finished->error() == 0
                              ? QString()
                              : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

void KioRemoteRenamer::cancel(quint64 generation) {
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
