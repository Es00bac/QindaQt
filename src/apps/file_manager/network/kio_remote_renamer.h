// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "remote_renamer.h"

#include <QHash>

namespace KIO {
class SimpleJob;
}

namespace QindaQt::Apps::FileManager {

// Production RemoteRenamer (ADR-0153) built on KIO's supported rename()
// job (a KIO::SimpleJob, so it carries KIO's standard UI delegate and
// ordinary authentication prompts work like the listing backend's). Each
// job is parented to this object, so destruction kills any pending job --
// and any prompt it owns -- quietly; explicit cancel() retires one
// generation without touching others.
//
// Not final: tst_kio_remote_renamer.cpp subclasses this to override
// createRenameJob(), proving the scheme/parent boundary and the retained
// KIO UI delegate without ever starting a real KIO job.
class KioRemoteRenamer : public RemoteRenamer {
  Q_OBJECT

public:
  explicit KioRemoteRenamer(QObject *parent = nullptr);
  ~KioRemoteRenamer() override;

  void rename(quint64 generation, const QUrl &source, const QUrl &destination) override;
  void cancel(quint64 generation) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URLs rename() reached -- or to return a real but never-started
  // KIO::SimpleJob -- without contacting a network or filesystem.
  // The production body instantiates the real job.
  [[nodiscard]] virtual KIO::SimpleJob *createRenameJob(const QUrl &source,
                                                          const QUrl &destination) const;

private:
  struct PendingRename final {
    KIO::SimpleJob *job = nullptr;
  };

  QHash<quint64, PendingRename> m_pending;
};

} // namespace QindaQt::Apps::FileManager
