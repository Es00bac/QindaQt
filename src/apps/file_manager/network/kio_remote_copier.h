// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "remote_copier.h"

#include <QHash>

namespace KIO {
class CopyJob;
}

namespace QindaQt::Apps::FileManager {

// Production RemoteCopier (ADR-0155) built on KIO's supported copy()
// job. The job carries KIO's standard UI delegate, so ordinary
// authentication prompts work exactly like the ADR-0151 listing backend's.
// Each job is parented to this object, so destruction kills any pending
// job -- and any prompt it owns -- quietly; explicit cancel() retires one
// generation without touching others.
//
// Not final: tst_kio_remote_copier.cpp subclasses this to override
// createCopyJob(), proving the scheme boundary and the retained KIO UI
// delegate without ever starting a real KIO job.
class KioRemoteCopier : public RemoteCopier {
  Q_OBJECT

public:
  explicit KioRemoteCopier(QObject *parent = nullptr);
  ~KioRemoteCopier() override;

  void copy(quint64 generation, const QUrl &source, const QUrl &destination) override;
  void cancel(quint64 generation) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URLs copy() reached -- or to return a real but never-started
  // KIO::CopyJob -- without contacting a network or filesystem. The
  // production body instantiates the real job.
  [[nodiscard]] virtual KIO::CopyJob *createCopyJob(const QUrl &source,
                                                    const QUrl &destination) const;

private:
  struct PendingCopy final {
    KIO::CopyJob *job = nullptr;
  };

  QHash<quint64, PendingCopy> m_pending;
};

} // namespace QindaQt::Apps::FileManager
