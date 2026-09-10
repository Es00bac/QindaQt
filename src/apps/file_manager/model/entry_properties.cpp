// SPDX-License-Identifier: GPL-3.0-or-later
#include "entry_properties.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QPointer>
#include <QThread>
#include <QVariantList>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QString symbolicPermissions(quint32 mode, bool isDirectory,
                                          bool isSymlink) {
  QString text;
  text.reserve(10);
  text.append(isDirectory ? QLatin1Char('d')
              : isSymlink ? QLatin1Char('l')
                          : QLatin1Char('-'));
  static constexpr quint32 bits[9] = {0400, 0200, 0100, 0040, 0020,
                                      0010, 0004, 0002, 0001};
  static constexpr char chars[9] = {'r', 'w', 'x', 'r', 'w', 'x', 'r', 'w', 'x'};
  for (int i = 0; i < 9; ++i) {
    text.append((mode & bits[i]) ? QLatin1Char(chars[i]) : QLatin1Char('-'));
  }
  return text;
}

struct WalkRoot {
  QString path;
  bool directory;
  bool symlink;
  qint64 size;
};

// Bounded total-size walk over the selection. Same thread-confinement and
// cancellation pattern as the search worker; the result crosses back by value
// in the single totalReady emission.
class TotalSizeWorker final : public QThread {
  Q_OBJECT
public:
  TotalSizeWorker(QVector<WalkRoot> roots,
                  std::shared_ptr<std::atomic_bool> cancellation,
                  QObject *parent)
      : QThread(parent),
        m_roots(std::move(roots)),
        m_cancellation(std::move(cancellation)) {}

signals:
  // Always emitted at the end of run(), cancelled or not, so the controller
  // has exactly one completion signal to fence on.
  void totalReady(qint64 totalBytes, bool truncated, bool cancelled);

protected:
  void run() override {
    struct Visit {
      QString path;
      int depth;
    };
    QVector<Visit> stack;
    int visited = 0;
    for (const WalkRoot &root : m_roots) {
      if (wasCancelled()) {
        emit totalReady(m_totalBytes, m_truncated, true);
        return;
      }
      if (++visited > EntryPropertiesController::maximumVisited) {
        m_truncated = true;
        return;
      }
      if (!root.directory || root.symlink) {
        m_totalBytes += root.size;
        continue;
      }
      stack.append({root.path, 0});
      while (!stack.isEmpty()) {
        if (wasCancelled()) {
          emit totalReady(m_totalBytes, m_truncated, true);
          return;
        }
        const Visit visit = stack.takeLast();
        QDir directory(visit.path);
        directory.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System |
                            QDir::NoDotAndDotDot);
        const QFileInfoList infos = directory.entryInfoList();
        for (const QFileInfo &info : infos) {
          if (wasCancelled()) {
            emit totalReady(m_totalBytes, m_truncated, true);
            return;
          }
          if (++visited > EntryPropertiesController::maximumVisited) {
            m_truncated = true;
            emit totalReady(m_totalBytes, m_truncated, false);
            return;
          }
          const bool symlink = info.isSymLink();
          if (info.isDir() && !symlink) {
            if (visit.depth < EntryPropertiesController::maximumDepth) {
              stack.append({info.absoluteFilePath(), visit.depth + 1});
            } else {
              m_truncated = true;
            }
            continue;
          }
          m_totalBytes += info.size();
        }
      }
    }
    emit totalReady(m_totalBytes, m_truncated, false);
  }

private:
  [[nodiscard]] bool wasCancelled() const {
    return m_cancellation->load(std::memory_order_relaxed);
  }

  QVector<WalkRoot> m_roots;
  std::shared_ptr<std::atomic_bool> m_cancellation;
  qint64 m_totalBytes = 0;
  bool m_truncated = false;
};

} // namespace

EntryPropertiesController::EntryPropertiesController(QObject *parent)
    : QObject(parent) {}

EntryPropertiesController::~EntryPropertiesController() {
  stopWorker();
}

