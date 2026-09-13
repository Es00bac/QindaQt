// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "remote_file_opener.h"

namespace KIO {
class OpenUrlJob;
}

namespace QindaQt::Apps::FileManager {

// Production RemoteFileOpener (ADR-0152) built on KIO::OpenUrlJob, the
// platform's supported "open with the desktop's default handler" facility.
// KIO performs any temporary download a remote URL needs and owns the
// resulting temp file lifecycle; QindaQt adds no downloader, handler picker,
// or credential authority. Each job is parented to this object, so the
// opener's destruction (e.g. its NavigationController dying) kills every
// pending job -- and any prompt it owns -- quietly.
//
// Not final: tst_kio_remote_file_opener.cpp subclasses this to override
// createOpenUrlJob(), proving the scheme/policy boundary and the retained
// KIO UI delegate without ever starting a real KIO::OpenUrlJob.
class KioRemoteFileOpener : public RemoteFileOpener {
  Q_OBJECT

public:
  explicit KioRemoteFileOpener(QObject *parent = nullptr);
  ~KioRemoteFileOpener() override;

  void open(const QUrl &url) override;

protected:
  // AGENT-NOTE: test seam. A test subclass overrides this to record the
  // exact URL open() reached -- or to return a real but never-started
  // KIO::OpenUrlJob -- without contacting a network or desktop handler.
  // The production body instantiates the real job.
  [[nodiscard]] virtual KIO::OpenUrlJob *createOpenUrlJob(const QUrl &url) const;
};

} // namespace QindaQt::Apps::FileManager
