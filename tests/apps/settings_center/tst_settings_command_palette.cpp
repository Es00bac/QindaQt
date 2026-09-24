// SPDX-License-Identifier: GPL-3.0-or-later
// Settings search palette in the real Main.qml (ADR-0257), offscreen with
// QT_FATAL_WARNINGS: Ctrl+K opens it, typing filters, Enter navigates or
// deep-links, Escape closes only the palette, unavailable routes are listed
// but not selected, and the compact 420x320 window works keyboard-only.
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"
#include "tests/apps/settings/power/power_navigation_assertions.h"
#include "tests/apps/settings_center/settings_navigation_page_fixture.h"
#include "tests/apps/settings_center/settings_navigation_page_test_support.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Apps::SettingsCenter;
using namespace QindaQt::Apps::SettingsCenter::TestSupport;
using QindaQt::Apps::SettingsPower::TestSupport::StubIdleDisplaySettings;
using QindaQt::Apps::SettingsPower::TestSupport::StubScreenLockSettings;

class SettingsCommandPaletteTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void ctrlKFindsNetworkByKeyword();
  void destinationOpensInputSubPageEvenWhenInputIsOpen();
  void escapeClosesOnlyThePalette();
  void compactWindowSearchesByKeyboard();
  void unavailableRouteIsListedWithItsReasonButNotSelected();

private:
  std::unique_ptr<QObject> createWindow(SettingsNavigationController &navigation,
                                        QSize size);
  static QObject *palette(QObject *root);
  static QVariantMap currentRow(QObject *palette);
  static void search(QQuickWindow *window, QObject *palette, const QString &text);

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

void SettingsCommandPaletteTest::initTestCase() {
  m_engine = std::make_unique<QQmlApplicationEngine>();
  m_engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  m_engine->addImportPath(QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR));
  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_engine, &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString publishError;
  QVERIFY2(facade->publish(loaded.theme, {}, &publishError),
           qPrintable(publishError));
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

std::unique_ptr<QObject>
SettingsCommandPaletteTest::createWindow(SettingsNavigationController &navigation,
                                         QSize size) {
  QQmlComponent component(m_engine.get());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/Main.qml")));
  if (!component.isReady()) {
    qWarning("%s", qPrintable(component.errorString()));
    return {};
  }
  const auto object = [](QObject *value) { return QVariant::fromValue(value); };
  std::unique_ptr<QObject> root(component.createWithInitialProperties({
      {QStringLiteral("navigation"), object(&navigation)},
      {QStringLiteral("quietingSettings"), object(m_quieting.get())},
      {QStringLiteral("quietingSchedule"), object(m_quietingSchedule.get())},
      {QStringLiteral("appearanceSettings"), object(m_appearance.get())},
      {QStringLiteral("customizeSettings"), object(m_customize.get())},
      {QStringLiteral("networkSettings"), object(m_network.get())},
      {QStringLiteral("audioSettings"), object(m_audio.get())},
      {QStringLiteral("bluetoothSettings"), object(m_bluetooth.get())},
      {QStringLiteral("powerSettings"), object(m_power.get())},
      {QStringLiteral("screenLockSettings"), object(m_screenLock.get())},
      {QStringLiteral("idleDisplaySettings"), object(m_idleDisplay.get())},
      {QStringLiteral("clipboardSettings"), object(m_clipboard.get())},
  }));
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  if (window == nullptr) {
    return {};
  }
  window->resize(size);
  window->show();
  if (!QTest::qWaitForWindowExposed(window)) {
    return {};
  }
  window->requestActivate();
  return root;
}

QObject *SettingsCommandPaletteTest::palette(QObject *root) {
  return root->findChild<QObject *>(QStringLiteral("settingsCommandPalette"));
}

QVariantMap SettingsCommandPaletteTest::currentRow(QObject *palette) {
  const QVariantList rows = palette->property("rows").toList();
  const int current = palette->property("currentRow").toInt();
  return current >= 0 && current < rows.size() ? rows.at(current).toMap()
                                               : QVariantMap{};
}

// Opens the palette with Ctrl+K and types into it, as a keyboard user would.
void SettingsCommandPaletteTest::search(QQuickWindow *window, QObject *palette,
                                        const QString &text) {
  // A closing palette is still visible and modal during its exit
  // transition, and a modal popup blocks window shortcuts; wait it out.
  QTRY_VERIFY(!palette->property("visible").toBool());
  QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
  QTRY_VERIFY(palette->property("opened").toBool());
  QTRY_VERIFY(window->activeFocusItem() != nullptr);
  auto *contentItem = palette->property("contentItem").value<QQuickItem *>();
  QVERIFY(contentItem != nullptr);
  auto *input = sceneItem(contentItem, QStringLiteral("paletteInput"));
  QVERIFY(input != nullptr);
  QTRY_VERIFY(input->hasActiveFocus());
  // QTest::keyClicks() has no QWindow overload; send the characters singly.
  for (const QChar character : text) {
    QTest::keyClick(window, character.toLatin1());
  }
  QTRY_COMPARE(palette->property("filterText").toString(), text);
}

