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
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <memory>
#include <QTest>
#include <QUrl>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Apps::SettingsCenter;
using namespace QindaQt::Apps::SettingsCenter::TestSupport;
using QindaQt::Apps::SettingsPower::TestSupport::StubScreenLockSettings;
using QindaQt::Apps::SettingsPower::TestSupport::StubIdleDisplaySettings;

namespace {
const char *const SettingsQmlDir = QINDAQT_SETTINGS_SOURCE_DIR;
}

class SettingsNavigationInteractionTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testKeyboardNavigationAndShortcuts();
  void testNotificationsUseTokenBoundControlsAndActions();
  void testQuietHoursControlsOnlyFollowTheSchedule();
  void testUnavailableRouteFailClosed();

private:
  std::unique_ptr<QQmlApplicationEngine> m_engine;
  std::unique_ptr<StubQuietingModel> m_quieting;
  std::unique_ptr<StubQuietingScheduleModel> m_quietingSchedule;
  std::unique_ptr<StubAppearanceModel> m_appearance;
  std::unique_ptr<StubNetworkSettingsModel> m_network;
  std::unique_ptr<StubAudioSettingsModel> m_audio;
  std::unique_ptr<StubBluetoothSettingsModel> m_bluetooth;
  std::unique_ptr<StubPowerSettingsModel> m_power;
  std::unique_ptr<StubScreenLockSettings> m_screenLock;
  std::unique_ptr<StubIdleDisplaySettings> m_idleDisplay;
  std::unique_ptr<StubClipboardSettingsModel> m_clipboard;
  std::unique_ptr<StubCustomizeSettingsModel> m_customize;
};

void SettingsNavigationInteractionTest::initTestCase() {
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
  m_quietingSchedule = std::make_unique<StubQuietingScheduleModel>();
  m_appearance = std::make_unique<StubAppearanceModel>();
  m_network = std::make_unique<StubNetworkSettingsModel>();
  m_audio = std::make_unique<StubAudioSettingsModel>();
  m_bluetooth = std::make_unique<StubBluetoothSettingsModel>();
  m_power = std::make_unique<StubPowerSettingsModel>();
  m_screenLock = std::make_unique<StubScreenLockSettings>();
  m_idleDisplay = std::make_unique<StubIdleDisplaySettings>();
  m_clipboard = std::make_unique<StubClipboardSettingsModel>();
  m_customize = std::make_unique<StubCustomizeSettingsModel>();
}

void SettingsNavigationInteractionTest::testKeyboardNavigationAndShortcuts() {
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
      {QStringLiteral("quietingSchedule"),
       QVariant::fromValue(static_cast<QObject *>(m_quietingSchedule.get()))},
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
      {QStringLiteral("idleDisplaySettings"), QVariant::fromValue(m_idleDisplay.get())},
      {QStringLiteral("clipboardSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_clipboard.get()))},
  });
  QVERIFY(rootObj != nullptr);
  std::unique_ptr<QObject> rootGuard(rootObj);
  // AGENT-GUARD: Keep Main.qml's optional application-policy model at its null
  // default; this route must remain constructible when its independent catalog
  // is unavailable, and this CTest treats QML warnings as fatal.
  QVERIFY(rootObj->property("applicationPolicies").value<QObject *>() == nullptr);

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
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("settingsDoNotDisturbSwitch")) != nullptr);
  QVERIFY(rootObj->property("applicationPolicies").value<QObject *>() == nullptr);

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

