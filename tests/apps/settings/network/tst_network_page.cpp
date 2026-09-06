// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_network_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_network/network_settings_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtCore/QMetaObject>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsNetwork::TestSupport::StubNetworkSettingsModel;

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *match = findItem(child, objectName); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

void attach(QQuickView &view, QQuickItem &page, const QSize size) {
  view.resize(size);
  page.setParentItem(view.contentItem());
  page.setSize(size);
  view.show();
  QCoreApplication::processEvents();
}

} // namespace

class NetworkPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersTruthAndSecretBoundaryAccessibly();
  void routesScanConnectDisconnectAndReloadIntents();
  void showsStaleTruthReadOnlyAndOwnerLossEmpty();
  void keepsCompactFocusVisibleWithoutAPageCloseAction();
  void supportsDocumentPagingKeys();
  void stubMatchesRealModelSurface();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubNetworkSettingsModel> m_model;

  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(const QSize size);
};

void NetworkPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString publishError;
  QVERIFY2(facade->publish(loaded.theme, {}, &publishError),
           qPrintable(publishError));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
NetworkPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubNetworkSettingsModel>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_NETWORK_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("networkSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  if (object == nullptr) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto guard = std::unique_ptr<QObject>(object);
  auto *page = qobject_cast<QQuickItem *>(object);
  if (page == nullptr) {
    return {};
  }
  attach(*m_view, *page, size);
  return {std::move(guard), page};
}

void NetworkPageTest::rendersTruthAndSecretBoundaryAccessibly() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  QVERIFY(findItem(page, QStringLiteral("networkPageHeading")) != nullptr);
  auto *state = findItem(page, QStringLiteral("networkServiceState"));
  auto *credential =
      findItem(page, QStringLiteral("networkCredentialBoundary"));
  auto *scan = findItem(page, QStringLiteral("networkScanButton"));
  auto *connect = findItem(
      page, QStringLiteral("networkConnect_") + QString(64, u'b'));
  auto *disconnect =
      findItem(page, QStringLiteral("networkDisconnect_wlan0"));
  auto *visibleConnect = findItem(
      page, QStringLiteral("networkConnectVisible_") + QString(64, u'c'));
  auto *visiblePrompt = findItem(
      page, QStringLiteral("networkVisiblePrompt_") + QString(64, u'c'));
  QVERIFY(state != nullptr);
  QVERIFY(credential != nullptr);
  QVERIFY(scan != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(disconnect != nullptr);
  QVERIFY(visibleConnect != nullptr);
  QVERIFY(visiblePrompt != nullptr);
  QVERIFY(scan->isEnabled());
  QVERIFY(connect->isEnabled());
  QVERIFY(disconnect->isEnabled());
  QVERIFY(visibleConnect->isEnabled());
  QVERIFY(visiblePrompt->property("text").toString().contains(
      QStringLiteral("password prompt"), Qt::CaseInsensitive));

  auto *credentialAccessible =
      QAccessible::queryAccessibleInterface(credential);
  auto *scanAccessible = QAccessible::queryAccessibleInterface(scan);
  QVERIFY(credentialAccessible != nullptr);
  QVERIFY(scanAccessible != nullptr);
  QCOMPARE(credentialAccessible->role(), QAccessible::StaticText);
  QVERIFY(credentialAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("credential prompt"),
                        Qt::CaseInsensitive));
  QCOMPARE(scanAccessible->role(), QAccessible::Button);
}

