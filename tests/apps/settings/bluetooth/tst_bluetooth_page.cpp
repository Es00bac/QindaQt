// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_bluetooth_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsBluetooth::TestSupport::StubBluetoothSettingsModel;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}
} // namespace

class BluetoothPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersWideInventoryAndPairingActions();
  void routesPowerDiscoveryAndConnectionActions();
  void rendersAccessibleInlinePairingPrompt();
  void keepsCompactFallbackFocusEnabled();
  void presentsUnavailableAndBusyTruth();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubBluetoothSettingsModel> m_model;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void BluetoothPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &error);
  QVERIFY2(facade != nullptr, qPrintable(error));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
BluetoothPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubBluetoothSettingsModel>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_BLUETOOTH_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("bluetoothSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  if (object == nullptr) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto guard = std::unique_ptr<QObject>(object);
  auto *page = qobject_cast<QQuickItem *>(object);
  if (page == nullptr) return {};
  m_view->resize(size);
  page->setParentItem(m_view->contentItem());
  page->setSize(size);
  m_view->show();
  QCoreApplication::processEvents();
  return {std::move(guard), page};
}

void BluetoothPageTest::rendersWideInventoryAndPairingActions() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *power = findItem(page, QStringLiteral("bluetoothPower_adapter-61-400"));
  auto *discover = findItem(
      page, QStringLiteral("bluetoothDiscovery_adapter-61-400"));
  auto *disconnect = findItem(
      page, QStringLiteral("bluetoothDisconnect_device-61-700"));
  auto *connect = findItem(
      page, QStringLiteral("bluetoothConnect_device-61-701"));
  auto *classIcon = findItem(
      page, QStringLiteral("bluetoothClassIcon_device-61-700"));
  auto *pair = findItem(page, QStringLiteral("bluetoothPair_device-61-702"));
  auto *trust = findItem(page, QStringLiteral("bluetoothTrust_device-61-701"));
  auto *forget = findItem(page, QStringLiteral("bluetoothForget_device-61-701"));
  auto *adapterLayout = findItem(
      page, QStringLiteral("bluetoothAdapterLayout_adapter-61-400"));
  QVERIFY(power != nullptr);
  QVERIFY(discover != nullptr);
  QVERIFY(disconnect != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(classIcon != nullptr);
  QVERIFY(pair != nullptr);
  QVERIFY(trust != nullptr);
  QVERIFY(forget != nullptr);
  QCOMPARE(classIcon->property("deviceIconName").toString(), QStringLiteral("audio-headphones"));
  QVERIFY(adapterLayout != nullptr);
  QCOMPARE(adapterLayout->property("columns").toInt(), 3);
  QVERIFY(power->isEnabled());
  QVERIFY(discover->isEnabled());
  QVERIFY(connect->isEnabled());
  QVERIFY(pair->isEnabled());
  QVERIFY(trust->isEnabled());
  QVERIFY(forget->isEnabled());
  auto *powerAccessible = QAccessible::queryAccessibleInterface(power);
  QVERIFY(powerAccessible != nullptr);
  QCOMPARE(powerAccessible->role(), QAccessible::CheckBox);
  QVERIFY(powerAccessible->state().checked);
}