void SettingsNavigationInteractionTest::testNotificationsUseTokenBoundControlsAndActions() {
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
      {QStringLiteral("quietingSchedule"),
       QVariant::fromValue(static_cast<QObject *>(m_quietingSchedule.get()))},
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
      {QStringLiteral("idleDisplaySettings"), QVariant::fromValue(m_idleDisplay.get())},
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
  // The ring runs Do Not Disturb -> schedule switch -> start -> end ->
  // the visible action, so the action is four Tabs away, not one.
  toggle->forceActiveFocus(Qt::TabFocusReason);
  for (int step = 0; step < 4; ++step) {
    QTest::keyClick(window, Qt::Key_Tab);
  }
  QTRY_COMPARE(window->activeFocusItem(), conflict);
  QVERIFY(QMetaObject::invokeMethod(conflict, "click"));
  QCOMPARE(m_quieting->applyCount, 1);

  m_quieting->conflict = false;
  m_quieting->unavailable = true;
  Q_EMIT m_quieting->changed();
  QTRY_VERIFY(retry->isVisible());
  toggle->forceActiveFocus(Qt::TabFocusReason);
  for (int step = 0; step < 4; ++step) {
    QTest::keyClick(window, Qt::Key_Tab);
  }
  QTRY_COMPARE(window->activeFocusItem(), retry);
  QVERIFY(QMetaObject::invokeMethod(retry, "click"));
  QCOMPARE(m_quieting->retryCount, 1);
}