void EntryPropertiesController::inspect(const QVariantList &entries) {
  stopWorker();
  m_active = !entries.isEmpty();
  m_entryCount = static_cast<int>(entries.size());
  m_totalSizeText.clear();
  m_totalTruncated = false;
  m_computingTotal = false;
  if (entries.isEmpty()) {
    m_name.clear();
    m_kindText.clear();
    m_mimeText.clear();
    m_sizeText.clear();
    m_modifiedText.clear();
    m_permissionsText.clear();
    m_pathText.clear();
    emit stateChanged();
    return;
  }

  const QVariantMap first = entries.first().toMap();
  const bool single = entries.size() == 1;
  const QString path = first.value(QStringLiteral("path")).toString();
  const bool isDirectory = first.value(QStringLiteral("isDirectory")).toBool();
  const bool isSymlink = first.value(QStringLiteral("isSymlink")).toBool();
  bool modeOk = false;
  const quint32 mode =
      first.value(QStringLiteral("mode")).toString().toUInt(&modeOk, 10);

  m_name = single ? first.value(QStringLiteral("name")).toString()
                  : QStringLiteral("%1 items").arg(entries.size());
  m_kindText = single
      ? first.value(QStringLiteral("kindText")).toString()
      : QStringLiteral("Multiple items");
  if (single) {
    QMimeDatabase mimeDatabase;
    m_mimeText = isDirectory
        ? QStringLiteral("inode/directory")
        : mimeDatabase.mimeTypeForFile(QFileInfo(path)).name();
    m_pathText = path;
    const QDateTime modified = first.value(QStringLiteral("modified")).toDateTime();
    m_modifiedText = modified.isValid()
        ? QLocale().toString(modified, QLocale::LongFormat)
        : QString();
    m_permissionsText =
        modeOk ? symbolicPermissions(mode, isDirectory, isSymlink) : QString();
    m_sizeText = isDirectory
        ? QString()
        : QLocale().formattedDataSize(first.value(QStringLiteral("size")).toLongLong());
  } else {
    m_mimeText.clear();
    m_pathText.clear();
    m_modifiedText.clear();
    m_permissionsText.clear();
    m_sizeText.clear();
  }
  emit stateChanged();

  // Folder totals need a real walk; run it off-thread so the dialog opens
  // instantly even over deep trees.
  bool needsWalk = !single || isDirectory;
  if (!needsWalk) {
    return;
  }
  startTotalWalk(entries);
}

void EntryPropertiesController::clear() {
  stopWorker();
  if (!m_active && m_entryCount == 0) {
    return;
  }
  m_active = false;
  m_entryCount = 0;
  m_computingTotal = false;
  m_totalSizeText.clear();
  m_totalTruncated = false;
  emit stateChanged();
}

void EntryPropertiesController::stopWorker() {
  // AGENT-GUARD: Same disposal contract as SearchController::cancel() —
  // invalidate the generation BEFORE touching the worker; the queued
  // totalReady lambda re-checks it and never dereferences a worker this
  // object already joined and deleted (Qt delivers queued context-bound
  // calls even after sender destruction).
  ++m_generation;
  if (m_worker == nullptr) {
    return;
  }
  m_cancellation->store(true, std::memory_order_relaxed);
  m_worker->wait();
  delete m_worker;
  m_worker = nullptr;
}

void EntryPropertiesController::startTotalWalk(const QVariantList &entries) {
  QVector<WalkRoot> roots;
  roots.reserve(entries.size());
  for (const QVariant &entry : entries) {
    const QVariantMap map = entry.toMap();
    roots.append({map.value(QStringLiteral("path")).toString(),
                  map.value(QStringLiteral("isDirectory")).toBool(),
                  map.value(QStringLiteral("isSymlink")).toBool(),
                  map.value(QStringLiteral("size")).toLongLong()});
  }
  m_cancellation = std::make_shared<std::atomic_bool>(false);
  const quint64 generation = ++m_generation;
  auto *worker = new TotalSizeWorker(roots, m_cancellation, nullptr);
  m_worker = worker;
  m_computingTotal = true;
  emit stateChanged();
  const QPointer<EntryPropertiesController> guard(this);
  // Results arrive by value; the lambda touches the worker only while this
  // controller still owns it (generation still current).
  QObject::connect(worker, &TotalSizeWorker::totalReady, this,
                   [guard, generation](qint64 totalBytes, bool truncated,
                                       bool cancelled) {
                     if (!guard || guard->m_generation != generation) {
                       return;
                     }
                     QThread *finished = guard->m_worker;
                     guard->m_worker = nullptr;
                     if (finished != nullptr) {
                       finished->wait();
                       delete finished;
                     }
                     guard->m_computingTotal = false;
                     if (!cancelled) {
                       guard->m_totalTruncated = truncated;
                       guard->m_totalSizeText =
                           QLocale().formattedDataSize(totalBytes);
                     }
                     emit guard->stateChanged();
                   });
  worker->start();
}

} // namespace QindaQt::Apps::FileManager

#include "entry_properties.moc"
