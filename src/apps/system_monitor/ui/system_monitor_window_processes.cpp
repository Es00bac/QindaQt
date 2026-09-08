// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_window.h"

#include "../core/monitor_engine.h"
#include "monitor_controller.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

namespace QindaQt::Apps::SystemMonitor {
namespace {

QString formatBytes(const QVariant &value, bool rate) {
  if (!value.isValid() || value.isNull()) {
    return QObject::tr("Unavailable");
  }
  static const QStringList units{QStringLiteral("B"), QStringLiteral("KiB"),
                                 QStringLiteral("MiB"), QStringLiteral("GiB"),
                                 QStringLiteral("TiB")};
  const qint64 bytes = qMax<qint64>(0, value.toLongLong());
  double number = static_cast<double>(bytes);
  int unit = 0;
  while (number >= 1024.0 && unit < units.size() - 1) {
    number /= 1024.0;
    ++unit;
  }
  return QString::number(number, 'f', unit == 0 ? 0 : 1) + QLatin1Char(' ') +
         units.at(unit) + (rate ? QStringLiteral("/s") : QString());
}

class ProcessSortProxyModel final : public QSortFilterProxyModel {
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      static const QStringList labels{
          QStringLiteral("PID"), QStringLiteral("Process"),
          QStringLiteral("CPU (%)"), QStringLiteral("Memory"),
          QStringLiteral("Read"), QStringLiteral("Write"),
          QStringLiteral("User"), QStringLiteral("State"),
          QStringLiteral("Threads"), QStringLiteral("Nice"),
          QStringLiteral("Command")};
      return labels.value(section);
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
  }
};

class ProcessValueDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

protected:
  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    if (index.column() == 2 && index.data().isValid()) {
      option->text = QString::number(index.data().toDouble(), 'f', 1) +
                     QLatin1Char('%');
    } else if (index.column() == 3) {
      option->text = formatBytes(index.data(), false);
    } else if (index.column() == 4 || index.column() == 5) {
      option->text = formatBytes(index.data(), true);
    }
  }
};

} // namespace

QWidget *SystemMonitorWindow::buildProcessesPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *filter = new QLineEdit(page);
  filter->setAccessibleName(tr("Filter processes"));
  filter->setPlaceholderText(tr("Filter by name, user, or command"));
  filter->setClearButtonEnabled(true);
  layout->addWidget(filter);

  auto &engine = m_controller.engine();
  m_processProxy = new ProcessSortProxyModel(this);
  m_processProxy->setSourceModel(engine.processes());
  m_processProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_processProxy->setFilterKeyColumn(-1);
  m_processProxy->setSortCaseSensitivity(Qt::CaseInsensitive);
  m_processTable = new QTableView(page);
  m_processTable->setObjectName(QStringLiteral("processTable"));
  m_processTable->setModel(m_processProxy);
  m_processTable->setItemDelegate(new ProcessValueDelegate(m_processTable));
  m_processTable->setSortingEnabled(true);
  m_processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_processTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_processTable->setWordWrap(false);
  m_processTable->verticalHeader()->hide();
  m_processTable->horizontalHeader()->setStretchLastSection(true);
  layout->addWidget(m_processTable, 1);

  m_processDetails =
      new QLabel(tr("Select a process for details and actions."), page);
  m_processDetails->setWordWrap(true);
  layout->addWidget(m_processDetails);
  connect(filter, &QLineEdit::textChanged, m_processProxy,
          &QSortFilterProxyModel::setFilterFixedString);
  connect(m_processTable->selectionModel(),
          &QItemSelectionModel::selectionChanged, this, [this] {
            const QVariant pid = selectedValue("pid");
            const QVariant startTicks = selectedValue("startTicks");
            if (pid.isValid() && startTicks.isValid()) {
              m_selectedProcessPid = pid.toLongLong();
              m_selectedProcessStartTicks = startTicks.toULongLong();
            }
            showSelectedProcessDetails();
            updateProcessActionAvailability();
          });
  connect(m_processProxy, &QAbstractItemModel::modelReset, this,
          &SystemMonitorWindow::restoreSelectedProcess);
  return page;
}

int SystemMonitorWindow::processRole(const QByteArray &name) const {
  const QHash<int, QByteArray> roles =
      m_controller.engine().processes()->roleNames();
  for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
    if (it.value() == name) {
      return it.key();
    }
  }
  return -1;
}

