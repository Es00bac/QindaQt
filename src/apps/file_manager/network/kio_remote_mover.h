// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "remote_mover.h"

#include <QHash>

namespace KIO {
class CopyJob;
}

namespace QindaQt::Apps::FileManager {

// Production RemoteMover (ADR-0156) built on KIO's supported move()
// job. KIO::move() is a CopyJob, so -- exactly like the ADR-0155 copier --
// the job carries KIO's standard UI delegate and ordinary authentication
// prompts work like the ADR-0151 listing backend's. Each job is parented to
// this object, so destruction kills any pending job -- and any prompt it
// owns -- quietly; explicit cancel() retires one generation without
// touching others.
//
// Not final: tst_kio_remote_mover.cpp subclasses this to override
// createMoveJob(), proving the scheme boundary and the retained KIO UI
// delegate without ever starting a real KIO job.
class KioRemoteMover : public RemoteMover {
  Q_OBJECT

public:
  explicit KioRemoteMover(QObject *parent = nullptr);
  ~KioRemoteMover() override;

  void move(quint64 generation, const QUrl &source, const QUrl &destination) override;
  void cancel(quint64 generation) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URLs move() reached -- or to return a real but never-started
  // KIO::CopyJob -- without contacting a network or filesystem. The
  // production body instantiates the real job.
  [[nodiscard]] virtual KIO::CopyJob *createMoveJob(const QUrl &source,
                                                    const QUrl &destination) const;

private:
  struct PendingMove final {
    KIO::CopyJob *job = nullptr;
  };

  QHash<quint64, PendingMove> m_pending;
};

} // namespace QindaQt::Apps::FileManager
