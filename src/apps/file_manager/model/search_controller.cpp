// SPDX-License-Identifier: GPL-3.0-or-later
#include "search_controller.h"

#include "../mutation/local_mutation_backend.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThread>

namespace QindaQt::Apps::FileManager {
namespace {

// One bounded recursive walk. Lives on its own thread; results cross back by
// value in the single resultsReady emission consumed by the controller's
// queued lambda.
class SearchWorker final : public QThread {
  Q_OBJECT
public:
  SearchWorker(QString rootPath, QString query, bool includeHidden,
               quint64 token,
               std::shared_ptr<std::atomic_bool> cancellation, QObject *parent)
      : QThread(parent),
        m_rootPath(std::move(rootPath)),
        m_query(std::move(query)),
        m_includeHidden(includeHidden),
        m_token(token),
        m_cancellation(std::move(cancellation)) {}

signals:
  // Always emitted at the end of run(), cancelled or not, so the controller
  // has exactly one completion signal to fence on.
  void resultsReady(quint64 token,
                    const QVector<QindaQt::Apps::FileManager::DirectoryEntry> &entries,
                    const QString &status, bool cancelled);

protected:
  void run() override {
    const auto cancelled = [this] {
      return m_cancellation->load(std::memory_order_relaxed);
    };
    struct Visit {
      QString path;
      int depth;
    };
    QVector<Visit> stack{{m_rootPath, 0}};
    int visited = 0;
    bool truncated = false;
    while (!stack.isEmpty() && !cancelled()) {
      const Visit visit = stack.takeLast();
      QDir directory(visit.path);
      directory.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System |
                          QDir::NoDotAndDotDot);
      const QFileInfoList infos = directory.entryInfoList();
      for (const QFileInfo &info : infos) {
        if (cancelled()) {
          break;
        }
        if (++visited > SearchController::maximumVisited) {
          truncated = true;
          break;
        }
        DirectoryEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDirectory = info.isDir();
        entry.isSymlink = info.isSymLink();
        entry.isHidden = entry.name.startsWith(QLatin1Char('.'));
        entry.isReadable = info.isReadable();
        entry.size = entry.isDirectory ? 0 : info.size();
        entry.lastModified = info.lastModified();
        if (const auto identity =
                LocalMutationBackend::identityForPath(entry.absolutePath)) {
          entry.device = identity->device;
          entry.inode = identity->inode;
          entry.identitySize = identity->size;
          entry.modifiedNanoseconds = identity->modifiedNanoseconds;
          entry.mode = identity->mode;
        }
        if (!m_includeHidden && entry.isHidden) {
          continue;
        }
        if (entry.name.contains(m_query, Qt::CaseInsensitive)) {
          m_entries.append(entry);
          if (m_entries.size() >= SearchController::maximumResults) {
            truncated = true;
            break;
          }
        }
        // AGENT-GUARD: Never descend through a symlink; a link cycle would
        // otherwise defeat every bound above.
        if (entry.isDirectory && !entry.isSymlink &&
            visit.depth < SearchController::maximumDepth) {
          stack.append({entry.absolutePath, visit.depth + 1});
        }
      }
      if (truncated) {
        break;
      }
    }
    const bool wasCancelled = cancelled();
    if (!wasCancelled) {
      const qsizetype count = m_entries.size();
      if (count == 0) {
        m_status = QStringLiteral("No items match \"%1\"").arg(m_query);
      } else if (truncated) {
        m_status = QStringLiteral("Showing the first %1 matches for \"%2\" (limit reached)")
                       .arg(count)
                       .arg(m_query);
      } else {
        m_status = count == 1
            ? QStringLiteral("1 match for \"%1\"").arg(m_query)
            : QStringLiteral("%1 matches for \"%2\"").arg(count).arg(m_query);
      }
    }
    emit resultsReady(m_token, m_entries, m_status, wasCancelled);
  }

private:
  QString m_rootPath;
  QString m_query;
  bool m_includeHidden;
  quint64 m_token;
  std::shared_ptr<std::atomic_bool> m_cancellation;
  QVector<DirectoryEntry> m_entries;
  QString m_status;
};

} // namespace

SearchController::SearchController(QObject *parent) : QObject(parent) {
  qRegisterMetaType<QVector<DirectoryEntry>>();
}

SearchController::~SearchController() {
  cancel();
}

quint64 SearchController::startSearch(const QString &rootPath,
                                      const QString &query, bool includeHidden) {
  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty() || !QFileInfo(rootPath).isDir()) {
    // A refused search also forgets the previous parameters so a later
    // restart() cannot resurrect a listing the UI already dismissed.
    cancel();
    m_rootPath.clear();
    m_query.clear();
    return 0;
  }
  m_rootPath = QFileInfo(rootPath).absoluteFilePath();
  m_query = trimmed;
  m_includeHidden = includeHidden;
  cancel();
  launch();
  return m_activeToken;
}

void SearchController::cancel() {
  // AGENT-GUARD: Invalidate the token BEFORE touching the worker, and never
  // let the queued resultsReady lambda dereference a worker pointer. Qt
  // delivers context-bound queued calls even after the sender object is
  // destroyed (verified on Qt 6.11), so the lambda re-checks the live token
  // and disposes only the worker still recorded in m_worker. Deleting a
  // joined worker here is then safe: any already-queued lambda observes the
  // invalidated token and returns without touching the worker.
  m_activeToken = 0;
  if (m_worker != nullptr) {
    m_cancellation->store(true, std::memory_order_relaxed);
    m_worker->wait();
    delete m_worker;
    m_worker = nullptr;
  }
  if (m_searching) {
    m_searching = false;
    emit stateChanged();
  }
}

quint64 SearchController::restart() {
  if (m_query.isEmpty() || m_rootPath.isEmpty()) {
    return 0;
  }
  cancel();
  launch();
  return m_activeToken;
}

void SearchController::launch() {
  m_cancellation = std::make_shared<std::atomic_bool>(false);
  const quint64 token = ++m_nextToken;
  m_activeToken = token;
  auto *worker = new SearchWorker(m_rootPath, m_query, m_includeHidden, token,
                                  m_cancellation, nullptr);
  m_worker = worker;
  const QPointer<SearchController> guard(this);
  // Results and the cancelled flag arrive by value; the lambda touches the
  // worker only while this controller still owns it (token still active).
  QObject::connect(worker, &SearchWorker::resultsReady, this,
                   [guard, token](quint64 readyToken,
                                  const QVector<DirectoryEntry> &entries,
                                  const QString &status, bool cancelled) {
                     if (!guard || readyToken != token ||
                         guard->m_activeToken != token) {
                       return;
                     }
                     QThread *finished = guard->m_worker;
                     guard->m_worker = nullptr;
                     if (finished != nullptr) {
                       finished->wait();
                       delete finished;
                     }
                     if (guard->m_searching) {
                       guard->m_searching = false;
                       emit guard->stateChanged();
                     }
                     if (!cancelled) {
                       emit guard->searchReady(token, entries, status);
                     }
                   });
  m_searching = true;
  emit stateChanged();
  worker->start();
}

} // namespace QindaQt::Apps::FileManager

#include "search_controller.moc"
