// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/directory_lister.h"
#include "model/launch_intent.h"
#include "network/network_directory_backend.h"
#include "network/remote_file_opener.h"
#include "network/remote_renamer.h"

#include <QHash>
#include <QStringList>
#include <QVector>

namespace QindaQt::Apps::FileManager::Test {

// Test-only DirectoryLister that answers from a caller-populated table instead
// of touching the real filesystem, so NavigationController's dispatch and
// status-mapping logic can be verified independently of LocalDirectoryLister.
class FakeDirectoryLister final : public DirectoryLister {
public:
  [[nodiscard]] ListingResult list(const QString &absolutePath) const override {
    m_requestedPaths.append(absolutePath);
    const auto it = m_results.constFind(absolutePath);
    if (it == m_results.constEnd()) {
      ListingResult notFound;
      notFound.path = absolutePath;
      notFound.error = ListingError::NotFound;
      notFound.diagnostic = QStringLiteral("%1 was not configured in the fake lister")
                                .arg(absolutePath);
      return notFound;
    }
    return it.value();
  }

  void setResult(const QString &absolutePath, ListingResult result) {
    m_results.insert(absolutePath, std::move(result));
  }

  [[nodiscard]] const QStringList &requestedPaths() const { return m_requestedPaths; }

private:
  mutable QHash<QString, ListingResult> m_results;
  mutable QStringList m_requestedPaths;
};

// Test-only FileLauncher that never touches a real desktop handler; it just
// records the requested path and returns a caller-configured canned result.
class FakeFileLauncher final : public FileLauncher {
public:
  [[nodiscard]] LaunchResult launch(const QString &absolutePath) const override {
    m_requestedPaths.append(absolutePath);
    return m_result;
  }

  void setResult(LaunchResult result) { m_result = std::move(result); }

  [[nodiscard]] const QStringList &requestedPaths() const { return m_requestedPaths; }

private:
  LaunchResult m_result;
  mutable QStringList m_requestedPaths;
};

// Test-only NetworkDirectoryBackend: requestListing()/cancel() just record
// their call; the test fires listingReady manually via emitReady(), so
// async success/error/stale/truncated/cancelled scenarios are fully
// deterministic. No timer, thread, or real network/KIO involvement.
class FakeNetworkDirectoryBackend final : public NetworkDirectoryBackend {
public:
  struct Request final {
    quint64 generation = 0;
    QUrl url;
  };

  void requestListing(quint64 generation, const QUrl &url) override {
    m_requests.append({generation, url});
  }

  void cancel(quint64 generation) override {
    m_cancelled.append(generation);
  }

  void emitReady(quint64 generation, const QUrl &url, NetworkListingResult result) {
    Q_EMIT listingReady(generation, url, std::move(result));
  }

  [[nodiscard]] const QVector<Request> &requests() const { return m_requests; }
  [[nodiscard]] const QVector<quint64> &cancelled() const { return m_cancelled; }

private:
  QVector<Request> m_requests;
  QVector<quint64> m_cancelled;
};

// Test-only RemoteFileOpener: records the requested URL; the test fires
// openFinished manually, so success/failure wiring is deterministic without
// a real KIO job, network, desktop handler, or credential prompt.
class FakeRemoteFileOpener final : public RemoteFileOpener {
public:
  void open(const QUrl &url) override { m_requestedUrls.append(url); }

  void finishSuccess() { Q_EMIT openFinished(QString()); }
  void finishFailure(const QString &diagnostic) { Q_EMIT openFinished(diagnostic); }

  [[nodiscard]] const QVector<QUrl> &requestedUrls() const { return m_requestedUrls; }

private:
  QVector<QUrl> m_requestedUrls;
};

// Test-only RemoteRenamer: records rename()/cancel() calls; the test fires
// renameFinished manually, so success/failure/stale/cancel wiring is
// deterministic without a real KIO job, network, or filesystem.
class FakeRemoteRenamer final : public RemoteRenamer {
public:
  struct Request final {
    quint64 generation = 0;
    QUrl source;
    QUrl destination;
  };

  void rename(quint64 generation, const QUrl &source, const QUrl &destination) override {
    m_requests.append({generation, source, destination});
  }

  void cancel(quint64 generation) override { m_cancelled.append(generation); }

  void finishSuccess(quint64 generation) { Q_EMIT renameFinished(generation, QString()); }
  void finishFailure(quint64 generation, const QString &diagnostic) {
    Q_EMIT renameFinished(generation, diagnostic);
  }

  [[nodiscard]] const QVector<Request> &requests() const { return m_requests; }
  [[nodiscard]] const QVector<quint64> &cancelled() const { return m_cancelled; }

private:
  QVector<Request> m_requests;
  QVector<quint64> m_cancelled;
};

} // namespace QindaQt::Apps::FileManager::Test
