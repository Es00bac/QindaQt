// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_table_model.h"

#include <QSet>

#include <pwd.h>
#include <unistd.h>

#include <algorithm>

namespace QindaQt::SystemMonitor {

namespace {

// Role numbers of the engine's ProcessModel, resolved by name so this view
// model does not include its header or depend on the enum's order.
int roleFor(const QAbstractItemModel *model, const QByteArray &name) {
  const QHash<int, QByteArray> names = model->roleNames();
  for (auto it = names.cbegin(); it != names.cend(); ++it) {
    if (it.value() == name) {
      return it.key();
    }
  }
  return -1;
}

} // namespace

ProcessTableModel::ProcessTableModel(QObject *parent)
    : QAbstractListModel(parent) {
  // Resolved once: "my processes" must mean the user the monitor runs as,
  // not $USER, which a sudo or a service unit can disagree with.
  if (const passwd *entry = getpwuid(geteuid())) {
    m_ownUser = QString::fromLocal8Bit(entry->pw_name);
  }
}

int ProcessTableModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> ProcessTableModel::roleNames() const {
  return {{PidRole, "pid"},
          {PpidRole, "ppid"},
          {StartTicksRole, "startTicks"},
          {NameRole, "name"},
          {CpuRole, "cpu"},
          {MemoryRole, "memory"},
          {ReadRateRole, "readRate"},
          {WriteRateRole, "writeRate"},
          {UserRole, "user"},
          {StateRole, "state"},
          {ThreadsRole, "threads"},
          {NiceRole, "nice"},
          {CommandRole, "command"},
          {DepthRole, "depth"},
          {HasChildrenRole, "hasChildren"},
          {CollapsedRole, "collapsed"},
          {CpuHistoryRole, "cpuHistory"}};
}

QVariant ProcessTableModel::data(const QModelIndex &index, int role) const {
  if (index.row() < 0 || index.row() >= m_rows.size()) {
    return {};
  }
  const Entry &entry = m_rows.at(index.row());
  switch (role) {
  case PidRole:
    return entry.pid;
  case PpidRole:
    return entry.ppid;
  case StartTicksRole:
    return QVariant::fromValue(entry.startTicks);
  case NameRole:
    return entry.name;
  // AGENT-GUARD: an unavailable counter stays null. Substituting 0 would
  // report "this process did no I/O" when the truth is "procfs would not say".
  case CpuRole:
    return entry.cpuKnown ? QVariant(entry.cpu) : QVariant();
  case MemoryRole:
    return entry.memoryKnown ? QVariant(entry.memory) : QVariant();
  case ReadRateRole:
    return entry.readKnown ? QVariant(entry.readRate) : QVariant();
  case WriteRateRole:
    return entry.writeKnown ? QVariant(entry.writeRate) : QVariant();
  case UserRole:
    return entry.user;
  case StateRole:
    return entry.state;
  case ThreadsRole:
    return entry.threads;
  case NiceRole:
    return entry.nice;
  case CommandRole:
    return entry.command;
  case DepthRole:
    return entry.depth;
  case HasChildrenRole:
    return entry.hasChildren;
  case CollapsedRole:
    return m_collapsed.value({entry.pid, entry.startTicks}, false);
  case CpuHistoryRole: {
    QVariantList out;
    const QVector<float> &samples = m_history[{entry.pid, entry.startTicks}];
    out.reserve(samples.size());
    for (const float sample : samples) {
      out.append(double(sample));
    }
    return out;
  }
  default:
    return {};
  }
}

QVariantMap ProcessTableModel::get(int row) const {
  QVariantMap record;
  if (row < 0 || row >= m_rows.size()) {
    return record;
  }
  const QHash<int, QByteArray> names = roleNames();
  for (auto it = names.cbegin(); it != names.cend(); ++it) {
    record.insert(QString::fromLatin1(it.value()), data(index(row), it.key()));
  }
  return record;
}

int ProcessTableModel::indexOfProcess(qint64 pid, quint64 startTicks) const {
  for (int i = 0; i < m_rows.size(); ++i) {
    if (m_rows.at(i).pid == pid && m_rows.at(i).startTicks == startTicks) {
      return i;
    }
  }
  return -1;
}

void ProcessTableModel::setSource(QAbstractItemModel *source) {
  if (m_source == source) {
    return;
  }
  if (m_source != nullptr) {
    m_source->disconnect(this);
  }
  m_source = source;
  if (m_source != nullptr) {
    connect(m_source, &QAbstractItemModel::modelReset, this,
            &ProcessTableModel::rebuild);
    connect(m_source, &QAbstractItemModel::dataChanged, this,
            &ProcessTableModel::rebuild);
    connect(m_source, &QAbstractItemModel::rowsInserted, this,
            &ProcessTableModel::rebuild);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this,
            &ProcessTableModel::rebuild);
  }
  Q_EMIT sourceChanged();
  rebuild();
}

