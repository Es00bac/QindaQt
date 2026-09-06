// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"
#include "tests/apps/settings/audio/stub_audio_settings_model.h"
#include "tests/apps/settings/network/stub_network_settings_model.h"
#include "tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h"
#include "tests/apps/settings/power/power_navigation_assertions.h"
#include "tests/apps/settings/power/stub_power_settings_model.h"
#include "tests/apps/settings/clipboard/stub_clipboard_settings_model.h"
#include "tests/apps/settings/clipboard/clipboard_settings_center_assertions.h"
#include "tests/apps/settings/customize/stub_customize_settings_model.h"
#include "tests/apps/settings_center/settings_navigation_page_test_support.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::SettingsCenter;
using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsNetwork::TestSupport::StubNetworkSettingsModel;
using QindaQt::Apps::SettingsBluetooth::TestSupport::StubBluetoothSettingsModel;
using QindaQt::Apps::SettingsPower::TestSupport::StubPowerSettingsModel;
using QindaQt::Apps::SettingsClipboard::TestSupport::StubClipboardSettingsModel;
using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;
using QindaQt::Apps::SettingsCenter::TestSupport::sceneItem;
using QindaQt::Apps::SettingsCenter::TestSupport::sceneObject;

namespace {

const char *const SettingsQmlDir = QINDAQT_SETTINGS_SOURCE_DIR;

class StubQuietingModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled MEMBER enabled NOTIFY changed)
  Q_PROPERTY(bool canToggle MEMBER canToggle NOTIFY changed)
  Q_PROPERTY(bool conflict MEMBER conflict NOTIFY changed)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
  Q_PROPERTY(int requestCount MEMBER requestCount NOTIFY changed)
  Q_PROPERTY(int retryCount MEMBER retryCount NOTIFY changed)
  Q_PROPERTY(int applyCount MEMBER applyCount NOTIFY changed)

public:
  bool enabled = false;
  bool canToggle = true;
  bool conflict = false;
  bool unavailable = false;
  QString statusText;
  QString errorText;
  int requestCount = 0;
  int retryCount = 0;
  int applyCount = 0;

  explicit StubQuietingModel(QObject *parent = nullptr) : QObject(parent) {}

  Q_INVOKABLE bool requestSet(bool val) {
    ++requestCount;
    enabled = val;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE void retry() {
    ++retryCount;
    Q_EMIT changed();
  }
  Q_INVOKABLE bool applyMyChoice() {
    ++applyCount;
    Q_EMIT changed();
    return true;
  }

Q_SIGNALS:
  void changed();
};

class StubAppearanceModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
  Q_PROPERTY(bool saving MEMBER saving NOTIFY stateChanged)
  Q_PROPERTY(bool conflict MEMBER conflict NOTIFY stateChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
  Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
  Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
  Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)
  Q_PROPERTY(QString saveResultsText MEMBER saveResultsText NOTIFY stateChanged)
  Q_PROPERTY(bool saveResultsHaveFailure MEMBER saveResultsHaveFailure NOTIFY
                 stateChanged)
  Q_PROPERTY(QVariantMap draft MEMBER draft NOTIFY draftChanged)
  Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
  Q_PROPERTY(QVariantList installedThemes MEMBER installedThemes CONSTANT)
  Q_PROPERTY(QString resolvedThemeId MEMBER resolvedThemeId NOTIFY draftChanged)
  Q_PROPERTY(bool configuredThemeInstalled MEMBER configuredThemeInstalled
                 NOTIFY draftChanged)
  Q_PROPERTY(QString fallbackNotice MEMBER fallbackNotice NOTIFY draftChanged)