void NetworkPageTest::routesScanConnectDisconnectAndReloadIntents() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  auto *reload = findItem(page, QStringLiteral("networkReloadButton"));
  auto *scan = findItem(page, QStringLiteral("networkScanButton"));
  auto *connect = findItem(
      page, QStringLiteral("networkConnect_") + QString(64, u'b'));
  auto *disconnect =
      findItem(page, QStringLiteral("networkDisconnect_wlan0"));
  auto *visibleConnect = findItem(
      page, QStringLiteral("networkConnectVisible_") + QString(64, u'c'));
  QVERIFY(reload != nullptr);
  QVERIFY(scan != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(disconnect != nullptr);
  QVERIFY(visibleConnect != nullptr);

  QVERIFY(QMetaObject::invokeMethod(reload, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(scan, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(connect, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(disconnect, "clicked"));
  QVERIFY(QMetaObject::invokeMethod(visibleConnect, "clicked"));
  QCOMPARE(m_model->reloadCount, 1);
  QCOMPARE(m_model->scanCount, 1);
  QCOMPARE(m_model->connectedNetwork, QString(64, u'b'));
  QCOMPARE(m_model->disconnectedDevice, QStringLiteral("wlan0"));
  QCOMPARE(m_model->connectedAccessPoint, QString(64, u'c'));
}

void NetworkPageTest::showsStaleTruthReadOnlyAndOwnerLossEmpty() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  m_model->ready = false;
  m_model->degraded = true;
  m_model->stale = true;
  m_model->scanAvailable = false;
  QVariantMap known = m_model->knownNetworks.at(0).toMap();
  known[QStringLiteral("connectAvailable")] = false;
  m_model->knownNetworks[0] = known;
  QVariantMap device = m_model->devices.at(0).toMap();
  device[QStringLiteral("disconnectAvailable")] = false;
  m_model->devices[0] = device;
  m_model->statusText = QStringLiteral("Network information is stale");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();

  auto *scan = findItem(page, QStringLiteral("networkScanButton"));
  auto *connect = findItem(
      page, QStringLiteral("networkConnect_") + QString(64, u'b'));
  auto *state = findItem(page, QStringLiteral("networkServiceState"));
  QVERIFY(scan != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(state != nullptr);
  QVERIFY(!scan->isEnabled());
  QVERIFY(!connect->isEnabled());
  QCOMPARE(state->property("status").toInt(), 2); // StateCard.Warning

  m_model->stale = false;
  m_model->degraded = false;
  m_model->unavailable = true;
  m_model->serviceOwner.clear();
  m_model->radios.clear();
  m_model->devices.clear();
  m_model->knownNetworks.clear();
  m_model->accessPoints.clear();
  m_model->statusText = QStringLiteral("The network service is unavailable");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QVERIFY(findItem(page, QStringLiteral("networkConnect_") + QString(64, u'b'))
          == nullptr);
}

void NetworkPageTest::keepsCompactFocusVisibleWithoutAPageCloseAction() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *scan = findItem(page, QStringLiteral("networkScanButton"));
  auto *connect = findItem(
      page, QStringLiteral("networkConnect_") + QString(64, u'b'));
  auto *viewport = findItem(page, QStringLiteral("networkFormViewport"));
  QVERIFY(scan != nullptr);
  QVERIFY(connect != nullptr);
  QVERIFY(findItem(page, QStringLiteral("networkCloseButton")) == nullptr);
  QVERIFY(viewport != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), scan);

  scan->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), scan);
  connect->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), connect);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
}

void NetworkPageTest::supportsDocumentPagingKeys() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *scan = findItem(page, QStringLiteral("networkScanButton"));
  auto *viewport = findItem(page, QStringLiteral("networkFormViewport"));
  QVERIFY(scan != nullptr);
  QVERIFY(viewport != nullptr);
  scan->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), scan);

  viewport->setProperty("contentY", 0.0);
  QTest::keyClick(m_view.get(), Qt::Key_PageDown);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
  QTest::keyClick(m_view.get(), Qt::Key_End, Qt::ControlModifier);
  const qreal maximum = qMax(
      0.0, viewport->property("contentHeight").toReal()
               - viewport->property("height").toReal());
  QTRY_COMPARE(viewport->property("contentY").toReal(), maximum);
  QTest::keyClick(m_view.get(), Qt::Key_PageUp);
  QTRY_VERIFY(viewport->property("contentY").toReal() < maximum);
  QTest::keyClick(m_view.get(), Qt::Key_Home, Qt::ControlModifier);
  QTRY_COMPARE(viewport->property("contentY").toReal(), 0.0);
}

void NetworkPageTest::stubMatchesRealModelSurface() {
  const auto propertySurface = [](const QMetaObject &meta) {
    QStringList surface;
    for (int index = meta.propertyOffset(); index < meta.propertyCount();
         ++index) {
      const QMetaProperty property = meta.property(index);
      surface.append(QString::fromLatin1(property.name()) + u':'
                     + QString::fromLatin1(property.typeName()));
    }
    surface.sort();
    return surface;
  };
  const auto invokableSurface = [](const QMetaObject &meta) {
    QStringList surface;
    for (int index = meta.methodOffset(); index < meta.methodCount(); ++index) {
      const QMetaMethod method = meta.method(index);
      if (method.methodType() == QMetaMethod::Method) {
        surface.append(QString::fromLatin1(method.methodSignature()));
      }
    }
    surface.sort();
    return surface;
  };

  QCOMPARE(propertySurface(
               QindaQt::Apps::SettingsNetwork::NetworkSettingsModel::staticMetaObject),
           propertySurface(StubNetworkSettingsModel::staticMetaObject));
  QCOMPARE(invokableSurface(
               QindaQt::Apps::SettingsNetwork::NetworkSettingsModel::staticMetaObject),
           invokableSurface(StubNetworkSettingsModel::staticMetaObject));
}

QTEST_MAIN(NetworkPageTest)
#include "tst_network_page.moc"
