// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAbstractListModel>
#include <QSignalSpy>
#include <QTest>

#include "process_table_model.h"

using namespace QindaQt::SystemMonitor;

namespace {

// A stand-in for the engine's ProcessModel: same role names, contents under
// the test's control. Using the real one would make these tests depend on
// whatever this machine happens to be running.
class FakeProcessModel final : public QAbstractListModel {
  Q_OBJECT

public:
  struct Row final {
    qint64 pid;
    qint64 ppid;
    QString name;
    QString user;
    double cpu;
    qulonglong memory;
    bool cpuKnown = true;
    bool memoryKnown = true;
  };

  void setRows(QVector<Row> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
  }

  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : int(m_rows.size());
  }

  [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
    return {{Qt::UserRole + 1, "pid"},        {Qt::UserRole + 2, "ppid"},
            {Qt::UserRole + 3, "startTicks"}, {Qt::UserRole + 4, "name"},
            {Qt::UserRole + 5, "cpu"},        {Qt::UserRole + 6, "memory"},
            {Qt::UserRole + 7, "readRate"},   {Qt::UserRole + 8, "writeRate"},
            {Qt::UserRole + 9, "user"},       {Qt::UserRole + 10, "state"},
            {Qt::UserRole + 11, "threads"},   {Qt::UserRole + 12, "nice"},
            {Qt::UserRole + 13, "command"}};
  }

  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override {
    if (index.row() < 0 || index.row() >= m_rows.size()) {
      return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role - Qt::UserRole) {
    case 1:
      return row.pid;
    case 2:
      return row.ppid;
    case 3:
      return QVariant::fromValue(quint64(1000));
    case 4:
      return row.name;
    case 5:
      return row.cpuKnown ? QVariant(row.cpu) : QVariant();
    case 6:
      return row.memoryKnown ? QVariant(row.memory) : QVariant();
    case 9:
      return row.user;
    case 13:
      return row.name + QStringLiteral(" --flag");
    default:
      return {};
    }
  }

private:
  QVector<Row> m_rows;
};

QVector<FakeProcessModel::Row> sampleRows() {
  return {{1, 0, QStringLiteral("systemd"), QStringLiteral("root"), 0.1, 10},
          {100, 1, QStringLiteral("shell"), QStringLiteral("cabewse"), 5.0, 400},
          {200, 100, QStringLiteral("editor"), QStringLiteral("cabewse"), 2.0, 300},
          {300, 100, QStringLiteral("browser"), QStringLiteral("cabewse"), 9.0, 900},
          {400, 999, QStringLiteral("orphan"), QStringLiteral("root"), 1.0, 50}};
}

} // namespace

class TestProcessTableModel : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void sortsNumericallyNotAsText();
  void sortOrderReverses();
  void filterMatchesNameCommandUserAndPid();
  void treeNestsChildrenUnderParents();
  void treeKeepsOrphansAsRoots();
  void treeFoldingHidesDescendants();
  void getReturnsOneRecordPerRole();
  void unavailableCountersStayNull();
  void cpuHistoryAccumulatesPerIdentity();
  void steadyRowsUpdateWithoutResetting();

private:
  FakeProcessModel *m_source = nullptr;
  ProcessTableModel *m_model = nullptr;
};

void TestProcessTableModel::init() {
  m_source = new FakeProcessModel;
  m_model = new ProcessTableModel;
  m_source->setRows(sampleRows());
  m_model->setSource(m_source);
}

void TestProcessTableModel::cleanup() {
  delete m_model;
  delete m_source;
}

void TestProcessTableModel::sortsNumericallyNotAsText() {
  m_model->setSortKey(QStringLiteral("memory"));
  m_model->setSortOrder(Qt::DescendingOrder);
  // As text, "900" would sort below "10"; the whole reason the table asks
  // the model to sort is that only the model knows these are numbers.
  QCOMPARE(m_model->get(0).value(QStringLiteral("memory")).toULongLong(), 900ULL);
  QCOMPARE(m_model->get(4).value(QStringLiteral("memory")).toULongLong(), 10ULL);
}

void TestProcessTableModel::sortOrderReverses() {
  m_model->setSortKey(QStringLiteral("cpu"));
  m_model->setSortOrder(Qt::DescendingOrder);
  QCOMPARE(m_model->get(0).value(QStringLiteral("name")).toString(),
           QStringLiteral("browser"));
  m_model->setSortOrder(Qt::AscendingOrder);
  QCOMPARE(m_model->get(0).value(QStringLiteral("name")).toString(),
           QStringLiteral("systemd"));
}

void TestProcessTableModel::filterMatchesNameCommandUserAndPid() {
  m_model->setFilterText(QStringLiteral("brow"));
  QCOMPARE(m_model->count(), 1);
  // The unfiltered total stays visible so the header can say "1 / 5".
  QCOMPARE(m_model->totalCount(), 5);

  m_model->setFilterText(QStringLiteral("cabewse"));
  QCOMPARE(m_model->count(), 3);

  m_model->setFilterText(QStringLiteral("--flag"));
  QCOMPARE(m_model->count(), 5);

  m_model->setFilterText(QStringLiteral("300"));
  QCOMPARE(m_model->count(), 1);

  m_model->setFilterText(QString());
  QCOMPARE(m_model->count(), 5);
}

