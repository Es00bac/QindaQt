// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"
#include "tests/apps/settings/power/power_navigation_assertions.h"
#include "tests/apps/settings/clipboard/clipboard_settings_center_assertions.h"
#include "tests/apps/settings_center/settings_navigation_page_test_support.h"
#include "tests/apps/settings_center/settings_navigation_page_fixture.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <memory>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::SettingsCenter;
using namespace QindaQt::Apps::SettingsCenter::TestSupport;

namespace {
const char *const SettingsQmlDir = QINDAQT_SETTINGS_SOURCE_DIR;
}

class SettingsNavigationLayoutTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testWideTwoColumnLayoutAndRouteSwitching();
  void testCompactLayoutAdaptation();

private:
  std::unique_ptr<QQmlApplicationEngine> m_engine;
  std::unique_ptr<StubQuietingModel> m_quieting;
  std::unique_ptr<StubAppearanceModel> m_appearance;
  std::unique_ptr<StubNetworkSettingsModel> m_network;
  std::unique_ptr<StubAudioSettingsModel> m_audio;
  std::unique_ptr<StubBluetoothSettingsModel> m_bluetooth;
  std::unique_ptr<StubPowerSettingsModel> m_power;
  std::unique_ptr<StubScreenLockSettings> m_screenLock;
  std::unique_ptr<StubClipboardSettingsModel> m_clipboard;
  std::unique_ptr<StubCustomizeSettingsModel> m_customize;
};

void SettingsNavigationLayoutTest::initTestCase() {
  m_engine = std::make_unique<QQmlApplicationEngine>();
  m_engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  m_engine->addImportPath(QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR));
  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*m_engine, &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString pubError;
  QVERIFY2(facade->publish(loaded.theme, {}, &pubError), qPrintable(pubError));
  m_quieting = std::make_unique<StubQuietingModel>();
  m_appearance = std::make_unique<StubAppearanceModel>();
  m_network = std::make_unique<StubNetworkSettingsModel>();
  m_audio = std::make_unique<StubAudioSettingsModel>();
  m_bluetooth = std::make_unique<StubBluetoothSettingsModel>();
  m_power = std::make_unique<StubPowerSettingsModel>();
  m_screenLock = std::make_unique<StubScreenLockSettings>();
  m_clipboard = std::make_unique<StubClipboardSettingsModel>();
  m_customize = std::make_unique<StubCustomizeSettingsModel>();
}