void ProcessTableModel::setSortKey(const QString &key) {
  if (m_sortKey != key) {
    m_sortKey = key;
    Q_EMIT orderChanged();
    refresh(false);
  }
}

void ProcessTableModel::setSortOrder(Qt::SortOrder order) {
  if (m_sortOrder != order) {
    m_sortOrder = order;
    Q_EMIT orderChanged();
    refresh(false);
  }
}

void ProcessTableModel::setFilterText(const QString &text) {
  if (m_filterText != text) {
    m_filterText = text;
    Q_EMIT filterChanged();
    refresh(false);
  }
}

void ProcessTableModel::setTreeMode(bool tree) {
  if (m_treeMode != tree) {
    m_treeMode = tree;
    Q_EMIT orderChanged();
    refresh(false);
  }
}

void ProcessTableModel::setOwnProcessesOnly(bool own) {
  if (m_ownOnly != own) {
    m_ownOnly = own;
    Q_EMIT filterChanged();
    refresh(false);
  }
}

void ProcessTableModel::setHistoryLength(int length) {
  const int wanted = std::clamp(length, 2, 600);
  if (m_historyLength != wanted) {
    m_historyLength = wanted;
    Q_EMIT historyLengthChanged();
  }
}

void ProcessTableModel::toggleCollapsed(int row) {
  if (!m_treeMode || row < 0 || row >= m_rows.size()) {
    return;
  }
  const Entry &entry = m_rows.at(row);
  const Identity identity{entry.pid, entry.startTicks};
  m_collapsed.insert(identity, !m_collapsed.value(identity, false));
  refresh(false);
}

void ProcessTableModel::expandAll() {
  if (m_collapsed.isEmpty()) {
    return;
  }
  m_collapsed.clear();
  refresh(false);
}

