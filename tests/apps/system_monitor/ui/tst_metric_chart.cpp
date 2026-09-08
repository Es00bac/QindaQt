// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/metric_chart.h"
#include "ui/monitor_controller.h"
#include "ui/system_monitor_appearance.h"
#include "ui/system_monitor_window.h"

#include "qindaqt/themes/theme_loader.h"

#include "core/monitor_engine.h"
#include "core/process_model.h"
#include "core/sample_types.h"
#include "hardware/hardware_sampler.h"

#include <QAction>
#include <QApplication>
#include <QStringList>
#include <QComboBox>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QtTest>

#include <cmath>
#include <limits>
#include <memory>

using QindaQt::Apps::SystemMonitor::MetricChart;
using QindaQt::Apps::SystemMonitor::MonitorController;
using QindaQt::Apps::SystemMonitor::SystemMonitorWindow;
using QindaQt::SystemMonitor::MonitorEngine;

class MetricChartTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rendersAtCompactAndWideWidths();
  void rendersLiveWindowAtCompactAndWideWidths();
  void keepsRemainingViewAliveAfterPeerCloses();
  void synchronizesPauseAndGpuSelectionAcrossUpdates();
  void retainsProcessSelectionAcrossPidChurn();
};

void MetricChartTest::rendersAtCompactAndWideWidths() {
  MetricChart chart(QStringLiteral("CPU utilization"), QStringLiteral("%"));
  chart.setLegend(QStringLiteral("Live samples"));
  chart.setSamples({QPointF(1000, 10), QPointF(2000, 55),
                    QPointF(3000, std::numeric_limits<double>::quiet_NaN()),
                    QPointF(4000, 80)},
                   QApplication::palette().highlight().color(), 100.0);

  for (const QSize size : {QSize(800, 600), QSize(1440, 800)}) {
    chart.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    chart.render(&painter);
    painter.end();
    QVERIFY2(!image.isNull(), "Chart must render at the requested viewport.");
    QVERIFY(image.pixelColor(size.width() / 2, size.height() / 2).alpha() > 0);
  }
}

void MetricChartTest::rendersLiveWindowAtCompactAndWideWidths() {
  const QString themePath = QDir(QString::fromLatin1(QT_TESTCASE_SOURCEDIR))
                                .absoluteFilePath(QStringLiteral(
                                    "../../../../data/themes/qinda-dark.json"));
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(themePath);
  QVERIFY2(theme.ok, qPrintable(theme.error));
  const auto appearance =
      QindaQt::Apps::SystemMonitor::SystemMonitorAppearanceAdapter::fromTheme(
          theme.theme);
  QVERIFY2(appearance.ok(), qPrintable(appearance.diagnostic));
  QApplication::setPalette(appearance.appearance->palette);

  MonitorEngine engine;
  engine.setInterval(250);
  QSignalSpy updated(&engine, &MonitorEngine::updated);
  const auto hardware = std::make_shared<QindaQt::SystemMonitor::HardwareSampler>();
  MonitorController controller(engine, [hardware] { return hardware->sample(); });
  SystemMonitorWindow window(controller);
  QTRY_VERIFY_WITH_TIMEOUT(updated.count() >= 3, 2500);
  const QString outputDirectory =
      qEnvironmentVariable("QINDAQT_SYSTEM_MONITOR_SCREENSHOT_DIR");

  const QStringList views{QStringLiteral("Overview"), QStringLiteral("Processes"),
                          QStringLiteral("CPU"), QStringLiteral("Memory"),
                          QStringLiteral("Disks"), QStringLiteral("Network"),
                          QStringLiteral("Hardware")};
  for (const QSize size : {QSize(800, 600), QSize(1440, 800)}) {
    window.resize(size);
    window.show();
    for (const QString &view : views) {
      QAction *action = nullptr;
      for (QAction *candidate : window.findChildren<QAction *>()) {
        if (candidate->text() == view) {
          action = candidate;
          break;
        }
      }
      QVERIFY2(action, qPrintable(QStringLiteral("Missing view action: %1").arg(view)));
      action->trigger();
      QTest::qWait(20);
      const QPixmap image = window.grab();
      QCOMPARE(image.size(), size);
      QVERIFY(!image.toImage().isNull());
      if (!outputDirectory.isEmpty()) {
        QDir().mkpath(outputDirectory);
        const QString pageName = view.toLower();
        QVERIFY2(
            image.save(QDir(outputDirectory)
                           .filePath(QStringLiteral("system-monitor-%1-%2x%3.png")
                                         .arg(pageName)
                                         .arg(size.width())
                                         .arg(size.height()))),
            "The live window render must be writable for visual review.");
      }
    }
  }
  window.close();
}

void MetricChartTest::keepsRemainingViewAliveAfterPeerCloses() {
  MonitorEngine engine;
  MonitorController controller(engine, [] { return QVariantMap{}; });
  auto *first = new SystemMonitorWindow(controller);
  auto *second = new SystemMonitorWindow(controller);
  first->show();
  second->show();
  QTest::qWait(40);

  delete first;
  QVERIFY(second->isVisible());
  delete second;
}