public:
  bool loading = false;
  bool ready = true;
  bool saving = false;
  bool conflict = false;
  bool unavailable = false;
  bool canEdit = true;
  bool draftDirty = false;
  bool draftValid = true;
  bool applyAvailable = false;
  QString statusText;
  QString errorText;
  QString saveResultsText;
  bool saveResultsHaveFailure = false;
  QVariantMap draft{
      {QStringLiteral("appearance.theme"), QStringLiteral("qinda-dark")},
      {QStringLiteral("appearance.colorScheme"), QStringLiteral("dark")},
      {QStringLiteral("appearance.wallpaper"), QString{}},
      {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
      {QStringLiteral("display.uiScale"), 1.0},
      {QStringLiteral("fonts.family"), QStringLiteral("Noto Sans")},
      {QStringLiteral("fonts.pointSize"), 10.0},
      {QStringLiteral("fonts.antialiasing"), true},
      {QStringLiteral("fonts.hinting"), QStringLiteral("slight")},
      {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("rgb")},
  };
  QVariantMap fieldErrors;
  QVariantList installedThemes;
  QString resolvedThemeId;
  bool configuredThemeInstalled = true;
  QString fallbackNotice;

  explicit StubAppearanceModel(QObject *parent = nullptr) : QObject(parent) {}

  Q_INVOKABLE bool setDraftValue(const QString &key, const QVariant &value) {
    draft[key] = value;
    draftDirty = true;
    Q_EMIT draftChanged();
    return true;
  }
  Q_INVOKABLE bool applyDraft() { return true; }
  Q_INVOKABLE bool cancelDraft() {
    draftDirty = false;
    Q_EMIT draftChanged();
    return true;
  }
  Q_INVOKABLE void retry() { Q_EMIT stateChanged(); }

Q_SIGNALS:
  void stateChanged();
  void draftChanged();
};

} // namespace

class SettingsNavigationPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testWideTwoColumnLayoutAndRouteSwitching();
  void testCompactLayoutAdaptation();
  void testKeyboardNavigationAndShortcuts();
  void testNotificationsUseTokenBoundControlsAndActions();
  void testUnavailableRouteFailClosed();

private:
  std::unique_ptr<QQmlApplicationEngine> m_engine;
  std::unique_ptr<StubQuietingModel> m_quieting;
  std::unique_ptr<StubAppearanceModel> m_appearance;
  std::unique_ptr<StubNetworkSettingsModel> m_network;
  std::unique_ptr<StubAudioSettingsModel> m_audio;
  std::unique_ptr<StubBluetoothSettingsModel> m_bluetooth;
  std::unique_ptr<StubPowerSettingsModel> m_power;
  std::unique_ptr<StubClipboardSettingsModel> m_clipboard;
  std::unique_ptr<StubCustomizeSettingsModel> m_customize;
};

void SettingsNavigationPageTest::initTestCase() {
  m_engine = std::make_unique<QQmlApplicationEngine>();
  m_engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  m_engine->addImportPath(QString::fromUtf8(SettingsQmlDir));

  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_engine, &facadeError);
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
  m_clipboard = std::make_unique<StubClipboardSettingsModel>();
  m_customize = std::make_unique<StubCustomizeSettingsModel>();
}

