// SPDX-License-Identifier: GPL-3.0-or-later
#include "system_monitor_window.h"

#include "../app_shell/system_monitor_action_catalog.h"
#include "../core/monitor_engine.h"
#include "metric_chart.h"
#include "monitor_controller.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDBusConnection>
#include <QDateTime>
#include <QDockWidget>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <array>
#include <cmath>
#include <limits>
#include <tuple>

namespace QindaQt::Apps::SystemMonitor {
namespace {

QString titleFor(const QString &id) {
  if (id == QLatin1String("cpu")) {
    return SystemMonitorWindow::tr("CPU");
  }
  return id.left(1).toUpper() + id.mid(1);
}

} // namespace

SystemMonitorWindow::SystemMonitorWindow(MonitorController &controller,
                                         QWidget *parent)
    : QMainWindow(parent), m_controller(controller), m_appShell(this) {
  setObjectName(QStringLiteral("systemMonitorWindow"));
  setWindowTitle(tr("QindaQt System Monitor"));
  setMinimumSize(400, 360);
  resize(1120, 720);
  buildChrome();

  auto &engine = m_controller.engine();
  connect(&engine, &QindaQt::SystemMonitor::MonitorEngine::updated, this,
          &SystemMonitorWindow::updateSnapshot);
  connect(&engine, &QindaQt::SystemMonitor::MonitorEngine::pausedChanged, this,
          &SystemMonitorWindow::synchronizePausedAction);
  connect(&engine, &QindaQt::SystemMonitor::MonitorEngine::intervalChanged, this,
          &SystemMonitorWindow::synchronizeIntervalActions);
  synchronizePausedAction();
  synchronizeIntervalActions();
  connect(
      &engine, &QindaQt::SystemMonitor::MonitorEngine::errorOccurred, this,
      [this](const QString &error) { statusBar()->showMessage(error, 6000); });
  connect(&m_controller, &MonitorController::hardwareUpdated, this,
          &SystemMonitorWindow::updateHardware);

  engine.requestSample();
}

SystemMonitorWindow::~SystemMonitorWindow() {
  // The exporter retains platform-window state, so tear it down while this
  // QWidget still owns a live QWindow; each view has its own bus connection.
  m_menuExport.reset();
}

void SystemMonitorWindow::composeMenuExport(const QDBusConnection &connection) {
  if (QWindow *handle = windowHandle()) {
    m_menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
        appShellCoordinator(), *handle, connection,
        [this](bool visible) { menuBar()->setVisible(visible); });
  }
}

QindaQt::AppShell::ApplicationCoordinator &
SystemMonitorWindow::appShellCoordinator() {
  return m_appShell.coordinator();
}