QVector<ProcessTableModel::Entry> ProcessTableModel::collect() const {
  QVector<Entry> entries;
  if (m_source == nullptr) {
    return entries;
  }
  const int pid = roleFor(m_source, "pid");
  const int ppid = roleFor(m_source, "ppid");
  const int startTicks = roleFor(m_source, "startTicks");
  const int name = roleFor(m_source, "name");
  const int cpu = roleFor(m_source, "cpu");
  const int memory = roleFor(m_source, "memory");
  const int readRate = roleFor(m_source, "readRate");
  const int writeRate = roleFor(m_source, "writeRate");
  const int user = roleFor(m_source, "user");
  const int state = roleFor(m_source, "state");
  const int threads = roleFor(m_source, "threads");
  const int nice = roleFor(m_source, "nice");
  const int command = roleFor(m_source, "command");

  const int rows = m_source->rowCount();
  entries.reserve(rows);
  for (int row = 0; row < rows; ++row) {
    const QModelIndex index = m_source->index(row, 0);
    Entry entry;
    entry.pid = m_source->data(index, pid).toLongLong();
    entry.ppid = m_source->data(index, ppid).toLongLong();
    entry.startTicks = m_source->data(index, startTicks).toULongLong();
    entry.name = m_source->data(index, name).toString();
    entry.user = m_source->data(index, user).toString();
    entry.state = m_source->data(index, state).toString();
    entry.command = m_source->data(index, command).toString();
    entry.threads = m_source->data(index, threads).toInt();
    entry.nice = m_source->data(index, nice).toInt();

    const QVariant cpuValue = m_source->data(index, cpu);
    entry.cpuKnown = cpuValue.isValid() && !cpuValue.isNull();
    entry.cpu = entry.cpuKnown ? cpuValue.toDouble() : 0.0;
    const QVariant memoryValue = m_source->data(index, memory);
    entry.memoryKnown = memoryValue.isValid() && !memoryValue.isNull();
    entry.memory = entry.memoryKnown ? memoryValue.toULongLong() : 0;
    const QVariant readValue = m_source->data(index, readRate);
    entry.readKnown = readValue.isValid() && !readValue.isNull();
    entry.readRate = entry.readKnown ? readValue.toDouble() : 0.0;
    const QVariant writeValue = m_source->data(index, writeRate);
    entry.writeKnown = writeValue.isValid() && !writeValue.isNull();
    entry.writeRate = entry.writeKnown ? writeValue.toDouble() : 0.0;
    entries.append(entry);
  }
  return entries;
}

bool ProcessTableModel::matches(const Entry &entry) const {
  if (m_ownOnly && !m_ownUser.isEmpty() && entry.user != m_ownUser) {
    return false;
  }
  if (m_filterText.isEmpty()) {
    return true;
  }
  const Qt::CaseSensitivity insensitive = Qt::CaseInsensitive;
  return entry.name.contains(m_filterText, insensitive)
         || entry.command.contains(m_filterText, insensitive)
         || entry.user.contains(m_filterText, insensitive)
         || QString::number(entry.pid).contains(m_filterText);
}

void ProcessTableModel::recordHistory(const QVector<Entry> &entries) {
  QHash<Identity, QVector<float>> next;
  next.reserve(entries.size());
  for (const Entry &entry : entries) {
    const Identity identity{entry.pid, entry.startTicks};
    QVector<float> samples = m_history.value(identity);
    samples.append(float(entry.cpu));
    while (samples.size() > m_historyLength) {
      samples.removeFirst();
    }
    next.insert(identity, samples);
  }
  // Histories of processes that have exited are dropped here rather than
  // accumulating for the life of the session.
  m_history = std::move(next);
}

void ProcessTableModel::sortEntries(QVector<Entry> &entries) const {
  const bool ascending = m_sortOrder == Qt::AscendingOrder;
  const QString key = m_sortKey;
  auto less = [&key](const Entry &a, const Entry &b) {
    if (key == QLatin1String("pid")) {
      return a.pid < b.pid;
    }
    if (key == QLatin1String("memory")) {
      return a.memory < b.memory;
    }
    if (key == QLatin1String("threads")) {
      return a.threads < b.threads;
    }
    if (key == QLatin1String("nice")) {
      return a.nice < b.nice;
    }
    if (key == QLatin1String("readRate")) {
      return a.readRate < b.readRate;
    }
    if (key == QLatin1String("writeRate")) {
      return a.writeRate < b.writeRate;
    }
    if (key == QLatin1String("user")) {
      return a.user.compare(b.user, Qt::CaseInsensitive) < 0;
    }
    if (key == QLatin1String("command")) {
      return a.command.compare(b.command, Qt::CaseInsensitive) < 0;
    }
    if (key == QLatin1String("name")) {
      return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    }
    return a.cpu < b.cpu;
  };
  std::stable_sort(entries.begin(), entries.end(),
                   [&](const Entry &a, const Entry &b) {
                     return ascending ? less(a, b) : less(b, a);
                   });
}

QVector<ProcessTableModel::Entry>
ProcessTableModel::flatten(QVector<Entry> entries) const {
  sortEntries(entries);
  return entries;
}