void BluetoothPageTest::routesPowerDiscoveryAndConnectionActions() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *power = findItem(page, QStringLiteral("bluetoothPower_adapter-61-400"));
  auto *discover = findItem(
      page, QStringLiteral("bluetoothDiscovery_adapter-61-400"));
  auto *connect = findItem(
      page, QStringLiteral("bluetoothConnect_device-61-701"));
  auto *pair = findItem(page, QStringLiteral("bluetoothPair_device-61-702"));
  QVERIFY(power != nullptr);
  QVERIFY(discover != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(pair != nullptr);
  QVERIFY(QMetaObject::invokeMethod(power, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(discover, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(connect, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(pair, "clicked"));
  QCOMPARE(m_model->powerRequests, 1);
  QCOMPARE(m_model->discoveryRequests, 1);
  QCOMPARE(m_model->connectionRequests, 1);
  QCOMPARE(m_model->pairingRequests, 1);
  QCOMPARE(m_model->lastDevice, QStringLiteral("device-61-702"));
}

void BluetoothPageTest::rendersAccessibleInlinePairingPrompt() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  m_model->pairingPrompt = {
      {QStringLiteral("active"), true},
      {QStringLiteral("kind"), QStringLiteral("confirm-passkey")},
      {QStringLiteral("deviceLabel"), QStringLiteral("New phone")},
      {QStringLiteral("detail"), QStringLiteral("123456")},
      {QStringLiteral("serviceUuid"), QString{}},
      {QStringLiteral("entered"), 0},
      {QStringLiteral("confirmationAvailable"), true},
      {QStringLiteral("passkeyInput"), false},
      {QStringLiteral("pinInput"), false},
  };
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *message = findItem(page, QStringLiteral("bluetoothPairingMessage"));
  auto *confirm = findItem(page, QStringLiteral("bluetoothPairingConfirm"));
  auto *cancel = findItem(page, QStringLiteral("bluetoothPairingCancel"));
  QVERIFY(message != nullptr);
  QVERIFY(confirm != nullptr);
  QVERIFY(cancel != nullptr);
  QVERIFY(message->property("text").toString().contains(QStringLiteral("123456")));
  auto *confirmAccessible = QAccessible::queryAccessibleInterface(confirm);
  auto *cancelAccessible = QAccessible::queryAccessibleInterface(cancel);
  QVERIFY(confirmAccessible != nullptr);
  QVERIFY(cancelAccessible != nullptr);
  QCOMPARE(confirmAccessible->role(), QAccessible::Button);
  QCOMPARE(cancelAccessible->role(), QAccessible::Button);
  QVERIFY(!confirmAccessible->text(QAccessible::Description).isEmpty());
  QVERIFY(!cancelAccessible->text(QAccessible::Description).isEmpty());
  QVERIFY(QMetaObject::invokeMethod(confirm, "clicked"));
  QCOMPARE(m_model->promptReplies, 1);
  QVERIFY(m_model->lastBoolean);
  confirm->forceActiveFocus();
  QTRY_VERIFY(confirm->hasActiveFocus());
  QTest::keyClick(m_view.get(), Qt::Key_Escape);
  QTRY_COMPARE(m_model->promptReplies, 2);
  QVERIFY(!m_model->lastBoolean);
}

void BluetoothPageTest::keepsCompactFallbackFocusEnabled() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *close = findItem(page, QStringLiteral("bluetoothCloseButton"));
  auto *adapterLayout = findItem(
      page, QStringLiteral("bluetoothAdapterLayout_adapter-61-400"));
  QVERIFY(close != nullptr);
  QVERIFY(close->isEnabled());
  QVERIFY(adapterLayout != nullptr);
  QCOMPARE(adapterLayout->property("columns").toInt(), 1);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), close);
  close->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), close);
  auto *closeAccessible = QAccessible::queryAccessibleInterface(close);
  QVERIFY(closeAccessible != nullptr);
  QCOMPARE(closeAccessible->role(), QAccessible::Button);
}

void BluetoothPageTest::presentsUnavailableAndBusyTruth() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  m_model->ready = false;
  m_model->unavailable = true;
  m_model->busy = true;
  m_model->adapters.clear();
  m_model->devices.clear();
  m_model->statusText = QStringLiteral("The Bluetooth service is unavailable.");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *state = findItem(page, QStringLiteral("bluetoothServiceState"));
  auto *close = findItem(page, QStringLiteral("bluetoothCloseButton"));
  QVERIFY(state != nullptr);
  QCOMPARE(state->property("status").toInt(), 3); // StateCard.Error
  QVERIFY(close != nullptr);
  QVERIFY(close->isEnabled());
}

QTEST_MAIN(BluetoothPageTest)
#include "tst_bluetooth_page.moc"
