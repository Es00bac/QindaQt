// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network_directory_backend.h"

#include <QHash>

namespace KIO {
class ListJob;
}

namespace QindaQt::Apps::FileManager {

// Production NetworkDirectoryBackend confining every KIO::ListJob this
// module starts (ADR-0137, authentication flow amended by ADR-0151). Owns no
// other KIO job kind and no wallet/keyring handle: requestListing() leaves
// the platform's standard KIO UI delegate on each supported job, so a slave
// that needs credentials or a mount shows KIO's ordinary prompt instead of
// failing with a typed error; QindaQt itself never reads, stores, or owns a
// credential. Local listing/launch/mutation/preview/search authorities never
// construct or receive this type.
//
// Not final: tst_kio_network_directory_backend.cpp subclasses this to
// override createListJob(), proving the scheme/policy boundary without ever
// starting a real KIO::ListJob.
class KioNetworkDirectoryBackend : public NetworkDirectoryBackend {
  Q_OBJECT

public:
  explicit KioNetworkDirectoryBackend(QObject *parent = nullptr);
  ~KioNetworkDirectoryBackend() override;

  void requestListing(quint64 generation, const QUrl &url) override;
  void cancel(quint64 generation) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URL requestListing() reached without ever starting a real
  // KIO::ListJob, proving the scheme/policy boundary (which locations reach
  // job creation at all) without a network request. The production body
  // instantiates the real job.
  [[nodiscard]] virtual KIO::ListJob *createListJob(const QUrl &url) const;

private:
  struct PendingRequest final {
    KIO::ListJob *job = nullptr;
    QUrl url;
    QVector<DirectoryEntry> entries;
    bool truncated = false;
  };

  void finish(quint64 generation, NetworkListingError error,
             const QString &diagnostic);

  QHash<quint64, PendingRequest> m_pending;
};

} // namespace QindaQt::Apps::FileManager