void SettingsNavigationInteractionTest::testQuietHoursControlsOnlyFollowTheSchedule() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry,
                                          QStringLiteral("notifications"));

  m_quieting->conflict = false;
  m_quieting->unavailable = false;
  m_quietingSchedule->available = true;
  m_quietingSchedule->scheduleEnabled = false;
  m_quietingSchedule->startText = QStringLiteral("22:00");
  m_quietingSchedule->endText = QStringLiteral("07:00");
  m_quietingSchedule->summaryText = QStringLiteral("Quiet hours are off");
  m_quietingSchedule->errorText.clear();
  m_quietingSchedule->enabledCount = 0;
  m_quietingSchedule->startCount = 0;
  m_quietingSchedule->endCount = 0;

  QQmlComponent component(m_engine.get());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(SettingsQmlDir) +
                                        QStringLiteral("/Main.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  QObject *rootObj = component.createWithInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_quieting.get()))},
      {QStringLiteral("quietingSchedule"),
       QVariant::fromValue(static_cast<QObject *>(m_quietingSchedule.get()))},
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
      {QStringLiteral("idleDisplaySettings"), QVariant::fromValue(m_idleDisplay.get())},
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

  auto *scheduleSwitch =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursSwitch"));
  auto *start =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursStart"));
  auto *end =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursEnd"));
  auto *summary =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursSummary"));
  auto *error =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursError"));
  auto *status =
      sceneItem(window->contentItem(), QStringLiteral("settingsQuietHoursStatus"));
  QVERIFY(scheduleSwitch != nullptr && start != nullptr && end != nullptr);
  QVERIFY(summary != nullptr && error != nullptr && status != nullptr);

  // QTest has no keyClicks() for a QWindow, so type the masked field a key at
  // a time the way the input method would.
  const auto typeDigits = [window](const QString &digits) {
    for (const QChar digit : digits) {
      QTest::keyClick(window, digit.toLatin1());
    }
  };

  QCOMPARE(summary->property("text").toString(),
           QStringLiteral("Quiet hours are off"));
  QVERIFY(!error->isVisible());
  QCOMPARE(start->property("text").toString(), QStringLiteral("22:00"));
  QCOMPARE(end->property("text").toString(), QStringLiteral("07:00"));
  QVERIFY(!scheduleSwitch->property("checked").toBool());

  // The switch asks the model and shows whatever the model then reports.
  QVERIFY(QMetaObject::invokeMethod(scheduleSwitch, "click"));
  QTRY_COMPARE(m_quietingSchedule->enabledCount, 1);
  QVERIFY(m_quietingSchedule->scheduleEnabled);
  QTRY_VERIFY(scheduleSwitch->property("checked").toBool());

  // The input mask admits 99:99, which is not a time. The model refuses it in
  // silence, so the field must put itself back to the time that is really set
  // -- the page may never claim a quiet window the machine does not have.
  start->forceActiveFocus(Qt::MouseFocusReason);
  QTRY_COMPARE(window->activeFocusItem(), start);
  QVERIFY(QMetaObject::invokeMethod(start, "selectAll"));
  typeDigits(QStringLiteral("9999"));
  QCOMPARE(start->property("text").toString(), QStringLiteral("99:99"));
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_COMPARE(m_quietingSchedule->startCount, 1);
  QCOMPARE(m_quietingSchedule->lastStartHour, 99);
  QCOMPARE(m_quietingSchedule->lastStartMinute, 99);
  QCOMPARE(m_quietingSchedule->startMinutes, 22 * 60);
  QTRY_COMPARE(start->property("text").toString(), QStringLiteral("22:00"));

  // An accepted edit is shown because the model reports it, not because the
  // user typed it.
  end->forceActiveFocus(Qt::MouseFocusReason);
  QTRY_COMPARE(window->activeFocusItem(), end);
  QVERIFY(QMetaObject::invokeMethod(end, "selectAll"));
  typeDigits(QStringLiteral("0630"));
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_COMPARE(m_quietingSchedule->endCount, 1);
  QCOMPARE(m_quietingSchedule->lastEndHour, 6);
  QCOMPARE(m_quietingSchedule->lastEndMinute, 30);
  QCOMPARE(m_quietingSchedule->endMinutes, 6 * 60 + 30);
  QTRY_COMPARE(end->property("text").toString(), QStringLiteral("06:30"));

  // An error from the model is shown, not swallowed.
  m_quietingSchedule->errorText = QStringLiteral("Settings refused the change");
  Q_EMIT m_quietingSchedule->viewChanged();
  QTRY_VERIFY(error->isVisible());
  QCOMPARE(error->property("text").toString(),
           QStringLiteral("Settings refused the change"));

  // Every schedule control is on the keyboard ring between Do Not Disturb and
  // the actions below it.
  auto *toggle = sceneItem(window->contentItem(),
                           QStringLiteral("settingsDoNotDisturbSwitch"));
  QVERIFY(toggle != nullptr);
  toggle->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), scheduleSwitch);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), start);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), end);
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_COMPARE(window->activeFocusItem(), toggle);

  // A pending schedule save leaves the confirmed switch and times visible
  // while the controls are inert and the result state is announced.
  m_quietingSchedule->canEdit = false;
  m_quietingSchedule->pending = true;
  m_quietingSchedule->statusText = QStringLiteral("Checking saved quiet hours");
  Q_EMIT m_quietingSchedule->viewChanged();
  QTRY_VERIFY(!scheduleSwitch->isEnabled());
  QVERIFY(!start->isEnabled());
  QVERIFY(!end->isEnabled());
  QTRY_VERIFY(status->isVisible());
  QCOMPARE(status->property("text").toString(),
           QStringLiteral("Checking saved quiet hours"));
  QVERIFY(scheduleSwitch->property("checked").toBool());
  m_quietingSchedule->canEdit = true;
  m_quietingSchedule->pending = false;
  m_quietingSchedule->statusText.clear();
  Q_EMIT m_quietingSchedule->viewChanged();
  QTRY_VERIFY(scheduleSwitch->isEnabled());

  // A schedule the service cannot serve leaves the controls inert.
  m_quietingSchedule->available = false;
  m_quietingSchedule->canEdit = false;
  Q_EMIT m_quietingSchedule->viewChanged();
  QTRY_VERIFY(!scheduleSwitch->isEnabled());
  QVERIFY(!start->isEnabled());
  QVERIFY(!end->isEnabled());
}

void SettingsNavigationInteractionTest::testUnavailableRouteFailClosed() {
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
      {QStringLiteral("quietingSchedule"),
       QVariant::fromValue(static_cast<QObject *>(m_quietingSchedule.get()))},
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
      {QStringLiteral("idleDisplaySettings"), QVariant::fromValue(m_idleDisplay.get())},
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

QTEST_MAIN(SettingsNavigationInteractionTest)
#include "tst_settings_navigation_interaction.moc"