void SystemMonitorWindow::buildChrome() {
  QHash<QString, QAction *> appShellTargets;
  auto *navigation = new QDockWidget(tr("Views"), this);
  navigation->setFeatures(QDockWidget::NoDockWidgetFeatures);
  auto *rail = new QWidget(navigation);
  auto *railLayout = new QVBoxLayout(rail);
  railLayout->setContentsMargins(6, 8, 6, 8);
  railLayout->setSpacing(3);

  m_pages = new QStackedWidget(this);
  auto *viewActions = new QActionGroup(this);
  viewActions->setExclusive(true);
  const auto addPage = [this, rail, railLayout, viewActions](const QString &id,
                                                             QWidget *page) {
    const int index = m_pages->addWidget(page);
    m_pageIndex.insert(id, index);
    auto *action = new QAction(titleFor(id), this);
    action->setCheckable(true);
    action->setActionGroup(viewActions);
    m_viewActions.insert(id, action);
    auto *button = new QToolButton(rail);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setMinimumHeight(32);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    railLayout->addWidget(button);
    connect(action, &QAction::triggered, this,
            [this, index] { m_pages->setCurrentIndex(index); });
    if (index == 0) {
      action->setChecked(true);
    }
  };

  addPage(QStringLiteral("overview"), buildOverviewPage());
  addPage(QStringLiteral("processes"), buildProcessesPage());
  addPage(QStringLiteral("cpu"), buildCpuPage());
  addPage(QStringLiteral("memory"), buildMemoryPage());
  addPage(QStringLiteral("disks"), buildStoragePage(false));
  addPage(QStringLiteral("network"), buildStoragePage(true));
  addPage(QStringLiteral("hardware"), buildHardwarePage());
  railLayout->addStretch();
  navigation->setWidget(rail);
  addDockWidget(Qt::LeftDockWidgetArea, navigation);
  setCentralWidget(m_pages);

  auto *viewMenu = menuBar()->addMenu(tr("View"));
  auto *openView = viewMenu->addAction(tr("Open view in new window"));
  openView->setShortcut(QKeySequence::New);
  connect(openView, &QAction::triggered, this,
          &SystemMonitorWindow::openViewInWindow);
  appShellTargets.insert(QLatin1String(AppShellActionIds::ViewOpen), openView);
  auto *pause = viewMenu->addAction(tr("Pause updates"));
  pause->setCheckable(true);
  m_pauseAction = pause;
  pause->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+P")));
  connect(pause, &QAction::toggled, this, &SystemMonitorWindow::setPaused);
  appShellTargets.insert(QLatin1String(AppShellActionIds::ViewPause), pause);

  auto *intervalMenu = viewMenu->addMenu(tr("Update interval"));
  auto *intervalGroup = new QActionGroup(this);
  intervalGroup->setExclusive(true);
  const std::array intervalChoices{
      std::tuple{250, AppShellActionIds::ViewInterval250, "Ctrl+Alt+1"},
      std::tuple{500, AppShellActionIds::ViewInterval500, "Ctrl+Alt+2"},
      std::tuple{1000, AppShellActionIds::ViewInterval1000, "Ctrl+Alt+3"},
      std::tuple{2000, AppShellActionIds::ViewInterval2000, "Ctrl+Alt+4"},
      std::tuple{5000, AppShellActionIds::ViewInterval5000, "Ctrl+Alt+5"}};
  for (const auto &[milliseconds, id, shortcut] : intervalChoices) {
    auto *action = intervalMenu->addAction(tr("%1 ms").arg(milliseconds));
    action->setObjectName(QLatin1String(id));
    action->setCheckable(true);
    action->setActionGroup(intervalGroup);
    action->setShortcut(QKeySequence(QLatin1String(shortcut)));
    m_intervalActions.insert(milliseconds, action);
    appShellTargets.insert(QLatin1String(id), action);
    connect(action, &QAction::triggered, this,
            [this, milliseconds] { setInterval(milliseconds); });
  }
  auto *durationMenu = viewMenu->addMenu(tr("Graph duration"));
  auto *durationGroup = new QActionGroup(this);
  durationGroup->setExclusive(true);
  const std::array durationChoices{
      std::tuple{30, AppShellActionIds::ViewDuration30, "Ctrl+Shift+1"},
      std::tuple{60, AppShellActionIds::ViewDuration60, "Ctrl+Shift+2"},
      std::tuple{120, AppShellActionIds::ViewDuration120, "Ctrl+Shift+3"},
      std::tuple{300, AppShellActionIds::ViewDuration300, "Ctrl+Shift+4"}};
  for (const auto &[seconds, id, shortcut] : durationChoices) {
    auto *action = durationMenu->addAction(tr("%1 seconds").arg(seconds));
    action->setObjectName(QLatin1String(id));
    action->setCheckable(true);
    action->setActionGroup(durationGroup);
    action->setShortcut(QKeySequence(QLatin1String(shortcut)));
    action->setChecked(seconds == 120);
    appShellTargets.insert(QLatin1String(id), action);
    connect(action, &QAction::triggered, this,
            [this, seconds] { setDuration(seconds); });
  }

  auto *processMenu = menuBar()->addMenu(tr("Process"));
  const QHash<QString, QKeySequence> processShortcuts{
      {QStringLiteral("terminate"), QKeySequence(QStringLiteral("Ctrl+Shift+T"))},
      {QStringLiteral("kill"), QKeySequence(QStringLiteral("Ctrl+Shift+K"))},
      {QStringLiteral("pause"), QKeySequence(QStringLiteral("Ctrl+Shift+P"))},
      {QStringLiteral("resume"), QKeySequence(QStringLiteral("Ctrl+Shift+R"))}};
  for (const auto &[label, actionId] :
       std::array<std::pair<QString, QString>, 4>{
           {{tr("Terminate"), QStringLiteral("terminate")},
            {tr("Kill"), QStringLiteral("kill")},
            {tr("Pause"), QStringLiteral("pause")},
            {tr("Resume"), QStringLiteral("resume")}}}) {
    auto *action = processMenu->addAction(label);
    action->setShortcut(processShortcuts.value(actionId));
    action->setEnabled(false);
    connect(action, &QAction::triggered, this,
            [this, actionId] { triggerProcessAction(actionId); });
    const QString shellId = QStringLiteral("process.") + actionId;
    action->setObjectName(shellId);
    m_processActions.insert(actionId, action);
    appShellTargets.insert(shellId, action);
  }
  auto *priority = processMenu->addAction(tr("Set priority…"));
  priority->setObjectName(QLatin1String(AppShellActionIds::ProcessNice));
  priority->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+N")));
  priority->setEnabled(false);
  m_processActions.insert(QStringLiteral("nice"), priority);
  appShellTargets.insert(QLatin1String(AppShellActionIds::ProcessNice), priority);
  connect(priority, &QAction::triggered, this, [this] {
    bool accepted = false;
    const int nice = QInputDialog::getInt(
        this, tr("Set priority"), tr("Nice value"), 0, -20, 19, 1, &accepted);
    if (accepted) {
      triggerProcessAction(QStringLiteral("nice"), nice);
    }
  });

  const auto published = m_appShell.publishActionCatalog();
  if (!published.ok()) {
    statusBar()->showMessage(published.message, 6000);
    return;
  }
  m_appShell.bindActivationTargets(appShellTargets);
  updateProcessActionAvailability();
  statusBar()->showMessage(tr("Sampling live system state"));
}

