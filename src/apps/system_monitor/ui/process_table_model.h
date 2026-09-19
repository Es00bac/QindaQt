// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::SystemMonitor {

/**
 * The process list as the interface consumes it: filtered, ordered either
 * flat or as a parent/child tree, and carrying a short per-process CPU
 * history for the row sparklines.
 *
 * This is a view model, not a second sampler. It copies each generation out
 * of the engine's ProcessModel and never reads procfs itself, which keeps the
 * ADR-0108 boundary intact.
 *
 * Why not a QSortFilterProxyModel: tree assembly is not a filter. A tree has
 * to place a surviving child under a parent that the filter itself removed,
 * and produce a stable depth for each row; expressing that as filterAcceptsRow
 * requires the proxy to answer questions about rows it has already rejected.
 * Flattening the order here makes both modes one code path and gives QML an
 * O(1) get().
 *
 * get(row) returns one record for Tk.DataTable, whose cells reach their data
 * only through it. `sortKey` is a role name; sorting is applied here rather
 * than in QML because only this class knows that "cpu" is a number and
 * "command" is text.
 */
class ProcessTableModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(QAbstractItemModel *source READ source WRITE setSource NOTIFY
                 sourceChanged)
  Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY orderChanged)
  Q_PROPERTY(
      Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY orderChanged)
  Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
                 filterChanged)
  Q_PROPERTY(bool treeMode READ treeMode WRITE setTreeMode NOTIFY orderChanged)
  Q_PROPERTY(bool ownProcessesOnly READ ownProcessesOnly WRITE
                 setOwnProcessesOnly NOTIFY filterChanged)
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
  Q_PROPERTY(int historyLength READ historyLength WRITE setHistoryLength NOTIFY
                 historyLengthChanged)

public:
  enum Role {
    PidRole = Qt::UserRole + 1,
    PpidRole,
    StartTicksRole,
    NameRole,
    CpuRole,
    MemoryRole,
    ReadRateRole,
    WriteRateRole,
    UserRole,
    StateRole,
    ThreadsRole,
    NiceRole,
    CommandRole,
    DepthRole,
    HasChildrenRole,
    CollapsedRole,
    CpuHistoryRole,
  };
  Q_ENUM(Role)

  explicit ProcessTableModel(QObject *parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index,
                              int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] QAbstractItemModel *source() const { return m_source; }
  void setSource(QAbstractItemModel *source);
  [[nodiscard]] QString sortKey() const { return m_sortKey; }
  void setSortKey(const QString &key);
  [[nodiscard]] Qt::SortOrder sortOrder() const { return m_sortOrder; }
  void setSortOrder(Qt::SortOrder order);
  [[nodiscard]] QString filterText() const { return m_filterText; }
  void setFilterText(const QString &text);
  [[nodiscard]] bool treeMode() const { return m_treeMode; }
  void setTreeMode(bool tree);
  [[nodiscard]] bool ownProcessesOnly() const { return m_ownOnly; }
  void setOwnProcessesOnly(bool own);
  [[nodiscard]] int count() const { return int(m_rows.size()); }
  [[nodiscard]] int totalCount() const { return m_totalCount; }
  [[nodiscard]] int historyLength() const { return m_historyLength; }
  void setHistoryLength(int length);

  /// One record for Tk.DataTable. An out-of-range row returns an empty map.
  Q_INVOKABLE [[nodiscard]] QVariantMap get(int row) const;
  /// The row showing this process, or -1 when it is filtered away or gone.
  Q_INVOKABLE [[nodiscard]] int indexOfProcess(qint64 pid,
                                               quint64 startTicks) const;
  /// Folds or unfolds a subtree. No effect outside tree mode.
  Q_INVOKABLE void toggleCollapsed(int row);
  Q_INVOKABLE void expandAll();

Q_SIGNALS:
  void sourceChanged();
  void orderChanged();
  void filterChanged();
  void countChanged();
  void historyLengthChanged();

private:
  struct Entry final {
    qint64 pid = 0;
    qint64 ppid = 0;
    quint64 startTicks = 0;
    QString name;
    QString user;
    QString state;
    QString command;
    double cpu = 0.0;
    bool cpuKnown = false;
    qulonglong memory = 0;
    bool memoryKnown = false;
    double readRate = 0.0;
    bool readKnown = false;
    double writeRate = 0.0;
    bool writeKnown = false;
    int threads = 0;
    int nice = 0;
    int depth = 0;
    bool hasChildren = false;
  };

  /// (pid, startTicks) -- a pid alone is reused and would splice one
  /// process' history onto another's row.
  using Identity = QPair<qint64, quint64>;

  void rebuild();
  [[nodiscard]] QVector<Entry> collect() const;
  [[nodiscard]] bool matches(const Entry &entry) const;
  void recordHistory(const QVector<Entry> &entries);
  [[nodiscard]] QVector<Entry> flatten(QVector<Entry> entries) const;
  [[nodiscard]] QVector<Entry> asTree(const QVector<Entry> &entries) const;
  void sortEntries(QVector<Entry> &entries) const;

  QAbstractItemModel *m_source = nullptr;
  QVector<Entry> m_rows;
  QHash<Identity, QVector<float>> m_history;
  QHash<Identity, bool> m_collapsed;
  QString m_sortKey = QStringLiteral("cpu");
  QString m_filterText;
  QString m_ownUser;
  Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
  int m_totalCount = 0;
  int m_historyLength = 40;
  bool m_treeMode = false;
  bool m_ownOnly = false;
};

} // namespace QindaQt::SystemMonitor