void SettingsCommandPaletteTest::ctrlKFindsNetworkByKeyword() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault(),
                                          QStringLiteral("notifications"));
  auto root = createWindow(navigation, QSize(720, 520));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QObject *searchPalette = palette(root.get());
  QVERIFY(searchPalette != nullptr);
  QVERIFY(!searchPalette->property("visible").toBool());

  // With no query every route is listed, and the digit shortcut is printed
  // on exactly the first ten registered routes.
  QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
  QTRY_VERIFY(searchPalette->property("opened").toBool());
  QVariantMap shortcutsByRoute;
  for (const QVariant &row : searchPalette->property("rows").toList()) {
    const QVariantMap map = row.toMap();
    if (!map.value(QStringLiteral("header")).toBool()) {
      shortcutsByRoute.insert(map.value(QStringLiteral("id")).toString(),
                              map.value(QStringLiteral("shortcut")));
    }
  }
  QCOMPARE(shortcutsByRoute.size(), 21 + 5);
  QCOMPARE(shortcutsByRoute.value(QStringLiteral("route:notifications")).toString(),
           QStringLiteral("Ctrl+1"));
  QCOMPARE(shortcutsByRoute.value(QStringLiteral("route:color")).toString(),
           QStringLiteral("Ctrl+0"));
  QCOMPARE(shortcutsByRoute.value(QStringLiteral("route:accessibility")).toString(),
           QString());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());

  search(window, searchPalette, QStringLiteral("wifi"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QCOMPARE(searchPalette->property("resultCount").toInt(), 1);
  QCOMPARE(currentRow(searchPalette).value(QStringLiteral("id")).toString(),
           QStringLiteral("route:network"));
  QCOMPARE(currentRow(searchPalette).value(QStringLiteral("shortcut")).toString(),
           QStringLiteral("Ctrl+4"));
  QTest::keyClick(window, Qt::Key_Return);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("network"));
  QTRY_VERIFY(!searchPalette->property("visible").toBool());
  // Focus lands in the page, not back on a destroyed or stale item.
  auto *networkScan =
      sceneItem(window->contentItem(), QStringLiteral("networkScanButton"));
  QVERIFY(networkScan != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), networkScan);

  // "battery" must rank Power first even though About this computer is in an
  // earlier section; the exact keyword outranks a partial one.
  search(window, searchPalette, QStringLiteral("battery"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QCOMPARE(currentRow(searchPalette).value(QStringLiteral("id")).toString(),
           QStringLiteral("route:power"));
  QTest::keyClick(window, Qt::Key_Return);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("power"));
}

void SettingsCommandPaletteTest::destinationOpensInputSubPageEvenWhenInputIsOpen() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault(),
                                          QStringLiteral("notifications"));
  auto root = createWindow(navigation, QSize(720, 520));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QObject *searchPalette = palette(root.get());
  QVERIFY(searchPalette != nullptr);

  search(window, searchPalette, QStringLiteral("shortcuts"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QCOMPARE(currentRow(searchPalette).value(QStringLiteral("id")).toString(),
           QStringLiteral("destination:input/shortcuts"));
  QTest::keyClick(window, Qt::Key_Return);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("input"));
  QCOMPARE(navigation.requestedDestination(), QStringLiteral("shortcuts"));
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("inputDestinationPage_shortcuts")) != nullptr);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());

  // Input is now open: a second destination must still switch the sub-page.
  search(window, searchPalette, QStringLiteral("keyboard"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QCOMPARE(currentRow(searchPalette).value(QStringLiteral("id")).toString(),
           QStringLiteral("destination:input/keyboard"));
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("inputDestinationPage_keyboard")) != nullptr);

  // The user moves inside the page, then searches the destination the
  // controller already holds: the repeated request must still arrive.
  QQuickItem *inputPage = sceneItem(window->contentItem(), QStringLiteral("inputPage"));
  QVERIFY(inputPage != nullptr);
  QVERIFY(QMetaObject::invokeMethod(inputPage, "selectDestination",
                                    Q_ARG(QVariant, QStringLiteral("pointers"))));
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("inputDestinationPage_pointers")) != nullptr);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());
  search(window, searchPalette, QStringLiteral("keyboard"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("inputDestinationPage_keyboard")) != nullptr);
  QCOMPARE(inputPage->property("currentDestination").toString(),
           QStringLiteral("keyboard"));
}