QVariant SystemMonitorWindow::selectedValue(const QByteArray &role) const {
  if (!m_processTable || !m_processTable->selectionModel()) {
    return {};
  }
  const QModelIndex proxyIndex =
      m_processTable->selectionModel()->currentIndex();
  if (!proxyIndex.isValid()) {
    return {};
  }
  const int roleId = processRole(role);
  if (roleId < 0) {
    return {};
  }
  return m_controller.engine().processes()->data(
      m_processProxy->mapToSource(proxyIndex), roleId);
}

void SystemMonitorWindow::updateProcessActionAvailability() {
  const bool selected = selectedValue("pid").isValid() &&
                        selectedValue("startTicks").isValid();
  for (QAction *action : m_processActions) {
    action->setEnabled(selected);
  }
}

void SystemMonitorWindow::restoreSelectedProcess() {
  if (m_selectedProcessPid < 0 || !m_processTable) {
    updateProcessActionAvailability();
    return;
  }
  const int pidRole = processRole("pid");
  const int startRole = processRole("startTicks");
  for (int row = 0; row < m_processProxy->rowCount(); ++row) {
    const QModelIndex proxyIndex = m_processProxy->index(row, 0);
    const QModelIndex sourceIndex = m_processProxy->mapToSource(proxyIndex);
    const auto *model = m_controller.engine().processes();
    if (model->data(sourceIndex, pidRole).toLongLong() == m_selectedProcessPid &&
        model->data(sourceIndex, startRole).toULongLong() ==
            m_selectedProcessStartTicks) {
      m_processTable->selectionModel()->setCurrentIndex(
          proxyIndex, QItemSelectionModel::ClearAndSelect |
                          QItemSelectionModel::Rows);
      showSelectedProcessDetails();
      updateProcessActionAvailability();
      return;
    }
  }
  m_selectedProcessPid = -1;
  m_selectedProcessStartTicks = 0;
  m_processTable->clearSelection();
  showSelectedProcessDetails();
  updateProcessActionAvailability();
}

void SystemMonitorWindow::showSelectedProcessDetails() {
  const QVariant pid = selectedValue("pid");
  if (!pid.isValid()) {
    m_processDetails->setText(tr("Select a process for details and actions."));
    return;
  }
  const QVariant memory = selectedValue("memory");
  m_processDetails->setText(
      tr("%1 (PID %2) · %3 · CPU %4% · Memory %5")
          .arg(selectedValue("name").toString(), pid.toString(),
               selectedValue("user").toString(),
               selectedValue("cpu").toString(),
               memory.isValid() && !memory.isNull()
                   ? bytes(memory.toLongLong())
                   : tr("Unavailable")));
}

void SystemMonitorWindow::triggerProcessAction(const QString &action,
                                               int value) {
  const QVariant pid = selectedValue("pid");
  const QVariant startTicks = selectedValue("startTicks");
  if (!pid.isValid() || !startTicks.isValid()) {
    statusBar()->showMessage(tr("Select a process first."), 3000);
    return;
  }
  const bool destructive = action == QLatin1String("terminate") ||
                           action == QLatin1String("kill");
  if (destructive && QMessageBox::question(
                         this, tr("Confirm process action"),
                         tr("%1 %2 (PID %3)?")
                             .arg(action.left(1).toUpper() + action.mid(1),
                                  selectedValue("name").toString(),
                                  pid.toString())) != QMessageBox::Yes) {
    return;
  }
  const QString engineAction =
      action == QLatin1String("pause") ? QStringLiteral("stop")
      : action == QLatin1String("resume") ? QStringLiteral("continue")
                                              : action;
  const QString error = m_controller.engine().processAction(
      pid.toLongLong(), startTicks.toULongLong(), engineAction, value);
  statusBar()->showMessage(
      error.isEmpty()
          ? tr("%1 requested for PID %2.").arg(action, pid.toString())
          : error,
      5000);
}

QString SystemMonitorWindow::bytes(qint64 value) {
  return formatBytes(value, false);
}

QString SystemMonitorWindow::rate(const QVariant &value) {
  return !value.isValid() || value.isNull()
             ? tr("Waiting for baseline")
             : formatBytes(value, true);
}

} // namespace QindaQt::Apps::SystemMonitor