QWidget *SystemMonitorWindow::buildOverviewPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(18, 18, 18, 18);
  auto *headline = new QLabel(tr("Overview"));
  headline->setProperty("heading", true);
  layout->addWidget(headline);
  m_overviewSummary = new QLabel;
  m_overviewSummary->setWordWrap(true);
  layout->addWidget(m_overviewSummary);

  auto *split = new QSplitter(Qt::Horizontal, page);
  m_cpuChart =
      new MetricChart(tr("CPU utilization"), QStringLiteral("%"), split);
  m_memoryChart =
      new MetricChart(tr("Memory in use"), QStringLiteral("%"), split);
  split->addWidget(m_cpuChart);
  split->addWidget(m_memoryChart);
  layout->addWidget(split, 1);
  return page;
}

QWidget *SystemMonitorWindow::buildCpuPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *headline = new QLabel(tr("CPU"), page);
  headline->setProperty("heading", true);
  layout->addWidget(headline);
  m_coreTable = new QTableWidget(page);
  m_coreTable->setObjectName(QStringLiteral("coreTable"));
  m_coreTable->setColumnCount(2);
  m_coreTable->setHorizontalHeaderLabels({tr("Core"), tr("Utilization")});
  m_coreTable->horizontalHeader()->setStretchLastSection(true);
  m_coreTable->verticalHeader()->hide();
  m_coreTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_coreTable->setMaximumHeight(150);
  layout->addWidget(m_coreTable);
  auto *chart =
      new MetricChart(tr("CPU utilization"), QStringLiteral("%"), page);
  layout->addWidget(chart, 1);
  connect(&m_controller.engine(),
          &QindaQt::SystemMonitor::MonitorEngine::updated, m_coreTable, [this] {
            const QVariantList cores =
                m_controller.engine().snapshot().value("cores").toList();
            m_coreTable->setRowCount(static_cast<int>(cores.size()));
            for (int row = 0; row < static_cast<int>(cores.size()); ++row) {
              const QVariantMap values = cores.at(row).toMap();
              m_coreTable->setItem(row, 0,
                                   new QTableWidgetItem(tr("Core %1").arg(
                                       values.value("id").toString())));
              m_coreTable->setItem(
                  row, 1,
                  new QTableWidgetItem(tr("%1 %").arg(QString::number(
                      values.value("usage").toDouble(), 'f', 1))));
            }
          });
  connect(&m_controller.engine(),
          &QindaQt::SystemMonitor::MonitorEngine::updated, chart,
          [this, chart] {
            chart->setSamples(m_histories.value(QStringLiteral("cpu")),
                              palette().color(QPalette::Highlight), 100.0);
          });
  return page;
}

QWidget *SystemMonitorWindow::buildMemoryPage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *headline = new QLabel(tr("Memory"), page);
  headline->setProperty("heading", true);
  layout->addWidget(headline);
  m_memoryDetails = new QLabel(page);
  m_memoryDetails->setWordWrap(true);
  layout->addWidget(m_memoryDetails);
  auto *chart = new MetricChart(tr("Memory in use"), QStringLiteral("%"), page);
  layout->addWidget(chart, 1);
  connect(&m_controller.engine(),
          &QindaQt::SystemMonitor::MonitorEngine::updated, chart,
          [this, chart] {
            chart->setSamples(m_histories.value(QStringLiteral("memory")),
                              palette().color(QPalette::Link), 100.0);
          });
  return page;
}

