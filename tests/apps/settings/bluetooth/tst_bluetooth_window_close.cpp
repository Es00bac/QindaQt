// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_bluetooth_settings_model.h"
#include "bluetooth_settings_test_support.h"
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtCore/QObject>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsCenter;
using QindaQt::Apps::SettingsBluetooth::TestSupport::StubBluetoothSettingsModel;
using namespace QindaQt::Apps::SettingsBluetooth;
using namespace QindaQt::Apps::SettingsBluetooth::TestSupport;
using namespace QindaQt::Bluetooth;

namespace {

class StubCustomizeSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool dirty MEMBER dirty NOTIFY changed)

public:
  bool dirty = false;

Q_SIGNALS:
  void changed();
};

QQuickItem *sceneItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == objectName) return root;
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *match = sceneItem(child, objectName); match != nullptr)
      return match;
  }
  return nullptr;
}

} // namespace

class BluetoothWindowCloseTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void waitsForDiscoveryReleaseBeforeClosing();
  void unsuccessfulAcquireDuringCloseCompletesClose_data();
  void unsuccessfulAcquireDuringCloseCompletesClose();
  void compactHostProvidesBluetoothFocusPath();
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

void BluetoothWindowCloseTest::
    unsuccessfulAcquireDuringCloseCompletesClose_data() {
  QTest::addColumn<QString>("outcome");
  QTest::newRow("rejected-too-many-leases") << QStringLiteral("rejected");
  QTest::newRow("uncertain") << QStringLiteral("uncertain");
  QTest::newRow("owner-loss") << QStringLiteral("owner-loss");
  QTest::newRow("owner-replacement") << QStringLiteral("owner-replacement");
}

void BluetoothWindowCloseTest::unsuccessfulAcquireDuringCloseCompletesClose() {
  QFETCH(QString, outcome);
  FakeBluetoothTransport transport;
  BluetoothClient client{&transport};
  BluetoothSettingsModel model{client};
  client.start();
  transport.announceOwner(QStringLiteral(":1.42"));
  QCOMPARE(transport.fetches.size(), 1);
  transport.finishSnapshot(QStringLiteral(":1.42"),
                           transport.fetches.constFirst().second,
                           readySnapshot());

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
  QQmlComponent component(&engine);
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.createWithInitialProperties({
      {QStringLiteral("navigation"), QVariant::fromValue(&navigation)},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("customizeSettings"), QVariant::fromValue(&customize)},
      {QStringLiteral("bluetoothSettings"), QVariant::fromValue(&model)},
  }));
  QVERIFY2(root != nullptr, qPrintable(component.errorString()));
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(window != nullptr);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));
  QTRY_VERIFY(model.routeActive());

  QVERIFY(model.requestDiscovery(QStringLiteral("adapter-61-400"), true));
  QCOMPARE(transport.submissions.size(), 1);
  const auto acquire = transport.submissions.constLast();
  window->close();
  QTRY_VERIFY(window->isVisible());
  QVERIFY(window->property("bluetoothClosePending").toBool());

  if (outcome == QStringLiteral("owner-loss")) {
    transport.announceOwner({});
  } else if (outcome == QStringLiteral("owner-replacement")) {
    transport.announceOwner(QStringLiteral(":1.84"));
  } else {
    const OperationStatus status = outcome == QStringLiteral("uncertain")
        ? OperationStatus::Uncertain : OperationStatus::Rejected;
    const OperationResult result{
        .kind = OperationKind::AcquireDiscovery,
        .status = status,
        .initiatingEpoch = 61,
        .initiatingRevision = 5,
        .observedEpoch = 61,
        .observedRevision = 5,
        .reasonCode = status == OperationStatus::Uncertain
            ? QStringLiteral("operation-timeout")
            : QStringLiteral("too-many-leases"),
        .diagnostic = {},
        .wireValid = true,
    };
    transport.finishOperation(acquire, result);
  }
  QTRY_VERIFY(!model.busy());
  QVERIFY(!model.discoveryLeaseHeld());
  QVERIFY(!model.departureReleasePending());
  QTRY_VERIFY(!window->isVisible());
  QCOMPARE(transport.submissions.size(), 1);

  if (!client.owner().isEmpty()) {
    QTRY_COMPARE(transport.fetches.size(), 2);
    const quint64 epoch = client.owner() == QStringLiteral(":1.42") ? 61 : 72;
    transport.finishSnapshot(client.owner(),
                             transport.fetches.constLast().second,
                             readySnapshot(epoch, 8, false));
  }
  QVERIFY(!window->isVisible());
}

void BluetoothWindowCloseTest::compactHostProvidesBluetoothFocusPath() {
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
  QQmlComponent component(&engine);
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.createWithInitialProperties({
      {QStringLiteral("navigation"), QVariant::fromValue(&navigation)},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(&unusedRouteModel)},
      {QStringLiteral("customizeSettings"), QVariant::fromValue(&customize)},
      {QStringLiteral("bluetoothSettings"), QVariant::fromValue(&bluetooth)},
  }));
  QVERIFY2(root != nullptr, qPrintable(component.errorString()));
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(window != nullptr);
  window->resize(440, 360);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));
  QVERIFY(root->property("isCompact").toBool());

  auto *bluetoothTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsCompactTab_bluetooth"));
  auto *bluetoothClose = sceneItem(
      window->contentItem(), QStringLiteral("bluetoothCloseButton"));
  QVERIFY(bluetoothTab != nullptr);
  QVERIFY(bluetoothClose != nullptr);
  auto *accessible = QAccessible::queryAccessibleInterface(bluetoothTab);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::PageTab);
  QVERIFY(accessible->state().selected);

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(), bluetoothTab);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), bluetoothClose);
  QVERIFY(bluetoothClose->isEnabled());
}

QTEST_MAIN(BluetoothWindowCloseTest)
#include "tst_bluetooth_window_close.moc"
