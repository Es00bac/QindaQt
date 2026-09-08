// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPointF>
#include <QVariantMap>

#include <memory>

#include "../app_shell/system_monitor_app_shell_bridge.h"

class QAction;
class QComboBox;
class QDBusConnection;
class QLabel;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTableWidget;

namespace QindaQt::Apps::SystemMonitor {
class MetricChart;
class MonitorController;
} // namespace QindaQt::Apps::SystemMonitor

namespace QindaQt::Apps::SystemMonitor {

class SystemMonitorWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit SystemMonitorWindow(MonitorController &controller,
                               QWidget *parent = nullptr);
  ~SystemMonitorWindow() override;
  void setInitialView(const QString &viewId);
  void composeMenuExport(const QDBusConnection &connection);
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &
  appShellCoordinator();

Q_SIGNALS:
  void openViewRequested(const QString &viewId);

private:
  void buildChrome();
  QWidget *buildOverviewPage();
  QWidget *buildProcessesPage();
  QWidget *buildCpuPage();
  QWidget *buildMemoryPage();
  QWidget *buildStoragePage(bool network);
  QWidget *buildHardwarePage();
  void updateSnapshot();
  void updateHardware();
  void updateDeviceCharts(bool network);
  void showSelectedProcessDetails();
  void restoreSelectedProcess();
  void updateProcessActionAvailability();
  void triggerProcessAction(const QString &action, int value = 0);
  void setPaused(bool paused);
  void synchronizePausedAction();
  void synchronizeIntervalActions();
  void setInterval(int milliseconds);
  void setDuration(int seconds);
  void recomputeHistoryLimit();
  void openViewInWindow();
  int processRole(const QByteArray &name) const;
  QVariant selectedValue(const QByteArray &role) const;
  void appendSample(const QString &series, qint64 timestamp,
                    const QVariant &value);
  void trimHistories();
  static QString bytes(qint64 value);
  static QString rate(const QVariant &value);

  MonitorController &m_controller;
  SystemMonitorAppShellBridge m_appShell;
  QStackedWidget *m_pages = nullptr;
  QHash<QString, int> m_pageIndex;
  QHash<QString, QAction *> m_viewActions;
  QHash<int, QAction *> m_intervalActions;
  QHash<QString, QAction *> m_processActions;
  qint64 m_selectedProcessPid = -1;
  quint64 m_selectedProcessStartTicks = 0;
  QHash<QString, QVector<QPointF>> m_histories;
  int m_historyLimit = 120;
  int m_graphDurationSeconds = 120;
  MetricChart *m_cpuChart = nullptr;
  MetricChart *m_memoryChart = nullptr;
  MetricChart *m_diskReadChart = nullptr;
  MetricChart *m_diskWriteChart = nullptr;
  MetricChart *m_networkReceiveChart = nullptr;
  MetricChart *m_networkSendChart = nullptr;
  MetricChart *m_gpuChart = nullptr;
  QAction *m_pauseAction = nullptr;
  QComboBox *m_diskSelector = nullptr;
  QComboBox *m_networkSelector = nullptr;
  QComboBox *m_gpuSelector = nullptr;
  QTableView *m_processTable = nullptr;
  QSortFilterProxyModel *m_processProxy = nullptr;
  QLabel *m_processDetails = nullptr;
  QLabel *m_overviewSummary = nullptr;
  QLabel *m_memoryDetails = nullptr;
  QTableWidget *m_filesystemTable = nullptr;
  QLabel *m_hardwareSummary = nullptr;
  QLabel *m_sensorDetails = nullptr;
  QTableWidget *m_coreTable = nullptr;
  std::unique_ptr<QObject> m_menuExport;
};

} // namespace QindaQt::Apps::SystemMonitor
