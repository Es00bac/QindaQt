// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "transfer_worker.h"

#include <QHash>

namespace KIO {
class CopyJob;
}

namespace QindaQt::Apps::FileManager {

// Production TransferWorker (ADR-0195) on KIO's supported copy()/move()
// jobs. Each job carries KIO's standard UI delegate, so the platform's own
// authentication and overwrite prompts work exactly as they do for the
// ADR-0151 listing backend; QindaQt installs no delegate of its own and
// never sees a credential.
//
// AGENT-GUARD: the scheme boundary is re-proved here, independently of
// TransferRouter, before any job exists. Exactly one endpoint may be a local
// file:// URL and the other a canonical smb/sftp URL, or both may be
// canonical smb/sftp URLs; neither may carry userinfo, and a transfer with
// no network endpoint is refused outright -- purely local copies belong to
// MutationController, which has the identity checks, Trash, and undo.
//
// Not final: tst_kio_transfer_worker.cpp subclasses this to override
// createJob(), proving the boundary, the operation mapping, and the retained
// KIO UI delegate without ever starting a real KIO job.
class KioTransferWorker : public TransferWorker {
  Q_OBJECT

public:
  explicit KioTransferWorker(QObject *parent = nullptr);
  ~KioTransferWorker() override;

  void start(quint64 id, const QUrl &source, const QUrl &destinationFolder,
             TransferOperation operation) override;
  void pause(quint64 id) override;
  void resume(quint64 id) override;
  void cancel(quint64 id) override;

  // The independent policy check, exposed so its refusals are testable
  // without a job. True only for the endpoint pairs described above.
  [[nodiscard]] static bool isTransferableAcrossRealms(const QUrl &source,
                                                       const QUrl &destinationFolder);

protected:
  // AGENT-NOTE: test seam. A subclass overrides this to record the exact
  // URLs and operation start() reached -- or to return a real but
  // never-started KIO::CopyJob -- without contacting a network or filesystem.
  [[nodiscard]] virtual KIO::CopyJob *createJob(const QUrl &source,
                                                const QUrl &destinationFolder,
                                                TransferOperation operation) const;

private:
  QHash<quint64, KIO::CopyJob *> m_pending;
};

} // namespace QindaQt::Apps::FileManager