void SettingsNavigationPageTest::testWideTwoColumnLayoutAndRouteSwitching() {
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

void SettingsNavigationPageTest::testCompactLayoutAdaptation() {
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

void SettingsNavigationPageTest::testKeyboardNavigationAndShortcuts() {
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

  auto *doNotDisturb = sceneItem(window->contentItem(),
                                 QStringLiteral("settingsDoNotDisturbSwitch"));
  QVERIFY(doNotDisturb != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), doNotDisturb);

  QTest::keyClick(window, Qt::Key_Escape);
  auto *notificationsTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsNavButton_notifications"));
  QVERIFY(notificationsTab != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), notificationsTab);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), doNotDisturb);

  // Shortcut Ctrl+2 switches to appearance
  QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("appearance"));

  // Shortcut Ctrl+1 switches to notifications
  QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));

  // Ctrl+4 selects the production Network route, and its declared first
  // target is the capability-gated scan action.
  QTest::keyClick(window, Qt::Key_4, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("network"));
  auto *networkLoader = sceneObject(
      window->contentItem(), QStringLiteral("wideSettingsRouteNetworkLoader"));
  auto *networkScan =
      sceneItem(window->contentItem(), QStringLiteral("networkScanButton"));
  QVERIFY(networkLoader != nullptr);
  QCOMPARE(networkLoader->property("active").toBool(), true);
  QVERIFY(networkScan != nullptr);
  QTest::keyClick(window, Qt::Key_Escape);
  auto *networkTab = sceneItem(window->contentItem(),
                               QStringLiteral("settingsNavButton_network"));
  QVERIFY(networkTab != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), networkTab);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), networkScan);

  // Shortcut Alt+Left returns to the immediately previous route.
  QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));

  // Ctrl+5 reaches the dedicated panel and applet editor route.
  QTest::keyClick(window, Qt::Key_5, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("customize"));
  QVERIFY(sceneObject(window->contentItem(),
                       QStringLiteral("wideSettingsRouteCustomizeLoader"))
          ->property("active").toBool());

  // Ctrl+6 selects the Audio route; Escape returns to its route tab and Tab
  // enters the page's declared first focus target, the default output's
  // volume slider.
  QTest::keyClick(window, Qt::Key_6, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("audio"));
  auto *audioLoader = sceneObject(
      window->contentItem(), QStringLiteral("wideSettingsRouteAudioLoader"));
  auto *audioVolume =
      sceneItem(window->contentItem(), QStringLiteral("audioOutputVolume_10"));
  QVERIFY(audioLoader != nullptr);
  QCOMPARE(audioLoader->property("active").toBool(), true);
  QVERIFY(audioVolume != nullptr);
  auto *audioTab = sceneItem(window->contentItem(),
                             QStringLiteral("settingsNavButton_audio"));
  QVERIFY(audioTab != nullptr);
  auto *audioTabAccessible =
      QAccessible::queryAccessibleInterface(audioTab);
  QVERIFY(audioTabAccessible != nullptr);
  QCOMPARE(audioTabAccessible->role(), QAccessible::PageTab);
  QCOMPARE(audioTabAccessible->text(QAccessible::Name),
           QStringLiteral("Audio"));
  QVERIFY(audioTabAccessible->state().selected);
  auto *audioVolumeAccessible =
      QAccessible::queryAccessibleInterface(audioVolume);
  QVERIFY(audioVolumeAccessible != nullptr);
  QCOMPARE(audioVolumeAccessible->role(), QAccessible::Slider);
  auto *audioHeading =
      sceneItem(window->contentItem(), QStringLiteral("audioPageHeading"));
  QVERIFY(audioHeading != nullptr);
  auto *audioHeadingAccessible =
      QAccessible::queryAccessibleInterface(audioHeading);
  QVERIFY(audioHeadingAccessible != nullptr);
  QCOMPARE(audioHeadingAccessible->role(), QAccessible::Heading);

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(), audioTab);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), audioVolume);

  // Ctrl+7 selects Bluetooth in its appended position. Escape returns to the
  // route tab, Tab enters the page, and Alt+Left returns to Audio.
  QTest::keyClick(window, Qt::Key_7, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("bluetooth"));
  auto *bluetoothLoader = sceneObject(
      window->contentItem(), QStringLiteral("wideSettingsRouteBluetoothLoader"));
  QVERIFY(bluetoothLoader != nullptr);
  QCOMPARE(bluetoothLoader->property("active").toBool(), true);
  QVERIFY(sceneItem(window->contentItem(), QStringLiteral("bluetoothCloseButton"))
          == nullptr);
  QTest::keyClick(window, Qt::Key_Escape);
  auto *bluetoothTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsNavButton_bluetooth"));
  QVERIFY(bluetoothTab != nullptr);
  auto *bluetoothAccessible =
      QAccessible::queryAccessibleInterface(bluetoothTab);
  QVERIFY(bluetoothAccessible != nullptr);
  QCOMPARE(bluetoothAccessible->role(), QAccessible::PageTab);
  QVERIFY(bluetoothAccessible->state().selected);
  QTRY_COMPARE(window->activeFocusItem(), bluetoothTab);

  QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("audio"));

  // The route-owned helper verifies Ctrl+8, PageTab accessibility, Escape,
  // and Tab entry without growing this shared host matrix past its boundary.
  QindaQt::Apps::SettingsPower::TestSupport::verifyWidePowerNavigation(
      *window, navigation);
  QindaQt::Apps::SettingsClipboard::TestSupport::verifyClipboardRouteInHost(
      *window, navigation, false);
}

