// SPDX-License-Identifier: GPL-3.0-or-later

#include "color_navigation_assertions.h"
#include "stub_color_settings_model.h"
#include "tests/apps/settings/power/stub_power_settings_model.h"

#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsCenter;
using QindaQt::Apps::SettingsColor::TestSupport::StubColorSettingsModel;
using QindaQt::Apps::SettingsPower::TestSupport::StubScreenLockSettings;
using QindaQt::Apps::SettingsPower::TestSupport::StubIdleDisplaySettings;

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

// Color-route-owned Settings Center host proof: tenth-route Ctrl+0 selection,
// PageTab accessibility, Escape/Tab focus entry, and wide/compact Loader
// exclusivity through the real Main.qml. Kept out of the shared navigation
// page test, which is at its decomposition budget.
class ColorNavigationPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void wideHostNavigatesToColor();
  void compactHostNavigatesToColor();

private:
  std::unique_ptr<QQmlApplicationEngine> m_engine;
  QObject m_unusedRouteModel;
  StubCustomizeSettings m_customize;
  StubColorSettingsModel m_color;
  StubScreenLockSettings m_screenLock;
  StubIdleDisplaySettings m_idleDisplay;
  std::unique_ptr<QObject> createWindow(
      SettingsNavigationController &navigation, const QSize &size);
};

void ColorNavigationPageTest::initTestCase() {
  m_engine = std::make_unique<QQmlApplicationEngine>();
  m_engine->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
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
}

std::unique_ptr<QObject>
ColorNavigationPageTest::createWindow(
    SettingsNavigationController &navigation, const QSize &size) {
  QQmlComponent component(m_engine.get());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/Main.qml")));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return nullptr;
  }
  std::unique_ptr<QObject> root(component.createWithInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_unusedRouteModel))},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_unusedRouteModel))},
      {QStringLiteral("customizeSettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_customize))},
      {QStringLiteral("colorSettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_color))},
      {QStringLiteral("screenLockSettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_screenLock))},
      {QStringLiteral("idleDisplaySettings"),
       QVariant::fromValue(static_cast<QObject *>(&m_idleDisplay))},
  }));
  if (root == nullptr) {
    qWarning().noquote() << component.errorString();
    return nullptr;
  }
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  if (window == nullptr) return nullptr;
  window->resize(size);
  window->show();
  if (!QTest::qWaitForWindowExposed(window)) return nullptr;
  return root;
}

void ColorNavigationPageTest::wideHostNavigatesToColor() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  QCOMPARE(registry.indexOf(QStringLiteral("color")), 9);
  SettingsNavigationController navigation(registry, QStringLiteral("power"));
  auto root = createWindow(navigation, QSize(720, 520));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(window != nullptr);

  QindaQt::Apps::SettingsColor::TestSupport::verifyWideColorNavigation(
      *window, navigation);

  // The compact host's Color loader stays inactive in the wide layout.
  auto *compactLoader = QindaQt::Apps::SettingsColor::TestSupport::colorSceneItem(
      window->contentItem(), QStringLiteral("compactSettingsRouteColorLoader"));
  QVERIFY(compactLoader != nullptr);
  QVERIFY(!compactLoader->property("active").toBool());
}

void ColorNavigationPageTest::compactHostNavigatesToColor() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  SettingsNavigationController navigation(registry, QStringLiteral("power"));
  auto root = createWindow(navigation, QSize(440, 360));
  QVERIFY(root != nullptr);
  auto *window = qobject_cast<QQuickWindow *>(root.get());
  QVERIFY(window != nullptr);
  QVERIFY(root->property("isCompact").toBool());

  QindaQt::Apps::SettingsColor::TestSupport::verifyCompactColorNavigation(
      *window, navigation);

  // The wide host's Color loader stays inactive in the compact layout.
  auto *wideLoader = QindaQt::Apps::SettingsColor::TestSupport::colorSceneItem(
      window->contentItem(), QStringLiteral("wideSettingsRouteColorLoader"));
  QVERIFY(wideLoader != nullptr);
  QVERIFY(!wideLoader->property("active").toBool());
}

QTEST_MAIN(ColorNavigationPageTest)
#include "tst_color_navigation_page.moc"