QVector<ProcessTableModel::Entry>
ProcessTableModel::asTree(const QVector<Entry> &entries) const {
  QHash<qint64, QVector<int>> childrenOf;
  QHash<qint64, int> indexOfPid;
  for (int i = 0; i < entries.size(); ++i) {
    indexOfPid.insert(entries.at(i).pid, i);
  }
  QVector<int> roots;
  for (int i = 0; i < entries.size(); ++i) {
    const Entry &entry = entries.at(i);
    // A process whose parent did not survive the filter (or is outside this
    // namespace) is shown as a root rather than dropped.
    if (entry.ppid > 0 && indexOfPid.contains(entry.ppid)
        && entry.ppid != entry.pid) {
      childrenOf[entry.ppid].append(i);
    } else {
      roots.append(i);
    }
  }

  auto orderIndices = [&](QVector<int> &indices) {
    QVector<Entry> slice;
    slice.reserve(indices.size());
    for (const int i : indices) {
      slice.append(entries.at(i));
    }
    sortEntries(slice);
    QVector<int> ordered;
    ordered.reserve(slice.size());
    for (const Entry &entry : slice) {
      ordered.append(indexOfPid.value(entry.pid, 0));
    }
    indices = ordered;
  };

  QVector<Entry> out;
  out.reserve(entries.size());
  orderIndices(roots);

  // Iterative walk: a deep process tree would otherwise risk the stack, and
  // a cycle in ppid (which a malformed namespace can present) must terminate.
  struct Frame final {
    int index;
    int depth;
  };
  QVector<Frame> stack;
  for (qsizetype i = roots.size() - 1; i >= 0; --i) {
    stack.append({roots.at(i), 0});
  }
  QSet<int> seen;
  while (!stack.isEmpty()) {
    const Frame frame = stack.takeLast();
    if (seen.contains(frame.index)) {
      continue;
    }
    seen.insert(frame.index);
    Entry entry = entries.at(frame.index);
    entry.depth = frame.depth;
    QVector<int> children = childrenOf.value(entry.pid);
    entry.hasChildren = !children.isEmpty();
    out.append(entry);
    if (entry.hasChildren
        && !m_collapsed.value({entry.pid, entry.startTicks}, false)) {
      orderIndices(children);
      for (qsizetype i = children.size() - 1; i >= 0; --i) {
        stack.append({children.at(i), frame.depth + 1});
      }
    }
  }
  return out;
}

void ProcessTableModel::rebuild() { refresh(true); }

void ProcessTableModel::refresh(bool advanceHistory) {
  const QVector<Entry> all = collect();
  if (advanceHistory) {
    recordHistory(all);
  }

  QVector<Entry> kept;
  kept.reserve(all.size());
  for (const Entry &entry : all) {
    if (matches(entry)) {
      kept.append(entry);
    }
  }

  QVector<Entry> next = m_treeMode ? asTree(kept) : flatten(std::move(kept));

  // AGENT-GUARD: a reset every tick would send the list back to the top and
  // drop the selection once a second, which makes the table unusable while
  // the machine is busy -- exactly when someone is reading it. When the rows
  // are the same processes in the same order, publish the new readings as a
  // data change instead and the view keeps its scroll position.
  bool sameShape = next.size() == m_rows.size();
  for (qsizetype i = 0; sameShape && i < next.size(); ++i) {
    sameShape = next.at(i).pid == m_rows.at(i).pid
                && next.at(i).startTicks == m_rows.at(i).startTicks
                && next.at(i).depth == m_rows.at(i).depth;
  }

  const int previousTotal = m_totalCount;
  m_totalCount = int(all.size());
  if (sameShape) {
    m_rows = std::move(next);
    if (!m_rows.isEmpty()) {
      Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1));
    }
  } else {
    beginResetModel();
    m_rows = std::move(next);
    endResetModel();
  }
  if (!sameShape || previousTotal != m_totalCount) {
    Q_EMIT countChanged();
  }
}

} // namespace QindaQt::SystemMonitor