void SettingsNavigationPageTest::testNotificationsUseTokenBoundControlsAndActions() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry,
                                          QStringLiteral("notifications"));

  m_quieting->enabled = false;
  m_quieting->canToggle = false;
  m_quieting->conflict = false;
  m_quieting->unavailable = false;
  m_quieting->requestCount = 0;
  m_quieting->retryCount = 0;
  m_quieting->applyCount = 0;

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
      {QStringLiteral("clipboardSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_clipboard.get()))},
  });
  QVERIFY(rootObj != nullptr);
  std::unique_ptr<QObject> rootGuard(rootObj);
  auto *window = qobject_cast<QQuickWindow *>(rootObj);
  QVERIFY(window != nullptr);
  window->resize(900, 640);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window));

  auto *toggle = sceneItem(window->contentItem(),
                           QStringLiteral("settingsDoNotDisturbSwitch"));
  auto *conflict = sceneItem(window->contentItem(),
                             QStringLiteral("settingsConflictApplyButton"));
  auto *retry = sceneItem(window->contentItem(),
                          QStringLiteral("settingsRetryButton"));
  QVERIFY(toggle != nullptr && conflict != nullptr && retry != nullptr);
  QVERIFY(!toggle->isEnabled());
  QVERIFY(sceneItem(window->contentItem(), QStringLiteral("settingsCloseButton"))
          == nullptr);

  m_quieting->canToggle = true;
  Q_EMIT m_quieting->changed();
  QTRY_VERIFY(toggle->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(toggle, "click"));
  QTRY_COMPARE(m_quieting->requestCount, 1);
  QVERIFY(m_quieting->enabled);

  m_quieting->conflict = true;
  Q_EMIT m_quieting->changed();
  QTRY_VERIFY(conflict->isVisible());
  toggle->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), conflict);
  QVERIFY(QMetaObject::invokeMethod(conflict, "click"));
  QCOMPARE(m_quieting->applyCount, 1);

  m_quieting->conflict = false;
  m_quieting->unavailable = true;
  Q_EMIT m_quieting->changed();
  QTRY_VERIFY(retry->isVisible());
  toggle->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), retry);
  QVERIFY(QMetaObject::invokeMethod(retry, "click"));
  QCOMPARE(m_quieting->retryCount, 1);
}

void SettingsNavigationPageTest::testUnavailableRouteFailClosed() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsRoute brokenRoute{
      .id = QStringLiteral("broken"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Broken Route"),
      .description = QStringLiteral("Unavailable hardware"),
      .iconName = QString(),
      .category = QStringLiteral("System"),
      .available = false,
      .unavailableReason = QStringLiteral("Subsystem daemon crashed"),
  };
  QVERIFY(registry.registerRoute(brokenRoute));

  SettingsNavigationController navigation(registry, QStringLiteral("broken"));

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

  // Fail-closed DegradedNotice is active and visible
  auto *notice = sceneItem(window->contentItem(),
                           QStringLiteral("settingsUnavailableNotice"));
  QVERIFY(notice != nullptr);
  QVERIFY(notice->isVisible());
  QCOMPARE(notice->property("reason").toString(),
           QStringLiteral("Subsystem daemon crashed"));
  auto *noticeAccessible = QAccessible::queryAccessibleInterface(notice);
  QVERIFY(noticeAccessible != nullptr);
  QCOMPARE(noticeAccessible->role(), QAccessible::AlertMessage);

  auto *wideBrokenTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsNavButton_broken"));
  QVERIFY(wideBrokenTab != nullptr);
  auto *wideBrokenAccessible =
      QAccessible::queryAccessibleInterface(wideBrokenTab);
  QVERIFY(wideBrokenAccessible != nullptr);
  QCOMPARE(wideBrokenAccessible->role(), QAccessible::PageTab);
  QVERIFY(wideBrokenAccessible->state().selected);
  QVERIFY(wideBrokenTab->isEnabled());
  QVERIFY(wideBrokenAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("Subsystem daemon crashed")));

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(), wideBrokenTab);

  window->resize(440, 360);
  QTRY_VERIFY(rootObj->property("isCompact").toBool());
  auto *compactBrokenTab = sceneItem(
      window->contentItem(), QStringLiteral("settingsCompactTab_broken"));
  QVERIFY(compactBrokenTab != nullptr);
  auto *compactBrokenAccessible =
      QAccessible::queryAccessibleInterface(compactBrokenTab);
  QVERIFY(compactBrokenAccessible != nullptr);
  QCOMPARE(compactBrokenAccessible->role(), QAccessible::PageTab);
  QVERIFY(compactBrokenAccessible->state().selected);
  QVERIFY(compactBrokenTab->isEnabled());
  QVERIFY(compactBrokenAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("Subsystem daemon crashed")));

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(), compactBrokenTab);

  QVERIFY(navigation.selectRoute(QStringLiteral("notifications")));
  QVERIFY(QMetaObject::invokeMethod(compactBrokenTab, "click"));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));
}

QTEST_MAIN(SettingsNavigationPageTest)
#include "tst_settings_navigation_page.moc"