void MetricChartTest::synchronizesPauseAndGpuSelectionAcrossUpdates() {
  MonitorEngine engine;
  int hardwareGeneration = 0;
  MonitorController controller(engine, [&hardwareGeneration] {
    ++hardwareGeneration;
    const QVariantMap firstGpu{{QStringLiteral("id"), QStringLiteral("gpu-a")},
                               {QStringLiteral("name"), QStringLiteral("GPU A")},
                               {QStringLiteral("busy"), 18.0},
                               {QStringLiteral("memoryUsed"), 1024LL},
                               {QStringLiteral("memoryTotal"), 4096LL}};
    const QVariantMap secondGpu{{QStringLiteral("id"), QStringLiteral("gpu-b")},
                                {QStringLiteral("name"), QStringLiteral("GPU B")},
                                {QStringLiteral("busy"), 42.0},
                                {QStringLiteral("memoryUsed"), 2048LL},
                                {QStringLiteral("memoryTotal"), 8192LL}};
    return QVariantMap{{QStringLiteral("gpus"),
                        QVariantList{firstGpu, secondGpu}}};
  });
  QSignalSpy hardwareUpdated(&controller, &MonitorController::hardwareUpdated);
  SystemMonitorWindow first(controller);
  SystemMonitorWindow second(controller);
  first.show();
  second.show();
  QVERIFY(hardwareUpdated.wait(1500));

  QAction *firstPause = nullptr;
  QAction *secondPause = nullptr;
  for (QAction *action : first.findChildren<QAction *>()) {
    if (action->text() == QStringLiteral("Pause updates")) {
      firstPause = action;
    }
  }
  for (QAction *action : second.findChildren<QAction *>()) {
    if (action->text() == QStringLiteral("Pause updates")) {
      secondPause = action;
    }
  }
  QVERIFY(firstPause);
  QVERIFY(secondPause);
  engine.setPaused(true);
  QCOMPARE(firstPause->isChecked(), true);
  QCOMPARE(secondPause->isChecked(), true);
  engine.setPaused(false);
  QCOMPARE(firstPause->isChecked(), false);
  QCOMPARE(secondPause->isChecked(), false);
  engine.setInterval(500);
  QCOMPARE(first.findChild<QAction *>(QStringLiteral("view.interval-500"))->isChecked(), true);
  QCOMPARE(second.findChild<QAction *>(QStringLiteral("view.interval-500"))->isChecked(), true);

  auto *selector = first.findChild<QComboBox *>(QStringLiteral("gpuSelector"));
  QVERIFY(selector);
  QCOMPARE(selector->count(), 2);
  selector->setCurrentIndex(1);
  QCOMPARE(selector->currentData().toMap().value(QStringLiteral("id")).toString(),
           QStringLiteral("gpu-b"));
  controller.refreshHardware();
  QVERIFY(hardwareUpdated.wait(1500));
  QCOMPARE(selector->currentData().toMap().value(QStringLiteral("id")).toString(),
           QStringLiteral("gpu-b"));
}

void MetricChartTest::retainsProcessSelectionAcrossPidChurn() {
  MonitorEngine engine;
  MonitorController controller(engine, [] { return QVariantMap{}; });
  SystemMonitorWindow window(controller);
  auto *model = qobject_cast<QindaQt::SystemMonitor::ProcessModel *>(
      engine.processes());
  QVERIFY(model);
  QindaQt::SystemMonitor::ProcessSample retained;
  retained.counters.pid = 4242;
  retained.counters.startTicks = 77;
  retained.counters.name = QStringLiteral("retained");
  retained.counters.memoryAvailable = true;
  retained.counters.memoryBytes = 1024;
  QindaQt::SystemMonitor::ProcessSample departed;
  departed.counters.pid = 1234;
  departed.counters.startTicks = 10;
  departed.counters.name = QStringLiteral("departed");
  model->replace({retained, departed});

  auto *table = window.findChild<QTableView *>(QStringLiteral("processTable"));
  QVERIFY(table);
  table->selectionModel()->setCurrentIndex(
      table->model()->index(0, 0),
      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  QAction *terminate =
      window.findChild<QAction *>(QStringLiteral("process.terminate"));
  QVERIFY(terminate);
  QVERIFY(terminate->isEnabled());

  QindaQt::SystemMonitor::ProcessSample arriving;
  arriving.counters.pid = 9999;
  arriving.counters.startTicks = 12;
  arriving.counters.name = QStringLiteral("arriving");
  model->replace({arriving, retained});
  QTRY_VERIFY(table->selectionModel()->currentIndex().isValid());
  const QModelIndex source = qobject_cast<QSortFilterProxyModel *>(table->model())
                                 ->mapToSource(table->selectionModel()->currentIndex());
  QCOMPARE(model->data(source, QindaQt::SystemMonitor::ProcessModel::PidRole)
               .toLongLong(),
           4242LL);
  QCOMPARE(model->data(source, QindaQt::SystemMonitor::ProcessModel::StartTicksRole)
               .toULongLong(),
           77ULL);
  QVERIFY(terminate->isEnabled());
}

QTEST_MAIN(MetricChartTest)
#include "tst_metric_chart.moc"
