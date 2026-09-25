// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCache>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <memory>

namespace QindaQt::Apps::FileManager {

// ADR-0270: the Details columns whose values are not in the listing -- a
// folder's item count, an image's pixel dimensions, and the owner and group
// names behind the listing's numeric ids -- fetched for the rows a view
// actually shows, never for the whole folder.
//
// AGENT-CONTRACT: a GUI-thread QObject. itemCount() and dimensions() answer
// from a bounded cache, or return an empty string and queue one read on a
// private one-thread pool; `revision` increments (at most every 50 ms) when
// answers land, and a QML cell re-asks inside a binding that reads it. Cache
// keys include the entry's modification stamp, so a changed file is read
// again. Only local absolute paths are read: a network URL or an
// Applications row stays unknown (an empty string, shown as a dash). The
// dimensions read follows the preview pipeline's format allowlist (ADR-0111)
// and never follows a symlink. Destruction drops queued reads and waits for
// the one in flight.
class EntryFacts final : public QObject {
  Q_OBJECT
  Q_PROPERTY(quint64 revision READ revision NOTIFY revisionChanged FINAL)

public:
  static constexpr int maximumCachedFacts = 4096;
  static constexpr int maximumPendingReads = 256;
  static constexpr int maximumCountedItems = 100000;

  explicit EntryFacts(QObject *parent = nullptr);
  ~EntryFacts() override;

  [[nodiscard]] quint64 revision() const { return m_revision; }

  // A user or group name for a listing id; the number itself when the
  // system has no name for it, and "" for an unknown (-1) id.
  Q_INVOKABLE [[nodiscard]] QString ownerName(qint64 id);
  Q_INVOKABLE [[nodiscard]] QString groupName(qint64 id);
  // "12", or "100,000+" at the bound; "" while unknown or unreadable.
  // Counts the entries a folder shows by default (no dot names).
  Q_INVOKABLE [[nodiscard]] QString itemCount(const QString &path, const QString &stamp);
  // "1920 × 1080" for a supported raster image; "" otherwise or while unknown.
  Q_INVOKABLE [[nodiscard]] QString dimensions(const QString &path, const QString &stamp);

signals:
  void revisionChanged();

private:
  enum class Fact { Items, Dimensions };

  [[nodiscard]] QString lookup(Fact fact, const QString &path, const QString &stamp);
  void deliver(const QString &key, const QString &value);

  QCache<QString, QString> m_answers{maximumCachedFacts};
  QSet<QString> m_pending;
  QHash<qint64, QString> m_owners;
  QHash<qint64, QString> m_groups;
  std::shared_ptr<std::atomic_bool> m_closing = std::make_shared<std::atomic_bool>(false);
  quint64 m_revision = 0;
  QTimer m_publish;
  QThreadPool m_pool;
};

} // namespace QindaQt::Apps::FileManager
