// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

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

public:
  bool enabled = false;
  bool canToggle = true;
  bool conflict = false;
  bool unavailable = false;
  QString statusText;
  QString errorText;

  explicit StubQuietingModel(QObject *parent = nullptr) : QObject(parent) {}

  Q_INVOKABLE bool requestSet(bool val) {
    enabled = val;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE void retry() { Q_EMIT changed(); }
  Q_INVOKABLE bool applyMyChoice() {
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
  void testUnavailableRouteFailClosed();

private:
  std::unique_ptr<QQmlApplicationEngine> m_engine;
  std::unique_ptr<StubQuietingModel> m_quieting;
  std::unique_ptr<StubAppearanceModel> m_appearance;
};

namespace {

QQuickItem *sceneItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (auto *match = sceneItem(child, objectName); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

QObject *sceneObject(QQuickItem *root, const QString &objectName) {
  return sceneItem(root, objectName);
}

} // namespace

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
  QVERIFY(notifBtn != nullptr);
  QVERIFY(appBtn != nullptr);
  QCOMPARE(notifBtn->property("active").toBool(), true);
  QCOMPARE(appBtn->property("active").toBool(), false);
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

  // Shortcut Alt+Left returns to previous route (appearance)
  QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("appearance"));
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
}

QTEST_MAIN(SettingsNavigationPageTest)
#include "tst_settings_navigation_page.moc"
