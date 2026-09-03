// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_bluetooth_settings_model.h"
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtCore/QObject>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsCenter;
using QindaQt::Apps::SettingsBluetooth::TestSupport::StubBluetoothSettingsModel;

namespace {

class StubCustomizeSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool dirty MEMBER dirty NOTIFY changed)

public:
  bool dirty = false;

Q_SIGNALS:
  void changed();
};

} // namespace

class BluetoothWindowCloseTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void waitsForDiscoveryReleaseBeforeClosing();
};

void BluetoothWindowCloseTest::waitsForDiscoveryReleaseBeforeClosing() {
  QQmlApplicationEngine engine;
  engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  engine.addImportPath(QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR));

  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      engine, &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString publishError;
  QVERIFY2(facade->publish(loaded.theme, {}, &publishError),
           qPrintable(publishError));

  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry,
                                          QStringLiteral("bluetooth"));
  QObject unusedRouteModel;
  StubCustomizeSettings customize;
  StubBluetoothSettingsModel bluetooth;
  bluetooth.discoveryLeaseHeld = true;
  bluetooth.departureReleasePending = true;

  QQmlComponent component(&engine);
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.createWithInitialProperties({
      {QStringLiteral("navigation"), QVariant::fromValue(&navigation)},
      {QStringLiteral("quietingSettings"), QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("appearanceSettings"), QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("customizeSettings"), QVariant::fromValue(&customize)},
      {QStringLiteral("bluetoothSettings"), QVariant::fromValue(&bluetooth)},
  }));
  QVERIFY2(root != nullptr, qPrintable(component.errorString()));
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(window != nullptr);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));

  window->close();
  QTRY_VERIFY(window->isVisible());
  QVERIFY(!bluetooth.routeActive);
  QVERIFY(window->property("bluetoothClosePending").toBool());

  bluetooth.departureReleasePending = false;
  Q_EMIT bluetooth.viewChanged();
  QTRY_VERIFY(!window->isVisible());
}

QTEST_MAIN(BluetoothWindowCloseTest)
#include "tst_bluetooth_window_close.moc"
