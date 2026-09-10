// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include <QObject>
#include <atomic>
#include <memory>

class QThread;

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: GUI-thread confined owner of one recursive-search worker.
// The worker thread only reads directories; it never mutates and never
// descends through symbolic links. Results cross back as one queued,
// token-fenced signal; a stale token (newer search, cancel, destruction)
// must be ignored by consumers. Destruction cancels and joins the worker.
class SearchController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool searching READ searching NOTIFY stateChanged FINAL)

public:
  explicit SearchController(QObject *parent = nullptr);
  ~SearchController() override;

  // Bounds for one search: result count, directory depth below the root, and
  // total visited entries. Hitting any bound truncates and says so in the
  // status text rather than scanning forever.
  static constexpr int maximumResults = 2000;
  static constexpr int maximumDepth = 24;
  static constexpr int maximumVisited = 100000;

  // Starts a case-insensitive literal substring match on entry names below
  // rootPath. Returns the token the matching searchReady signal carries.
  // A previous search is cancelled first. An empty query or a missing root
  // is refused with token 0 and no worker.
  Q_INVOKABLE quint64 startSearch(const QString &rootPath, const QString &query,
                                  bool includeHidden);
  Q_INVOKABLE void cancel();
  // Re-runs the last accepted parameters (e.g. after a mutation committed
  // inside the result set). Returns 0 when no search has run yet.
  Q_INVOKABLE quint64 restart();

  [[nodiscard]] bool searching() const { return m_searching; }
  // Root of the latest accepted search; the composition root compares it
  // against the window's current folder before publishing results.
  [[nodiscard]] QString rootPath() const { return m_rootPath; }

signals:
  void stateChanged();
  void searchReady(quint64 token,
                   const QVector<QindaQt::Apps::FileManager::DirectoryEntry> &entries,
                   const QString &statusText);

private:
  void launch();

  QString m_rootPath;
  QString m_query;
  bool m_includeHidden = false;
  bool m_searching = false;
  quint64 m_nextToken = 0;
  quint64 m_activeToken = 0;
  // Owned worker; non-null only while a search runs. Cancellation is shared
  // with the worker through this flag.
  QThread *m_worker = nullptr;
  std::shared_ptr<std::atomic_bool> m_cancellation;
};

} // namespace QindaQt::Apps::FileManager
