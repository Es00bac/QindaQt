// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_remote_file_opener.h"
#include "network_location.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KJob>

namespace QindaQt::Apps::FileManager {

KioRemoteFileOpener::KioRemoteFileOpener(QObject *parent) : RemoteFileOpener(parent) {}

KioRemoteFileOpener::~KioRemoteFileOpener() = default;

KIO::OpenUrlJob *KioRemoteFileOpener::createOpenUrlJob(const QUrl &url) const {
  // AGENT-NOTE: parented to this so destruction kills any pending job (and
  // its prompt) with the opener; OpenUrlJob auto-deletes after result.
  return new KIO::OpenUrlJob(url, const_cast<KioRemoteFileOpener *>(this));
}

void KioRemoteFileOpener::open(const QUrl &url) {
  // AGENT-GUARD: same policy boundary as KioNetworkDirectoryBackend -- a
  // non-smb/sftp URL never reaches KIO from this module, independent of the
  // caller's own validation.
  if (!NetworkLocation::isSupportedScheme(url)) {
    Q_EMIT openFinished(QStringLiteral("Unsupported network location scheme"));
    return;
  }
  KIO::OpenUrlJob *job = createOpenUrlJob(url);
  if (job == nullptr) {
    Q_EMIT openFinished(QStringLiteral("The network location could not be reached"));
    return;
  }
  // AGENT-CONTRACT (ADR-0151/0152): unlike KIO::Job, OpenUrlJob does not
  // auto-install a UI delegate, so receive the platform's standard one from
  // KIO's registered factory -- the same delegate listing jobs carry. A
  // slave that needs credentials can then prompt; if no KIO GUI library is
  // loaded the job simply has no delegate and failures still arrive here as
  // a typed result. QindaQt installs no custom delegate and never sees a
  // credential. AGENT-NOTE: that standard delegate prompts with QWidgets
  // (Open With dialog, credential/message boxes), so the surrounding process
  // must be the QWidget-capable application from
  // runtime/file_manager_application.h -- under a bare QGuiApplication the
  // Open With dialog aborts the process (review P1 on 2b37f9c1).
  if (job->uiDelegate() == nullptr) {
    job->setUiDelegate(KIO::createDefaultJobUiDelegate());
  }
  connect(job, &KJob::result, this, [this](KJob *finished) {
    Q_EMIT openFinished(finished->error() == 0
                            ? QString()
                            : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

} // namespace QindaQt::Apps::FileManager
