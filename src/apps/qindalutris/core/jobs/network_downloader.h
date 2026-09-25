// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "downloader.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>

class QNetworkReply;

namespace QindaQt::QindaLutris {

// Production Downloader over QNetworkAccessManager (ADR-0275 section 6).
// HTTPS only; every redirect hop is verified by DownloadGuard before Qt
// follows it (UserVerifiedRedirectPolicy); bytes go to `<destination>.part`
// and are renamed into place only after the guard accepts the completed
// length. TLS verification is Qt's default and is never relaxed. No resume:
// a failed transfer is discarded whole.
class NetworkDownloader final : public Downloader {
  Q_OBJECT
public:
  explicit NetworkDownloader(DownloadLimits limits = {}, QObject *parent = nullptr);
  ~NetworkDownloader() override;

  void start(const QUrl &url, const QString &destinationFile) override;
  void cancel() override;

  // AGENT-GUARD: tests only. Production never calls this, so the HTTPS
  // allowlist (isAllowedDownloadUrl) stays the only policy in the app.
  void setUrlPolicyForTesting(DownloadUrlPolicy policy);

private:
  void onRedirected(const QUrl &target);
  void onReadyRead();
  void onFinished();
  bool acceptHeadersOnce();
  void fail(const QString &reason);
  void finishLater(bool ok, const QString &reason);
  void abortReply();

  QNetworkAccessManager m_network;
  DownloadLimits m_limits;
  DownloadGuard m_guard;
  QPointer<QNetworkReply> m_reply;
  QFile m_part;
  QString m_destination;
  QTimer m_deadline;
  qint64 m_received = 0;
  quint64 m_generation = 0;
  bool m_active = false;
  bool m_headersAccepted = false;
};

} // namespace QindaQt::QindaLutris
