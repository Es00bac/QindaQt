// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_settings_test_support.h"
#include "stub_network_settings_model.h"
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>
#include <memory>
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_network/network_settings_model.h>
#include <qindaqt/themes/theme_loader.h>
using namespace QindaQt::Apps::SettingsNetwork;
using namespace QindaQt::Apps::SettingsNetwork::TestSupport;
using namespace QindaQt::Network::Client;
class HiddenSettingsTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void absentAgentCannotBeBypassed();
  void literalFormAndAdmission();
};
void HiddenSettingsTests::absentAgentCannotBeBypassed() {
  FakeNetworkTransport transport;
  NetworkClient client(transport);
  NetworkSettingsModel model(
      client, QDBusConnection(QStringLiteral("absent-hidden-agent")));
  transport.setSnapshot(readySnapshot());
  QVERIFY(client.start());
  transport.announceOwner(QStringLiteral(":1.20"));
  QTRY_VERIFY(model.ready());
  QCOMPARE(model.hiddenNetworkDevices().size(), 1);
  QCOMPARE(model.hiddenNetworkDevices()
               .first()
               .toMap()
               .value(QStringLiteral("interfaceName"))
               .toString(),
           QStringLiteral("wlan0"));
  QVERIFY(!model.credentialEntrySupported());
  QVERIFY(!model.hiddenJoinAvailable(QStringLiteral("wlan0"),
                                     QStringLiteral("hidden"), 2));
  QSignalSpy rejected(&model, &NetworkSettingsModel::actionRejected);
  QVERIFY(!model.connectHiddenNetwork(QStringLiteral("wlan0"),
                                      QStringLiteral("hidden"), 2));
  QCOMPARE(transport.operations.size(), 0);
  QCOMPARE(rejected.size(), 1);
  QCOMPARE(rejected.first().at(0).toString(),
           QStringLiteral("secret-agent-unavailable"));
}
namespace {
QQuickItem *hiddenItem(QQuickItem *root, const QString &name) {
  if (root->objectName() == name)
    return root;
  for (auto *child : root->childItems()) {
    if (auto *found = hiddenItem(child, name))
      return found;
  }
  return nullptr;
}
} // namespace
void HiddenSettingsTests::literalFormAndAdmission() {
  QQuickView view;
  view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *view.engine(), &error);
  QVERIFY2(facade, qPrintable(error));
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY(theme.ok);
  QVERIFY(facade->publish(theme.theme, {}, &error));
  StubNetworkSettingsModel model;
  QQmlComponent component(view.engine());
  component.loadFromModule(QStringLiteral("QindaQt.SettingsApp.Network"),
                           QStringLiteral("NetworkHiddenJoinSection"));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> object(component.createWithInitialProperties(
      {{QStringLiteral("networkSettings"),
        QVariant::fromValue<QObject *>(&model)}}));
  QVERIFY(object);
  auto *root = qobject_cast<QQuickItem *>(object.get());
  QVERIFY(root);
  root->setParentItem(view.contentItem());
  root->setWidth(640);
  view.resize(640, 480);
  view.show();
  QCoreApplication::processEvents();
  auto *ssid = hiddenItem(root, QStringLiteral("networkHiddenSsid"));
  QVERIFY(ssid);
  auto *join = hiddenItem(root, QStringLiteral("networkHiddenJoin"));
  QVERIFY(join);
  QVERIFY(!join->property("enabled").toBool());
  const QString literal = QStringLiteral("  <b>name</b>  ");
  QVERIFY(ssid->setProperty("text", literal));
  QCoreApplication::processEvents();
  QVERIFY(join->property("enabled").toBool());
  QVERIFY(QMetaObject::invokeMethod(join, "clicked"));
  QCOMPARE(model.hiddenSsid, literal);
  QCOMPARE(model.hiddenDevice, QStringLiteral("wlan0"));
  QCOMPARE(model.hiddenSecurity, 2u);
  model.secretAgentRegistered = false;
  Q_EMIT model.viewChanged();
  QCoreApplication::processEvents();
  QVERIFY(!join->property("enabled").toBool());
  QVERIFY(root->findChild<QObject *>(QStringLiteral("password")) == nullptr);
}
QTEST_MAIN(HiddenSettingsTests)
#include "tst_network_hidden_join.moc"