void SettingsNavigationLayoutTest::testWideTwoColumnLayoutAndRouteSwitching() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry,
                                          QStringLiteral("notifications"));

  QQmlComponent component(m_engine.get());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(SettingsQmlDir) +
                                        QStringLiteral("/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *rootObj = component.createWithInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_quieting.get()))},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_appearance.get()))},
      {QStringLiteral("customizeSettings"), QVariant::fromValue(m_customize.get())},
      {QStringLiteral("networkSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_network.get()))},
      {QStringLiteral("audioSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_audio.get()))},
      {QStringLiteral("bluetoothSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_bluetooth.get()))},
      {QStringLiteral("powerSettings"), QVariant::fromValue(m_power.get())},
      {QStringLiteral("screenLockSettings"), QVariant::fromValue(m_screenLock.get())},
      {QStringLiteral("clipboardSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_clipboard.get()))},
  });
  QVERIFY(rootObj != nullptr);
  std::unique_ptr<QObject> rootGuard(rootObj);

  auto *window = qobject_cast<QQuickWindow *>(rootObj);
  QVERIFY(window != nullptr);
  window->resize(720, 520);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));

  // Wide layout: sidebar should be visible
  auto *sidebar =
      sceneItem(window->contentItem(), QStringLiteral("settingsSidebar"));
  QVERIFY(sidebar != nullptr);
  QVERIFY(sidebar->isVisible());
  auto *sidebarTitle =
      sceneItem(window->contentItem(), QStringLiteral("settingsSidebarTitle"));
  QVERIFY(sidebarTitle != nullptr);
  auto *facade = m_engine->singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      "QindaQt.Tokens", "Tokens");
  QVERIFY(facade != nullptr);
  QCOMPARE(sidebarTitle->property("font").value<QFont>().pointSizeF(),
           facade->type().value(QStringLiteral("subtitle")).toDouble());
  QVERIFY(sceneItem(window->contentItem(),
                    QStringLiteral("settingsSidebarCategory_General")) != nullptr);
  QVERIFY(sceneItem(window->contentItem(),
                    QStringLiteral("settingsSidebarCategory_Personalization")) != nullptr);
  QVERIFY(sceneItem(window->contentItem(),
                    QStringLiteral("settingsSidebarCategory_Hardware")) != nullptr);

  // Notifications route is initially active
  auto *notifLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("wideSettingsRouteNotificationsLoader"));
  auto *appLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("wideSettingsRouteAppearanceLoader"));
  auto *compactNotifLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("compactSettingsRouteNotificationsLoader"));
  QVERIFY(notifLoader != nullptr);
  QVERIFY(appLoader != nullptr);
  QCOMPARE(notifLoader->property("active").toBool(), true);
  QCOMPARE(appLoader->property("active").toBool(), false);
  QVERIFY(compactNotifLoader != nullptr);
  QCOMPARE(compactNotifLoader->property("active").toBool(), false);

  // Nav buttons exist in sidebar
  auto *notifBtn = sceneItem(window->contentItem(),
                             QStringLiteral("settingsNavButton_notifications"));
  auto *appBtn = sceneItem(window->contentItem(),
                           QStringLiteral("settingsNavButton_appearance"));
  auto *networkBtn = sceneItem(window->contentItem(),
                               QStringLiteral("settingsNavButton_network"));
  auto *audioBtn = sceneItem(window->contentItem(),
                             QStringLiteral("settingsNavButton_audio"));
  QVERIFY(notifBtn != nullptr);
  QVERIFY(appBtn != nullptr);
  QVERIFY(networkBtn != nullptr);
  QVERIFY(audioBtn != nullptr);
  QCOMPARE(notifBtn->property("active").toBool(), true);
  QCOMPARE(appBtn->property("active").toBool(), false);
  QCOMPARE(audioBtn->property("active").toBool(), false);
  auto *sidebarAccessible = QAccessible::queryAccessibleInterface(sidebar);
  auto *notifAccessible = QAccessible::queryAccessibleInterface(notifBtn);
  QVERIFY(sidebarAccessible != nullptr);
  QVERIFY(notifAccessible != nullptr);
  QCOMPARE(sidebarAccessible->role(), QAccessible::PageTabList);
  QCOMPARE(notifAccessible->role(), QAccessible::PageTab);
  QVERIFY(notifAccessible->state().selected);

  // Switch to Appearance via navigation controller
  QVERIFY(navigation.selectRoute(QStringLiteral("appearance")));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("appearance"));
  QCOMPARE(notifLoader->property("active").toBool(), false);
  QCOMPARE(appLoader->property("active").toBool(), true);
  QCOMPARE(notifBtn->property("active").toBool(), false);
  QCOMPARE(appBtn->property("active").toBool(), true);
  QVERIFY(window->title().contains(QStringLiteral("Appearance")));

  // Switch back to Notifications
  QVERIFY(navigation.selectRoute(QStringLiteral("notifications")));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));
  QCOMPARE(notifLoader->property("active").toBool(), true);
  QCOMPARE(appLoader->property("active").toBool(), false);
  QVERIFY(window->title().contains(QStringLiteral("Notifications")));
}

