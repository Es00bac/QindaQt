// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/apps/settings/power/power_route_composition.h"

#include <qindaqt/apps/settings_power/external_display_brightness_model.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsPower;

// CTest poisons both host buses, and XDG roots live under the test binary
// directory, so the real composition contacts no host service or preference.
class PowerRouteCompositionTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void injectsExternalDisplayBrightnessIntoThePowerModel();

private:
  std::unique_ptr<QTemporaryDir> m_home;
};

void PowerRouteCompositionTest::initTestCase() {
  m_home = std::make_unique<QTemporaryDir>(
      QCoreApplication::applicationDirPath() +
      QStringLiteral("/power-composition-home-XXXXXX"));
  QVERIFY(m_home->isValid());
  qputenv("XDG_CONFIG_HOME", (m_home->path() + QStringLiteral("/config")).toLocal8Bit());
  qputenv("XDG_DATA_HOME", (m_home->path() + QStringLiteral("/data")).toLocal8Bit());
  qputenv("XDG_STATE_HOME", (m_home->path() + QStringLiteral("/state")).toLocal8Bit());
  qputenv("XDG_CACHE_HOME", (m_home->path() + QStringLiteral("/cache")).toLocal8Bit());
}

void PowerRouteCompositionTest::injectsExternalDisplayBrightnessIntoThePowerModel() {
  const PowerRouteComposition composition;
  const QObject *model = composition.model();
  QVERIFY(model != nullptr);
  auto *external = qobject_cast<ExternalDisplayBrightnessModel *>(
      model->property("externalBrightness").value<QObject *>());
  QVERIFY2(external != nullptr,
           "the Power model has no injected external-display brightness rows");

  // Without a reachable Display1 there is no row, fence, or request.
  QTest::qWait(100);
  QVERIFY(external->rows().isEmpty());
  QVERIFY(!external->busy());
  QVERIFY(!external->requestBrightness(QStringLiteral("external-1"), 5'000));
  QVERIFY(!external->busy());
}

QTEST_GUILESS_MAIN(PowerRouteCompositionTest)
#include "tst_power_route_composition.moc"
