// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_downloader.h"

#include <QNetworkReply>
#include <QNetworkRequest>

#include <chrono>

namespace QindaQt::QindaLutris {

NetworkDownloader::NetworkDownloader(DownloadLimits limits, QObject *parent)
    : NetworkDownloader(limits, {}, parent) {}

NetworkDownloader::NetworkDownloader(DownloadLimits limits, DownloadUrlPolicy policy,
                                     QObject *parent)
    : Downloader(parent), m_guard(limits, std::move(policy)) {
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, [this] {
    fail(QStringLiteral("The download took too long and was stopped."));
  });
}

NetworkDownloader::~NetworkDownloader() {
  if (m_active) {
    cancel();
  }
}

void NetworkDownloader::finishLater(bool ok, const QString &reason) {
  const quint64 generation = m_generation;
  QTimer::singleShot(0, this, [this, generation, ok, reason] {
    if (generation == m_generation) {
      m_active = false;
      Q_EMIT finished(ok, reason);
    }
  });
}

void NetworkDownloader::start(const QUrl &url, const QString &destinationFile) {
  ++m_generation;
  if (m_active) {
    // AGENT-GUARD: a second start() would orphan the first reply's .part.
    finishLater(false, QStringLiteral("Another download is already running."));
    return;
  }
  m_active = true;
  m_guard.reset();
  m_received = 0;
  m_headersAccepted = false;
  m_destination = destinationFile;

  if (const auto refused = m_guard.checkStart(url)) {
    finishLater(false, *refused);
    return;
  }
  m_part.setFileName(destinationFile + QStringLiteral(".part"));
  if (!m_part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    finishLater(false, QStringLiteral("The download could not be saved: %1")
                           .arg(m_part.errorString()));
    return;
  }

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::UserVerifiedRedirectPolicy);
  // The guard's hop count is the effective limit; Qt's is one higher so it
  // never answers first with its own, vaguer error.
  request.setMaximumRedirectsAllowed(m_guard.limits().maxRedirects + 1);
  request.setTransferTimeout(std::chrono::milliseconds(m_guard.limits().stallTimeoutMs));
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QindaLutris"));

  QNetworkReply *reply = m_network.get(request);
  m_reply = reply;
  connect(reply, &QNetworkReply::redirected, this, &NetworkDownloader::onRedirected);
  connect(reply, &QNetworkReply::readyRead, this, &NetworkDownloader::onReadyRead);
  connect(reply, &QNetworkReply::finished, this, &NetworkDownloader::onFinished);
  connect(reply, &QNetworkReply::downloadProgress, this,
          [this](qint64 received, qint64 total) {
            if (m_active) {
              Q_EMIT progress(received, total);
            }
          });
  m_deadline.start(m_guard.limits().totalTimeoutMs);
}

void NetworkDownloader::onRedirected(const QUrl &target) {
  if (!m_active || m_reply.isNull()) {
    return;
  }
  const QUrl resolved = m_reply->url().resolved(target);
  if (const auto refused = m_guard.checkRedirect(resolved)) {
    fail(*refused);
    return;
  }
  Q_EMIT m_reply->redirectAllowed();
}

bool NetworkDownloader::acceptHeadersOnce() {
  if (m_headersAccepted) {
    return true;
  }
  const int status =
      m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QVariant length = m_reply->header(QNetworkRequest::ContentLengthHeader);
  const qint64 announced = length.isValid() ? length.toLongLong() : -1;
  if (const auto refused = m_guard.checkHeaders(status, announced)) {
    fail(*refused);
    return false;
  }
  m_headersAccepted = true;
  return true;
}

void NetworkDownloader::onReadyRead() {
  if (!m_active || m_reply.isNull()) {
    return;
  }
  const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (status >= 300 && status < 400) {
    (void)m_reply->readAll(); // a redirect's own body; the guard judges the hop
    return;
  }
  if (!acceptHeadersOnce()) {
    return;
  }
  const QByteArray chunk = m_reply->readAll();
  m_received += chunk.size();
  if (const auto refused = m_guard.checkProgress(m_received)) {
    fail(*refused);
    return;
  }
  if (m_part.write(chunk) != chunk.size()) {
    fail(QStringLiteral("The download could not be written to disk: %1")
             .arg(m_part.errorString()));
  }
}

void NetworkDownloader::onFinished() {
  if (!m_active || m_reply.isNull()) {
    return;
  }
  const QNetworkReply::NetworkError error = m_reply->error();
  switch (error) {
  case QNetworkReply::NoError:
    break;
  case QNetworkReply::OperationCanceledError: // Qt's transfer timeout
  case QNetworkReply::TimeoutError:
    fail(QStringLiteral("The server stopped sending data, so the download was stopped."));
    return;
  case QNetworkReply::RemoteHostClosedError:
    fail(QStringLiteral("The server closed the connection before the download finished."));
    return;
  case QNetworkReply::TooManyRedirectsError:
    fail(QStringLiteral("The server redirected too many times."));
    return;
  case QNetworkReply::InsecureRedirectError:
    fail(QStringLiteral("The server redirected to an insecure address."));
    return;
  default:
    break;
  }
  if (error != QNetworkReply::NoError) {
    const int status =
        m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    fail(status > 0 ? QStringLiteral("The server answered with an error (HTTP %1).")
                          .arg(status)
                    : QStringLiteral("The server could not be reached: %1")
                          .arg(m_reply->errorString()));
    return;
  }
  onReadyRead();
  // AGENT-GUARD: onReadyRead() may have failed and released the reply.
  if (!m_active || m_reply.isNull() || !acceptHeadersOnce()) {
    return;
  }
  if (const auto refused = m_guard.checkCompletion(m_received)) {
    fail(*refused);
    return;
  }
  if (!m_part.flush()) {
    fail(QStringLiteral("The download could not be written to disk."));
    return;
  }
  m_part.close();
  QFile::remove(m_destination);
  if (!QFile::rename(m_part.fileName(), m_destination)) {
    fail(QStringLiteral("The finished download could not be moved into place."));
    return;
  }
  m_deadline.stop();
  m_reply->deleteLater();
  m_reply.clear();
  ++m_generation;
  finishLater(true, {});
}

void NetworkDownloader::abortReply() {
  m_deadline.stop();
  if (!m_reply.isNull()) {
    QNetworkReply *reply = m_reply;
    m_reply.clear();
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
  }
  if (m_part.isOpen()) {
    m_part.close();
  }
  if (!m_part.fileName().isEmpty()) {
    QFile::remove(m_part.fileName());
  }
}

void NetworkDownloader::fail(const QString &reason) {
  if (!m_active) {
    return;
  }
  abortReply();
  ++m_generation;
  finishLater(false, reason);
  m_active = false; // stops every other callback; the queued signal still fires
}

void NetworkDownloader::cancel() {
  ++m_generation; // drops any queued finished()
  abortReply();
  m_active = false;
}

} // namespace QindaQt::QindaLutris
