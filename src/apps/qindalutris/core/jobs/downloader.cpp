// SPDX-License-Identifier: GPL-3.0-or-later
#include "downloader.h"

#include "download_allowlist.h"

namespace QindaQt::QindaLutris {

Downloader::Downloader(QObject *parent) : QObject(parent) {}
Downloader::~Downloader() = default;

DownloadGuard::DownloadGuard(DownloadLimits limits, DownloadUrlPolicy policy)
    : m_limits(limits), m_policy(std::move(policy)) {}

bool DownloadGuard::allowed(const QUrl &url) const {
  return m_policy ? m_policy(url) : isAllowedDownloadUrl(url);
}

void DownloadGuard::reset() {
  m_redirects = 0;
  m_announced = -1;
}

std::optional<QString> DownloadGuard::checkStart(const QUrl &url) const {
  if (!allowed(url)) {
    return QStringLiteral("The address %1 is not on the list of places "
                          "QindaLutris may download from.")
        .arg(url.toDisplayString(QUrl::RemoveUserInfo));
  }
  return std::nullopt;
}

std::optional<QString> DownloadGuard::checkRedirect(const QUrl &target) {
  ++m_redirects;
  if (m_redirects > m_limits.maxRedirects) {
    return QStringLiteral("The server redirected too many times.");
  }
  if (!allowed(target)) {
    return QStringLiteral("The server redirected to %1, which is not on the "
                          "list of places QindaLutris may download from.")
        .arg(target.toDisplayString(QUrl::RemoveUserInfo));
  }
  return std::nullopt;
}

std::optional<QString> DownloadGuard::checkHeaders(int httpStatus,
                                                   qint64 contentLength) {
  if (httpStatus < 200 || httpStatus > 299) {
    return QStringLiteral("The server answered with an error (HTTP %1).")
        .arg(httpStatus);
  }
  if (contentLength > m_limits.maxBytes) {
    return QStringLiteral("The file is larger (%1 bytes) than QindaLutris "
                          "allows (%2 bytes).")
        .arg(contentLength)
        .arg(m_limits.maxBytes);
  }
  m_announced = contentLength;
  return std::nullopt;
}

std::optional<QString> DownloadGuard::checkProgress(qint64 received) const {
  if (received > m_limits.maxBytes) {
    return QStringLiteral("The file grew past the %1-byte limit.")
        .arg(m_limits.maxBytes);
  }
  if (m_announced >= 0 && received > m_announced) {
    return QStringLiteral("The server sent more data than it announced.");
  }
  return std::nullopt;
}

std::optional<QString> DownloadGuard::checkCompletion(qint64 received) const {
  if (const auto overrun = checkProgress(received)) {
    return overrun;
  }
  if (m_announced >= 0 && received != m_announced) {
    return QStringLiteral("The download was cut short (%1 of %2 bytes).")
        .arg(received)
        .arg(m_announced);
  }
  if (received == 0) {
    return QStringLiteral("The server sent an empty file.");
  }
  return std::nullopt;
}

} // namespace QindaQt::QindaLutris