QWidget *SystemMonitorWindow::buildStoragePage(bool network) {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *head = new QHBoxLayout;
  auto *headline = new QLabel(network ? tr("Network") : tr("Disks"), page);
  headline->setProperty("heading", true);
  head->addWidget(headline);
  head->addStretch();
  auto *selector = new QComboBox(page);
  selector->setAccessibleName(network ? tr("Select network interface")
                                      : tr("Select disk"));
  head->addWidget(selector);
  layout->addLayout(head);
  if (!network) {
    m_filesystemTable = new QTableWidget(page);
    m_filesystemTable->setObjectName(QStringLiteral("filesystemTable"));
    m_filesystemTable->setColumnCount(3);
    m_filesystemTable->setHorizontalHeaderLabels(
        {tr("Mount"), tr("Used"), tr("Capacity")});
    m_filesystemTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_filesystemTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_filesystemTable->setWordWrap(false);
    m_filesystemTable->verticalHeader()->hide();
    m_filesystemTable->horizontalHeader()->setStretchLastSection(true);
    m_filesystemTable->setMaximumHeight(150);
    layout->addWidget(m_filesystemTable);
  }

  auto *split = new QSplitter(Qt::Horizontal, page);
  auto *first =
      new MetricChart(network ? tr("Receive") : tr("Read"), tr("B/s"), split);
  auto *second =
      new MetricChart(network ? tr("Send") : tr("Write"), tr("B/s"), split);
  split->addWidget(first);
  split->addWidget(second);
  layout->addWidget(split, 1);
  if (network) {
    m_networkSelector = selector;
    m_networkReceiveChart = first;
    m_networkSendChart = second;
    connect(selector, &QComboBox::currentIndexChanged, this,
            [this] { updateDeviceCharts(true); });
  } else {
    m_diskSelector = selector;
    m_diskReadChart = first;
    m_diskWriteChart = second;
    connect(selector, &QComboBox::currentIndexChanged, this,
            [this] { updateDeviceCharts(false); });
  }
  return page;
}

QWidget *SystemMonitorWindow::buildHardwarePage() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *headline = new QLabel(tr("Hardware"), page);
  headline->setProperty("heading", true);
  layout->addWidget(headline);
  m_hardwareSummary = new QLabel(tr("Waiting for hardware sample…"), page);
  m_hardwareSummary->setWordWrap(true);
  layout->addWidget(m_hardwareSummary);
  auto *selectorLayout = new QFormLayout;
  m_gpuSelector = new QComboBox(page);
  m_gpuSelector->setObjectName(QStringLiteral("gpuSelector"));
  m_gpuSelector->setAccessibleName(tr("Select GPU"));
  selectorLayout->addRow(tr("GPU:"), m_gpuSelector);
  layout->addLayout(selectorLayout);
  m_sensorDetails = new QLabel(page);
  m_sensorDetails->setWordWrap(true);
  layout->addWidget(m_sensorDetails);
  m_gpuChart =
      new MetricChart(tr("GPU utilization"), QStringLiteral("%"), page);
  layout->addWidget(m_gpuChart, 1);
  connect(m_gpuSelector, &QComboBox::currentIndexChanged, this,
          [this] { updateHardware(); });
  return page;
}

void SystemMonitorWindow::setInitialView(const QString &viewId) {
  if (QAction *action = m_viewActions.value(viewId)) {
    action->trigger();
  }
}

void SystemMonitorWindow::setPaused(bool paused) {
  m_controller.engine().setPaused(paused);
  statusBar()->showMessage(
      paused ? tr("Updates paused") : tr("Updates resumed"), 3000);
}

void SystemMonitorWindow::synchronizePausedAction() {
  const bool paused = m_controller.engine().paused();
  if (m_pauseAction) {
    const QSignalBlocker blocker(m_pauseAction);
    m_pauseAction->setChecked(paused);
  }
  static_cast<void>(m_appShell.coordinator().setActionChecked(
      QLatin1String(AppShellActionIds::ViewPause), paused));
}

void SystemMonitorWindow::setInterval(int milliseconds) {
  m_controller.engine().setInterval(milliseconds);
}

void SystemMonitorWindow::synchronizeIntervalActions() {
  recomputeHistoryLimit();
  const int interval = m_controller.engine().interval();
  for (auto it = m_intervalActions.cbegin(); it != m_intervalActions.cend(); ++it) {
    const QSignalBlocker blocker(it.value());
    it.value()->setChecked(it.key() == interval);
    static_cast<void>(m_appShell.coordinator().setActionChecked(
        it.value()->objectName(), it.key() == interval));
  }
}

void SystemMonitorWindow::setDuration(int seconds) {
  m_graphDurationSeconds = seconds;
  recomputeHistoryLimit();
}

void SystemMonitorWindow::recomputeHistoryLimit() {
  // Device histories are UI-owned and bounded independently from the core's
  // aggregate history, so a requested duration remains true after interval changes.
  m_historyLimit = qBound(2, m_graphDurationSeconds * 1000 /
                                  qMax(250, m_controller.engine().interval()),
                          1200);
  trimHistories();
}

void SystemMonitorWindow::openViewInWindow() {
  const QString id =
      m_pageIndex.key(m_pages->currentIndex(), QStringLiteral("overview"));
  Q_EMIT openViewRequested(id);
}

} // namespace QindaQt::Apps::SystemMonitor