void SettingsNavigationLayoutTest::testCompactLayoutAdaptation() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry,
                                          QStringLiteral("notifications"));

  QQmlComponent component(m_engine.get());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(SettingsQmlDir) +
                                        QStringLiteral("/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *rootObj = component.createWithInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_quieting.get()))},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_appearance.get()))},
      {QStringLiteral("customizeSettings"), QVariant::fromValue(m_customize.get())},
      {QStringLiteral("networkSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_network.get()))},
      {QStringLiteral("audioSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_audio.get()))},
      {QStringLiteral("bluetoothSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_bluetooth.get()))},
      {QStringLiteral("powerSettings"), QVariant::fromValue(m_power.get())},
      {QStringLiteral("screenLockSettings"), QVariant::fromValue(m_screenLock.get())},
      {QStringLiteral("clipboardSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_clipboard.get()))},
  });
  QVERIFY(rootObj != nullptr);
  std::unique_ptr<QObject> rootGuard(rootObj);

  auto *window = qobject_cast<QQuickWindow *>(rootObj);
  QVERIFY(window != nullptr);
  // Resize to compact viewport (< 540)
  window->resize(440, 360);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));

  QCOMPARE(rootObj->property("isCompact").toBool(), true);

  auto *compactHeader =
      sceneItem(window->contentItem(), QStringLiteral("settingsCompactHeader"));
  QVERIFY(compactHeader != nullptr);
  QVERIFY(compactHeader->isVisible());
  auto *compactNotifLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("compactSettingsRouteNotificationsLoader"));
  auto *wideNotifLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("wideSettingsRouteNotificationsLoader"));
  QVERIFY(compactNotifLoader != nullptr);
  QVERIFY(wideNotifLoader != nullptr);
  QCOMPARE(compactNotifLoader->property("active").toBool(), true);
  QCOMPARE(wideNotifLoader->property("active").toBool(), false);

  auto *compactNotifications =
      sceneItem(window->contentItem(),
                QStringLiteral("settingsCompactTab_notifications"));
  QVERIFY(compactNotifications != nullptr);
  auto *compactAccessible =
      QAccessible::queryAccessibleInterface(compactNotifications);
  QVERIFY(compactAccessible != nullptr);
  QCOMPARE(compactAccessible->role(), QAccessible::PageTab);
  QVERIFY(compactAccessible->state().selected);

  // Switch to Appearance via navigation controller
  QVERIFY(navigation.selectRoute(QStringLiteral("appearance")));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("appearance"));
  auto *compactAppLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("compactSettingsRouteAppearanceLoader"));
  QVERIFY(compactAppLoader != nullptr);
  QCOMPARE(compactNotifLoader->property("active").toBool(), false);
  QCOMPARE(compactAppLoader->property("active").toBool(), true);

  QTest::keyClick(window, Qt::Key_Escape);
  auto *compactAppearance = sceneItem(
      window->contentItem(), QStringLiteral("settingsCompactTab_appearance"));
  QVERIFY(compactAppearance != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), compactAppearance);

  // The Audio route follows the same compact contract: Ctrl+6 selection,
  // route-tab accessibility, Escape return, and Tab entry into the page.
  QTest::keyClick(window, Qt::Key_6, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("audio"));
  auto *compactAudioLoader =
      sceneObject(window->contentItem(),
                  QStringLiteral("compactSettingsRouteAudioLoader"));
  QVERIFY(compactAudioLoader != nullptr);
  QCOMPARE(compactAudioLoader->property("active").toBool(), true);
  auto *compactAudioTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsCompactTab_audio"));
  QVERIFY(compactAudioTab != nullptr);
  auto *compactAudioAccessible =
      QAccessible::queryAccessibleInterface(compactAudioTab);
  QVERIFY(compactAudioAccessible != nullptr);
  QCOMPARE(compactAudioAccessible->role(), QAccessible::PageTab);
  QCOMPARE(compactAudioAccessible->text(QAccessible::Name),
           QStringLiteral("Audio"));
  QVERIFY(compactAudioAccessible->state().selected);

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(), compactAudioTab);
  QTest::keyClick(window, Qt::Key_Tab);
  auto *compactAudioVolume =
      sceneItem(window->contentItem(), QStringLiteral("audioOutputVolume_10"));
  QVERIFY(compactAudioVolume != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), compactAudioVolume);

  QindaQt::Apps::SettingsPower::TestSupport::verifyCompactPowerNavigation(
      *window, navigation);
  QindaQt::Apps::SettingsClipboard::TestSupport::verifyClipboardRouteInHost(
      *window, navigation, true);
}

QTEST_MAIN(SettingsNavigationLayoutTest)
#include "tst_settings_navigation_layout.moc"