void TestProcessTableModel::treeNestsChildrenUnderParents() {
  m_model->setTreeMode(true);
  m_model->setSortKey(QStringLiteral("pid"));
  m_model->setSortOrder(Qt::AscendingOrder);

  QCOMPARE(m_model->count(), 5);
  QCOMPARE(m_model->get(0).value(QStringLiteral("name")).toString(),
           QStringLiteral("systemd"));
  QCOMPARE(m_model->get(0).value(QStringLiteral("depth")).toInt(), 0);
  QVERIFY(m_model->get(0).value(QStringLiteral("hasChildren")).toBool());
  QCOMPARE(m_model->get(1).value(QStringLiteral("name")).toString(),
           QStringLiteral("shell"));
  QCOMPARE(m_model->get(1).value(QStringLiteral("depth")).toInt(), 1);
  // Both of shell's children sit under it, one level deeper.
  QCOMPARE(m_model->get(2).value(QStringLiteral("depth")).toInt(), 2);
  QCOMPARE(m_model->get(3).value(QStringLiteral("depth")).toInt(), 2);
}

void TestProcessTableModel::treeKeepsOrphansAsRoots() {
  m_model->setTreeMode(true);
  // pid 400's parent (999) is not in the list. Dropping it would hide a
  // running process because its parent lives in another namespace.
  const int row = m_model->indexOfProcess(400, 1000);
  QVERIFY(row >= 0);
  QCOMPARE(m_model->get(row).value(QStringLiteral("depth")).toInt(), 0);
}

void TestProcessTableModel::treeFoldingHidesDescendants() {
  m_model->setTreeMode(true);
  m_model->setSortKey(QStringLiteral("pid"));
  m_model->setSortOrder(Qt::AscendingOrder);
  QCOMPARE(m_model->count(), 5);

  const int shellRow = m_model->indexOfProcess(100, 1000);
  QVERIFY(shellRow >= 0);
  m_model->toggleCollapsed(shellRow);
  // editor and browser fold away; shell itself stays.
  QCOMPARE(m_model->count(), 3);
  QVERIFY(m_model->get(shellRow).value(QStringLiteral("collapsed")).toBool());

  m_model->expandAll();
  QCOMPARE(m_model->count(), 5);
}

void TestProcessTableModel::getReturnsOneRecordPerRole() {
  const QVariantMap record = m_model->get(0);
  for (const QByteArray &role : m_model->roleNames()) {
    QVERIFY2(record.contains(QString::fromLatin1(role)), role.constData());
  }
  QVERIFY(m_model->get(-1).isEmpty());
  QVERIFY(m_model->get(999).isEmpty());
}

void TestProcessTableModel::unavailableCountersStayNull() {
  QVector<FakeProcessModel::Row> rows = sampleRows();
  rows[0].memoryKnown = false;
  rows[0].cpuKnown = false;
  m_source->setRows(rows);
  m_model->setSortKey(QStringLiteral("pid"));
  m_model->setSortOrder(Qt::AscendingOrder);

  const QVariantMap record = m_model->get(0);
  // A 0 here would say "this process uses no memory", which is a claim
  // procfs declined to make.
  QVERIFY(record.value(QStringLiteral("memory")).isNull());
  QVERIFY(record.value(QStringLiteral("cpu")).isNull());
}

void TestProcessTableModel::cpuHistoryAccumulatesPerIdentity() {
  m_model->setSortKey(QStringLiteral("pid"));
  m_model->setSortOrder(Qt::AscendingOrder);
  QVector<FakeProcessModel::Row> rows = sampleRows();
  for (int tick = 0; tick < 3; ++tick) {
    rows[1].cpu = 10.0 + tick;
    m_source->setRows(rows);
  }
  const QVariantList history =
      m_model->get(1).value(QStringLiteral("cpuHistory")).toList();
  QCOMPARE(history.size(), 4);
  QCOMPARE(history.last().toDouble(), 12.0);

  // A process that exits takes its history with it rather than leaking for
  // the life of the session.
  QVector<FakeProcessModel::Row> fewer = sampleRows();
  fewer.removeAt(1);
  m_source->setRows(fewer);
  QCOMPARE(m_model->indexOfProcess(100, 1000), -1);
}

void TestProcessTableModel::steadyRowsUpdateWithoutResetting() {
  m_model->setSortKey(QStringLiteral("pid"));
  m_model->setSortOrder(Qt::AscendingOrder);

  QSignalSpy resets(m_model, &QAbstractItemModel::modelReset);
  QSignalSpy changes(m_model, &QAbstractItemModel::dataChanged);

  QVector<FakeProcessModel::Row> rows = sampleRows();
  rows[1].cpu = 77.0;
  m_source->setRows(rows);

  // AGENT-GUARD: the same processes in the same order must arrive as a data
  // change. A reset every tick sends the table back to the top and drops the
  // selection once a second -- exactly while the machine is busy.
  QCOMPARE(resets.count(), 0);
  QVERIFY(changes.count() >= 1);

  // A different set of processes is a real structural change.
  rows.removeLast();
  m_source->setRows(rows);
  QCOMPARE(resets.count(), 1);
}

QTEST_MAIN(TestProcessTableModel)
#include "tst_process_table_model.moc"