void SettingsCommandPaletteTest::escapeClosesOnlyThePalette() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault(),
                                          QStringLiteral("notifications"));
  auto root = createWindow(navigation, QSize(720, 520));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QObject *searchPalette = palette(root.get());
  QVERIFY(searchPalette != nullptr);
  auto *escape = root->findChild<QObject *>(QStringLiteral("settingsEscapeShortcut"));
  QVERIFY(escape != nullptr);
  QVERIFY(escape->property("enabled").toBool());

  auto *doNotDisturb =
      sceneItem(window->contentItem(), QStringLiteral("settingsDoNotDisturbSwitch"));
  QVERIFY(doNotDisturb != nullptr);
  QTRY_COMPARE(window->activeFocusItem(), doNotDisturb);

  search(window, searchPalette, QStringLiteral("net"));
  if (QTest::currentTestFailed()) {
    return;
  }
  // AGENT-GUARD: two enabled Escape shortcuts are ambiguous in Qt; the host
  // one must stand down while the palette is up.
  QVERIFY(!escape->property("enabled").toBool());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));
  // The host Escape would have moved focus to the Notifications route tab.
  QTRY_COMPARE(window->activeFocusItem(), doNotDisturb);
  QVERIFY(escape->property("enabled").toBool());

  // With the palette gone, Escape is the host's again.
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_COMPARE(window->activeFocusItem(),
               sceneItem(window->contentItem(),
                         QStringLiteral("settingsNavButton_notifications")));
}

void SettingsCommandPaletteTest::compactWindowSearchesByKeyboard() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault(),
                                          QStringLiteral("notifications"));
  auto root = createWindow(navigation, QSize(420, 320));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(root->property("isCompact").toBool());
  QObject *searchPalette = palette(root.get());
  QVERIFY(searchPalette != nullptr);

  auto *searchButton = sceneItem(window->contentItem(),
                                 QStringLiteral("settingsCompactSearchButton"));
  QVERIFY(searchButton != nullptr);
  QVERIFY(searchButton->isVisible());
  QVERIFY(searchButton->width() > 0);
  // The button is keyboard-operable: focus it and press Space.
  searchButton->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Space);
  QTRY_VERIFY(searchPalette->property("opened").toBool());
  QVERIFY(searchPalette->property("width").toReal() <= 420.0);
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());

  search(window, searchPalette, QStringLiteral("shortcuts"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QTest::keyClick(window, Qt::Key_Return);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("input"));
  QCOMPARE(navigation.requestedDestination(), QStringLiteral("shortcuts"));
  QTRY_VERIFY(sceneItem(window->contentItem(),
                        QStringLiteral("inputDestinationPage_shortcuts")) != nullptr);

  // Down arrow moves past the first match; Enter takes the second.
  search(window, searchPalette, QStringLiteral("wi"));
  if (QTest::currentTestFailed()) {
    return;
  }
  const QString first = currentRow(searchPalette).value(QStringLiteral("id")).toString();
  QTest::keyClick(window, Qt::Key_Down);
  const QString second = currentRow(searchPalette).value(QStringLiteral("id")).toString();
  QVERIFY(first != second);
  QVERIFY(second.startsWith(QStringLiteral("route:")));
  QTest::keyClick(window, Qt::Key_Return);
  QCOMPARE(QStringLiteral("route:") + navigation.activeRouteId(), second);
}

void SettingsCommandPaletteTest::unavailableRouteIsListedWithItsReasonButNotSelected() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  QVERIFY(registry.registerRoute(SettingsRoute{
      .id = QStringLiteral("broken"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Broken Route"),
      .description = QStringLiteral("Unavailable hardware"),
      .iconName = QString(),
      .category = QStringLiteral("Hardware"),
      .available = false,
      .unavailableReason = QStringLiteral("Subsystem daemon crashed"),
      .keywords = {QStringLiteral("widget")},
  }));
  SettingsNavigationController navigation(registry, QStringLiteral("notifications"));
  auto root = createWindow(navigation, QSize(720, 520));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QObject *searchPalette = palette(root.get());
  QVERIFY(searchPalette != nullptr);

  search(window, searchPalette, QStringLiteral("broken"));
  if (QTest::currentTestFailed()) {
    return;
  }
  QCOMPARE(searchPalette->property("resultCount").toInt(), 1);
  const QVariantMap row = currentRow(searchPalette);
  QCOMPARE(row.value(QStringLiteral("id")).toString(), QStringLiteral("route:broken"));
  QVERIFY2(row.value(QStringLiteral("label")).toString().contains(
               QStringLiteral("Subsystem daemon crashed")),
           qPrintable(row.value(QStringLiteral("label")).toString()));
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_VERIFY(!searchPalette->property("visible").toBool());
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("notifications"));
}

QTEST_MAIN(SettingsCommandPaletteTest)
#include "tst_settings_command_palette.moc"
