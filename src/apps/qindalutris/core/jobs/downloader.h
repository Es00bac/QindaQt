// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the download seam of ADR-0275 section 6. Jobs depend only
// on this interface; production wires NetworkDownloader, tests a scripted
// fake. Semantics every implementation must keep:
//  - start() must not be called while a download is active; it never emits
//    synchronously (finished() always arrives from the event loop).
//  - exactly one finished() follows each start(), unless cancel() is called
//    first; after cancel() no further signal is emitted for that download.
//  - on finished(true) destinationFile exists and is complete; on
//    finished(false) and after cancel() neither destinationFile nor its
//    `.part` sibling is left behind.
//  - `reason` is one short plain sentence suitable for the details log; jobs
//    wrap it in their own user-facing sentence.
// Threading: confined to the thread that owns the object.
class Downloader : public QObject {
  Q_OBJECT
public:
  explicit Downloader(QObject *parent = nullptr);
  ~Downloader() override;

  virtual void start(const QUrl &url, const QString &destinationFile) = 0;
  virtual void cancel() = 0;

Q_SIGNALS:
  // bytesTotal is -1 while unknown.
  void progress(qint64 bytesReceived, qint64 bytesTotal);
  void finished(bool ok, const QString &reason);
};

struct DownloadLimits final {
  qint64 maxBytes = qint64(2) * 1024 * 1024 * 1024; // hard size cap
  int stallTimeoutMs = 60 * 1000;                    // no bytes for this long
  int totalTimeoutMs = 3 * 60 * 60 * 1000;           // whole transfer
  int maxRedirects = 8;
};

// AGENT-NOTE: the production downloader's decisions, kept pure so every
// failure mode (bad redirect, HTTP error, oversize, truncation) is tested
// without a network or a TLS test server. NetworkDownloader only feeds it
// observations; it never decides on its own. Each check returns a plain
// reason when the transfer must stop.
class DownloadGuard final {
public:
  explicit DownloadGuard(DownloadLimits limits = {});

  void reset();
  [[nodiscard]] const DownloadLimits &limits() const { return m_limits; }

  [[nodiscard]] std::optional<QString> checkStart(const QUrl &url) const;
  // Counts hops; `target` must already be resolved against the current URL.
  [[nodiscard]] std::optional<QString> checkRedirect(const QUrl &target);
  // httpStatus of the final (non-redirect) response; contentLength -1 when
  // the server did not announce one.
  [[nodiscard]] std::optional<QString> checkHeaders(int httpStatus,
                                                    qint64 contentLength);
  [[nodiscard]] std::optional<QString> checkProgress(qint64 received) const;
  [[nodiscard]] std::optional<QString> checkCompletion(qint64 received) const;

private:
  DownloadLimits m_limits;
  int m_redirects = 0;
  qint64 m_announced = -1;
};

} // namespace QindaQt::QindaLutris
