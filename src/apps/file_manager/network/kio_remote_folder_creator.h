// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "remote_folder_creator.h"

#include <QHash>

namespace KIO {
class MkdirJob;
}

namespace QindaQt::Apps::FileManager {

// Production RemoteFolderCreator (ADR-0154) built on KIO's supported
// mkdir() job. The job carries KIO's standard UI delegate, so ordinary
// authentication prompts work exactly like the ADR-0151 listing backend's.
// Each job is parented to this object, so destruction kills any pending
// job -- and any prompt it owns -- quietly; explicit cancel() retires one
// generation without touching others.
//
// Not final: tst_kio_remote_folder_creator.cpp subclasses this to override
// createMkdirJob(), proving the scheme boundary and the retained KIO UI
// delegate without ever starting a real KIO job.
class KioRemoteFolderCreator : public RemoteFolderCreator {
  Q_OBJECT

public:
  explicit KioRemoteFolderCreator(QObject *parent = nullptr);
  ~KioRemoteFolderCreator() override;

  void createFolder(quint64 generation, const QUrl &url) override;
  void cancel(quint64 generation) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URL createFolder() reached -- or to return a real but
  // never-started KIO::MkdirJob -- without contacting a network or
  // filesystem. The production body instantiates the real job.
  [[nodiscard]] virtual KIO::MkdirJob *createMkdirJob(const QUrl &url) const;

private:
  struct PendingCreate final {
    KIO::MkdirJob *job = nullptr;
  };

  QHash<quint64, PendingCreate> m_pending;
};

} // namespace QindaQt::Apps::FileManager
